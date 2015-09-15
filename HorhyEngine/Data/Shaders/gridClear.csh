RWTexture3D<float4> gridColorBuffer: register(u0);
RWTexture3D<float4> gridNormalBuffer: register(u1);
RWTexture3D<float4> gridColorIlluminatedBuffer1: register(u2);
RWTexture3D<float4> gridColorIlluminatedBuffer2: register(u3);

[numthreads(8, 8, 8)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	gridColorBuffer[dispatchThreadID.xyz] = 0;
	gridNormalBuffer[dispatchThreadID.xyz] = 0;
	gridColorIlluminatedBuffer1[dispatchThreadID.xyz] = 0;
	gridColorIlluminatedBuffer2[dispatchThreadID.xyz] = 0;
}