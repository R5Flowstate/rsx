#include <pch.h>
#include <game/rtech/utils/studio/studio_generic.h>

// generic studio data
// studio hw group
studio_hw_groupdata_t::studio_hw_groupdata_t(const r5::studio_hw_groupdata_v16_t* const group) : dataOffset(group->dataOffset), dataSizeCompressed(group->dataSizeCompressed), dataSizeDecompressed(group->dataSizeDecompressed), dataCompression(group->dataCompression),
lodIndex(group->lodIndex), lodCount(group->lodCount), lodMap(group->lodMap)
{

};

studio_hw_groupdata_t::studio_hw_groupdata_t(const r5::studio_hw_groupdata_v12_1_t* const group) : dataOffset(group->dataOffset), dataSizeCompressed(-1), dataSizeDecompressed(group->dataSize), dataCompression(eCompressionType::NONE),
lodIndex(static_cast<uint8_t>(group->lodIndex)), lodCount(static_cast<uint8_t>(group->lodCount)), lodMap(static_cast<uint8_t>(group->lodMap))
{

};

// studiohdr
// clamp silly studiomdl offsets (negative is base ptr)
#define FIX_FILE_OFFSET(offset) (offset < 0 ? 0 : offset)
studiohdr_generic_t::studiohdr_generic_t(const r1::studiohdr_t* const pHdr, StudioLooseData_t* const pData) : baseptr(reinterpret_cast<const char*>(pHdr)), szNameOffset(pHdr->pStudioHdr2()->sznameindex), length(pHdr->length), flags(pHdr->flags), mass(pHdr->mass), contents(pHdr->contents),
	eyeposition(pHdr->eyeposition), illumposition(pHdr->illumposition), hull_min(pHdr->hull_min), hull_max(pHdr->hull_max), view_bbmin(pHdr->view_bbmin), view_bbmax(pHdr->view_bbmax),

	// bone
	boneCount(pHdr->numbones), boneOffset(pHdr->boneindex), boneDataOffset(-1), hitboxSetCount(pHdr->numhitboxsets), hitboxSetOffset(pHdr->hitboxsetindex), localAttachmentCount(pHdr->numlocalattachments), localAttachmentOffset(pHdr->localattachmentindex),
	linearBoneOffset(pHdr->pStudioHdr2()->linearboneindex), srcBoneTransformCount(pHdr->pStudioHdr2()->numsrcbonetransform), srcBoneTransformOffset(pHdr->pStudioHdr2()->srcbonetransformindex), boneFollowerCount(0), boneFollowerOffset(0),
	boneStateCount(0), boneStateOffset(0), bvhOffset(-1),

	// animations
	includeModelCount(pHdr->numincludemodels), includeModelOffset(pHdr->includemodelindex), localAnimationCount(pHdr->numlocalanim), localAnimationOffset(pHdr->localanimindex), localSequenceCount(pHdr->numlocalseq), localSequenceOffset(pHdr->localseqindex),
	ikChainCount(pHdr->numikchains), ikChainOffset(pHdr->ikchainindex), localPoseParamCount(pHdr->numlocalposeparameters), localPoseParamOffset(pHdr->localposeparamindex), localIkAutoPlayLockCount(pHdr->numlocalikautoplaylocks), localIkAutoPlayLockOffset(pHdr->localikautoplaylockindex),
	localNodeCount(pHdr->numlocalnodes), localNodeNameOffset(pHdr->localnodenameindex), localNodeNameType(0),

	// material
	textureCount(pHdr->numtextures), textureOffset(pHdr->textureindex), cdTexturesCount(pHdr->numcdtextures), cdTexturesOffset(pHdr->cdtextureindex), numSkinRef(pHdr->numskinref), numSkinFamilies(pHdr->numskinfamilies), skinOffset(pHdr->skinindex),
	
	// misc
	surfacePropOffset(pHdr->surfacepropindex), keyValueOffset(pHdr->keyvalueindex), keyValueSize(pHdr->keyvaluesize), constdirectionallightdot(pHdr->constdirectionallightdot), rootLOD(pHdr->rootLOD), numAllowedRootLODs(pHdr->numAllowedRootLODs),
	fadeDistance(pHdr->fadeDistance), gatherSize(0.0f), illumpositionattachmentindex(pHdr->pStudioHdr2()->illumpositionattachmentindex), flMaxEyeDeflection(pHdr->pStudioHdr2()->flMaxEyeDeflection),
	
	// file
	vtxOffset(pData->VertOffset(StudioLooseData_t::SLD_VTX)), vvdOffset(pData->VertOffset(StudioLooseData_t::SLD_VVD)), vvcOffset(pData->VertOffset(StudioLooseData_t::SLD_VVC)), vvwOffset(-1), phyOffset(pData->PhysOffset()),
	vtxSize(pData->VertSize(StudioLooseData_t::SLD_VTX)), vvdSize(pData->VertSize(StudioLooseData_t::SLD_VVD)), vvcSize(pData->VertSize(StudioLooseData_t::SLD_VVC)), vvwSize(-1), phySize(pData->PhysSize()), hwDataSize(0),
	groupCount(0), groups(),

	pad(), smallIndices(false)
{
	// update offsets local to studiohdr2_t
	const int baseOffset = static_cast<int>(reinterpret_cast<const char* const>(pHdr->pStudioHdr2()) - baseptr);

	szNameOffset -= baseOffset;
	linearBoneOffset -= baseOffset;
};

