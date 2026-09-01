#include <pch.h>
#include <core/filehandling/load.h>
#include <core/filehandling/export.h>
#include <core/utils/cli_parser.h>
#include <game/rtech/cpakfile.h>
#include <game/rtech/assets/shader.h>
#include <fstream>
#include <unordered_set>
#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

extern CBufferManager g_BufferManager;

extern std::atomic<bool> inJobAction;

typedef void(HandleFileLoadCallback_t)(const CCommandLine* const);

static void HandleFileLoad(std::vector<std::string> filePaths, HandleFileLoadCallback_t cb = nullptr, const CCommandLine* const cli = nullptr)
{
    std::vector<std::string> pathsByExtension[CAsset::ContainerType::_COUNT];

    for (auto& path : filePaths)
    {
        const std::string extension = std::filesystem::path(path).extension().string();

        if (extension == ".rpak")
            pathsByExtension[CAsset::ContainerType::PAK].emplace_back(path);
        else if (extension == ".mbnk")
            pathsByExtension[CAsset::ContainerType::AUDIO].emplace_back(path);
        else if (extension == ".mdl")
            pathsByExtension[CAsset::ContainerType::MDL].emplace_back(path);
        else if (extension == ".bpk")
            pathsByExtension[CAsset::ContainerType::BP_PAK].emplace_back(path);
        else
            Log("LOAD: Invalid file extension found in path: %s.\n", path.c_str());
    }

    for (uint32_t i = 0; i < CAsset::ContainerType::_COUNT; ++i)
    {
        // [rika]: we should skip a function if we don't have files for it
        if (pathsByExtension[i].empty())
            continue;

        switch (i)
        {
        case CAsset::ContainerType::PAK:
            HandlePakLoad(pathsByExtension[i]);
            break;
        case CAsset::ContainerType::AUDIO:
            HandleMBNKLoad(pathsByExtension[i]);
            break;
        case CAsset::ContainerType::MDL:
            HandleMDLLoad(pathsByExtension[i]);
            break;
        case CAsset::ContainerType::BP_PAK:
            HandleBPKLoad(pathsByExtension[i]);
            break;
        }
    }
    
    // Only run post-load if we are running with GUI, or if CLI has requested the export of some assets
    // (-validateshaders also needs post-load: it reads shaderAsset->shaderBuffers populated there).
    if(!cli || !cli->HasParam("-nogui") || cli->HasParam("-export") || cli->HasParam("-validateshaders"))
        g_assetData.ProcessAssetsPostLoad();

    // This callback is only really needed for CLI, since users aren't able to access the assets before postloading anyway
    if (cli && cli->HasParam("-nogui"))
    {
        if (cb && cli)
            cb(cli);
    }
}

// -validateshaders: OFFLINE shader validator. Creates a headless DEBUG-layer D3D11 device and runs
// the matching CreateXShader on every REAL (non-ref) bytecode buffer of every loaded shader asset,
// so we learn -- without launching the game -- exactly which converted shaders the DX11 runtime
// rejects (E_INVALIDARG) or the debug layer flags (the would-be heap corruptor). Validates exactly
// what gets packed: PostLoadShaderAsset has already applied our conversions to shaderBuffers.
static void DXBC_ShaderModel(const char* const buf, int& maj, int& min)
{
    maj = 0; min = 0;
    if (!buf || memcmp(buf, "DXBC", 4) != 0) return;
    const uint32_t chunkCount = *reinterpret_cast<const uint32_t*>(buf + 0x1C);
    if (chunkCount == 0 || chunkCount > 32) return;
    const uint32_t* const offs = reinterpret_cast<const uint32_t*>(buf + 0x20);
    for (uint32_t k = 0; k < chunkCount; ++k)
    {
        const char* const chunk = buf + offs[k];
        if (memcmp(chunk, "SHEX", 4) == 0 || memcmp(chunk, "SHDR", 4) == 0)
        {
            const uint32_t ver = *reinterpret_cast<const uint32_t*>(chunk + 8);
            maj = (ver >> 4) & 0xF; min = ver & 0xF; return;
        }
    }
}

