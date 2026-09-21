#include <pch.h>
#include <core/mdl/animdata.h>
#include <game/rtech/assets/animseq_data.h>

void LoadAnimSeqDataAsset(CAssetContainer* const container, CAsset* const asset)
{
	UNUSED(container);

	CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

	AnimSeqDataAsset* animSeqDataAsset = nullptr;

	switch (pakAsset->version())
	{
	case 1:
	{
		AnimSeqDataAssetHeader_v1_t* hdr = reinterpret_cast<AnimSeqDataAssetHeader_v1_t*>(pakAsset->header());
		animSeqDataAsset = new AnimSeqDataAsset(hdr);
		break;
	}
	default:
	{
		assertm(false, "unaccounted asset version, will cause major issues!");
		return;
	}
	}

	pakAsset->setExtraData(animSeqDataAsset);
}

static const char* const s_PathPrefixASQD = s_AssetTypePaths.find(AssetType_t::ASQD)->second;
bool ExportAnimSeqDataAsset(CAsset* const asset, const int setting)
{
	UNUSED(setting);

	CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

	if (!pakAsset->hasExtraData())
		return false;

	const AnimSeqDataAsset* const animSeqDataAsset = pakAsset->extraData<const AnimSeqDataAsset* const>();

	if (animSeqDataAsset->dataSize == 0)
	{
		Log("ASQD: Asset had size of 0. Skipping export\n", pakAsset->GetAssetGUID());
		return true;
	}

	// Create exported path + asset path.
	std::filesystem::path exportPath = g_ExportSettings.GetExportDirectory();
	const std::filesystem::path animPath(pakAsset->GetAssetName());
	const std::string animStem(animPath.stem().string());

	// truncate paths?
	if (g_ExportSettings.exportPathsFull)
		exportPath.append(animPath.parent_path().string());
	else
		exportPath.append(std::format("{}/{}", s_PathPrefixASQD, animStem));

	if (!CreateDirectories(exportPath))
	{
		assertm(false, "Failed to create asset directory.");
		return false;
	}

	exportPath.append(std::format("{}.asqd", animStem));

	StreamIO out(exportPath, eStreamIOMode::Write);
	out.write(animSeqDataAsset->data, animSeqDataAsset->dataSize);

	return true;
}

void ParseAnimSeqDataForSeq(ModelSeq_t* const seqdesc, const size_t boneCount, const bool sixBitFlags)
{
	for (size_t i = 0; i < seqdesc->AnimCount(); i++)
	{
		ModelAnim_t* const animdesc = seqdesc->anims + i;

		// [rika]: the animdesc has no animation
		if (!animdesc->animDataAsset)
			continue;

		CPakAsset* const dataAsset = g_assetData.FindAssetByGUID<CPakAsset>(animdesc->animDataAsset);
		if (!dataAsset)
		{
			animdesc->animData = nullptr;
			continue;
		}

		AnimSeqDataAsset* const animSeqDataAsset = dataAsset->extraData<AnimSeqDataAsset* const>();

		if (!animSeqDataAsset->data)
		{
			assertm(false, "animseq data ptr was invalid");
			continue;
		}

		animdesc->animData = animSeqDataAsset->data;

		// [rika]: has already been parsed
		if (animSeqDataAsset->dataSize > 0)
			continue;

		int index = 0;

		if (animdesc->sectionframes)
		{
			for (int section = animdesc->SectionCount(true) - 1; section >= 0; section--)
			{
				const ModelAnimSection_t* const pSection = animdesc->sections + section;

				if (pSection->isExternal)
					continue;

				// Take the last inline section, not the first: section data is
				// laid out in table order, so only a walk from the highest
				// inline entry covers every inline section. Stopping at the
				// first truncates multi-inline asqds (S30 gladcard/prop anims
				// with all sections inline dumped 84 B of a 244 B payload).
				index = pSection->animindex;
				break;
			}
		}

		const uint8_t* const boneFlagArray = reinterpret_cast<const uint8_t* const>(animdesc->animData + index);
		// S30 (seq v13) packs the bone flag array at 6 bits per bone, IALIGN2.
		const size_t bfaSize = sixBitFlags ? IALIGN2((6 * boneCount + 7) / 8) : ANIM_BONEFLAG_SIZE(boneCount);
		const r5::mstudio_rle_anim_t* panim = reinterpret_cast<const r5::mstudio_rle_anim_t*>(&boneFlagArray[bfaSize]);

		for (size_t bone = 0; bone < boneCount; bone++)
		{
			uint8_t boneFlags;
			if (sixBitFlags)
			{
				const size_t bit = bone * 6;
				uint32_t v = boneFlagArray[bit / 8] >> (bit % 8);
				if ((bit % 8) > 2)
					v |= static_cast<uint32_t>(boneFlagArray[bit / 8 + 1]) << (8 - (bit % 8));
				boneFlags = static_cast<uint8_t>(v & 0x3F);
			}
			else
				boneFlags = ANIM_BONEFLAGS_FLAG(boneFlagArray, bone);

			// no header for this bone
			if ((boneFlags & r5::RleBoneFlags_t::STUDIO_ANIM_MASK) == false)
			{
				continue;
			}

			panim = panim->pNext();
		}

		animSeqDataAsset->dataSize = reinterpret_cast<const char* const>(panim) - animdesc->animData;
		assertm(animSeqDataAsset->dataSize, "parsed asqd had a size of 0");
	}
}

void InitAnimSeqDataAssetType()
{
	AssetTypeBinding_t type =
	{
		.name = "Animation Sequence Data",
		.type = 'dqsa',
		.headerAlignment = 8,
		.loadFunc = LoadAnimSeqDataAsset,
		.postLoadFunc = nullptr,
		.previewFunc = nullptr,
		.e = { ExportAnimSeqDataAsset, 0, nullptr, 0ull },
	};

	REGISTER_TYPE(type);
}