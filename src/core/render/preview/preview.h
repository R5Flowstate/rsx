#pragma once

// Vertex Shader: Shader Resources
#define VSRSRC_BONE_MATRIX            60u // g_boneMatrix
#define VSRSRC_BONE_MATRIX_PREV_FRAME 62u // g_boneMatrixPrevFrame

// Pixel Shader: Shader Resources
#define PSRSRC_CUBEMAP                40u // IndirectSpecularTextureCubeArray
#define PSRSRC_CSMDEPTHATLASSAMPLER   43u // CSMAtlasDepthSampler
#define PSRSRC_SHADOWMAP              44u // t_shadowMaps
#define PSRSRC_CLOUDMASK              45u // cloudMaskTexture
#define PSRSRC_STATICSHADOWTEXTURE    46u // staticShadowTexture
#define PSRSRC_GLOBAL_LIGHTS          62u // s_globalLights
#define PSRSRC_CUBEMAP_SAMPLES        72u // g_cubemapSamples


void Preview_MapTransformsBuffer(CDXDrawData* drawData);
void Preview_MapModelInstanceBuffer(CDXDrawData* drawData);
