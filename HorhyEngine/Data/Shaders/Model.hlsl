#include "globals.hlsl"
#include "AdaptiveModel.hlsl"

#define MAX_BONE_MATRICES 64
//#define BLEND_SHAPE_DEFORMER_COUNT 16

struct Influence
{
	vector position;
	vector normal;
};

Texture2D g_baseTexture : register(t0);
Texture2D g_nmhTexture :  register(t1);
Texture2D g_DensityTexture : register( t2 );
StructuredBuffer<Influence> g_influenceBuffer[BLEND_SHAPE_DEFORMER_COUNT] : register(t3);

SamplerState g_samLinear : register(s0);

GLOBAL_CAMERA_UB(cameraUB);
GLOBAL_POINT_LIGHT_UB(pointLightUB);

cbuffer CustomUB: register(CUSTOM0_UB_BP)
{
	struct
	{
		matrix	mWorld;								// World matrix
		float	material;							// Obfect ID, or anything else ID
		vector	vTessellationFactor;				// Edge, inside, minimum tessellation factor and 1/desired triangle size
		float	vdetailTessellationHeightScale;		// Height scale for detail tessellation of grid surface
		vector	vFrustumPlaneEquation[4];			// View frustum plane equations
		matrix	mConstBone[MAX_BONE_MATRICES];
		vector	vDiffuseColor;
		float	specularLevel;
		float	timeElapsed;
		bool	hasDiffuseTexture;
		bool	hasSpecularTexture;
		bool	hasNormalTexture;
	}modelCustomUB;
};

cbuffer cbPerCubeRender : register(CUSTOM1_UB_BP)
{
	matrix g_mViewCM[6];
};

struct VS_OUTPUT_GS_INPUT
{
	float4 vPosition    : SV_POSITION;
	float4 vWorldPos	: POSITION;
	float3 vNormal		: NORMAL;
	float3 vTangent		: TANGENT;
	float2 texCoord		: TEXCOORD0;
	uint RTIndex : RTARRAYINDEX;
};

struct PS_INPUT_INSTCUBEMAP
{
	float4 vPosition        : SV_POSITION;
	float4 vWorldPos		: POSITION;
	float3 vNormal			: NORMAL;
	float3 vTangent			: TANGENT;
	float2 texCoord			: TEXCOORD0;
	uint RTIndex : SV_RenderTargetArrayIndex;
};

struct PS_OUTPUT_INSTCUBEMAP
{
#if DEPTH_ONLY==1
	float depth : SV_TARGET;
#else
	float4 color : SV_TARGET;
#endif
};

[maxvertexcount(3)]
void GS_CubeMap_Inst(triangle VS_OUTPUT_GS_INPUT input[3], inout TriangleStream<PS_INPUT_INSTCUBEMAP> CubeMapStream)
{
	PS_INPUT_INSTCUBEMAP output;
	for (int v = 0; v < 3; v++)
	{
		output.vPosition = mul(input[v].vWorldPos, g_mViewCM[input[v].RTIndex]);
		output.vPosition = mul(output.vPosition, cameraUB.projMatrix);
		output.vWorldPos = input[v].vWorldPos;
		output.vNormal = input[v].vNormal;
		output.vTangent = input[v].vTangent;
		output.texCoord = input[v].texCoord;
		output.RTIndex = input[v].RTIndex;
		CubeMapStream.Append(output);
	}
}

PS_OUTPUT_INSTCUBEMAP PS_CubeMap_Inst(PS_INPUT_INSTCUBEMAP i) : SV_TARGET
{
	PS_OUTPUT_INSTCUBEMAP output;

#if DEPTH_ONLY==1
	output.depth = length(pointLightUB.position.xyz - i.vWorldPos.xyz);
#else
	output.color = g_baseTexture.Sample(g_samLinear, i.texCoord);
#endif

	return output;
}