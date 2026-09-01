// This file is made available as part of the reSource Xtractor (RSX) asset extractor
// Licensed under AGPLv3. Details available at https://github.com/r-ex/rsx/blob/main/LICENSE

#include <pch.h>
#include <array>
#include <filesystem>
#include <string>

#include <game/rtech/utils/zstd_loader.h>

namespace
{
    struct ZSTD_DCtx_s;
    using ZSTD_DCtx = ZSTD_DCtx_s;

    using ZSTD_createDCtx_Fn = ZSTD_DCtx* (__cdecl*)();
    using ZSTD_freeDCtx_Fn = size_t (__cdecl*)(ZSTD_DCtx*);
    using ZSTD_decompressDCtx_Fn = size_t (__cdecl*)(ZSTD_DCtx*, void*, size_t, const void*, size_t);
    using ZSTD_isError_Fn = unsigned (__cdecl*)(size_t);
    using ZSTD_getErrorName_Fn = const char* (__cdecl*)(size_t);

    using ZSTD_findFrameCompressedSize_Fn = size_t (__cdecl*)(const void*, size_t);

    struct ZstdBindings
    {
        HMODULE module = nullptr;
        ZSTD_createDCtx_Fn createCtx = nullptr;
        ZSTD_freeDCtx_Fn freeCtx = nullptr;
        ZSTD_decompressDCtx_Fn decompressCtx = nullptr;
        ZSTD_isError_Fn isError = nullptr;
        ZSTD_getErrorName_Fn getErrorName = nullptr;
        bool loaded = false;
        ZSTD_findFrameCompressedSize_Fn findFrameCompressedSize = nullptr;
    };

    ZstdBindings g_bindings;
    std::once_flag g_initFlag;

    void ResetBindings()
    {
        if (g_bindings.module)
            FreeLibrary(g_bindings.module);
        g_bindings = {};
    }

    std::wstring ReadEnvOverride()
    {
        const DWORD required = GetEnvironmentVariableW(L"RSX_ZSTD_DLL", nullptr, 0);
        if (required == 0)
            return {};

        std::wstring buffer;
        buffer.resize(required);
        const DWORD written = GetEnvironmentVariableW(L"RSX_ZSTD_DLL", buffer.data(), required);
        if (written == 0 || written >= required)
            return {};

        buffer.resize(written);
        return buffer;
    }

    bool TryLoadModule(const std::wstring& path)
    {
        if (path.empty())
            return false;

        HMODULE module = LoadLibraryW(path.c_str());
        if (!module)
            return false;

        g_bindings.module = module;
        return true;
    }

    bool ResolveImports()
    {
        g_bindings.createCtx = reinterpret_cast<ZSTD_createDCtx_Fn>(GetProcAddress(g_bindings.module, "ZSTD_createDCtx"));
        g_bindings.freeCtx = reinterpret_cast<ZSTD_freeDCtx_Fn>(GetProcAddress(g_bindings.module, "ZSTD_freeDCtx"));
        g_bindings.decompressCtx = reinterpret_cast<ZSTD_decompressDCtx_Fn>(GetProcAddress(g_bindings.module, "ZSTD_decompressDCtx"));
        g_bindings.isError = reinterpret_cast<ZSTD_isError_Fn>(GetProcAddress(g_bindings.module, "ZSTD_isError"));
        g_bindings.getErrorName = reinterpret_cast<ZSTD_getErrorName_Fn>(GetProcAddress(g_bindings.module, "ZSTD_getErrorName"));
        g_bindings.findFrameCompressedSize = reinterpret_cast<ZSTD_findFrameCompressedSize_Fn>(GetProcAddress(g_bindings.module, "ZSTD_findFrameCompressedSize"));

        if (!g_bindings.createCtx || !g_bindings.freeCtx || !g_bindings.decompressCtx || !g_bindings.isError || !g_bindings.getErrorName)
            return false;

        return true;
    }

    bool TryInitialize()
    {
        auto envPath = ReadEnvOverride();
        if (!envPath.empty())
        {
            if (!TryLoadModule(envPath))
                Log("RSX: Failed to load Zstd decoder from RSX_ZSTD_DLL='%ls'\n", envPath.c_str());
        }

        if (!g_bindings.module)
        {
            wchar_t exePath[MAX_PATH] = {};
            const DWORD pathLen = GetModuleFileNameW(nullptr, exePath, static_cast<DWORD>(_countof(exePath)));
            std::filesystem::path exeDir;
            if (pathLen > 0 && pathLen < _countof(exePath))
                exeDir = std::filesystem::path(exePath).remove_filename();

            constexpr std::array<const wchar_t*, 4> kDllNames = { L"zstdlib_win64.dll", L"zstdlib.dll", L"libzstd.dll", L"zstd.dll" };
            for (const wchar_t* name : kDllNames)
            {
                if (!exeDir.empty())
                {
                    std::filesystem::path fullPath = exeDir / name;
                    if (TryLoadModule(fullPath.native()))
                        break;
                }

                if (TryLoadModule(std::wstring(name)))
                    break;
            }
        }

        if (!g_bindings.module)
        {
            Log("RSX: Unable to locate a Zstd decoder DLL. Place zstdlib_win64.dll next to RSX or set RSX_ZSTD_DLL.\n");
            return false;
        }

        if (!ResolveImports())
        {
            Log("RSX: Loaded Zstd decoder is missing required exports.\n");
            ResetBindings();
            return false;
        }

        return true;
    }

    bool EnsureBindings()
    {
        std::call_once(g_initFlag, []()
        {
            g_bindings.loaded = TryInitialize();
        });

        return g_bindings.loaded;
    }
}

namespace RTechZstd
{
    bool Decompress(const void* src, size_t srcSize, void* dst, size_t dstCapacity, size_t& decodedBytes)
    {
        decodedBytes = 0;

        if (!src || !dst || srcSize == 0 || dstCapacity == 0)
            return false;

        if (!EnsureBindings())
            return false;

        ZSTD_DCtx* ctx = g_bindings.createCtx();
        if (!ctx)
        {
            Log("RSX: Zstd decoder failed to allocate a context.\n");
            return false;
        }

        const size_t result = g_bindings.decompressCtx(ctx, dst, dstCapacity, src, srcSize);
        g_bindings.freeCtx(ctx);

        if (g_bindings.isError(result))
        {
            Log("RSX: Zstd decode failed (%s).\n", g_bindings.getErrorName(result));
            return false;
        }

        decodedBytes = result;
        return true;
    }

    size_t FindFrameCompressedSize(const void* src, size_t srcSize)
    {
        if (!src || srcSize == 0 || !EnsureBindings() || !g_bindings.findFrameCompressedSize)
            return 0;
        return g_bindings.findFrameCompressedSize(src, srcSize);
    }
}
