#pragma once
#include <game/rtech/cpakfile.h>
#include <game/rtech/utils/utils.h>

struct AnimSeqDataAssetHeader_v1_t
{
	char* data;
};
static_assert(sizeof(AnimSeqDataAssetHeader_v1_t) == 0x8);

class AnimSeqDataAsset
{
public:
	AnimSeqDataAsset(AnimSeqDataAssetHeader_v1_t* hdr) : data(hdr->data), dataSize(0ull)
	{
	
	}

	char* data;

	size_t dataSize;
};

// sixBitFlags: S30 (seq v13) packs the bone flag array at 6 bits per bone.
void ParseAnimSeqDataForSeq(ModelSeq_t* const seqdesc, const size_t boneCount, const bool sixBitFlags = false);