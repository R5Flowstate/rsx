#include "pch.h"
#include "effect.h"

#include <game/rtech/cpakfile.h>
#include <game/rtech/utils/utils.h>
#include <thirdparty/imgui/imgui.h>
#include <deque>
#include <map>
#include <mutex>

extern ExportSettings_t g_ExportSettings;
static const char* const s_PathPrefixEFCT = s_AssetTypePaths.find(AssetType_t::EFCT)->second;

// ---------------------------------------------------------------------------
// efct rawData = the ParticleDefinition (name + operators). cpu() is the relocated
// base; for the 16-byte (V2) header the data is reached via the first header pointer.
// The layout is VERSION-DEPENDENT, so the operator-list
// base offset is dispatched per era. The effect NAME is a relocated char* at base+0x00
// in EVERY era. We only walk operators at an offset we have VERIFIED for that version;
// V7's operators are embedded (0x1A0 holds strings, not op pointers) and not yet decoded,
// so we emit the name + a marker rather than dereferencing a guessed offset.
// ---------------------------------------------------------------------------
enum class EfctLayout { Unknown, V2_16B, V5_24B, V7_24B, Baked_24B };

static EfctLayout ClassifyEffect(const uint32_t headerStructSize, const int version)
{
    if (headerStructSize == 16) return EfctLayout::V2_16B;       // S3 V2/V3
    if (headerStructSize == 24)
    {
        if (version >= 16) return EfctLayout::Baked_24B;         // v16 / v31 (oplists @ 0x160 / 0x188)
        if (version >= 7)  return EfctLayout::V7_24B;            // S10/S11 v7 (embedded ops, 0x1A0 = strings)
        return EfctLayout::V5_24B;                               // S7 v5/v6 (oplists @ 0x1A0)
    }
    return EfctLayout::Unknown;
}

// operator-list base offset, VERIFIED per asset version (0 = not decoded, don't walk).
// The baked ParticleDefinition's parms block grew across seasons, shifting the
// operatorLists: v16 parms=312B -> oplists @ 0x160; v31 parms=352B -> @ 0x188
// (both = 0x28 header + parms).
static size_t OperatorListOffset(const EfctLayout layout, const int version)
{
    switch (layout)
    {
    case EfctLayout::Baked_24B:
        if (version == 16) return 0x160; // v16
        if (version == 31) return 0x188; // v31
        return 0;                        // other baked versions: parms size unknown
    case EfctLayout::V5_24B:    return 0x1A0;
    case EfctLayout::V2_16B:    return 0x140;
    default:                    return 0; // V7 / Unknown: not decoded
    }
}

static constexpr int EFCT_NUM_OPLISTS = 6;
static const char* const s_EfctListNames[EFCT_NUM_OPLISTS] = {
    "initializers", "operators", "renderers", "emitters", "forcegenerators", "constraints"
};

static bool PtrLooksValid(const void* p)
{
    return p != nullptr && reinterpret_cast<uintptr_t>(p) >= 0x10000ull;
}

static std::string ReadName(const char* const p)
{
    if (!PtrLooksValid(p))
        return std::string();
    std::string out;
    for (size_t i = 0; i < 1024; i++) { const char c = p[i]; if (!c) break; out.push_back(c); }
    return out;
}

static void JsonEscape(std::ostream& os, const std::string& s)
{
    for (const char c : s)
    {
        switch (c)
        {
        case '\\': os << "\\\\"; break;
        case '"':  os << "\\\""; break;
        case '\n': os << "\\n";  break;
        case '\r': os << "\\r";  break;
        case '\t': os << "\\t";  break;
        default:   if (static_cast<unsigned char>(c) < 0x20) os << ' '; else os << c;
        }
    }
}

