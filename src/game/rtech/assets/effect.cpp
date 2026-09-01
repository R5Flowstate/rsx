#include "pch.h"
#include "effect.h"

#include <game/rtech/cpakfile.h>
#include <game/rtech/utils/utils.h>
#include <thirdparty/imgui/imgui.h>

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
