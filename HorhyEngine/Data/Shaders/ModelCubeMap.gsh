#include "globals.hlsl"
#include "AdaptiveModel.hlsl"

#define MAX_BONE_MATRICES 64

GLOBAL_CAMERA_UB(cameraUB);

cbuffer cbPerCubeRender : register(CUSTOM1_UB_BP)
{
	matrix g_mViewCM[6];
};

struct VS_OUTPUT_GS_INPUT
{
	float4 vPosition    : SV_POSITION;
	float4 vWorldPos	: POSITION;
	float3 vNormal		: NORMAL;
	float3 vBinormal	: BINORMAL;
	float3 vTangent		: TANGENT;
	float2 texCoord		: TEXCOORD0;
	uint RTIndex : RTARRAYINDEX;
};

struct PS_INPUT_INSTCUBEMAP
{
	float4 vPosition        : SV_POSITION;
	float4 vWorldPos		: POSITION;
	float3 vNormal			: NORMAL;
	float3 vBinormal		: BINORMAL;
	float3 vTangent			: TANGENT;
	float2 texCoord			: TEXCOORD0;
	uint RTIndex : SV_RenderTargetArrayIndex;
};

[maxvertexcount(3)]
void main(triangle VS_OUTPUT_GS_INPUT input[3], inout TriangleStream<PS_INPUT_INSTCUBEMAP> CubeMapStream)
{
	PS_INPUT_INSTCUBEMAP output;
	for (int v = 0; v < 3; v++)
	{
		output.vPosition = mul(input[v].vWorldPos, g_mViewCM[input[v].RTIndex]);
		output.vPosition = mul(output.vPosition, cameraUB.projMatrix);
		output.vWorldPos = input[v].vWorldPos;
		output.vNormal = input[v].vNormal;
		output.vBinormal = input[v].vBinormal;
		output.vTangent = input[v].vTangent;
		output.texCoord = input[v].texCoord;
		output.RTIndex = input[v].RTIndex;
		CubeMapStream.Append(output);
	}
}