studiohdr_generic_t::studiohdr_generic_t(const r2::studiohdr_t* const pHdr) : baseptr(reinterpret_cast<const char*>(pHdr)), szNameOffset(pHdr->sznameindex), length(pHdr->length), flags(pHdr->flags), mass(pHdr->mass), contents(pHdr->contents),
	eyeposition(pHdr->eyeposition), illumposition(pHdr->illumposition), hull_min(pHdr->hull_min), hull_max(pHdr->hull_max), view_bbmin(pHdr->view_bbmin), view_bbmax(pHdr->view_bbmax),

	// bone
	boneCount(pHdr->numbones), boneOffset(pHdr->boneindex), boneDataOffset(-1), hitboxSetCount(pHdr->numhitboxsets), hitboxSetOffset(pHdr->hitboxsetindex), localAttachmentCount(pHdr->numlocalattachments), localAttachmentOffset(pHdr->localattachmentindex),
	linearBoneOffset(pHdr->linearboneindex), srcBoneTransformCount(pHdr->numsrcbonetransform), srcBoneTransformOffset(pHdr->srcbonetransformindex), boneFollowerCount(pHdr->boneFollowerCount), boneFollowerOffset(pHdr->boneFollowerOffset),
	boneStateCount(0), boneStateOffset(0), bvhOffset(-1),

	// animations
	includeModelCount(pHdr->numincludemodels), includeModelOffset(pHdr->includemodelindex), localAnimationCount(pHdr->numlocalanim), localAnimationOffset(pHdr->localanimindex), localSequenceCount(pHdr->numlocalseq), localSequenceOffset(pHdr->localseqindex),
	ikChainCount(pHdr->numikchains), ikChainOffset(pHdr->ikchainindex), localPoseParamCount(pHdr->numlocalposeparameters), localPoseParamOffset(pHdr->localposeparamindex), localIkAutoPlayLockCount(pHdr->numlocalikautoplaylocks), localIkAutoPlayLockOffset(pHdr->localikautoplaylockindex),
	localNodeCount(pHdr->numlocalnodes), localNodeNameOffset(pHdr->localnodenameindex), localNodeNameType(0),

	// material
	textureCount(pHdr->numtextures), textureOffset(pHdr->textureindex), cdTexturesCount(pHdr->numcdtextures), cdTexturesOffset(pHdr->cdtextureindex), numSkinRef(pHdr->numskinref), numSkinFamilies(pHdr->numskinfamilies), skinOffset(pHdr->skinindex),
	
	// misc
	surfacePropOffset(pHdr->surfacepropindex), keyValueOffset(pHdr->keyvalueindex), keyValueSize(pHdr->keyvaluesize), constdirectionallightdot(pHdr->constdirectionallightdot), rootLOD(pHdr->rootLOD), numAllowedRootLODs(pHdr->numAllowedRootLODs),
	fadeDistance(pHdr->fadeDistance), gatherSize(0.0f), illumpositionattachmentindex(pHdr->illumpositionattachmentindex), flMaxEyeDeflection(0.0f),

	// file
	vtxOffset(pHdr->vtxOffset), vvdOffset(pHdr->vvdOffset), vvcOffset(pHdr->vvcOffset), vvwOffset(-1), phyOffset(pHdr->phyOffset),
	vtxSize(pHdr->vtxSize), vvdSize(pHdr->vvdSize), vvcSize(pHdr->vvcSize), vvwSize(-1), phySize(pHdr->phySize), hwDataSize(0),
	groupCount(0), groups(),

	pad(), smallIndices(false)
{

};

