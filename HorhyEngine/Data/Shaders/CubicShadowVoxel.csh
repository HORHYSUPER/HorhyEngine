#include "globals.hlsl"

Texture3D<float4> g_VoxelGridShadedInputTexture: register(t0);
Texture3D<float4> g_VoxelGridNormalTexture: register(t1);
TextureCube	g_texReflectCube: register(t2);

RWTexture3D<float4> g_VoxelGridShadedOutputTexture: register(u0);

SamplerState SampleTypeLinear : register(CUSTOM0_SAM_BP);

GLOBAL_POINT_LIGHT_UB(pointLightUB);

cbuffer CustomUB: register(CUSTOM0_UB_BP)
{
	struct
	{
		float voxelizeSize;
		float voxelSize;
		float voxelizeDimension;
		vector voxelizePosition;
	}customUB;
};

[numthreads(8, 8, 8)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	float4 normal = g_VoxelGridNormalTexture[dispatchThreadID.xyz];
	if (normal.w == 0)
		return;

	float4 outputColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float diffuse;
	float shadowTerm = 0.0f;
	float3 position = dispatchThreadID * customUB.voxelSize + customUB.voxelizePosition.xyz - customUB.voxelizeSize / 2;
	float3 lightDirection = pointLightUB.position - position.xyz;
	float depthValue;
	float lightVecLen = length(lightDirection);
	float bias = 0.02 * tan(acos(dot(normal.xyz, normalize(lightDirection)))) + lightVecLen * 0.002f;

	[unroll]
	for (int i = 0; i < 64; i++)
	{
		depthValue = g_texReflectCube.SampleLevel(SampleTypeLinear, -normalize(lightDirection) + (PoissonDisc[i] * 4 * (1.0f / 1024.0f)), 0).r;
		if (lightVecLen < depthValue + bias)
		{
			shadowTerm += depthValue;
		}
	}

	shadowTerm /= 64;

	diffuse = saturate(dot(normal.xyz, normalize(lightDirection)));
	if (diffuse > 0.0f)
	{
		outputColor += pointLightUB.color * pointLightUB.multiplier * diffuse;
	}

	outputColor = outputColor * saturate(shadowTerm);
	outputColor *= saturate(1.0f - (lightVecLen / pointLightUB.radius));

	g_VoxelGridShadedOutputTexture[dispatchThreadID.xyz] = outputColor + g_VoxelGridShadedInputTexture[dispatchThreadID.xyz];
}