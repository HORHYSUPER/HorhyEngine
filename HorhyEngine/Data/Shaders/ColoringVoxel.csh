#include "globals.hlsl"

Texture3D<float4> g_VoxelGridShadedInputTexture: register(t0);
Texture3D<float4> g_VoxelGridColorInputTexture: register(t1);

RWTexture3D<float4> g_VoxelGridColorOutputTexture: register(u0);

[numthreads(8, 8, 8)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	float4 voxelColor = g_VoxelGridColorInputTexture[dispatchThreadID.xyz];
	if (voxelColor.w == 0)
		return;

	g_VoxelGridColorOutputTexture[dispatchThreadID.xyz] = voxelColor * g_VoxelGridShadedInputTexture[dispatchThreadID.xyz];
}