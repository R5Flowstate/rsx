#include <pch.h>
#include <core/filehandling/export.h>
#include <core/render/dx.h>

extern CBufferManager g_BufferManager;

void ExportAssetListCSVToFileStream(std::vector<CGlobalAssetData::AssetLookup_t>* assets, std::ofstream* ofs)
{
    *ofs << "type,pakver,rsxver,guid,file_name,asset_name\n";
    for (size_t i = 0; i < assets->size(); ++i)
    {
        const CGlobalAssetData::AssetLookup_t& it = assets->at(i);

        // raw RPak asset-entry version int (what the engine asset-binding switches on);
        // -1 for non-pak containers (mdl/audio/bpk) where it does not apply.
        int pakVer = -1;
        if (it.m_asset->GetAssetContainerType() == CAsset::ContainerType::PAK)
            pakVer = reinterpret_cast<CPakAsset*>(it.m_asset)->version();

        *ofs << fourCCToString(it.m_asset->GetAssetType(), true) << ","
             << std::dec << pakVer << ","
             << it.m_asset->GetAssetVersion().ToString() << ","
             << std::hex << it.m_guid << "," << it.m_asset->GetContainerFileName() << "," << it.m_asset->GetAssetName();

        if(i != assets->size()-1)
            *ofs << "\n";
    }
}

void ExportAssetListTXTToFileStream(std::vector<CGlobalAssetData::AssetLookup_t>* assets, std::ofstream* ofs)
{
    for (size_t i = 0; i < assets->size(); ++i)
    {
        const CGlobalAssetData::AssetLookup_t& it = assets->at(i);

        *ofs << it.m_asset->GetAssetName();

        if (i != assets->size() - 1)
            *ofs << "\n";
    }
}

void ExportDependenciesToFileStream_AdjList(std::vector<CGlobalAssetData::AssetLookup_t>* assets, std::ofstream* ofs)
{
    Log("DEPS: Writing dependencies as an adjacency list for %lld assets\n", assets->size());
    for (size_t i = 0; i < assets->size(); ++i)
    {
        const CGlobalAssetData::AssetLookup_t& it = assets->at(i);

        // For now this is only usable on rpaks
        if (it.m_asset->GetAssetContainerType() == CAssetContainer::ContainerType::PAK)
        {
            *ofs << it.m_asset->GetAssetName();

            CPakAsset* pakAsset = reinterpret_cast<CPakAsset*>(it.m_asset);

            std::vector<AssetGuid_t> dependencies;
            pakAsset->getDependencies(dependencies);

            for (size_t depIdx = 0; depIdx < dependencies.size(); ++depIdx)
            {
                const AssetGuid_t depGuid = dependencies[depIdx];

                CAsset* depAsset = g_assetData.FindAssetByGUID<CPakAsset>(depGuid.guid);

                if (depAsset)
                    *ofs << "," << depAsset->GetAssetName();
                else
                    *ofs << "," << std::format("{:016X}*", depGuid.guid);
            }
        }

        if (i != assets->size() - 1)
            *ofs << "\n";
    }
}

void HandleListExportPakAssets(const HWND handle, std::vector<CGlobalAssetData::AssetLookup_t>* assets)
{
    std::vector<std::string> assetNames(assets->size());
    size_t i = 0;
    for (auto& it : *assets)
    {
        assetNames.at(i) = it.m_asset->GetAssetName(); 
        i++;
    }

    HandleListExport(handle, assetNames);
}

void HandleListExport(const HWND handle, std::vector<std::string> listElements)
{
    // We are in pak load now.
    //inJobAction = true;

    CManagedBuffer* fileNames = g_BufferManager.ClaimBuffer();
    memset(fileNames->Buffer(), 0, CBufferManager::MaxBufferSize());

    OPENFILENAMEA openFileName = {};

    openFileName.lStructSize = sizeof(OPENFILENAMEA);
    openFileName.hwndOwner = handle;
    openFileName.lpstrFilter = "Text File (*.txt)\0*.txt\0";
    openFileName.lpstrFile = fileNames->Buffer();
    openFileName.nMaxFile = static_cast<DWORD>(CBufferManager::MaxBufferSize());
    openFileName.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR;
    openFileName.lpstrDefExt = "";

    if (GetSaveFileNameA(&openFileName))
    {
        std::string listPath(fileNames->Buffer());

        std::ofstream ofs(listPath, std::ios::out);

        // Iterate over all elements of our string vector and write them to the selected file
        for (auto& str : listElements)
        {
            ofs << str << "\n";
        }

        ofs.close();
    }

    g_BufferManager.RelieveBuffer(fileNames);
}