studiohdr_generic_t::studiohdr_generic_t(const r5::studiohdr_v8_t* const pHdr) : baseptr(reinterpret_cast<const char*>(pHdr)), szNameOffset(pHdr->sznameindex), length(pHdr->length), flags(pHdr->flags), mass(pHdr->mass), contents(pHdr->contents),
	eyeposition(pHdr->eyeposition), illumposition(pHdr->illumposition), hull_min(pHdr->hull_min), hull_max(pHdr->hull_max), view_bbmin(pHdr->view_bbmin), view_bbmax(pHdr->view_bbmax),

	// bone
	boneCount(pHdr->numbones), boneOffset(pHdr->boneindex), boneDataOffset(-1), hitboxSetCount(pHdr->numhitboxsets), hitboxSetOffset(pHdr->hitboxsetindex), localAttachmentCount(pHdr->numlocalattachments), localAttachmentOffset(pHdr->localattachmentindex),
	linearBoneOffset(pHdr->linearboneindex), srcBoneTransformCount(pHdr->numsrcbonetransform), srcBoneTransformOffset(pHdr->srcbonetransformindex), boneFollowerCount(pHdr->boneFollowerCount), boneFollowerOffset(pHdr->boneFollowerOffset),
	boneStateCount(0), boneStateOffset(0), bvhOffset(pHdr->bvhOffset),

	// animations
	includeModelCount(pHdr->numincludemodels), includeModelOffset(pHdr->includemodelindex), localAnimationCount(pHdr->numlocalanim), localAnimationOffset(pHdr->localanimindex), localSequenceCount(pHdr->numlocalseq), localSequenceOffset(pHdr->localseqindex),
	ikChainCount(pHdr->numikchains), ikChainOffset(pHdr->ikchainindex), localPoseParamCount(pHdr->numlocalposeparameters), localPoseParamOffset(pHdr->localposeparamindex), localIkAutoPlayLockCount(pHdr->numlocalikautoplaylocks), localIkAutoPlayLockOffset(pHdr->localikautoplaylockindex),
	localNodeCount(pHdr->numlocalnodes), localNodeNameOffset(pHdr->localnodenameindex), localNodeNameType(0),

	// material
	textureCount(pHdr->numtextures), textureOffset(pHdr->textureindex), cdTexturesCount(pHdr->numcdtextures), cdTexturesOffset(pHdr->cdtextureindex), numSkinRef(pHdr->numskinref), numSkinFamilies(pHdr->numskinfamilies), skinOffset(pHdr->skinindex),
	
	// misc
	surfacePropOffset(pHdr->surfacepropindex), keyValueOffset(pHdr->keyvalueindex), keyValueSize(pHdr->keyvaluesize), constdirectionallightdot(pHdr->constdirectionallightdot), rootLOD(pHdr->rootLOD), numAllowedRootLODs(pHdr->numAllowedRootLODs), 
	fadeDistance(pHdr->fadeDistance), gatherSize(pHdr->gatherSize), illumpositionattachmentindex(pHdr->illumpositionattachmentindex), flMaxEyeDeflection(0.0f),
	
	// file
	vtxOffset(FIX_FILE_OFFSET(pHdr->vtxOffset)), vvdOffset(FIX_FILE_OFFSET(pHdr->vvdOffset)), vvcOffset(FIX_FILE_OFFSET(pHdr->vvcOffset)), vvwOffset(FIX_FILE_OFFSET(pHdr->vvwOffset)), phyOffset(FIX_FILE_OFFSET(pHdr->phyOffset)),
	vtxSize(pHdr->vtxSize), vvdSize(pHdr->vvdSize), vvcSize(pHdr->vvcSize), vvwSize(pHdr->vvwSize), phySize(pHdr->phySize), hwDataSize(0)/*set in vertex parsing*/,
	groupCount(1), groups(),

	pad(), smallIndices(false)
{

};

