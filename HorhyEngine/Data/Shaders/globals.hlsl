#define CAMERA_UB_BP b0
#define LIGHT_UB_BP b1
#define CUSTOM0_UB_BP b2
#define CUSTOM1_UB_BP b3

#define GLOBAL_CAMERA_UB(x) \
 cbuffer CameraUB: register(CAMERA_UB_BP) \
 { \
   struct \
   { \
     matrix viewMatrix; \
	 matrix projMatrix; \
	 matrix viewProjMatrix; \
     matrix invViewProjMatrix; \
     vector	vScreenResolution; \
     vector position; \
	 float nearClipDistance; \
	 float farClipDistance; \
	 float nearFarClipDistance; \
   }x; \
 } ; 

#define GLOBAL_POINT_LIGHT_UB(x) \
 cbuffer PointLightUB: register(LIGHT_UB_BP) \
 { \
   struct \
   { \
     float3 position; \
     float radius; \
     float4 color; \
     matrix worldMatrix; \
     float multiplier; \
   }x; \
 };

#define GLOBAL_DIR_LIGHT_UB(x) \
 cbuffer DirectionalLightUB: register(LIGHT_UB_BP) \
 { \
   struct \
   { \
	float4 color; \
	float multiplier; \
	matrix          mWorldViewProjection; \
	matrix          mWorld; \
	matrix          mWorldView; \
	matrix          mShadow; \
	float4          vCascadeOffset[8]; \
	float4          vCascadeScale[8]; \
	int             nCascadeLevels; \
	int             iVisualizeCascades; \
	int             iPCFBlurForLoopStart; \
	int             iPCFBlurForLoopEnd; \
	float           fMinBorderPadding; \
	float           fMaxBorderPadding; \
	float           fShadowBiasFromGUI; \
	float           fShadowPartitionSize; \
	float           fCascadeBlendArea; \
	float           fTexelSize; \
	float           fNativeTexelSizeInX; \
	float           fPaddingForCB3; \
	float4          fCascadeFrustumsEyeSpaceDepthsFloat[2]; \
	float4          fCascadeFrustumsEyeSpaceDepthsFloat4[8]; \
	float4          vLightDir; \
   }x; \
 };

 static const float3 PoissonDisc[] = {
	float3(0.352306f, 0.185003f, 0.0537126f),
	float3(0.809412f, 0.569628f, 0.368755f),
	float3(0.444441f, 0.920621f, 0.449995f),
	float3(0.876339f, 0.491775f, 0.863552f),
	float3(0.589679f, 0.900082f, 0.962828f),
	float3(0.0895718f, 0.349895f, 0.669912f),
	float3(0.96411f, 0.073336f, 0.403119f),
	float3(0.859767f, 0.981903f, 0.0107425f),
	float3(0.00271615f, 0.632069f, 0.147557f),
	float3(0.531785f, 0.0393994f, 0.66451f),
	float3(0.0557268f, 0.859249f, 0.929929f),
	float3(0.441511f, 0.281991f, 0.941893f),
	float3(0.447707f, 0.725852f, 0.00875881f),
	float3(0.0358898f, 0.142552f, 0.310709f),
	float3(0.955382f, 0.332713f, 0.0853603f),
	float3(0.963164f, 0.96411f, 0.928465f),
	float3(0.408673f, 0.483535f, 0.488021f),
	float3(0.037843f, 0.961364f, 0.469955f),
	float3(0.937986f, 0.0930509f, 0.804559f),
	float3(0.13892f, 0.00473037f, 0.809564f),
	float3(0.918668f, 0.876583f, 0.562914f),
	float3(0.3961f, 0.726463f, 0.677206f),
	float3(0.180425f, 0.949705f, 0.21717f),
	float3(0.709586f, 0.0392468f, 0.985473f),
	float3(0.526383f, 0.10303f, 0.301645f),
	float3(0.648061f, 0.997131f, 0.196844f),
	float3(0.0527665f, 0.565142f, 0.879727f),
	float3(0.774194f, 0.303995f, 0.601459f),
	float3(0.585772f, 0.513993f, 0.184606f),
	float3(0.0457167f, 0.287393f, 0.0573443f),
	float3(0.837275f, 0.0544755f, 0.128819f),
	float3(0.297311f, 0.0355541f, 0.50734f),
	float3(0.278176f, 0.973327f, 0.798883f),
	float3(0.751701f, 0.674581f, 0.670583f),
	float3(0.970244f, 0.740043f, 0.175726f),
	float3(0.998108f, 0.538591f, 0.588366f),
	float3(0.482925f, 0.57329f, 0.975127f),
	float3(0.0684835f, 0.619373f, 0.42909f),
	float3(0.0468154f, 0.283395f, 0.995788f),
	float3(0.279946f, 0.459395f, 0.134709f),
	float3(0.673147f, 0.949034f, 0.682821f),
	float3(0.0985443f, 0.828181f, 0.644368f),
	float3(0.979095f, 0.729942f, 0.397595f),
	float3(0.762932f, 0.0376904f, 0.3361f),
	float3(0.658162f, 0.783959f, 0.39964f),
	float3(0.690542f, 0.229163f, 0.832057f),
	float3(0.979888f, 0.328593f, 0.420911f),
	float3(0.277291f, 0.249123f, 0.757561f),
	float3(0.820276f, 0.0851161f, 0.590258f),
	float3(0.291665f, 0.285684f, 0.359172f),
	float3(0.479415f, 0.0744346f, 0.987945f),
	float3(0.775262f, 0.745628f, 0.0539567f),
	float3(0.283914f, 0.060213f, 0.216681f),
	float3(0.333689f, 0.787835f, 0.954344f),
	float3(0.571612f, 0.297159f, 0.459731f),
	float3(0.535722f, 0.0855739f, 0.013123f),
	float3(0.889248f, 0.938994f, 0.238166f),
	float3(0.724509f, 0.256203f, 0.30311f),
	float3(0.0809351f, 0.0573138f, 0.0911893f),
	float3(0.404309f, 0.717978f, 0.235115f),
	float3(0.979095f, 0.548784f, 0.00540178f),
	float3(0.264718f, 0.402661f, 0.962523f),
	float3(0.989898f, 0.362011f, 0.720786f),
	float3(0.495102f, 0.285226f, 0.664174f),
 };

 float3 DecodeColor(in uint colorMask)
 {
	 float3 color;
	 color.r = (colorMask >> 16u) & 0x000000ff;
	 color.g = (colorMask >> 8u) & 0x000000ff;
	 color.b = colorMask & 0x000000ff;
	 color /= 255.0f;
	 return color;
 }

 float3 DecodeNormal(in uint normalMask)
 {
	 int3 iNormal;
	 iNormal.x = (normalMask >> 18) & 0x000000ff;
	 iNormal.y = (normalMask >> 9) & 0x000000ff;
	 iNormal.z = normalMask & 0x000000ff;
	 int3 iNormalSigns;
	 iNormalSigns.x = (normalMask >> 25) & 0x00000002;
	 iNormalSigns.y = (normalMask >> 16) & 0x00000002;
	 iNormalSigns.z = (normalMask >> 7) & 0x00000002;
	 iNormalSigns = 1 - iNormalSigns;
	 float3 normal = float3(iNormal) / 255.0f;
	 normal *= iNormalSigns;
	 return normal;
 }

#define COLOR_TEX_BP t0
#define NORMAL_TEX_BP t1
#define SPECULAR_TEX_BP t2
#define CUSTOM0_TEX_BP t3
#define CUSTOM1_TEX_BP t4
#define CUSTOM2_TEX_BP t5
#define CUSTOM3_TEX_BP t6
#define CUSTOM4_TEX_BP t7
#define CUSTOM5_TEX_BP t8
#define CUSTOM6_TEX_BP t9

#define CUSTOM0_SB_BP t10
#define CUSTOM1_SB_BP t11

#define COLOR_SAM_BP s0
#define NORMAL_SAM_BP s1
#define SPECULAR_SAM_BP s2
#define CUSTOM0_SAM_BP s3
#define CUSTOM1_SAM_BP s4
#define CUSTOM2_SAM_BP s5
#define CUSTOM3_SAM_BP s6
#define CUSTOM4_SAM_BP s7
#define CUSTOM5_SAM_BP s8
#define CUSTOM6_SAM_BP s9