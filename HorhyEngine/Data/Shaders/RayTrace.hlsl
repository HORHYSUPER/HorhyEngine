cbuffer cbChangePerCall
{
	uint thread_count;
};

StructuredBuffer<float2>	g_SrcData : register(t0);
RWStructuredBuffer<float3>	g_DstData : register(u0);

[numthreads(1, 1, 1)]
void RayTrace_CS(uint3 thread_id : SV_DispatchThreadID)
{
    g_DstData[thread_id.x] = float3(1.0f, 1.0f, 1.0f);
}