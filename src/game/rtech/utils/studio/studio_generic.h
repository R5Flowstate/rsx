#pragma once
#include <game/rtech/utils/studio/studio.h>
#include <game/rtech/utils/studio/studio_r1.h>
#include <game/rtech/utils/studio/studio_r2.h>
#include <game/rtech/utils/studio/studio_r5.h>
#include <game/rtech/utils/studio/studio_r5_v12.h>
#include <game/rtech/utils/studio/studio_r5_v16.h>

struct studio_hw_groupdata_t
{
	studio_hw_groupdata_t() = default;
	studio_hw_groupdata_t(const r5::studio_hw_groupdata_v16_t* const group);
	studio_hw_groupdata_t(const r5::studio_hw_groupdata_v12_1_t* const group);

	int dataOffset;				// offset to this section in compressed vg
	int dataSizeCompressed;		// compressed size of this lod buffer in hwData
	int dataSizeDecompressed;	// decompressed size of this lod buffer in hwData

	eCompressionType dataCompression; // none and oodle, haven't seen anything else used.

	//
	uint8_t lodIndex;		// base lod idx?
	uint8_t lodCount;		// number of lods contained within this group
	uint8_t lodMap;		// lods in this group, each bit is a lod
};
static_assert(sizeof(studio_hw_groupdata_t) == 16);

struct studiohdr_generic_t
{
	studiohdr_generic_t() = default;
	studiohdr_generic_t(const r1::studiohdr_t* const pHdr, StudioLooseData_t* const pData);
	studiohdr_generic_t(const r2::studiohdr_t* const pHdr);
	studiohdr_generic_t(const r5::studiohdr_v8_t* const pHdr);
	studiohdr_generic_t(const r5::studiohdr_v12_1_t* const pHdr);
	studiohdr_generic_t(const r5::studiohdr_v12_2_t* const pHdr);
	studiohdr_generic_t(const r5::studiohdr_v12_4_t* const pHdr);
	studiohdr_generic_t(const r5::studiohdr_v14_t* const pHdr, const int version);
	studiohdr_generic_t(const r5::studiohdr_v16_t* const pHdr, int dataSizePhys, int dataSizeModel);
	studiohdr_generic_t(const r5::studiohdr_v17_t* const pHdr, int dataSizePhys, int dataSizeModel);

	const char* baseptr;	// studiohdr_t pointer and by extension mdl/rmdl ptr

	int szNameOffset;		// file path and interally stored path, this should match across the real directory, name[64] memeber, and sznameindex member.
	inline const char* const pszName() const { return baseptr + szNameOffset; }

	int length;				// file length in bytes

	Vector eyeposition;		// ideal eye position
	Vector illumposition;	// illumination center

	Vector hull_min;	// ideal movement hull size
	Vector hull_max;	// ideal movement hull size

	Vector view_bbmin;	// clipping bounding box
	Vector view_bbmax;	// clipping bounding box

	int flags;

	int boneCount;
	int boneOffset;		// offset to mstudiobone_t or mstudiobonehdr_t
	int boneDataOffset; // offset to mstudiobonedata_t (rmdl v16+)
	inline const char* const pBones() const { return baseptr + boneOffset; };
	inline const char* const pBoneData() const { return baseptr + boneDataOffset; };
	inline const char* const pLinearBone() const { return baseptr + linearBoneOffset; };

	int hitboxSetCount;
	int hitboxSetOffset;

	int localAnimationCount;
	int localAnimationOffset;	// offset to mstudioanimdesc_t (unused in apex, removed in retail)

	int localSequenceCount;
	int localSequenceOffset;	// offset to mstudioseqdesc_t

	int textureCount;
	int textureOffset;	// offset to mstudiotexture_t
	inline const char* const pTextures() const { return baseptr + textureOffset; }

	int cdTexturesCount;
	int cdTexturesOffset;
	inline const char* const pCdtexture(const int i) const { return baseptr + reinterpret_cast<const int* const>(baseptr + cdTexturesOffset)[i]; }

