Texture2D shaderTexture;
SamplerState SampleType;

cbuffer PixelBuffer
{
    float4 pixelColor;
};

cbuffer PerFrameBuffer
{
	float4x4 WVP;
};

struct VertexInputType
{
	float3 pos : POSITION;
	float2 tex : TEXCOORD;
};

struct PixelInputType
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD;
};

PixelInputType VS(VertexInputType input)
{
	PixelInputType output;

	output.pos = mul(float4(input.pos, 1.0f), WVP);
	output.tex = input.tex;

	return output;
}

float4 PS(PixelInputType input) : SV_TARGET
{
	return shaderTexture.Sample(SampleType, input.tex) * pixelColor;
}