studiohdr_generic_t::studiohdr_generic_t(const r5::studiohdr_v12_1_t* const pHdr) : baseptr(reinterpret_cast<const char*>(pHdr)), szNameOffset(pHdr->sznameindex), length(pHdr->length), flags(pHdr->flags), mass(pHdr->mass), contents(pHdr->contents),
	eyeposition(pHdr->eyeposition), illumposition(pHdr->illumposition), hull_min(pHdr->hull_min), hull_max(pHdr->hull_max), view_bbmin(pHdr->view_bbmin), view_bbmax(pHdr->view_bbmax),

	// bone
	boneCount(pHdr->numbones), boneOffset(pHdr->boneindex), boneDataOffset(-1), hitboxSetCount(pHdr->numhitboxsets), hitboxSetOffset(pHdr->hitboxsetindex), localAttachmentCount(pHdr->numlocalattachments), localAttachmentOffset(pHdr->localattachmentindex),
	linearBoneOffset(pHdr->linearboneindex), srcBoneTransformCount(pHdr->numsrcbonetransform), srcBoneTransformOffset(pHdr->srcbonetransformindex), boneFollowerCount(pHdr->boneFollowerCount), boneFollowerOffset(pHdr->boneFollowerOffset),
	boneStateOffset(offsetof(r5::studiohdr_v12_1_t, boneStateOffset) + pHdr->boneStateOffset), boneStateCount(pHdr->boneStateCount), bvhOffset(pHdr->bvhOffset),

	// animations
	includeModelCount(pHdr->numincludemodels), includeModelOffset(pHdr->includemodelindex), localAnimationCount(pHdr->numlocalanim), localAnimationOffset(pHdr->localanimindex), localSequenceCount(pHdr->numlocalseq), localSequenceOffset(pHdr->localseqindex),
	ikChainCount(pHdr->numikchains), ikChainOffset(pHdr->ikchainindex), localPoseParamCount(pHdr->numlocalposeparameters), localPoseParamOffset(pHdr->localposeparamindex), localIkAutoPlayLockCount(pHdr->numlocalikautoplaylocks), localIkAutoPlayLockOffset(pHdr->localikautoplaylockindex),
	localNodeCount(pHdr->numlocalnodes), localNodeNameOffset(pHdr->localnodenameindex), localNodeNameType(0),

	// material
	textureCount(pHdr->numtextures), textureOffset(pHdr->textureindex), cdTexturesCount(pHdr->numcdtextures), cdTexturesOffset(pHdr->cdtextureindex), numSkinRef(pHdr->numskinref), numSkinFamilies(pHdr->numskinfamilies), skinOffset(pHdr->skinindex),

	// misc
	surfacePropOffset(pHdr->surfacepropindex), keyValueOffset(pHdr->keyvalueindex), keyValueSize(pHdr->keyvaluesize), constdirectionallightdot(0), rootLOD(0), numAllowedRootLODs(0),
	fadeDistance(pHdr->fadeDistance), gatherSize(pHdr->gatherSize), illumpositionattachmentindex(pHdr->illumpositionattachmentindex), flMaxEyeDeflection(0.0f),

	// file
	vtxOffset(FIX_FILE_OFFSET(pHdr->vtxOffset)), vvdOffset(FIX_FILE_OFFSET(pHdr->vvdOffset)), vvcOffset(FIX_FILE_OFFSET(pHdr->vvcOffset)), vvwOffset(FIX_FILE_OFFSET(pHdr->vvwOffset)), phyOffset(FIX_FILE_OFFSET(pHdr->phyOffset)),
	vtxSize(pHdr->vtxSize), vvdSize(pHdr->vvdSize), vvcSize(pHdr->vvcSize), vvwSize(pHdr->vvwSize), phySize(pHdr->phySize), hwDataSize(pHdr->hwDataSize),
	groupCount(pHdr->groupHeaderCount), groups(),

	pad(), smallIndices(false)
{
	assertm(pHdr->groupHeaderCount <= 8, "model has more than 8 lods");

	for (int i = 0; i < groupCount; i++)
	{
		if (i == 8)
			break;

		groups[i] = studio_hw_groupdata_t(pHdr->pLODGroup(i));
	}
};

