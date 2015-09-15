cbuffer cbPerObject
{
	float4x4 WVP;
	float4x4 World;
};

Texture2D ObjTexture;
SamplerState ObjSamplerState;
TextureCube SkyMap;

struct SKYMAP_VS_OUTPUT	//output structure for skymap vertex shader
{
	float4 Pos : SV_POSITION;
	float3 texCoord : TEXCOORD0;
	float4 WorldPosition : TEXCOORD1;
};

struct SKYMAP_PS_OUTPUT
{
#if DEPTH_ONLY==1
	float4 depth : SV_Target0;
#else
	float4 position : SV_Target0;
	float4 color : SV_Target1;
	float4 normal : SV_Target2;
	float4 material : SV_Target3;
#endif
};

SKYMAP_VS_OUTPUT SKYMAP_VS(float3 inPos : POSITION, float2 inTexCoord : TEXCOORD, float3 normal : NORMAL)
{
	SKYMAP_VS_OUTPUT output = (SKYMAP_VS_OUTPUT)0;

	//Set Pos to xyww instead of xyzw, so that z will always be 1 (furthest from camera)
	output.Pos = mul(float4(inPos, 1.0f), WVP).xyww;
	output.WorldPosition = mul(float4(inPos, 1.0f), World);
	output.texCoord = inPos;

	return output;
}

SKYMAP_PS_OUTPUT SKYMAP_PS(SKYMAP_VS_OUTPUT input) : SV_Target
{
	SKYMAP_PS_OUTPUT output;

#if POSITION_ONLY==1
	float depthValue = vWVPPosition.z / vWVPPosition.w;
	//output.depth = float4(depthValue, depthValue, depthValue, 1.0f);
	output.depth = float4(100, 150, 200, 1.0f);
#else
	output.color = SkyMap.Sample(ObjSamplerState, input.texCoord);
	output.normal = float4(0.0f, 1.0f, 1.0f, 1.0f);
	output.position = input.WorldPosition;
	output.material = float4(1.0f, 1.0f, 1.0f, 1.0f);
#endif

	return output;
}