static void ValidateShaders(const CCommandLine* const cli)
{
    const char* const outPath = cli->GetParamValue("--validateout");
    std::ofstream ofs(outPath ? outPath : "shader_validate.csv", std::ios::out);
    ofs << "guid,bufferIdx,numBuffers,shaderType,shaderModel,hresult,debugMsg\n";

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    const D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_DEBUG,
                                   &fl, 1, D3D11_SDK_VERSION, &device, nullptr, &ctx);
    if (FAILED(hr) || !device) // debug layer or HW unavailable -> retry WARP+debug, then HW no-debug
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_DEBUG,
                               &fl, 1, D3D11_SDK_VERSION, &device, nullptr, &ctx);
    if (FAILED(hr) || !device)
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                               &fl, 1, D3D11_SDK_VERSION, &device, nullptr, &ctx);
    if (FAILED(hr) || !device)
    {
        printf("[VALIDATE] FAILED to create any D3D11 device (0x%08X)\n", hr);
        return;
    }

    ID3D11InfoQueue* iq = nullptr;
    device->QueryInterface(__uuidof(ID3D11InfoQueue), reinterpret_cast<void**>(&iq));
    printf("[VALIDATE] D3D11 device up (debugLayer=%s). Validating shaders...\n", iq ? "ON" : "off");

    int totalBufs = 0, flagged = 0, shaders = 0;
    for (auto& it : g_assetData.v_assets)
    {
        if (it.m_asset->GetAssetContainerType() != CAssetContainer::ContainerType::PAK) continue;
        if (it.m_asset->GetAssetType() != 'rdhs') continue;
        CPakAsset* const pa = static_cast<CPakAsset*>(it.m_asset);
        const ShaderAsset* const sh = pa->extraData<const ShaderAsset* const>();
        if (!sh) continue;
        shaders++;
        const unsigned long long guid = pa->GetAssetGUID();
        const size_t nbuf = sh->shaderBuffers.size();
        for (size_t bi = 0; bi < nbuf; ++bi)
        {
            const ShaderBufEntry_t& b = sh->shaderBuffers[bi];
            if (!b.buffer || b.isRef || b.isNullBuffer || b.bufferSize <= 0) continue;

            if (iq) iq->ClearStoredMessages();
            HRESULT chr = E_FAIL;
            switch (sh->type)
            {
            case eShaderType::Pixel:    { ID3D11PixelShader*    s = nullptr; chr = device->CreatePixelShader(b.buffer, b.bufferSize, nullptr, &s);    if (s) s->Release(); break; }
            case eShaderType::Vertex:   { ID3D11VertexShader*   s = nullptr; chr = device->CreateVertexShader(b.buffer, b.bufferSize, nullptr, &s);   if (s) s->Release(); break; }
            case eShaderType::Geometry: { ID3D11GeometryShader* s = nullptr; chr = device->CreateGeometryShader(b.buffer, b.bufferSize, nullptr, &s); if (s) s->Release(); break; }
            case eShaderType::Hull:     { ID3D11HullShader*     s = nullptr; chr = device->CreateHullShader(b.buffer, b.bufferSize, nullptr, &s);     if (s) s->Release(); break; }
            case eShaderType::Domain:   { ID3D11DomainShader*   s = nullptr; chr = device->CreateDomainShader(b.buffer, b.bufferSize, nullptr, &s);   if (s) s->Release(); break; }
            case eShaderType::Compute:  { ID3D11ComputeShader*  s = nullptr; chr = device->CreateComputeShader(b.buffer, b.bufferSize, nullptr, &s);  if (s) s->Release(); break; }
            default: continue;
            }
            totalBufs++;

            int smMaj = 0, smMin = 0; DXBC_ShaderModel(b.buffer, smMaj, smMin);

            std::string dbg;
            if (iq)
            {
                const UINT64 n = iq->GetNumStoredMessages();
                for (UINT64 mi = 0; mi < n; ++mi)
                {
                    SIZE_T len = 0; iq->GetMessage(mi, nullptr, &len);
                    if (!len) continue;
                    std::vector<char> mbuf(len);
                    D3D11_MESSAGE* const msg = reinterpret_cast<D3D11_MESSAGE*>(mbuf.data());
                    if (SUCCEEDED(iq->GetMessage(mi, msg, &len)) && msg->Severity <= D3D11_MESSAGE_SEVERITY_WARNING)
                        { dbg += msg->pDescription; dbg += " || "; }
                }
            }

            if (FAILED(chr) || !dbg.empty())
            {
                flagged++;
                ofs << "0x" << std::hex << guid << std::dec << "," << bi << "," << nbuf << ","
                    << (int)sh->type << "," << smMaj << "." << smMin << ",0x" << std::hex << (unsigned)chr << std::dec
                    << ",\"" << dbg << "\"\n";
                printf("[VALIDATE-FAIL] 0x%016llX buf %zu/%zu type=%d SM%d.%d hr=0x%08X %s\n",
                       guid, bi, nbuf, (int)sh->type, smMaj, smMin, (unsigned)chr, dbg.c_str());
            }
        }
    }
    ofs.close();
    printf("[VALIDATE] DONE: %d shaders, %d real buffers tested, %d FLAGGED. -> %s\n",
           shaders, totalBufs, flagged, outPath ? outPath : "shader_validate.csv");

    if (iq) iq->Release();
    if (ctx) ctx->Release();
    if (device) device->Release();
}