studiohdr_generic_t::studiohdr_generic_t(const r5::studiohdr_v12_2_t* const pHdr) : baseptr(reinterpret_cast<const char*>(pHdr)), szNameOffset(pHdr->sznameindex), length(pHdr->length), flags(pHdr->flags), mass(pHdr->mass), contents(pHdr->contents),
	eyeposition(pHdr->eyeposition), illumposition(pHdr->illumposition), hull_min(pHdr->hull_min), hull_max(pHdr->hull_max), view_bbmin(pHdr->view_bbmin), view_bbmax(pHdr->view_bbmax),

	// bone
	boneCount(pHdr->numbones), boneOffset(pHdr->boneindex), boneDataOffset(-1), hitboxSetCount(pHdr->numhitboxsets), hitboxSetOffset(pHdr->hitboxsetindex), localAttachmentCount(pHdr->numlocalattachments), localAttachmentOffset(pHdr->localattachmentindex),
	linearBoneOffset(pHdr->linearboneindex), srcBoneTransformCount(pHdr->numsrcbonetransform), srcBoneTransformOffset(pHdr->srcbonetransformindex), boneFollowerCount(pHdr->boneFollowerCount), boneFollowerOffset(pHdr->boneFollowerOffset),
	boneStateOffset(offsetof(r5::studiohdr_v12_2_t, boneStateOffset) + pHdr->boneStateOffset), boneStateCount(pHdr->boneStateCount), bvhOffset(pHdr->bvhOffset),

	// animations
	includeModelCount(pHdr->numincludemodels), includeModelOffset(pHdr->includemodelindex), localAnimationCount(pHdr->numlocalanim), localAnimationOffset(pHdr->localanimindex), localSequenceCount(pHdr->numlocalseq), localSequenceOffset(pHdr->localseqindex),
	ikChainCount(pHdr->numikchains), ikChainOffset(pHdr->ikchainindex), localPoseParamCount(pHdr->numlocalposeparameters), localPoseParamOffset(pHdr->localposeparamindex), localIkAutoPlayLockCount(pHdr->numlocalikautoplaylocks), localIkAutoPlayLockOffset(pHdr->localikautoplaylockindex),
	localNodeCount(pHdr->numlocalnodes), localNodeNameOffset(pHdr->localnodenameindex), localNodeNameType(0),

	// material
	textureCount(pHdr->numtextures), textureOffset(pHdr->textureindex), cdTexturesCount(pHdr->numcdtextures), cdTexturesOffset(pHdr->cdtextureindex), numSkinRef(pHdr->numskinref), numSkinFamilies(pHdr->numskinfamilies), skinOffset(pHdr->skinindex),

	// misc
	surfacePropOffset(pHdr->surfacepropindex), keyValueOffset(pHdr->keyvalueindex), keyValueSize(pHdr->keyvaluesize), constdirectionallightdot(0), rootLOD(0), numAllowedRootLODs(0),
	fadeDistance(pHdr->fadeDistance), gatherSize(pHdr->gatherSize), illumpositionattachmentindex(pHdr->illumpositionattachmentindex), flMaxEyeDeflection(0.0f),

	// file
	vtxOffset(FIX_FILE_OFFSET(pHdr->vtxOffset)), vvdOffset(FIX_FILE_OFFSET(pHdr->vvdOffset)), vvcOffset(FIX_FILE_OFFSET(pHdr->vvcOffset)), vvwOffset(FIX_FILE_OFFSET(pHdr->vvwOffset)), phyOffset(FIX_FILE_OFFSET(pHdr->phyOffset)),
	vtxSize(pHdr->vtxSize), vvdSize(pHdr->vvdSize), vvcSize(pHdr->vvcSize), vvwSize(pHdr->vvwSize), phySize(pHdr->phySize), hwDataSize(pHdr->hwDataSize),
	groupCount(pHdr->groupHeaderCount), groups(),

	pad(), smallIndices(false)
{
	assertm(pHdr->groupHeaderCount <= 8, "model has more than 8 lods");

	for (int i = 0; i < groupCount; i++)
	{
		if (i == 8)
			break;

		groups[i] = studio_hw_groupdata_t(pHdr->pLODGroup(i));
	}
};

// [rika]: v12.4 and v12.5
studiohdr_generic_t::studiohdr_generic_t(const r5::studiohdr_v12_4_t* const pHdr) : baseptr(reinterpret_cast<const char*>(pHdr)), szNameOffset(pHdr->sznameindex), length(pHdr->length), flags(pHdr->flags), mass(pHdr->mass), contents(pHdr->contents),
	eyeposition(pHdr->eyeposition), illumposition(pHdr->illumposition), hull_min(pHdr->hull_min), hull_max(pHdr->hull_max), view_bbmin(pHdr->view_bbmin), view_bbmax(pHdr->view_bbmax),

	// bone
	boneCount(pHdr->numbones), boneOffset(pHdr->boneindex), boneDataOffset(-1), hitboxSetCount(pHdr->numhitboxsets), hitboxSetOffset(pHdr->hitboxsetindex), localAttachmentCount(pHdr->numlocalattachments), localAttachmentOffset(pHdr->localattachmentindex),
	linearBoneOffset(pHdr->linearboneindex), srcBoneTransformCount(pHdr->numsrcbonetransform), srcBoneTransformOffset(pHdr->srcbonetransformindex), boneFollowerCount(pHdr->boneFollowerCount), boneFollowerOffset(pHdr->boneFollowerOffset),
	boneStateOffset(offsetof(r5::studiohdr_v12_4_t, boneStateOffset) + pHdr->boneStateOffset), boneStateCount(pHdr->boneStateCount), bvhOffset(pHdr->bvhOffset),

	// animations
	includeModelCount(pHdr->numincludemodels), includeModelOffset(pHdr->includemodelindex), localAnimationCount(pHdr->numlocalanim), localAnimationOffset(pHdr->localanimindex), localSequenceCount(pHdr->numlocalseq), localSequenceOffset(pHdr->localseqindex),
	ikChainCount(pHdr->numikchains), ikChainOffset(pHdr->ikchainindex), localPoseParamCount(pHdr->numlocalposeparameters), localPoseParamOffset(pHdr->localposeparamindex), localIkAutoPlayLockCount(pHdr->numlocalikautoplaylocks), localIkAutoPlayLockOffset(pHdr->localikautoplaylockindex),
	localNodeCount(pHdr->numlocalnodes), localNodeNameOffset(pHdr->localnodenameindex), localNodeNameType(0),

	// material
	textureCount(pHdr->numtextures), textureOffset(pHdr->textureindex), cdTexturesCount(pHdr->numcdtextures), cdTexturesOffset(pHdr->cdtextureindex), numSkinRef(pHdr->numskinref), numSkinFamilies(pHdr->numskinfamilies), skinOffset(pHdr->skinindex),

	// misc
	surfacePropOffset(pHdr->surfacepropindex), keyValueOffset(pHdr->keyvalueindex), keyValueSize(pHdr->keyvaluesize), constdirectionallightdot(0), rootLOD(0), numAllowedRootLODs(0),
	fadeDistance(pHdr->fadeDistance), gatherSize(pHdr->gatherSize), illumpositionattachmentindex(pHdr->illumpositionattachmentindex), flMaxEyeDeflection(0.0f),

	// file
	vtxOffset(FIX_FILE_OFFSET(pHdr->vtxOffset)), vvdOffset(FIX_FILE_OFFSET(pHdr->vvdOffset)), vvcOffset(FIX_FILE_OFFSET(pHdr->vvcOffset)), vvwOffset(FIX_FILE_OFFSET(pHdr->vvwOffset)), phyOffset(FIX_FILE_OFFSET(pHdr->phyOffset)),
	vtxSize(pHdr->vtxSize), vvdSize(pHdr->vvdSize), vvcSize(pHdr->vvcSize), vvwSize(pHdr->vvwSize), phySize(pHdr->phySize), hwDataSize(pHdr->hwDataSize),
	groupCount(pHdr->groupHeaderCount), groups(),

	pad(), smallIndices(false)
{
	assertm(pHdr->groupHeaderCount <= 8, "model has more than 8 lods");

	for (int i = 0; i < groupCount; i++)
	{
		if (i == 8)
			break;

		groups[i] = studio_hw_groupdata_t(pHdr->pLODGroup(i));
	}
};