// Dump name (+ operators where the layout is verified) to <name>.efct_def.json.
static void ExportEffectParticleDef(CPakAsset* const pakAsset, const std::string& outBase,
    const uint64_t guid, const int version, const uint32_t hdrSize)
{
    const EfctLayout layout = ClassifyEffect(hdrSize, version);

    // Resolve the ParticleDefinition base: cpu() normally; first header pointer for V2.
    const uint8_t* base = reinterpret_cast<const uint8_t*>(pakAsset->cpu());
    if (layout == EfctLayout::V2_16B)
    {
        const uint8_t* const hp = reinterpret_cast<const uint8_t*>(pakAsset->header());
        if (hp)
        {
            const uint8_t* const viaHeader = *reinterpret_cast<const uint8_t* const*>(hp + 0x00);
            if (PtrLooksValid(viaHeader)) base = viaHeader;
        }
    }

    const char* layoutName =
        layout == EfctLayout::V2_16B ? "v2_16byte" :
        layout == EfctLayout::V5_24B ? "v5_24byte" :
        layout == EfctLayout::V7_24B ? "v7_24byte" :
        layout == EfctLayout::Baked_24B ? "baked_24byte" : "unknown";

    std::ofstream f(outBase + ".efct_def.json", std::ios::out);
    if (!f.is_open())
        return;

    f << "{\n";
    f << "  \"guid\": \"0x" << std::uppercase << std::hex << guid << std::dec << "\",\n";
    f << "  \"version\": " << version << ",\n";
    f << "  \"headerStructSize\": " << hdrSize << ",\n";
    f << "  \"layout\": \"" << layoutName << "\",\n";

    if (!PtrLooksValid(base))
    {
        // No resolvable ParticleDefinition pointer -- announce loudly, never a silent ghost.
        printf("[EFCT-DEF] 0x%llX v%d (%s): no ParticleDefinition base (cpu=%p)\n",
            guid, version, layoutName, pakAsset->cpu());
        f << "  \"name\": null,\n  \"parsed\": false\n}\n";
        f.close();
        return;
    }

    const char* const namePtr = *reinterpret_cast<const char* const*>(base + 0x00);
    const std::string name = ReadName(namePtr);
    const uint64_t opsCheckSum = *reinterpret_cast<const uint64_t*>(base + 0x08);

    f << "  \"name\": \""; JsonEscape(f, name); f << "\",\n";
    f << "  \"opsCheckSum\": \"0x" << std::uppercase << std::hex << opsCheckSum << std::dec << "\",\n";

    const size_t opOff = OperatorListOffset(layout, version);
    if (opOff == 0)
    {
        // V7 operators are embedded at a layout we have not yet decoded; do
        // not dereference a guessed offset. Name is still authoritative.
        f << "  \"operatorsDecoded\": false\n}\n";
        f.close();
        return;
    }

    f << "  \"operatorsDecoded\": true,\n";
    f << "  \"operatorLists\": {\n";
    for (int L = 0; L < EFCT_NUM_OPLISTS; L++)
    {
        const uint8_t* const list = base + opOff + (static_cast<size_t>(L) * 0x10);
        const uint8_t* const* const opPtrs = *reinterpret_cast<const uint8_t* const* const*>(list + 0x00);
        const uint64_t opCount = *reinterpret_cast<const uint64_t*>(list + 0x08);

        f << "    \"" << s_EfctListNames[L] << "\": [";
        if (PtrLooksValid(opPtrs) && opCount > 0 && opCount < 4096)
        {
            bool first = true;
            for (uint64_t k = 0; k < opCount; k++)
            {
                const uint8_t* const op = opPtrs[k];
                if (!PtrLooksValid(op)) continue;
                const uint16_t opTypeIndex = *reinterpret_cast<const uint16_t*>(op + 0x00);
                const uint16_t tsSize      = *reinterpret_cast<const uint16_t*>(op + 0x02);
                if (!first) f << ", ";
                first = false;
                f << "{\"opTypeIndex\": " << opTypeIndex << ", \"typeSpecificParmsSize\": " << tsSize << "}";
            }
        }
        f << "]" << (L + 1 < EFCT_NUM_OPLISTS ? "," : "") << "\n";
    }
    f << "  }\n}\n";
    f.close();
}

// Where every pointed-to allocation starts, per page: each pointer target, each asset's
// cpu start, and each page end. A region runs from one of these to the next.
struct PakRegionMap_s
{
    struct Page_s { const char* start; const char* end; int index; };
    struct Pointer_s { uint32_t field; int targetPage; uint32_t targetOff; };

    std::vector<Page_s> pagesByAddress;
    std::vector<std::vector<uint32_t>> boundaries;  // per page, sorted
    std::vector<std::vector<Pointer_s>> pointers;   // per page, sorted by field

    bool Locate(const char* const ptr, int& page, uint32_t& off) const
    {
        auto it = std::upper_bound(pagesByAddress.begin(), pagesByAddress.end(), ptr,
            [](const char* p, const Page_s& pg) { return p < pg.start; });
        if (it == pagesByAddress.begin())
            return false;
        --it;
        if (ptr >= it->end)
            return false;
        page = it->index;
        off = static_cast<uint32_t>(ptr - it->start);
        return true;
    }
};

