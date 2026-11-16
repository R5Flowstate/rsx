#include <pch.h>
#include <game/rtech/cpakfile.h>
#include <game/rtech/utils/utils.h>

extern ExportSettings_t g_ExportSettings;

// Map cubemaps ('cube'). The asset has no cpu data; the BC6H_UF16 payload (6 faces x the
// full mip chain) streams from the map starpak as one compressed slice.
//  v2 (S30): 40-byte header, payload is mip-major (mip 0 of every face, then mip 1, ...).
//  v3 (S31): 56-byte header, payload is face-major (every mip of face 0, then face 1, ...),
//            described by a pointer to one {offset, rowPitch, size, pad} record per mip.
struct CubemapAssetHeader_t
{
    char unk_0[0x10];
    uint16_t width;
    uint16_t height;
    uint32_t index;          // cubemap index in the map
    uint32_t storedSize;     // streamed slice size (compressed, padded)
    uint32_t rawSize;        // decompressed payload size, all faces and mips
    uint16_t compression;    // eCompressionType
    uint16_t mipCount;       // v3; 0 in v2
    float meanLuminance;
};
static_assert(sizeof(CubemapAssetHeader_t) == 0x28);

struct CubemapMipLayout_t
{
    uint32_t offset;         // within one face
    uint32_t rowPitch;
    uint32_t size;
    uint32_t pad;
};

struct CubemapAssetHeader_v3_t : CubemapAssetHeader_t
{
    uint32_t faceSize;
    uint32_t unk_2C;
    const CubemapMipLayout_t* mips;
};
static_assert(sizeof(CubemapAssetHeader_v3_t) == 0x38);

class CubemapAsset
{
public:
    uint16_t width = 0;
    uint16_t height = 0;
    uint32_t index = 0;
    uint32_t storedSize = 0;
    uint32_t rawSize = 0;
    uint16_t compression = 0;
    float meanLuminance = 0.0f;
    bool faceMajor = false;
    std::vector<uint32_t> mipSizes; // per face
    AssetPtr_t stream{};
};

static constexpr int CUBE_FACES = 6;

static std::vector<uint32_t> CubemapMipSizes(const uint16_t width, const uint32_t rawSize)
{
    std::vector<uint32_t> sizes;
    uint64_t total = 0;
    for (uint32_t w = width; total * CUBE_FACES < rawSize && sizes.size() < 16; w = std::max(1u, w / 2))
    {
        const uint32_t blocks = std::max(1u, (w + 3) / 4);
        sizes.push_back(blocks * blocks * 16);
        total += sizes.back();
        if (w == 1)
            break;
    }
    return total * CUBE_FACES == rawSize ? sizes : std::vector<uint32_t>();
}

void LoadCubemapAsset(CAssetContainer* const container, CAsset* const asset)
{
    CPakFile* const pak = static_cast<CPakFile*>(container);
    CPakAsset* const pakAsset = static_cast<CPakAsset*>(asset);

    if (pakAsset->version() != 2 && pakAsset->version() != 3)
    {
        printf("WARNING: cubemap 0x%llX has unsupported version %d; listing without parse.\n",
            static_cast<unsigned long long>(pakAsset->GetAssetGUID()), pakAsset->version());
        return;
    }

    const CubemapAssetHeader_t* const hdr = reinterpret_cast<const CubemapAssetHeader_t*>(pakAsset->header());
    CubemapAsset* const cube = new CubemapAsset();
    cube->width = hdr->width;
    cube->height = hdr->height;
    cube->index = hdr->index;
    cube->storedSize = hdr->storedSize;
    cube->rawSize = hdr->rawSize;
    cube->compression = hdr->compression;
    cube->meanLuminance = hdr->meanLuminance;
    cube->stream = pakAsset->getStarPakStreamEntry(false);

    if (pakAsset->version() == 3)
    {
        const CubemapAssetHeader_v3_t* const hdr3 = reinterpret_cast<const CubemapAssetHeader_v3_t*>(hdr);
        cube->faceMajor = true;
        if (hdr3->mips && hdr->mipCount && hdr->mipCount <= 16)
            for (uint16_t i = 0; i < hdr->mipCount; i++)
                cube->mipSizes.push_back(hdr3->mips[i].size);
    }
    if (cube->mipSizes.empty())
        cube->mipSizes = CubemapMipSizes(hdr->width, hdr->rawSize);

    // Cubemaps are found by name: cubemap/<map bsp stem>/cubemap_<index>.rpak
    const std::string name = std::format("cubemap/maps/{}/cubemap_{}.rpak", pak->getPakStem(), hdr->index);
    if (RTech::StringToGuid(name.c_str()) == pakAsset->GetAssetGUID())
        pakAsset->SetAssetName(name, true);
    else
        pakAsset->SetAssetNameFromCache();

    pakAsset->setExtraData(cube);
}

enum eCubemapExportSetting : int
{
    CUBEMAP_DDS,
    CUBEMAP_RAW,
};

