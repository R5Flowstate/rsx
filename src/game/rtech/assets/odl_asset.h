#pragma once

struct ODLAssetHeader_t
{
	uint64_t odlPakAssetGuid; // GUID of the "odlp" asset that describes the pakfile that contains this on-demand asset
	char* assetName; // Name of the asset that "originalAssetGuid" points to

	char unk10[8];
	uint64_t originalAssetGuid; // GUID of the original asset that this "odla" asset points to

	uint64_t placeholderAssetGuid; // GUID of the placeholder asset that is used while the real asset's pak is being loaded
								   // This seems to only be used for model assets as it has a value of 0 for ODL sticker assets
};

static_assert(sizeof(ODLAssetHeader_t) == 0x28);