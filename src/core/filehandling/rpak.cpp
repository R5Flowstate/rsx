#include <pch.h>

#include <thirdparty/imgui/misc/imgui_utility.h>

#include <core/filehandling/load.h>
#include <core/filehandling/export.h>

#include <game/rtech/cpakfile.h>

void HandlePakLoad(std::vector<std::string> filePaths)
{
    std::atomic<uint32_t> pakLoadingProgress = 0;
    const ProgressBarEvent_t* const pakLoadProgress = g_pImGuiHandler->AddProgressBarEvent("Loading Paks..", static_cast<uint32_t>(filePaths.size()), &pakLoadingProgress, true);

    // If post-load has already been done when this function is called, then an ODL pak has been requested
    if (!g_assetData.m_donePostLoad)
    {
        if (g_assetData.m_pakPatchMaster)
            delete g_assetData.m_pakPatchMaster;

        g_assetData.m_patchMasterEntries.clear();
        g_assetData.m_pakLoadStatusMap.clear();
    }
    
    for (const std::string& path : filePaths)
    {
        std::filesystem::path fsPath = path;

        // If pak is not located in drive root (shouldn't happen but we might as well check)
        if (!g_assetData.m_pakPatchMaster && fsPath.has_parent_path())
        {
            const std::filesystem::path dirPath = fsPath.parent_path();
            const std::filesystem::path patchMasterPath = dirPath / "patch_master.rpak";

            if (std::filesystem::exists(patchMasterPath))
            {
                // [rika]: prevent double load on patch_master and catch if it fails to load
                g_assetData.m_pakPatchMaster = new CPakFile();
                if (static_cast<CPakFile*>(g_assetData.m_pakPatchMaster)->ParseFileBuffer(patchMasterPath.string()))
                {
                    const CPakFile* const pak = static_cast<CPakFile*>(g_assetData.m_pakPatchMaster);

                    if (pak->header()->crc != 0)
                        g_assetData.m_pakLoadStatusMap.emplace(pak->header()->crc, true);
                }
                else
                {
                    assertm(false, "Parsing patch_master from file failed.");
                    delete g_assetData.m_pakPatchMaster;
                    g_assetData.m_pakPatchMaster = nullptr;
                }

                //Log("[PTCH] Found %lld patch entries.\n", g_assetData.m_patchMasterEntries.size());           
            }
        }

        if (g_assetData.m_pakPatchMaster)
        {
            const std::string pakStem = GetPakFileStemNoPatchNum(path);
            const std::string basePakName = pakStem + ".rpak";

            auto& patchMap = g_assetData.m_patchMasterEntries;
            auto it = patchMap.find(basePakName);

            if (it != patchMap.end())
            {
                const uint8_t patchVersion = patchMap.at(basePakName);

                const std::string topPatchFileName = std::format("{}({:02}).rpak", pakStem, patchVersion);
                fsPath.replace_filename(topPatchFileName);

                Log("Loading highest patch '%s' instead of requested file '%s'\n", topPatchFileName.c_str(), path.c_str());
            }
        }

        if (CPakFile* const pak = new CPakFile(); pak->ParseFileBuffer(fsPath.string()))
        {
            if(pak->header()->crc != 0)
                g_assetData.m_pakLoadStatusMap.emplace(pak->header()->crc, true);

            g_assetData.v_assetContainers.emplace_back(pak);
        }
        else
        {
            //assertm(false, "Parsing pak from file failed.");
            delete pak;
        }
        ++pakLoadingProgress;
    }
    g_pImGuiHandler->FinishProgressBarEvent(pakLoadProgress);
}

static void TraverseAssetDependencies(CPakAsset* const asset, std::deque<CPakAsset*>& cpyAssets)
{
    std::vector<AssetGuid_t> dependencies;
    asset->getDependencies(dependencies);

    for (int d = 0; d < dependencies.size(); ++d)
    {
        const AssetGuid_t guid = dependencies[d];
        CPakAsset* const depAsset = g_assetData.FindAssetByGUID<CPakAsset>(guid.guid);

        if (!depAsset)
            continue;

        if (std::find(cpyAssets.begin(), cpyAssets.end(), depAsset) != cpyAssets.end())
            continue; // Already in the list.

        assert(depAsset != asset);
        cpyAssets.emplace_back(depAsset);

        // Add the dependencies of this asset into the list as well.
        TraverseAssetDependencies(depAsset, cpyAssets);
    }

    // Add the root asset itself to the list.
    if (std::find(cpyAssets.begin(), cpyAssets.end(), asset) == cpyAssets.end())
        cpyAssets.emplace_back(asset);
}

static void HandleExportBindingForAssetEx(CAsset* const asset)
{
    if (auto it = g_assetData.m_assetTypeBindings.find(asset->GetAssetType()); it != g_assetData.m_assetTypeBindings.end())
    {
        if (it->second.e.exportFunc)
        {
            const bool exported = it->second.e.exportFunc(asset, it->second.e.exportSetting);
            asset->SetExportedStatus(exported);
        }
    }
}

