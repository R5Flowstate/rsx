#pragma once
#include <game/rtech/cpakfile.h>

// rpak.cpp
FORCEINLINE void HandleExportBindingForAsset(CAsset* const asset, const bool exportDependencies, const bool exportDependents);
void HandlePakAssetExportList(std::deque<CAsset*> selectedAssets, const bool exportDependencies, const bool exportDependents);
void HandleExportAllPakAssets(std::vector<CGlobalAssetData::AssetLookup_t>* const pakAssets, const bool exportDependencies, const bool exportDependents);
void HandleExportSelectedAssetType(std::vector<CGlobalAssetData::AssetLookup_t> pakAssets, const bool exportDependencies, const bool exportDependents);

// list.cpp
void ExportAssetListCSVToFileStream(std::vector<CGlobalAssetData::AssetLookup_t>* assets, std::ofstream* ofs);
void ExportAssetListTXTToFileStream(std::vector<CGlobalAssetData::AssetLookup_t>* assets, std::ofstream* ofs);
void ExportDependenciesToFileStream_AdjList(std::vector<CGlobalAssetData::AssetLookup_t>* assets, std::ofstream* ofs);
void HandleListExportPakAssets(const HWND handle, std::vector<CGlobalAssetData::AssetLookup_t>* assets);
void HandleListExport(const HWND handle, std::vector<std::string> listElements);