static void WriteCubemapDDS(StreamIO& out, const CubemapAsset* const cube)
{
    struct DDSPixelFormat_t { uint32_t size, flags, fourCC, rgbBitCount, rMask, gMask, bMask, aMask; };
    struct DDSHeader_t
    {
        uint32_t size, flags, height, width, pitchOrLinearSize, depth, mipMapCount, reserved1[11];
        DDSPixelFormat_t ddspf;
        uint32_t caps, caps2, caps3, caps4, reserved2;
    };
    struct DDSHeaderDX10_t { uint32_t dxgiFormat, resourceDimension, miscFlag, arraySize, miscFlags2; };

    DDSHeader_t h{};
    h.size = sizeof(DDSHeader_t);
    h.flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000 | 0x80000; // caps, height, width, pixelformat, mipmapcount, linearsize
    h.height = cube->height;
    h.width = cube->width;
    h.pitchOrLinearSize = cube->mipSizes.empty() ? 0 : cube->mipSizes[0];
    h.mipMapCount = static_cast<uint32_t>(cube->mipSizes.size());
    h.ddspf.size = sizeof(DDSPixelFormat_t);
    h.ddspf.flags = 0x4; // fourcc
    h.ddspf.fourCC = MAKEFOURCC('D', 'X', '1', '0');
    h.caps = 0x1000 | 0x8 | 0x400000; // texture, complex, mipmap
    h.caps2 = 0x200 | 0xFC00;         // cubemap, all six faces

    DDSHeaderDX10_t x{};
    x.dxgiFormat = 95;        // DXGI_FORMAT_BC6H_UF16
    x.resourceDimension = 3;  // TEXTURE2D
    x.miscFlag = 0x4;         // TEXTURECUBE
    x.arraySize = 1;

    const uint32_t magic = MAKEFOURCC('D', 'D', 'S', ' ');
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&h), sizeof(h));
    out.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

static const char* const s_PathPrefixCUBE = "cubemap";
bool ExportCubemapAsset(CAsset* const asset, const int setting)
{
    CPakAsset* const pakAsset = static_cast<CPakAsset*>(asset);
    const CubemapAsset* const cube = pakAsset->extraData<const CubemapAsset* const>();
    if (!cube)
        return false;

    if (cube->stream.size == 0)
    {
        printf("WARNING: cubemap '%s' has no streamed payload (starpak missing?); skipping.\n", pakAsset->GetAssetName().c_str());
        return false;
    }

    std::filesystem::path exportPath = g_ExportSettings.GetExportDirectory();
    const std::filesystem::path cubePath(asset->GetAssetName());
    if (g_ExportSettings.exportPathsFull)
        exportPath.append(cubePath.parent_path().string());
    else
        exportPath.append(s_PathPrefixCUBE);
    if (!CreateDirectories(exportPath))
    {
        assertm(false, "Failed to create asset's directory.");
        return false;
    }
    exportPath.append(cubePath.filename().string());

    std::unique_ptr<char[]> stored = pakAsset->getStarPakData(cube->stream.offset, cube->stream.size, false);
    if (!stored)
        return false;

    if (setting == CUBEMAP_RAW)
    {
        exportPath.replace_extension(".cube_data");
        StreamIO out;
        if (!out.open(exportPath.string(), eStreamIOMode::Write))
            return false;
        out.write(stored.get(), cube->stream.size);
        out.close();

        exportPath.replace_extension(".json");
        std::ofstream j(exportPath, std::ios::out);
        j << "{\n  \"version\": " << pakAsset->version() << ",\n  \"width\": " << cube->width << ",\n  \"height\": " << cube->height
          << ",\n  \"index\": " << cube->index << ",\n  \"storedSize\": " << cube->storedSize << ",\n  \"rawSize\": " << cube->rawSize
          << ",\n  \"compression\": " << cube->compression << ",\n  \"meanLuminance\": " << std::setprecision(9) << cube->meanLuminance
          << ",\n  \"faceMajor\": " << (cube->faceMajor ? "true" : "false") << "\n}\n";
        return true;
    }

    if (cube->mipSizes.empty())
    {
        printf("WARNING: cubemap '%s' has no mip layout (raw size %u); export it raw.\n", pakAsset->GetAssetName().c_str(), cube->rawSize);
        return false;
    }

    uint64_t rawSize = cube->rawSize;
    std::unique_ptr<char[]> raw = std::move(stored);
    if (cube->compression != static_cast<uint16_t>(eCompressionType::NONE))
        raw = RTech::DecompressStreamedBuffer(std::move(raw), rawSize, static_cast<eCompressionType>(cube->compression));
    if (!raw || rawSize != cube->rawSize)
    {
        printf("WARNING: cubemap '%s' payload did not decompress to %u bytes.\n", pakAsset->GetAssetName().c_str(), cube->rawSize);
        return false;
    }

    exportPath.replace_extension(".dds");
    StreamIO out;
    if (!out.open(exportPath.string(), eStreamIOMode::Write))
        return false;
    WriteCubemapDDS(out, cube);

    if (cube->faceMajor)
    {
        out.write(raw.get(), cube->rawSize);
    }
    else
    {
        // DDS cubes are face-major; v2 payloads are mip-major.
        std::vector<uint64_t> mipStart(cube->mipSizes.size());
        uint64_t at = 0;
        for (size_t m = 0; m < cube->mipSizes.size(); m++)
        {
            mipStart[m] = at;
            at += static_cast<uint64_t>(cube->mipSizes[m]) * CUBE_FACES;
        }
        for (int f = 0; f < CUBE_FACES; f++)
            for (size_t m = 0; m < cube->mipSizes.size(); m++)
                out.write(raw.get() + mipStart[m] + static_cast<uint64_t>(f) * cube->mipSizes[m], cube->mipSizes[m]);
    }
    out.close();
    return true;
}

void InitCubemapAssetType()
{
    static const char* settings[] = { "DDS", "RAW" };
    AssetTypeBinding_t type =
    {
        .name = "Cubemap",
        .type = 'ebuc',
        .headerAlignment = 8,
        .loadFunc = LoadCubemapAsset,
        .postLoadFunc = nullptr,
        .previewFunc = nullptr,
        .e = { ExportCubemapAsset, CUBEMAP_DDS, settings, ARRSIZE(settings) },
    };

    REGISTER_TYPE(type);
}
