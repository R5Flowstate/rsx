#include <pch.h>
#include <game/rtech/assets/wrap.h>
#include <game/rtech/cpakfile.h>
#include <game/rtech/utils/utils.h>
#include <game/bsp/bsp.h>
#include <thirdparty/imgui/imgui.h>

void LoadWrapAsset(CAssetContainer* const pak, CAsset* const asset)
{
    UNUSED(pak);
    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);
    WrapAsset* wrapAsset = nullptr;

    switch (pakAsset->version())
    {
    case 1:
    {
        WrapAssetHeader_v1_t* hdr = reinterpret_cast<WrapAssetHeader_v1_t*>(pakAsset->header());
        wrapAsset = new WrapAsset(hdr);
        break;
    }
    case 7:
    {
        WrapAssetHeader_v7_t* hdr = reinterpret_cast<WrapAssetHeader_v7_t*>(pakAsset->header());
        wrapAsset = new WrapAsset(hdr);
        break;
    }
    default:
    {
        assertm(false, "unaccounted asset version, will cause major issues!");
        return;
    }
    }

    if (wrapAsset->path)
    {
        const std::string name = std::string(wrapAsset->path + wrapAsset->skipFirstFolderPos);
        pakAsset->SetAssetName(name, true);
    }

    pakAsset->setExtraData(wrapAsset);
}


static bool GetStreamedDataForWrapAsset(CPakAsset* const asset, const uint32_t size, const int32_t skipSize, std::unique_ptr<char[]>& wrapData)
{
    AssetPtr_t streamEntry = asset->getStarPakStreamEntry(false);
    if (!IS_ASSET_PTR_INVALID(streamEntry))
    {
        wrapData = asset->getStarPakData(streamEntry.offset + skipSize, size, false);
        return true;
    }

    streamEntry = asset->getStarPakStreamEntry(true);
    if (!IS_ASSET_PTR_INVALID(streamEntry))
    {
        wrapData = asset->getStarPakData(streamEntry.offset + skipSize, size, true);
        return true;
    }

    assertm(false, "GetStreamedDataForWrapAsset called but no streamed data?");
    return false;
}

std::unique_ptr<char[]> GetWrapAssetData(CAsset* const asset, uint64_t* outSize)
{
    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

    const WrapAsset* const wrapAsset = reinterpret_cast<WrapAsset*>(pakAsset->extraData());

    const uint32_t wrapSize = wrapAsset->isCompressed ? wrapAsset->cmpSize : wrapAsset->dcmpSize;
    std::unique_ptr<char[]> wrapData = std::make_unique<char[]>(wrapSize);

    if (wrapAsset->isStreamed)
    {
        if (!GetStreamedDataForWrapAsset(pakAsset, wrapSize, wrapAsset->skipSize, wrapData))
        {
            assertm(false, "Failed to get streamed data for wrap asset.");
            return nullptr;
        }
    }
    else
    {
        constexpr uint16_t staticAsset = WRAP_FLAG_FILE_IS_COMPRESSED | WRAP_FLAG_FILE_IS_PERMANENT;
        const char* buf = (const char*)wrapAsset->data;

        // [amos]: if this condition is met, some internal header needs to be skipped.
        // only appears to happen on small files that appear to be marked for compress
        // but failed during build, e.g. mp_rr_arena_phase_runner.bsp.0005.bsp_lump in
        // release "build R5pc_r5-180_J25_CL4941853_2023_07_27_16_31". permanent assets
        // only
        if (!wrapAsset->isCompressed && (wrapAsset->flags & staticAsset) == staticAsset)
            buf += 2;

        memcpy_s(wrapData.get(), wrapSize, buf, wrapSize);
    }

    uint64_t wrapOutSize = wrapAsset->dcmpSize;
    if (wrapAsset->isCompressed)
    {
        wrapData = RTech::DecompressStreamedBuffer(std::move(wrapData), wrapOutSize, eCompressionType::OODLE);
    }

    if (outSize)
        *outSize = wrapOutSize;

    return wrapData;
}