static const PakRegionMap_s& Pak_GetRegionMap(CPakFile* const pak)
{
    static std::mutex s_mutex;
    static std::unordered_map<const CPakFile*, std::unique_ptr<PakRegionMap_s>> s_cache;

    std::lock_guard<std::mutex> lock(s_mutex);

    if (const auto it = s_cache.find(pak); it != s_cache.end())
        return *it->second;

    auto map = std::make_unique<PakRegionMap_s>();
    const std::vector<char*>& buffers = pak->GetPageBuffers();
    const PakPageHdr_t* const pageHeaders = pak->header()->GetPageHeaders();
    const int pageCount = pak->pageCount();

    map->boundaries.resize(pageCount);
    map->pointers.resize(pageCount);

    for (int p = 0; p < pageCount; ++p)
    {
        if (!buffers[p])
            continue;
        map->pagesByAddress.push_back({ buffers[p], buffers[p] + pageHeaders[p].size, p });
        map->boundaries[p].push_back(pageHeaders[p].size);
    }
    std::sort(map->pagesByAddress.begin(), map->pagesByAddress.end(),
        [](const auto& a, const auto& b) { return a.start < b.start; });

    for (int i = 0; i < pak->assetCount(); ++i)
    {
        const uint64_t guid = *reinterpret_cast<const uint64_t*>(pak->rawAsset(i));
        CPakAsset* const other = g_assetData.FindAssetByGUID<CPakAsset>(guid);
        int page; uint32_t off;
        if (other && other->GetContainerFile<CPakFile>() == pak && other->cpu() && map->Locate(other->cpu(), page, off))
            map->boundaries[page].push_back(off);
    }

    const PakPointerHdr_t* const ptrHeaders = pak->GetPointerHeaders();
    for (int i = 0; i < pak->pointerCount(); ++i)
    {
        const int fieldPage = ptrHeaders[i].index;
        if (fieldPage < 0 || fieldPage >= pageCount || !buffers[fieldPage])
            continue;

        const uint32_t field = static_cast<uint32_t>(ptrHeaders[i].offset);
        const char* const target = *reinterpret_cast<const char* const*>(buffers[fieldPage] + field);

        int targetPage; uint32_t targetOff;
        if (!target || !map->Locate(target, targetPage, targetOff))
            continue;

        map->boundaries[targetPage].push_back(targetOff);
        map->pointers[fieldPage].push_back({ field, targetPage, targetOff });
    }

    for (int p = 0; p < pageCount; ++p)
    {
        std::vector<uint32_t>& b = map->boundaries[p];
        std::sort(b.begin(), b.end());
        b.erase(std::unique(b.begin(), b.end()), b.end());

        std::sort(map->pointers[p].begin(), map->pointers[p].end(),
            [](const auto& a, const auto& b) { return a.field < b.field; });
    }

    return *(s_cache[pak] = std::move(map));
}

size_t Pak_RegionSizeAt(CPakAsset* const asset, const void* const ptr)
{
    CPakFile* const pak = asset->GetContainerFile<CPakFile>();
    if (!pak || !ptr)
        return 0;

    const PakRegionMap_s& map = Pak_GetRegionMap(pak);

    int page; uint32_t off;
    if (!map.Locate(static_cast<const char*>(ptr), page, off))
        return 0;

    const std::vector<uint32_t>& b = map.boundaries[page];
    return *std::upper_bound(b.begin(), b.end(), off) - off;
}

// Re-packable <name>.efct_def, the input of RePak's efct v16 writer:
// ['EFCT'][version 1][blobSize][pointerCount][blob][pointerCount x (fieldOffset, targetOffset)],
// both offsets blob-relative. The blob is the definition plus everything reachable from it
// by pointer -- the operator arrays and operators live in a persistent segment apart from
// the definition, so a contiguous slice would miss them.
static bool ExportEffectRawDef(CPakAsset* const asset, const std::string& outBase)
{
    CPakFile* const pak = asset->GetContainerFile<CPakFile>();
    if (!pak || !asset->cpu())
        return false;

    const PakRegionMap_s& map = Pak_GetRegionMap(pak);
    const std::vector<char*>& buffers = pak->GetPageBuffers();

    int rootPage; uint32_t rootOff;
    if (!map.Locate(asset->cpu(), rootPage, rootOff))
    {
        printf("[EFCT-RAW] 0x%llX: definition not in any page\n", asset->GetAssetGUID());
        return false;
    }

    std::vector<char> blob;
    std::vector<std::pair<uint32_t, uint32_t>> links;
    std::map<std::pair<int, uint32_t>, uint32_t> placed; // (page, offset) -> blob offset
    std::deque<std::pair<int, uint32_t>> queue;

    auto Place = [&](const int page, const uint32_t off) -> uint32_t
    {
        if (const auto it = placed.find({ page, off }); it != placed.end())
            return it->second;

        const std::vector<uint32_t>& b = map.boundaries[page];
        const uint32_t end = *std::upper_bound(b.begin(), b.end(), off);

        const uint32_t at = static_cast<uint32_t>(IALIGN(blob.size(), 16));
        blob.resize(at + (end - off));
        memcpy(&blob[at], buffers[page] + off, end - off);

        placed[{ page, off }] = at;
        queue.emplace_back(page, off);
        return at;
    };

    Place(rootPage, rootOff);

    while (!queue.empty())
    {
        const auto [page, off] = queue.front();
        queue.pop_front();

        const uint32_t base = placed[{ page, off }];
        const std::vector<uint32_t>& b = map.boundaries[page];
        const uint32_t end = *std::upper_bound(b.begin(), b.end(), off);

        const auto& ptrs = map.pointers[page];
        auto it = std::lower_bound(ptrs.begin(), ptrs.end(), off,
            [](const PakRegionMap_s::Pointer_s& p, uint32_t v) { return p.field < v; });

        for (; it != ptrs.end() && it->field + sizeof(void*) <= end; ++it)
        {
            const uint32_t target = Place(it->targetPage, it->targetOff);
            links.emplace_back(base + (it->field - off), target);
        }
    }

    std::sort(links.begin(), links.end());

    StreamIO out;
    if (!out.open(outBase + ".efct_def", eStreamIOMode::Write))
        return false;

    const uint32_t head[4] = { 0x54434645u /*'EFCT'*/, 1u, static_cast<uint32_t>(blob.size()), static_cast<uint32_t>(links.size()) };
    out.write(reinterpret_cast<const char*>(head), sizeof(head));
    out.write(blob.data(), blob.size()); // pointer fields are rewritten from the link table on repack

    for (const auto& [field, target] : links)
    {
        const uint32_t link[2] = { field, target };
        out.write(reinterpret_cast<const char*>(link), sizeof(link));
    }

    out.close();
    return true;
}

