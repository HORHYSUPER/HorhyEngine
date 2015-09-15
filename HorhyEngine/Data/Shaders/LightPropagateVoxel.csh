#include "globals.hlsl"

Texture3D<float4> g_VoxelGridColorInputTexture: register(t0);
Texture3D<float4> g_VoxelGridNormalInputTexture: register(t1);
Texture3D<float4> g_VoxelGridIlluminatedInputTexture: register(t2);

RWTexture3D<float4> g_VoxelGridColorOutputTexture: register(u0);

SamplerState SampleTypeLinear : register(CUSTOM0_SAM_BP);

GLOBAL_CAMERA_UB(cameraUB);

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

float3 voxelTraceCone(float3 origin, float3 dir, float coneRatio)
{
	origin = origin / customUB.voxelizeDimension;
	float4 reflection = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float traceDistance = (1 + 2 * pow(2, 3 * coneRatio)) * 1.0 / customUB.voxelizeDimension; //1.0 / 128.0; // ensure that cone starts outside of current cell
	const float stepSize = 0.5 / customUB.voxelizeDimension;// 1 / (g_voxelizeSize / g_voxelSize);
	float occlusion = 0.0f;
	float maxDist = 1; // Было sqrt(1 + 1 + 1)
	float3 samplePos = 0;

	while (traceDistance <= maxDist && reflection.w < 1.0)
	{
		float sampleDiameter = max(stepSize, coneRatio * traceDistance);// slope * (traceDistance / (g_voxelizeSize / g_voxelSize))); // prevent too small steps
		float level = log2(sampleDiameter * customUB.voxelizeDimension); // calculate mip-map level
		samplePos = (dir * traceDistance) + origin; // не помню что я думал когда делил на 128)) урааа еще немного

		// fade out sampled values with maximum propagation distance to be synchronous with propagated virtual point lights
		//float fade = saturate(traceDistance / maxDist);// pust poka tak saturate(traceDistance / maxDist); // Было пусть пока так = 1
		float4 sampleValue = g_VoxelGridIlluminatedInputTexture.SampleLevel(SampleTypeLinear, samplePos, level)/* * fade*/;
		//sampleValue.w = saturate(sampleValue.w*(1 + 1.5*level));
		float sampleWeight = (1.0f - reflection.a);
		reflection += sampleValue * sampleWeight;

		// accumulate occlusion and stop tracing at full occlusion 
		/*occlusion += colorOcclusion.a;
		if (occlusion >= 1.0f)
		break;

		// Accumulate reflection by weighting with inverse occlusion. Additionally weight by the inverse of fade to prevent
		// blocky occlusion artifacts due to the limited resolution of the reflection voxel-grid (256x256x256).
		float sampleWeight = (1.0f - occlusion) * (1.0f - fade);
		reflection += colorOcclusion.rgb * sampleWeight;

		*/
		// step along reflection cone
		traceDistance += sampleDiameter;// *(g_voxelizeSize / g_voxelSize);
	}

	return reflection.xyz;
}

[numthreads(8, 8, 8)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	float4 color = g_VoxelGridColorInputTexture[dispatchThreadID.xyz];
	if (color.w == 0)
		return;

	float3 position = dispatchThreadID * customUB.voxelSize + customUB.voxelizePosition.xyz - customUB.voxelizeSize / 2;
	float4 normal = g_VoxelGridNormalInputTexture[dispatchThreadID.xyz];
	float3 l = cameraUB.position.xyz - position.xyz;
	float3 offset = position.xyz - customUB.voxelizePosition.xyz;
	float3 voxelPos = offset / customUB.voxelSize + customUB.voxelizeDimension / 2;
	float3 diffuseIllum = voxelTraceCone(voxelPos, normal.xyz, 0.6f);

	float3 tangent;
	float3 binormal;
	float3 c1 = cross(normal.xyz, float3(0.0, 0.0, 1.0));
	float3 c2 = cross(normal.xyz, float3(0.0, 1.0, 0.0));
	if (length(c1) > length(c2))
		tangent = c1;
	else
		tangent = c2;
	tangent = normalize(tangent);
	binormal = normalize(cross(normal.xyz, tangent));
	
	diffuseIllum += 0.707 * voxelTraceCone(voxelPos, normalize(normal.xyz + tangent), 0.8f);
	diffuseIllum += 0.707 * voxelTraceCone(voxelPos, normalize(normal.xyz - tangent), 0.8f);
	diffuseIllum += 0.707 * voxelTraceCone(voxelPos, normalize(normal.xyz + binormal), 0.8f);
	diffuseIllum += 0.707 * voxelTraceCone(voxelPos, normalize(normal.xyz - binormal), 0.8f);

	color.xyz = color.xyz * diffuseIllum + g_VoxelGridIlluminatedInputTexture[dispatchThreadID.xyz].xyz;

	g_VoxelGridColorOutputTexture[dispatchThreadID.xyz] = color;
}