studiohdr_generic_t::studiohdr_generic_t(const r5::studiohdr_v14_t* const pHdr, const int version) : baseptr(reinterpret_cast<const char*>(pHdr)), szNameOffset(pHdr->sznameindex), length(pHdr->length), flags(pHdr->flags), mass(pHdr->mass), contents(pHdr->contents),
	eyeposition(pHdr->eyeposition), illumposition(pHdr->illumposition), hull_min(pHdr->hull_min), hull_max(pHdr->hull_max), view_bbmin(pHdr->view_bbmin), view_bbmax(pHdr->view_bbmax),

	// bone
	boneCount(pHdr->numbones), boneOffset(pHdr->boneindex), boneDataOffset(-1), hitboxSetCount(pHdr->numhitboxsets), hitboxSetOffset(pHdr->hitboxsetindex), localAttachmentCount(pHdr->numlocalattachments), localAttachmentOffset(pHdr->localattachmentindex),
	linearBoneOffset(pHdr->linearboneindex), srcBoneTransformCount(pHdr->numsrcbonetransform), srcBoneTransformOffset(pHdr->srcbonetransformindex), boneFollowerCount(pHdr->boneFollowerCount), boneFollowerOffset(pHdr->boneFollowerOffset),
	boneStateOffset(offsetof(r5::studiohdr_v14_t, boneStateOffset) + pHdr->boneStateOffset), boneStateCount(pHdr->boneStateCount), bvhOffset(pHdr->bvhOffset),

	// animations
	includeModelCount(pHdr->numincludemodels), includeModelOffset(pHdr->includemodelindex), localAnimationCount(pHdr->numlocalanim), localAnimationOffset(pHdr->localanimindex), localSequenceCount(pHdr->numlocalseq), localSequenceOffset(pHdr->localseqindex),
	ikChainCount(pHdr->numikchains), ikChainOffset(pHdr->ikchainindex), localPoseParamCount(pHdr->numlocalposeparameters), localPoseParamOffset(pHdr->localposeparamindex), localIkAutoPlayLockCount(pHdr->numlocalikautoplaylocks), localIkAutoPlayLockOffset(pHdr->localikautoplaylockindex),
	localNodeCount(pHdr->numlocalnodes), localNodeNameOffset(pHdr->localnodenameindex), localNodeNameType(version),

	// material
	textureCount(pHdr->numtextures), textureOffset(pHdr->textureindex), cdTexturesCount(pHdr->numcdtextures), cdTexturesOffset(pHdr->cdtextureindex), numSkinRef(pHdr->numskinref), numSkinFamilies(pHdr->numskinfamilies), skinOffset(pHdr->skinindex),

	// misc
	surfacePropOffset(pHdr->surfacepropindex), keyValueOffset(pHdr->keyvalueindex), keyValueSize(pHdr->keyvaluesize), constdirectionallightdot(0), rootLOD(0), numAllowedRootLODs(0),
	fadeDistance(pHdr->fadeDistance), gatherSize(pHdr->gatherSize), illumpositionattachmentindex(pHdr->illumpositionattachmentindex), flMaxEyeDeflection(0.0f),

	// file
	vtxOffset(FIX_FILE_OFFSET(pHdr->vtxOffset)), vvdOffset(FIX_FILE_OFFSET(pHdr->vvdOffset)), vvcOffset(FIX_FILE_OFFSET(pHdr->vvcOffset)), vvwOffset(FIX_FILE_OFFSET(pHdr->vvwOffset)), phyOffset(FIX_FILE_OFFSET(pHdr->phyOffset)),
	vtxSize(pHdr->vtxSize), vvdSize(pHdr->vvdSize), vvcSize(pHdr->vvcSize), vvwSize(pHdr->vvwSize), phySize(pHdr->phySize), hwDataSize(pHdr->hwDataSize),
	groupCount(pHdr->groupHeaderCount), groups(),

	pad(), smallIndices(false)
{
	assertm(pHdr->groupHeaderCount <= 8, "model has more than 8 lods");

	for (int i = 0; i < groupCount; i++)
	{
		if (i == 8)
			break;

		groups[i] = studio_hw_groupdata_t(pHdr->pLODGroup(i));
	}
};
#undef FIX_FILE_OFFSET

