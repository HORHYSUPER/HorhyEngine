#include "globals.hlsl"

//cbuffer cbPerObject : register(b0)
//{
//	matrix g_mWorld;
//	matrix g_mProjection;
//	matrix g_mWorldViewProjection;
//};

SamplerState ObjSamplerState;
TextureCube SkyMap : register(t0);

GLOBAL_CAMERA_UB(cameraUB);

cbuffer CustomUB: register(CUSTOM0_UB_BP)
{
	struct
	{
		matrix	mWorld;
	}modelCustomUB;
};

struct PS_INPUT	//output structure for skymap vertex shader
{
	float4 Pos : SV_POSITION;
	float3 texCoord : TEXCOORD0;
	float4 WorldPosition : TEXCOORD1;
};

struct SKYMAP_PS_OUTPUT
{
#if DEPTH_ONLY==1
	float depth : SV_TARGET;
#else
	float4 position : SV_TARGET0;
	float4 color : SV_TARGET1;
	float4 normal : SV_TARGET2;
#endif
};

PS_INPUT VS(float3 inPos : POSITION, float2 inTexCoord : TEXCOORD)
{
	PS_INPUT output;

	//Set Pos to xyww instead of xyzw, so that z will always be 1 (furthest from camera)
	float4 vPositionWS = mul(float4(inPos, 1.0f), modelCustomUB.mWorld);
	output.Pos = mul(float4(vPositionWS.xyz, 0.0f), cameraUB.viewProjMatrix).xyww;
#if DEPTH_ONLY==1
	//output.Pos.x = output.Pos.x / (abs(output.Pos.x) + 1.0f) / 2;
	//output.Pos.y = output.Pos.y / (abs(output.Pos.y) + 1.0f) / 2;
#endif
	output.WorldPosition = mul(float4(inPos, 1.0f), modelCustomUB.mWorld);
	output.texCoord = inPos;

	return output;
}

SKYMAP_PS_OUTPUT PS(PS_INPUT input) : SV_Target
{
	SKYMAP_PS_OUTPUT output;

#if DEPTH_ONLY==1
	output.depth = input.Pos.z;
#else
	//output.position = float4(input.WorldPosition.xyz, 1.0f);
	output.color = float4(SkyMap.Sample(ObjSamplerState, input.texCoord).rgb, 1.0f);
	//output.normal = float4(0.0f, 0.0f, 0.0f, 0.0f);
#endif

	return output;
}


//--------------------------------------------------------------------------------------
// Cubemap via Instancing
//--------------------------------------------------------------------------------------
//cbuffer cbPerCubeRender : register(b1)
//{
//	matrix g_mViewCM[6]; // View matrices for cube map rendering
//	vector g_vLightPosition;
//};
//
//struct VS_OUTPUT_GS_INPUT
//{
//	float4 Pos : SV_POSITION;
//	float3 texCoord : TEXCOORD0;
//	float4 WorldPosition : TEXCOORD1;
//	uint RTIndex : RTARRAYINDEX;
//};
//
//struct PS_INPUT_INSTCUBEMAP
//{
//	float4 Pos : SV_POSITION;
//	float3 texCoord : TEXCOORD0;
//	float4 WorldPosition : TEXCOORD1;
//	uint RTIndex : SV_RenderTargetArrayIndex;
//};
//
//struct PS_OUTPUT_INSTCUBEMAP
//{
//#if DEPTH_ONLY==1
//	float depth : SV_TARGET;
//#else
//	float4 color : SV_TARGET;
//#endif
//};
//
//VS_OUTPUT_GS_INPUT VS_Inst(float3 inPos : POSITION, float2 inTexCoord : TEXCOORD, uint CubeSize : SV_InstanceID)
//{
//	VS_OUTPUT_GS_INPUT output;
//
//	//Set Pos to xyww instead of xyzw, so that z will always be 1 (furthest from camera)
//	output.Pos = mul(float4(inPos, 1.0f), g_mWorld);
//	output.Pos = mul(output.Pos, g_mViewCM[CubeSize]);
//	output.Pos = mul(output.Pos, g_mProjection).xyww;
//	output.RTIndex = CubeSize;
//#if DEPTH_ONLY==1
//	//output.Pos.x = output.Pos.x / (abs(output.Pos.x) + 1.0f) / 2;
//	//output.Pos.y = output.Pos.y / (abs(output.Pos.y) + 1.0f) / 2;
//#endif
//	output.WorldPosition = mul(float4(inPos, 1.0f), g_mWorld);
//	output.texCoord = inPos;
//
//	return output;
//}
//
//[maxvertexcount(3)]
//void GS_Inst(triangle VS_OUTPUT_GS_INPUT input[3], inout TriangleStream<PS_INPUT_INSTCUBEMAP> CubeMapStream)
//{
//	PS_INPUT_INSTCUBEMAP output;
//	for (int v = 0; v < 3; v++)
//	{
//		output.Pos = input[v].Pos;
//		output.texCoord = input[v].texCoord;
//		output.WorldPosition = input[v].WorldPosition;
//		output.RTIndex = input[v].RTIndex;
//		CubeMapStream.Append(output);
//	}
//}
//
//PS_OUTPUT_INSTCUBEMAP PS_Inst(PS_INPUT_INSTCUBEMAP input) : SV_Target
//{
//	PS_OUTPUT_INSTCUBEMAP output;
//
//#if DEPTH_ONLY==1
//	output.depth = length(g_vLightPosition.xyz - input.WorldPosition.xyz);
//#else
//	output.color = SkyMap.Sample(ObjSamplerState, input.texCoord);
//#endif
//
//	return output;
//}