void OnCLILoadComplete(const CCommandLine* const cli)
{
    // Hold this thread until asset loading is done on the newly spawned threads
    while (true)
    {
        if (g_assetData.m_donePostLoad)
            break;
    }

    if (cli->HasParam("-validateshaders"))
    {
        ValidateShaders(cli);
        return;
    }

    if (cli->HasParam("-export"))
    {
        std::vector<CGlobalAssetData::AssetLookup_t>& assets = g_assetData.v_assets;

        std::vector<uint32_t> filterTypes = GetExportFilterTypes(cli);

        // --exportpak <substr>: restrict the EXPORT set to assets whose source pak filename
        // contains <substr> (e.g. the map pak). Other loaded paks (common/common_mp/...) stay
        // loaded ONLY for dependency RESOLUTION -- with -exportdependencies this yields the map's
        // assets + just their referenced closure, instead of every loaded pak's assets.
        const char* const exportPak = cli->GetParamValue("--exportpak");

        // --exportguids <file>: restrict the EXPORT set to assets whose GUID appears in <file>
        // (one hex guid per line, optional 0x). This is the IMPORT-CLOSURE recipe: load the map
        // pak + all runtime paks (common/common_mp/...), compute the transitive set of
        // refs the map needs, and export EXACTLY those by guid -- avoids type-exporting 90k
        // textures (and streaming the whole 36GB starpak) just to pull the handful you need.
        const char* const exportGuidsFile = cli->GetParamValue("--exportguids");
        std::unordered_set<uint64_t> exportGuids;
        if (exportGuidsFile)
        {
            std::ifstream gf(exportGuidsFile);
            std::string ln;
            while (std::getline(gf, ln))
            {
                const size_t a = ln.find_first_not_of(" \t\r\n");
                if (a == std::string::npos) continue;
                const size_t b = ln.find_last_not_of(" \t\r\n");
                ln = ln.substr(a, b - a + 1);
                if (ln.rfind("0x", 0) == 0 || ln.rfind("0X", 0) == 0) ln = ln.substr(2);
                try { exportGuids.insert(std::stoull(ln, nullptr, 16)); }
                catch (...) {}
            }
            printf("EXPORT: --exportguids loaded %zu guids from %s\n", exportGuids.size(), exportGuidsFile);
        }

        if (filterTypes.size() != 0 || exportPak || !exportGuids.empty())
        {
            if (exportPak)
                printf("\nEXPORT: Restricting export to source pak \"%s\" (deps pulled from other loaded paks)\n", exportPak);
            if (filterTypes.size() != 0)
                printf("EXPORT: Filtering assets for export using type string \"%s\" (%lld valid type%s)\n", cli->GetParamValue("--exporttypes"), filterTypes.size(), filterTypes.size() == 1 ? "" : "s");

            std::vector<CGlobalAssetData::AssetLookup_t> filteredAssets;

            for (auto& it : assets)
            {
                if (exportPak && it.m_asset->GetContainerFileName().find(exportPak) == std::string::npos)
                    continue;

                if (!exportGuids.empty())
                {
                    if (it.m_asset->GetAssetContainerType() != CAssetContainer::ContainerType::PAK ||
                        exportGuids.find(reinterpret_cast<CPakAsset*>(it.m_asset)->GetAssetGUID()) == exportGuids.end())
                        continue;
                }

                if (filterTypes.size() != 0)
                {
                    bool typeMatch = false;
                    for (const uint32_t type : filterTypes)
                    {
                        if (it.m_asset->GetAssetType() == type)
                        {
                            typeMatch = true;
                            break;
                        }
                    }
                    if (!typeMatch)
                        continue;
                }

                filteredAssets.push_back(it);
            }

            CThread(HandleExportAllPakAssets, &filteredAssets, g_ExportSettings.exportAssetDeps, g_ExportSettings.exportAssetDependents).join();
        }
        else
            CThread(HandleExportAllPakAssets, &g_assetData.v_assets, g_ExportSettings.exportAssetDeps, g_ExportSettings.exportAssetDependents).join();
    }

    if (const char* const listPathStr = cli->GetParamValue("--list"))
    {
        std::ofstream ofs(listPathStr, std::ios::out | std::ios::binary);

        const char* const listFormat = cli->GetParamValue("--listformat");

        if (!listFormat || !_stricmp(listFormat, "txt"))
            ExportAssetListTXTToFileStream(&g_assetData.v_assets, &ofs);
        else if (!_stricmp(listFormat, "csv"))
            ExportAssetListCSVToFileStream(&g_assetData.v_assets, &ofs);
    }

    // Writes a file containing info about each asset's dependencies to the provided file path
    if (const char* const depFilePath = cli->GetParamValue("--depfilepath"))
    {
        std::ofstream ofs(depFilePath, std::ios::out | std::ios::binary);

        const char* depFileFormat = cli->GetParamValue("--depfileformat");

        if (!depFileFormat || !_stricmp(depFileFormat, "adjlist"))
            ExportDependenciesToFileStream_AdjList(&g_assetData.v_assets, &ofs);
    }
}

