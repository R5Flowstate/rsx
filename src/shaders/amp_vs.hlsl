struct VS_Input
{
    float3 position : POSITION;
    uint normal : NORMAL;
    uint color : COLOR;
    float2 uv : TEXCOORD;    
};
struct VS_Output
{
    float4 v0 : TEXCOORD0;
    float3 v1 : TEXCOORD4;
    float3 v2 : NORMAL;
    float3 v3 : TANGENT;
    float3 v4 : BINORMAL;
    float4 v5 : TEXCOORD6;
    float4 v6 : SV_Position0;
};

cbuffer CBufCommonPerCamera : register(b2)
{
  float c_gameTime : packoffset(c0);
  float3 c_cameraOrigin : packoffset(c0.y);
  row_major float4x4 c_cameraRelativeToClip : packoffset(c1);
  int c_frameNum : packoffset(c5);
  float3 c_cameraOriginPrevFrame : packoffset(c5.y);
  row_major float4x4 c_cameraRelativeToClipPrevFrame : packoffset(c6);
  float4 c_clipPlane : packoffset(c10);

  struct
  {
    float4 k0;
    float4 k1;
    float4 k2;
    float4 k3;
    float4 k4;
  } c_fogParams : packoffset(c11);

  float3 c_skyColor : packoffset(c16);
  float c_shadowBleedFudge : packoffset(c16.w);
  float c_envMapLightScale : packoffset(c17);
  float3 c_sunColor : packoffset(c17.y);
  float3 c_sunDir : packoffset(c18);
  float c_minShadowVariance : packoffset(c18.w);

  struct
  {
    float3 shadowRelConst;
    bool enableShadows;
    float3 shadowRelForX;
    int cascade3LogicData;
    float3 shadowRelForY;
    float cascadeWeightScale;
    float3 shadowRelForZ;
    float cascadeWeightBias;
    float4 normCoordsScale12;
    float4 normCoordsBias12;
    float2 normToAtlasCoordsScale0;
    float2 normToAtlasCoordsBias0;
    float4 normToAtlasCoordsScale12;
    float4 normToAtlasCoordsBias12;
    float4 normCoordsScaleBias3;
    float4 shadowRelToZ3;
    float2 normToAtlasCoordsScale3;
    float2 normToAtlasCoordsBias3;
    float3 cascade3LightDir;
    float worldUnitSizeInDepthStatic;
    float3 cascade3LightX;
    float cascade3CotFovX;
    float3 cascade3LightY;
    float cascade3CotFovY;
    float2 cascade3ZNearFar;
    float2 unused_2;
    float4 normCoordsScaleBiasStatic;
    float4 shadowRelToZStatic;
  } c_csm : packoffset(c19);

  uint c_lightTilesX : packoffset(c37);
  float c_maxLightingValue : packoffset(c37.y);
  uint c_debugShaderFlags : packoffset(c37.z);
  uint c_branchFlags : packoffset(c37.w);
  float2 c_renderTargetSize : packoffset(c38);
  float2 c_rcpRenderTargetSize : packoffset(c38.z);
  float2 c_cloudRelConst : packoffset(c39);
  float2 c_cloudRelForX : packoffset(c39.z);
  float2 c_cloudRelForY : packoffset(c40);
  float2 c_cloudRelForZ : packoffset(c40.z);
  float c_sunHighlightSize : packoffset(c41);
  float c_forceExposure : packoffset(c41.y);
  float c_zNear : packoffset(c41.z);
  float c_gameTimeLastFrame : packoffset(c41.w);
  float2 c_viewportMinMaxZ : packoffset(c42);
  float2 c_viewportBias : packoffset(c42.z);
  float2 c_viewportScale : packoffset(c43);
  float2 c_rcpViewportScale : packoffset(c43.z);
  float2 c_framebufferViewportScale : packoffset(c44);
  float2 c_rcpFramebufferViewportScale : packoffset(c44.z);
  int c_separateIndirectDiffuse : packoffset(c45);
  float c_cameraLenY : packoffset(c45.y);
  float c_cameraRcpLenY : packoffset(c45.z);
  int c_debugInt : packoffset(c45.w);
  float c_debugFloat : packoffset(c46);
  float3 unused : packoffset(c46.y);
}

cbuffer CBufModelInstance : register(b3)
{
  struct
  {
    row_major float3x4 objectToCameraRelative;
    row_major float3x4 objectToCameraRelativePrevFrame;
    float4 diffuseModulation;
    int cubemapID;
    int lightmapID;
    float2 unused;

    struct
    {
      float4 ambientSH[3];
      float4 skyDirSunVis;
      uint4 packedLightData;
    } lighting;

  } c_modelInst : packoffset(c0);
}

cbuffer VS_TransformConstants : register(b0)
{
    float4x4 modelMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
};

VS_Output vs_main(VS_Input input)
{
    VS_Output output;


    // float4 posToCamRel = mul(c_modelInst.objectToCameraRelative, float4(input.position, 1.0));
    float3 posToCamRelPrevFrame = mul(c_modelInst.objectToCameraRelativePrevFrame, float4(input.position, 1.0));
    
    // output.v6 = mul(c_cameraRelativeToClip, float4(posToCamRel, 1.0));
    output.v5 = mul(c_cameraRelativeToClipPrevFrame, float4(posToCamRelPrevFrame, 1.0));

    float3 pos = float3(-input.position.x, input.position.y, input.position.z);

    output.v6 = mul(float4(pos, 1.f), modelMatrix);
    output.v6 = mul(output.v6, viewMatrix);
    output.v6 = mul(output.v6, projectionMatrix);

    output.v0.xy = input.uv;
    output.v0.zw = 0.0;

    output.v1 = output.v6;//posToCamRel;

    // this shader currently doesn't unpack tangents or binormals so like yea
    output.v3 = float3(0,0,0); // tangent
    output.v4 = float3(0,0,0); // binormal


    // todo: check if normal needs to be modified in any way like the position
    bool sign = (input.normal >> 28) & 1;

    float val1 = sign ? -255.f : 255.f; // normal value 1
    float val2 = ((input.normal >> 19) & 0x1FF) - 256.f;
    float val3 = ((input.normal >> 10) & 0x1FF) - 256.f;

    int idx1 = (input.normal >> 29) & 3;
    int idx2 = (0x124u >> (2 * idx1 + 2)) & 3;
    int idx3 = (0x124u >> (2 * idx1 + 4)) & 3;

	// normalise the normal
    float normalised = rsqrt((255.f * 255.f) + (val2 * val2) + (val3 * val3));

    float3 vals = float3(val1 * normalised, val2 * normalised, val3 * normalised);
    float3 tmp;
    
    // indices are sequential, always:
    // 0 1 2
    // 2 0 1
    // 1 2 0
    
    if (idx1 == 0)
    {
        tmp.x = vals[idx1];
        tmp.y = vals[idx2];
        tmp.z = vals[idx3];
    }
    else if (idx1 == 1)
    {
        tmp.x = vals[idx3];
        tmp.y = vals[idx1];
        tmp.z = vals[idx2];
    }
    else if (idx1 == 2)
    {
        tmp.x = vals[idx2];
        tmp.y = vals[idx3];
        tmp.z = vals[idx1];
    }
    
    output.v2 = normalize(mul((float3x3)c_modelInst.objectToCameraRelative, tmp));

    return output;
}