studiohdr_generic_t::studiohdr_generic_t(const r5::studiohdr_v16_t* const pHdr, int dataSizePhys, int dataSizeModel) : baseptr(reinterpret_cast<const char*>(pHdr)), szNameOffset(pHdr->sznameindex), length(dataSizeModel), flags(pHdr->flags), mass(pHdr->mass), contents(pHdr->contents),
	eyeposition(0.0f), illumposition(pHdr->illumposition), hull_min(pHdr->hull_min), hull_max(pHdr->hull_max), view_bbmin(pHdr->view_bbmin), view_bbmax(pHdr->view_bbmax),

	// bone
	boneCount(pHdr->boneCount), boneOffset(FIX_OFFSET(pHdr->boneHdrOffset)), boneDataOffset(FIX_OFFSET(pHdr->boneDataOffset)), hitboxSetCount(pHdr->numhitboxsets), hitboxSetOffset(FIX_OFFSET(pHdr->hitboxsetindex)), localAttachmentCount(pHdr->numlocalattachments), localAttachmentOffset(FIX_OFFSET(pHdr->localattachmentindex)),
	linearBoneOffset(FIX_OFFSET(pHdr->linearboneindex)), srcBoneTransformCount(pHdr->numsrcbonetransform), srcBoneTransformOffset(FIX_OFFSET(pHdr->srcbonetransformindex)), boneFollowerCount(pHdr->boneFollowerCount), boneFollowerOffset(FIX_OFFSET(pHdr->boneFollowerOffset)),
	boneStateOffset(FIX_OFFSET(pHdr->boneStateOffset)), boneStateCount(pHdr->boneStateCount), bvhOffset(FIX_OFFSET(pHdr->bvhOffset)),

	// animations
	includeModelCount(0), includeModelOffset(0), localAnimationCount(0), localAnimationOffset(0), localSequenceCount(pHdr->numlocalseq), localSequenceOffset(FIX_OFFSET(pHdr->localseqindex)),
	ikChainCount(pHdr->numikchains), ikChainOffset(FIX_OFFSET(pHdr->ikchainindex)), localPoseParamCount(pHdr->numlocalposeparameters), localPoseParamOffset(FIX_OFFSET(pHdr->localposeparamindex)), localIkAutoPlayLockCount(0), localIkAutoPlayLockOffset(0),
	localNodeCount(pHdr->numlocalnodes), localNodeNameOffset(FIX_OFFSET(pHdr->localnodenameindex)), localNodeNameType(1),

	// material
	textureCount(pHdr->numtextures), textureOffset(FIX_OFFSET(pHdr->textureindex)), cdTexturesCount(-1), cdTexturesOffset(-1), numSkinRef(pHdr->numskinref), numSkinFamilies(pHdr->numskinfamilies), skinOffset(FIX_OFFSET(pHdr->skinindex)),

	// misc
	surfacePropOffset(FIX_OFFSET(pHdr->surfacepropindex)), keyValueOffset(FIX_OFFSET(pHdr->keyvalueindex)), keyValueSize(-1), constdirectionallightdot(0), rootLOD(0), numAllowedRootLODs(0),
	fadeDistance(pHdr->fadeDistance), gatherSize(pHdr->gatherSize), illumpositionattachmentindex(pHdr->illumpositionattachmentindex), flMaxEyeDeflection(0.0f),

	// file
	vtxOffset(-1), vvdOffset(-1), vvcOffset(-1), vvwOffset(-1), phyOffset(0),
	vtxSize(-1), vvdSize(-1), vvcSize(-1), vvwSize(-1), phySize(dataSizePhys), hwDataSize(0),
	groupCount(pHdr->groupHeaderCount), groups(),

	pad(), smallIndices(true)
{
	assertm(pHdr->groupHeaderCount <= 8, "model has more than 8 lods");

	for (int i = 0; i < groupCount; i++)
	{
		if (i == 8)
			break;

		groups[i] = studio_hw_groupdata_t(pHdr->pLODGroup(static_cast<uint16_t>(i)));
		hwDataSize += groups[i].dataSizeDecompressed;
	}
};