FORCEINLINE void HandleExportBindingForAsset(CAsset* const asset, const bool exportDependencies, const bool exportDependents)
{
    // only pak assets have dependencies/dependents so don't try to export them with other types
    if (asset->GetAssetContainerType() == CAsset::ContainerType::PAK && (exportDependencies || exportDependents))
    {
        std::deque<CPakAsset*> cpyAssets;
        CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

        // Add the asset itself first
        cpyAssets.emplace_back(pakAsset);

        // Add dependencies (recursive)
        if (exportDependencies)
        {
            std::vector<AssetGuid_t> dependencies;
            pakAsset->getDependencies(dependencies);

            for (int d = 0; d < dependencies.size(); ++d)
            {
                const AssetGuid_t guid = dependencies[d];
                CPakAsset* const depAsset = g_assetData.FindAssetByGUID<CPakAsset>(guid.guid);

                if (!depAsset)
                    continue;

                if (std::find(cpyAssets.begin(), cpyAssets.end(), depAsset) == cpyAssets.end())
                {
                    cpyAssets.emplace_back(depAsset);
                    // Recursively add dependencies of this dependency
                    TraverseAssetDependencies(depAsset, cpyAssets);
                }
            }
        }

        // Add direct dependents only (non-recursive to prevent infinite loops)
        if (exportDependents)
        {
            std::vector<AssetGuid_t> dependents;
            pakAsset->getDependents(dependents);

            for (int d = 0; d < dependents.size(); ++d)
            {
                const AssetGuid_t guid = dependents[d];
                CPakAsset* const depAsset = g_assetData.FindAssetByGUID<CPakAsset>(guid.guid);

                if (!depAsset)
                    continue;

                if (std::find(cpyAssets.begin(), cpyAssets.end(), depAsset) == cpyAssets.end())
                {
                    cpyAssets.emplace_back(depAsset);
                    // Do NOT recursively add dependents of dependents
                }
            }
        }

        for (CPakAsset* const dependency : cpyAssets)
        {
            HandleExportBindingForAssetEx(dependency);
        }
    }
    else
        HandleExportBindingForAssetEx(asset);
}

void HandlePakAssetExportList(std::deque<CAsset*> selectedAssets, const bool exportDependencies, const bool exportDependents)
{
    assertm(selectedAssets.size() > 0, "selectedAssets is empty.");

    CParallelTask parallelProcessTask(UtilsConfig->exportThreadCount);

    for (auto& asset : selectedAssets)
    {
        std::function<void()> task = [asset, exportDependencies, exportDependents]
        {
            HandleExportBindingForAsset(asset, exportDependencies, exportDependents);
        };
        parallelProcessTask.addTask(task, 1u);
    }

    const ProgressBarEvent_t* const exportAssetListEvent = g_pImGuiHandler->AddProgressBarEvent(
        "Exporting asset list...",
        parallelProcessTask.getRemainingTasks(),
        &parallelProcessTask,
        PB_FNCLASS_TO_VOID(&CParallelTask::getRemainingTasks));
    parallelProcessTask.execute();
    parallelProcessTask.wait();
    g_pImGuiHandler->FinishProgressBarEvent(exportAssetListEvent);
}



void HandleExportAllPakAssets(std::vector<CGlobalAssetData::AssetLookup_t>* const pakAssets, const bool exportDependencies, const bool exportDependents)
{
    assertm(g_assetData.v_assetContainers.size() > 0, "No paks loaded.");
    assertm(pakAssets->size() > 0, "No assets?");

    CParallelTask parallelProcessTask(UtilsConfig->exportThreadCount);

    for (auto& asset : *pakAssets)
    {
        std::function<void()> task = [asset, exportDependencies, exportDependents]
        {
            HandleExportBindingForAsset(asset.m_asset, exportDependencies, exportDependents);
        };
        parallelProcessTask.addTask(task, 1u);
    }

    const ProgressBarEvent_t* const exportAllAssetsEvent = g_pImGuiHandler->AddProgressBarEvent(
        "Exporting all assets...",
        parallelProcessTask.getRemainingTasks(),
        &parallelProcessTask,
        PB_FNCLASS_TO_VOID(&CParallelTask::getRemainingTasks)
    );
    parallelProcessTask.execute();
    parallelProcessTask.wait();
    g_pImGuiHandler->FinishProgressBarEvent(exportAllAssetsEvent);
}

void HandleExportSelectedAssetType(std::vector<CGlobalAssetData::AssetLookup_t> pakAssets, const bool exportDependencies, const bool exportDependents)
{
    HandleExportAllPakAssets(&pakAssets, exportDependencies, exportDependents);
}