// RAW efct exporter: dependency-graph subheader + the operator ParticleDefinition.
static bool ExportEffectAsset(CAsset* const asset, const int setting)
{
    UNUSED(setting);
    CPakAsset* const pakAsset = static_cast<CPakAsset*>(asset);
    const PakAsset_t* const d = pakAsset->data();

    std::filesystem::path exportPath = g_ExportSettings.GetExportDirectory() / fourCCToString(asset->GetAssetType());
    if (!CreateDirectories(exportPath))
    {
        assertm(false, "Failed to create asset type directory.");
        return false;
    }
    exportPath.append(asset->GetAssetName());
    if (!CreateDirectories(exportPath.parent_path()))
    {
        assertm(false, "Failed to create export directory.");
        return false;
    }

    const uint32_t hdrSize = d->headerStructSize;
    const std::string outBase = exportPath.string();

    // 1) raw subheader (16B V2 or 24B V5/V7/Baked)
    {
        StreamIO out;
        if (out.open(outBase + ".efct_hdr", eStreamIOMode::Write))
        {
            if (pakAsset->header() && hdrSize > 0)
                out.write(reinterpret_cast<const char*>(pakAsset->header()), hdrSize);
            out.close();
        }
    }

    // 2) child + material GUID arrays (24-byte header family only)
    if (pakAsset->header() && hdrSize >= 24)
    {
        const uint8_t* const hp = reinterpret_cast<const uint8_t*>(pakAsset->header());
        const uint64_t childRefs = *reinterpret_cast<const uint64_t*>(hp + 0x00);
        const uint64_t assetRefs = *reinterpret_cast<const uint64_t*>(hp + 0x08);
        const uint32_t childRefCount = *reinterpret_cast<const uint32_t*>(hp + 0x10);
        const uint32_t assetRefCount = *reinterpret_cast<const uint32_t*>(hp + 0x14);

        if (childRefs && childRefCount > 0 && childRefCount < 1000)
        {
            StreamIO out;
            if (out.open(outBase + ".efct_childrefs", eStreamIOMode::Write))
            {
                out.write(reinterpret_cast<const char*>(childRefs), static_cast<size_t>(childRefCount) * 8);
                out.close();
            }
        }
        if (assetRefs && assetRefCount > 0 && assetRefCount < 1000)
        {
            StreamIO out;
            if (out.open(outBase + ".efct_assetrefs", eStreamIOMode::Write))
            {
                out.write(reinterpret_cast<const char*>(assetRefs), static_cast<size_t>(assetRefCount) * 8);
                out.close();
            }
        }
    }

    // 3) ParticleDefinition (name + operators), version-dispatched
    ExportEffectParticleDef(pakAsset, outBase, d->guid, d->version, hdrSize);

    // 4) the definition blob itself, re-packable (v16 24-byte header family)
    if (d->version == 16 && hdrSize == 24)
        ExportEffectRawDef(pakAsset, outBase);

    return true;
}

void InitEffectAssetType()
{
    AssetTypeBinding_t type =
    {
        .name = "Particle Effect",
        .type = 'tcfe',
        .headerAlignment = 8,
        .loadFunc = nullptr,
        .postLoadFunc = nullptr,
        .previewFunc = nullptr,
        .e = { ExportEffectAsset, 0, nullptr, 0ull },
    };

    REGISTER_TYPE(type);
}