// -decompresspak: decompress an rpak (oodle/zstd/rtech) and write an UNCOMPRESSED
// copy to "<input>.dec.rpak" so external tooling (e.g. the cafe manifest generator)
// can read its header/tables/subheaders by raw offset. Reuses the engine's own
// CPakFile::DecompressFileBuffer so all three pak compression schemes are handled.
static void DecompressPakToDisk(const std::string& inPath)
{
    std::ifstream f(inPath, std::ios::binary | std::ios::ate);
    if (!f)
    {
        printf("[decompresspak] cannot open: %s\n", inPath.c_str());
        return;
    }

    const std::streamsize sz = f.tellg();
    f.seekg(0);
    std::shared_ptr<char[]> buf(new char[static_cast<size_t>(sz)]);
    f.read(buf.get(), sz);
    f.close();

    if (sz < 0x80 || *reinterpret_cast<const int*>(buf.get()) != pakFileMagic)
    {
        printf("[decompresspak] not an rpak: %s\n", inPath.c_str());
        return;
    }

    CPakFile pak;
    if (!pak.DecompressFileBuffer(buf.get(), &buf))
    {
        printf("[decompresspak] decompress FAILED: %s\n", inPath.c_str());
        return;
    }

    // buf now holds the decompressed image (or the original, if it was uncompressed).
    const int64_t dcmpSize = *reinterpret_cast<const int64_t*>(buf.get() + 0x30); // PakHdr_v8_t::dcmpSize
    const int64_t outSize = dcmpSize > 0 ? dcmpSize : static_cast<int64_t>(sz);

    // Make the on-disk header advertise "uncompressed": clear the compression flags
    // and set cmpSize == dcmpSize.
    short* const pFlags = reinterpret_cast<short*>(buf.get() + 0x6); // PakHdr_v8_t::flags
    *pFlags = static_cast<short>(*pFlags & ~PAK_HEADER_FLAGS_COMPRESSED);
    *reinterpret_cast<int64_t*>(buf.get() + 0x18) = outSize; // PakHdr_v8_t::cmpSize

    const std::string outPath = inPath + ".dec.rpak";
    std::ofstream o(outPath, std::ios::binary);
    o.write(buf.get(), static_cast<std::streamsize>(outSize));
    o.close();
    printf("[decompresspak] %s -> %s (%lld bytes)\n", inPath.c_str(), outPath.c_str(), static_cast<long long>(outSize));
}