studiohdr_generic_t::studiohdr_generic_t(const r5::studiohdr_v17_t* const pHdr, int dataSizePhys, int dataSizeModel) : baseptr(reinterpret_cast<const char*>(pHdr)), szNameOffset(pHdr->sznameindex), length(dataSizeModel), flags(pHdr->flags), mass(pHdr->mass), contents(pHdr->contents),
	eyeposition(0.0f), illumposition(pHdr->illumposition), hull_min(pHdr->hull_min), hull_max(pHdr->hull_max), view_bbmin(pHdr->view_bbmin), view_bbmax(pHdr->view_bbmax),

	// bone
	boneCount(pHdr->boneCount), boneOffset(FIX_OFFSET(pHdr->boneHdrOffset)), boneDataOffset(FIX_OFFSET(pHdr->boneDataOffset)), hitboxSetCount(pHdr->numhitboxsets), hitboxSetOffset(FIX_OFFSET(pHdr->hitboxsetindex)), localAttachmentCount(pHdr->numlocalattachments), localAttachmentOffset(FIX_OFFSET(pHdr->localattachmentindex)),
	linearBoneOffset(FIX_OFFSET(pHdr->linearboneindex)), srcBoneTransformCount(pHdr->numsrcbonetransform), srcBoneTransformOffset(FIX_OFFSET(pHdr->srcbonetransformindex)), boneFollowerCount(pHdr->boneFollowerCount), boneFollowerOffset(FIX_OFFSET(pHdr->boneFollowerOffset)),
	boneStateOffset(FIX_OFFSET(pHdr->boneStateOffset)), boneStateCount(pHdr->boneStateCount), bvhOffset(FIX_OFFSET(pHdr->bvhOffset)),

	// animations
	includeModelCount(0), includeModelOffset(0), localAnimationCount(0), localAnimationOffset(0), localSequenceCount(pHdr->numlocalseq), localSequenceOffset(FIX_OFFSET(pHdr->localseqindex)),
	ikChainCount(pHdr->numikchains), ikChainOffset(FIX_OFFSET(pHdr->ikchainindex)), localPoseParamCount(pHdr->numlocalposeparameters), localPoseParamOffset(FIX_OFFSET(pHdr->localposeparamindex)), localIkAutoPlayLockCount(0), localIkAutoPlayLockOffset(0),
	localNodeCount(pHdr->numlocalnodes), localNodeNameOffset(FIX_OFFSET(pHdr->localnodenameindex)), localNodeNameType(1),

	// material
	textureCount(pHdr->numtextures), textureOffset(FIX_OFFSET(pHdr->textureindex)), cdTexturesCount(-1), cdTexturesOffset(-1), numSkinRef(pHdr->numskinref), numSkinFamilies(pHdr->numskinfamilies), skinOffset(FIX_OFFSET(pHdr->skinindex)),

	// misc
	surfacePropOffset(FIX_OFFSET(pHdr->surfacepropindex)), keyValueOffset(FIX_OFFSET(pHdr->keyvalueindex)), keyValueSize(-1), constdirectionallightdot(0), rootLOD(0), numAllowedRootLODs(0),
	fadeDistance(pHdr->fadeDistance), gatherSize(pHdr->gatherSize), illumpositionattachmentindex(pHdr->illumpositionattachmentindex), flMaxEyeDeflection(0.0f),

	// file
	vtxOffset(-1), vvdOffset(-1), vvcOffset(-1), vvwOffset(-1), phyOffset(0),
	vtxSize(-1), vvdSize(-1), vvcSize(-1), vvwSize(-1), phySize(dataSizePhys), hwDataSize(0),
	groupCount(pHdr->groupHeaderCount), groups(),

	pad(), smallIndices(true)
{
	assertm(pHdr->groupHeaderCount <= 8, "model has more than 8 lods");

	for (int i = 0; i < groupCount; i++)
	{
		if (i == 8)
			break;

		groups[i] = studio_hw_groupdata_t(pHdr->pLODGroup(static_cast<uint16_t>(i)));
		hwDataSize += groups[i].dataSizeDecompressed;
	}
};