	int numSkinRef;			// skingroup width (how many textures/indices per group)
	int numSkinFamilies;	// number of skingroups
	int skinOffset;
	inline const int16_t* const pSkinref(const int i) const { return reinterpret_cast<const int16_t* const>(baseptr + skinOffset) + i; }
	inline const int16_t* const pSkinFamily(const int i) const { return pSkinref(numSkinRef * i); };
	inline const char* const pSkinName(const int i) const
	{
		// only stored for index 1 and up
		// [rika]: in code this actually returns '\0'
		if (i == 0)
			return STUDIO_DEFAULT_SKIN_NAME;

		const char* name = nullptr;

		if (smallIndices) LIKELY
		{
			const uint16_t index = *(reinterpret_cast<const uint16_t* const>(pSkinFamily(numSkinFamilies)) + (i - 1));
			name = baseptr + FIX_OFFSET(index);
		}
		else
		{
			// [rika]: this is actually aligned to four bytes, we just use IALIGN2 because we're getting the number of indices (each having a size of 2 bytes) and not the total size
			const int index = reinterpret_cast<const int* const>(pSkinFamily(0) + (IALIGN2(numSkinRef * numSkinFamilies)))[i - 1];
			name = baseptr + index;
		}

		if (IsStringZeroLength(name))
			return STUDIO_NULL_SKIN_NAME;

		return name;
	}

	// bodypart

	int localAttachmentCount;
	int localAttachmentOffset;	// offset to mstudioattachment_t

	int localNodeCount;
	int localNodeNameOffset;
	int localNodeNameType;

	int ikChainCount;
	int ikChainOffset;

	int localPoseParamCount;
	int localPoseParamOffset;

	int surfacePropOffset;	// offset to surface prop string
	inline const char* const pszSurfaceProp() const { return baseptr + surfacePropOffset; }

	int keyValueOffset;		// offset to keyvalues
	int keyValueSize;		// removed in later rmdl, keyvalues are null terminated
	inline const char* const KeyValueText() const { return baseptr + keyValueOffset; }

	int localIkAutoPlayLockCount;
	int localIkAutoPlayLockOffset;

	float mass;
	int contents;	// contents mask, see bspflags.h

	int includeModelCount;
	int includeModelOffset;

	uint8_t constdirectionallightdot;
	uint8_t rootLOD;
	uint8_t numAllowedRootLODs;

	uint8_t pad;

	float fadeDistance; // set to -1 to never fade. set above 0 if you want it to fade out, distance is in feet.
	float gatherSize;

	int	illumpositionattachmentindex;

	float flMaxEyeDeflection;

	int linearBoneOffset; // offset to mstudiolinearbone_t (null if not present)

	int srcBoneTransformCount;
	int srcBoneTransformOffset;
	inline const mstudiosrcbonetransform_t* const pSrcBoneTransform(const int i) const { return reinterpret_cast<const mstudiosrcbonetransform_t* const>(baseptr + srcBoneTransformOffset) + i; }

	int boneFollowerCount;
	int boneFollowerOffset;

	int vtxOffset; // VTX
	int vvdOffset; // VVD / IDSV
	int vvcOffset; // VVC / IDCV
	int vvwOffset; // index will come last after other vertex files
	int phyOffset; // VPHY / IVPS

	int vtxSize;
	int vvdSize;
	int vvcSize;
	int vvwSize;
	int phySize; // still used in models using vg

	size_t hwDataSize;
	
	int boneStateOffset;
	int boneStateCount;
	inline const uint8_t* const pBoneStates() const { return boneStateCount > 0 ? reinterpret_cast<uint8_t*>((char*)baseptr + boneStateOffset) : nullptr; }

	int bvhOffset;

	bool smallIndices; // this model uses smaller values for structs, v16 and later rmdl

	int groupCount;
	studio_hw_groupdata_t groups[8]; // early 'vg' will only have one group
};

// make x axis y axis, and y axis negative x axis
inline void StaticPropFlipFlop(Vector& in)
{
	const Vector tmp(in);

	in.x = tmp.y;
	in.y = -tmp.x;
}

// shouldn't append on first lod if name doesn't have _lod%i in it
inline void FixupExportLodNames(std::string& fileName, int lodLevel)
{
	char lodName[16]{};

	if (fileName.rfind("_lod0") != std::string::npos || fileName.rfind("_LOD0") != std::string::npos)
	{
		snprintf(lodName, 8, "%i", lodLevel);
		fileName.replace(fileName.length() - 1, 8, lodName);

		return;
	}

	snprintf(lodName, 8, "_lod%i", lodLevel);
	fileName.append(lodName);
}