void HandleLoadFromCommandLine(const CCommandLine* const cli)
{
    std::vector<std::string> filePaths;

    for (int i = cli->GetFirstNonFlagArgIdx(); i < cli->GetArgC(); ++i) // we skip 0 since its selfpath
    {
        std::filesystem::path path = std::filesystem::path(cli->GetParamValue(i));

        if (std::filesystem::exists(path) && std::filesystem::is_regular_file(path))
        {
            filePaths.emplace_back(path.string());
        }
    }

    // Standalone mode: just decompress the given rpak(s) to disk and exit (no asset load/export).
    if (cli->HasParam("-decompresspak"))
    {
        for (const std::string& p : filePaths)
        {
            if (std::filesystem::path(p).extension() == ".rpak")
                DecompressPakToDisk(p);
        }
        return;
    }

    CThread thread = CThread(HandleFileLoad, std::move(filePaths), OnCLILoadComplete, cli);

    // If this gets detached when running without the usual windows msg loop to hold up main thread, the main thread will exit
    // and clean up static vars before the other threads have finished execution. This will cause a crash when accessing anything static
    // such as s_AssetTypePaths in pakfile's ProcessAssets
    if (cli->HasParam("-nogui"))
        thread.join();
    else
        thread.detach();
}

void HandleOpenFileDialog(const HWND windowHandle)
{
    // We are in pak load now.
    inJobAction = true;

    CManagedBuffer* fileNames = g_BufferManager.ClaimBuffer();
    memset(fileNames->Buffer(), 0, CBufferManager::MaxBufferSize());

    OPENFILENAMEA openFileName = {};

    openFileName.lStructSize = sizeof(OPENFILENAMEA);
    openFileName.hwndOwner = windowHandle;
    openFileName.lpstrFilter = "reSource Asset Files (*.rpak, *.mbnk, *.mdl)\0*.RPAK;*.MBNK;*.MDL;*.BPK\0";
    openFileName.lpstrFile = fileNames->Buffer();
    openFileName.nMaxFile = static_cast<DWORD>(CBufferManager::MaxBufferSize());
    openFileName.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_NOCHANGEDIR;
    openFileName.lpstrDefExt = "";

    if (GetOpenFileNameA(&openFileName))
    {
        std::vector<std::string> filePaths;
        std::string directoryPath(fileNames->Buffer());

        // fileNames buffer is a collection of strings
        // the first string is the path of the directory
        // and the following strings are filenames within that directory
        const char* fileNamePtr = fileNames->Buffer() + directoryPath.length() + 1;

        if (*fileNamePtr) // check if there is actually a first filename or if it's just a null byte
        {
            for (; *fileNamePtr; fileNamePtr += strnlen(fileNamePtr, MAX_PATH) + 1)
            {
                std::string filePath = directoryPath + '\\' + fileNamePtr;
                filePaths.push_back(std::move(filePath));
            }
        }
        else
        {
            filePaths.push_back(std::move(directoryPath));
        }

        // We are moving the whole vector out of here into HandlePakLoad.
        HandleFileLoad(std::move(filePaths));
    }

    g_BufferManager.RelieveBuffer(fileNames);

    // We are done with pak loading.
    inJobAction = false;
}