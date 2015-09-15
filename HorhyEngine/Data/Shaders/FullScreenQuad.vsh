struct VS_OUTPUT
{
	float4 vPosition    : SV_POSITION;
	float2 texCoord		: TEXCOORD0;
};

VS_OUTPUT main(uint uVID : SV_VertexID, uint instanceID : SV_InstanceID)
{
	VS_OUTPUT Out;

	float2 texcoord = float2(uVID & 1, uVID >> 1);

	Out.vPosition = float4((texcoord.x - 0.5f) * 2, -(texcoord.y - 0.5f) * 2, 0, 1);
	Out.texCoord = texcoord;

	return Out;
}