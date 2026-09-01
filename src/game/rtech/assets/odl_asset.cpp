#include "pch.h"
#include "odl_asset.h"
#include "odl_pak.h"

#include <game/rtech/cpakfile.h>
#include <game/rtech/utils/utils.h>
#include <thirdparty/imgui/imgui.h>
#include <core/render/dx.h>

#if defined(HAS_ODL_ASSET)
void LoadODLAsset(CAssetContainer* container, CAsset* asset)
{
    CPakFile* const pak = static_cast<CPakFile* const>(container);
    CPakAsset* const pakAsset = static_cast<CPakAsset* const>(asset);

    UNUSED(pak);
    UNUSED(pakAsset);

    ODLAssetHeader_t* header = reinterpret_cast<ODLAssetHeader_t*>(pakAsset->header());

    pakAsset->SetAssetName(header->assetName, true);
}

static bool ExportODLAsset(CAsset* const asset, const int setting)
{
    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);
    UNUSED(setting);


    ODLAssetHeader_t* header = reinterpret_cast<ODLAssetHeader_t*>(pakAsset->header());

    CAsset* odlPakAsset = g_assetData.FindAssetByGUID(header->odlPakAssetGuid);

    if (odlPakAsset)
    {
        CPakAsset* odlPakPakAsset = reinterpret_cast<CPakAsset*>(odlPakAsset);
        ODLPakHeader_t* odlPakHeader = reinterpret_cast<ODLPakHeader_t*>(odlPakPakAsset->header());
        printf("ODL Asset '%s' depends on pak '%s'. Loading...\n", header->assetName, odlPakHeader->pakName);


        std::filesystem::path fullPakPath = reinterpret_cast<CPakFile*>(pakAsset->GetContainerFile())->getFilePath().parent_path() / odlPakHeader->pakName;

        extern void HandlePakLoad(std::vector<std::string> filePaths);

        HandlePakLoad({ fullPakPath.string() });
    }
    UNUSED(header);

    return true;
}

void* PreviewODLAsset(CAsset* const asset, const bool _firstFrame)
{
    UNUSED(_firstFrame); // at the moment we don't care about the odl asset's first preview frame
                         // in the future, this will be used to load the odl pak when this asset is being previewed

    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

    ODLAssetHeader_t* header = reinterpret_cast<ODLAssetHeader_t*>(pakAsset->header());

    CAsset* loadedAsset = g_assetData.FindAssetByGUID(header->originalAssetGuid);

    CDXDrawData* drawData = NULL;
    if (loadedAsset && loadedAsset->GetPostLoadStatus())
    {
        if (auto it = g_assetData.m_assetTypeBindings.find(loadedAsset->GetAssetType()); it != g_assetData.m_assetTypeBindings.end())
        {
            if (it->second.previewFunc)
            {
                static CAsset* lastPreviewedODLAsset = NULL;

                // First frame is a special case, we wanna reset some settings for the preview function.
                const bool firstFrameForAsset = loadedAsset != lastPreviewedODLAsset;

                drawData = reinterpret_cast<CDXDrawData*>(it->second.previewFunc(static_cast<CPakAsset*>(loadedAsset), firstFrameForAsset));
                lastPreviewedODLAsset = loadedAsset;
            }
            else
            {
                ImGui::Text("Asset type '%s' does not currently support Asset Preview.", fourCCToString(loadedAsset->GetAssetType()).c_str());
            }
        }
        
    }
    else
    {
        ImGui::Text("Export this asset to preview");
    }


    return drawData;
}
#endif

void InitODLAssetType()
{
#if defined(HAS_ODL_ASSET)
    AssetTypeBinding_t type =
    {
        .name = "On Demand Loaded Asset" // i can't think of a better way of describing this..
        .type = 'aldo',
        .headerAlignment = 8,
        .loadFunc = LoadODLAsset,
        .postLoadFunc = nullptr,
        .previewFunc = PreviewODLAsset,
        .e = { ExportODLAsset, 0, nullptr, 0ull },
    };

    REGISTER_TYPE(type);
#endif
}