void PostLoadWrapAsset(CAssetContainer* const pak, CAsset* const asset)
{
    UNUSED(pak);
    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

    WrapAsset* const wrapAsset = reinterpret_cast<WrapAsset*>(pakAsset->extraData());

    wrapAsset->parsedDataType = eWrapAssetParsedDataType::NONE;

#if defined(HAS_BSP_SUPPORT)
    std::filesystem::path assetPath = std::filesystem::path(asset->GetAssetName());
    std::string extension = assetPath.extension().string();

    if (extension == ".bsp")
    {
        wrapAsset->parsedDataType = eWrapAssetParsedDataType::BSP;

        std::unique_ptr<char[]> wrapData = GetWrapAssetData(asset, nullptr);

        CBSPData* bspData = new CBSPData(assetPath.stem().string());
        bspData->PopulateFromPakAsset(pakAsset, wrapData.get());

        wrapAsset->parsedData = bspData;
    }
#endif
}

bool ExportWrapAsset(CAsset* const asset, const int setting)
{
    UNUSED(setting);
    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

    const WrapAsset* const wrapAsset = reinterpret_cast<WrapAsset*>(pakAsset->extraData());

    // Create exported path + asset path.
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

    switch (wrapAsset->parsedDataType)
    {
    case eWrapAssetParsedDataType::NONE:
    default:
    {
        StreamIO wrapOut;

        if (!wrapOut.open(exportPath.string(), eStreamIOMode::Write))
        {
            assertm(false, "Failed to open file for write.");
            return false;
        }

        uint64_t wrapOutSize = 0;
        std::unique_ptr<char[]> wrapData = GetWrapAssetData(asset, &wrapOutSize);

        if (!wrapData)
            return false;

        wrapOut.write(wrapData.get(), wrapOutSize);
        wrapOut.close();
        break;
    }
#if defined(HAS_BSP_SUPPORT)
    case eWrapAssetParsedDataType::BSP:
    {
        StreamIO wrapOut;

        if (!wrapOut.open(exportPath.string(), eStreamIOMode::Write))
        {
            assertm(false, "Failed to open file for write.");
            return false;
        }

        CBSPData* bspData = reinterpret_cast<CBSPData*>(wrapAsset->parsedData);

        bspData->Export(wrapOut.W());

        wrapOut.close();

        break;
    }
#endif
    }

    return true;
}

void* PreviewWrapAsset(CAsset* const asset, const bool firstFrameForAsset)
{
    UNUSED(firstFrameForAsset);
    CPakAsset* const pakAsset = static_cast<CPakAsset*>(asset);
    assertm(pakAsset, "Asset should be valid.");

    WrapAsset* const wrapAsset = reinterpret_cast<WrapAsset*>(pakAsset->extraData());

#if defined(HAS_BSP_SUPPORT)
    ImGui::Text("WRAP asset preview is currently only available for .bsp files");
#else
    ImGui::Text("WRAP asset preview is unsupported on this build");
#endif

    if (!wrapAsset || wrapAsset->parsedDataType == eWrapAssetParsedDataType::NONE)
        return nullptr;

    // No need to include a preprocessor flag here. This data type isn't set unless the flag is defined
    if (wrapAsset->parsedDataType == eWrapAssetParsedDataType::BSP)
        return reinterpret_cast<CBSPData*>(wrapAsset->parsedData)->ConstructPreviewData();

    return nullptr;

}

void InitWrapAssetType()
{
    AssetTypeBinding_t type =
    {
        .name = "Wrapped File",
        .type = 'parw',
        .headerAlignment = 8,
        .loadFunc = LoadWrapAsset,
        .postLoadFunc = PostLoadWrapAsset,
        .previewFunc = PreviewWrapAsset,
        .e = { ExportWrapAsset, 0, nullptr, 0ull },
    };

    REGISTER_TYPE(type);
}