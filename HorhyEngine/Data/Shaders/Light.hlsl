#include "globals.hlsl"

#ifndef VOXEL_GLOBAL_ILLUMINATION
#define VOXEL_GLOBAL_ILLUMINATION 0
#endif

#ifndef MSAA
#define MSAA 1
#endif

#if MSAA > 1
Texture2DMS<float4>	positionTexture		: register(t0);
Texture2DMS<float4>	colorTexture		: register(t1);
Texture2DMS<float4>	normalTexture		: register(t2);
#else
Texture2D<float4>	positionTexture		: register(t0);
Texture2D<float4>	colorTexture		: register(t1);
Texture2D<float4>	normalTexture		: register(t2);
#endif
Texture2D<float4>	postProcess			: register(t4);
#if VOXEL_GLOBAL_ILLUMINATION
Texture3D<float4>	g_VoxelGridColor	: register(t5);
#endif
TextureCube<float4>	skyMap				: register(t6);

SamplerState SampleTypePoint : register(s0);
SamplerState SampleTypeLinear : register(CUSTOM0_SAM_BP);
SamplerComparisonState SamplerTypeComparisionPoint : register(s2);

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

cbuffer CustomUB: register(CUSTOM1_UB_BP)
{
	struct
	{
		float coneSlope;
	}paramsCustomUB;
};


cbuffer MatrixBuffer : register(b0)
{
	matrix g_mProjection;
	matrix camVP;
	matrix lightVP;
	vector g_vEye;
	//vector g_vAmbientColor;
};

struct VertexInputType
{
	float3 position : POSITION;
	float2 tex : TEXCOORD0;
};

struct PixelInputType
{
	float4 position : SV_POSITION;
	float2 tex : TEXCOORD0;
};

////////////////////////////////////////////////////////////////////////////////
// Vertex Shader
////////////////////////////////////////////////////////////////////////////////
PixelInputType VS(VertexInputType input)
{
	PixelInputType output;

	// Change the position vector to be 4 units for proper matrix calculations.
	//input.position.w = 1.0f;

	// Calculate the position of the vertex against the world, view, and projection matrices.
	output.position = mul(float4(input.position, 1.0f), g_mProjection);

	// Store the texture coordinates for the pixel shader.
	output.tex = input.tex;

	return output;
}

#if VOXEL_GLOBAL_ILLUMINATION
float4 voxelTraceCone(float3 origin, float3 dir, float coneRatio, in float startTraceDist)
{
	origin = origin / customUB.voxelizeDimension;
	float4 reflection = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float traceDistance = startTraceDist + (1 + 2 * pow(2, 3 * coneRatio)) * 1.0 / customUB.voxelizeDimension; //1.0 / 128.0; // ensure that cone starts outside of current cell
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
		float4 sampleValue = g_VoxelGridColor.SampleLevel(SampleTypeLinear, samplePos, level)/* * fade*/;
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

	return reflection;
}
#endif

float4 PS(PixelInputType input) : SV_TARGET
{
	float4 outputColor;
	float4 position = 0.0f;
	float4 colors = 0.0f;
	float4 normal = 0.0f;
	float4 diffuseLight = 0.0f;
	float material;
	float4 diffuseIllum = 0.0f;
	float4 reflection = 0.0f;
	float4 g_vAmbientColor = float4(.5, .5, .5, 1.0f);
	int2 screenCoords = int2(input.position.xy);

#if MSAA > 1
	for (int sampleIndex = 0; sampleIndex < MSAA; sampleIndex++)
	{
		position += positionTexture.Load(screenCoords, sampleIndex);
		colors += colorTexture.Load(screenCoords, sampleIndex);
		normal += normalTexture.Load(screenCoords, sampleIndex);
	}
	position /= MSAA;
	colors /= MSAA;
	normal /= MSAA;
#else
	position = positionTexture.Sample(SampleTypePoint, input.tex);
	colors = colorTexture.Sample(SampleTypePoint, input.tex);
	normal = normalTexture.Sample(SampleTypePoint, input.tex);
#endif
	diffuseLight = postProcess.Sample(SampleTypePoint, input.tex);
	material = position.w;

	//float4 projectTexCoord;
	//projectTexCoord.x = projectTexCoord.x / (abs(projectTexCoord.x / projectTexCoord.w) + 1.0f) / 2;
	//projectTexCoord.y = projectTexCoord.y / (abs(projectTexCoord.y / projectTexCoord.w) + 1.0f) / 2;
	//projectTexCoord.x *= 0.5f;
	//projectTexCoord.y = -projectTexCoord.y*0.5f;

	float3 l = cameraUB.position.xyz - position.xyz;
	//float3 reflect_vec = reflect(normal, -normalize(l));
	//lightDirection = lightPosition.xyz - position.xyz;

	/*float bias = 0.000001f * tan(acos(dot(normal, normalize(lightDirection))));
	bias = clamp(bias, 0.00001f, 0.01f);
	float depthValue = g_texReflectCube.Sample(SampleTypePoint, -lightDirection).r;
	lightDepthValue = length(lightDirection);
	bias = 0.0f;
	lightDepthValue = lightDepthValue - bias;*/

	/*diff = saturate(dot(normal, normalize(lightDirection)));
	spec = float4(0.9f, 0.5f, 0.3f, 1.0f) * pow(saturate(dot(reflect_vec, normalize(lightDirection))), 60.0f);
	if (diff > 0.0f)
	{
		color += float4(1.0f, 1.0f, 1.0f, 1.0f) * diff + spec;
	}

	diffuseLight = saturate(saturate(color * float4(postProcess.Sample(SampleTypeLinear, input.tex).xyz, 1.0f)))+ ramp * 0.8f;*/
	//spec = float4(1.0f, 1.0f, 0.6f, 1.0f) * pow(saturate(dot(reflect_vec, normalize(lightDirection))), 60.0f);
	//outputColor = saturate(saturate(color) + spec * diff);
	//outputColor *= postProcess.Sample(SampleTypePoint, input.tex);

	if (material == 0.2f)
	{
		//outputColor = colors * g_vAmbientColor;
		//float4 ramp = float4(g_texReflectCube.Sample(g_samCube, reflect_vec.xyz).xyz, 1.0f);
		//outputColor += ramp * 0.8f;
	}
	else if (material == 0.3f)
	{
		//outputColor = colors;
	}
	else if (material > 1.5f && material < 2.5f) // Model
	{
		//outputColor += ramp * 0.8f;
		//position = mul(camVP, position);
		/*float zFar = 100000.0f;
		float zNear = 1.0f;
		float4 depth = mul(camVP, position);
		float z = zFar*zNear / ((depth.z / depth.w) * (zFar - zNear) - zFar);
		float att = 0.0;*/

		//Testing photon mapping)
		//for (int i = 0; i < 2; i++)
		//{
		//	if (length(photons[i].xyz - position.xyz) < 150)
		//	{
		//		outputColor = colors * 1.0f;
		//	}
		//}

		//outputColor = float4(colors.xyz * diffuseLight.xyz, colors.w);
		//outputColor = float4(normal.xyz, 1);
#if VOXEL_GLOBAL_ILLUMINATION
		/*float3 offset = (position.xyz)*(0.0160000008f * 8.0f);
		offset = round(offset);
		int3 voxelPos = int3(128, 128, 128) + int3(offset.x, offset.y, offset.z);*/
		//outputColor = float4(DecodeColor(g_VoxelGrid[int3(128, 128, 128) + position.xyz]), 1.0f);
		float3 offset = (position.xyz - customUB.voxelizePosition.xyz);
		//offset = round(offset);
		int3 voxelPos = offset / customUB.voxelSize + customUB.voxelizeDimension / 2;
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

		//if ((voxelPos.x > -1) && (voxelPos.x < customUB.voxelizeDimension) &&
		//	(voxelPos.y > -1) && (voxelPos.y < customUB.voxelizeDimension) &&
		//	(voxelPos.z > -1) && (voxelPos.z < customUB.voxelizeDimension))
		{
			float startTraceDist = (1.0f / clamp(dot(normal, l), 0.001f, 1.0f)) / customUB.voxelizeDimension;
			reflection = 1.0 * voxelTraceCone(voxelPos, reflect(normalize(-l), normal), paramsCustomUB.coneSlope, startTraceDist);
			diffuseIllum = 1.0 * voxelTraceCone(voxelPos, normal, 0.6f, 0);
			diffuseIllum += .707 * voxelTraceCone(voxelPos, normalize(normal + tangent), 0.8f, 0);
			diffuseIllum += .707 * voxelTraceCone(voxelPos, normalize(normal - tangent), 0.8f, 0);
			diffuseIllum += .707 * voxelTraceCone(voxelPos, normalize(normal + binormal), 0.8f, 0);
			diffuseIllum += .707 * voxelTraceCone(voxelPos, normalize(normal - binormal), 0.8f, 0);
			//outputColor = float4(g_VoxelGridColor.Sample(SampleTypePoint, voxelPos / customUB.voxelizeDimension).xyz, 1.0f);
		}
#endif
		float4 skyReflect = skyMap.SampleLevel(SampleTypeLinear, reflect(normalize(-l), normal).xyz, paramsCustomUB.coneSlope * 10);
		float4 skyDiffuse = skyMap.SampleLevel(SampleTypeLinear, normal.xyz, 9);
		outputColor = float4(
			colors.rgb *
			(diffuseLight.rgb + diffuseIllum.rgb + (1.0f - diffuseIllum.a) * (g_vAmbientColor.rgb * .33f + skyDiffuse.rgb)) +
			(reflection.rgb + skyReflect.rgb * (1 - reflection.a)) * colors.a
			, 1.0f);
		//float4 skyReflect = skyMap.SampleLevel(SampleTypeLinear, reflect(normalize(-l), normal).xyz, paramsCustomUB.coneSlope);
		//outputColor = float4(colors.rgb * diffuseLight.rgb + skyReflect.rgb, 1.0f);
		//outputColor = float4(skyDiffuse.rgb, 1.0f);
	}
	else if (material == 1.0f) // Sky
	{
		outputColor = colors * g_vAmbientColor;
	}

	return float4(outputColor.rgb, 1.0f);
}

struct BlurPixelInputType
{
	float4 position : SV_POSITION;
	float2 tex : TEXCOORD0;
	float2 texCoord1 : TEXCOORD1;
	float2 texCoord2 : TEXCOORD2;
	float2 texCoord3 : TEXCOORD3;
	float2 texCoord4 : TEXCOORD4;
	float2 texCoord5 : TEXCOORD5;
	float2 texCoord6 : TEXCOORD6;
	float2 texCoord7 : TEXCOORD7;
	float2 texCoord8 : TEXCOORD8;
	float2 texCoord9 : TEXCOORD9;
};


////////////////////////////////////////////////////////////////////////////////
// Vertex Shader
////////////////////////////////////////////////////////////////////////////////
//BlurPixelInputType HorizontalBlurVertexShader(VertexInputType input)
//{
//	BlurPixelInputType output;
//	float texelSize;
//
//	// Change the position vector to be 4 units for proper matrix calculations.
//	//input.position.w = 1.0f;
//
//	// Calculate the position of the vertex against the world, view, and projection matrices.
//	output.position = mul(float4(input.position, 1.0f), g_mProjection);
//
//	// Store the texture coordinates for the pixel shader.
//	output.tex = input.tex;
//
//	// Determine the floating point size of a texel for a screen with this specific width.
//	texelSize = 1.0f / 1024;
//
//	// Create UV coordinates for the pixel and its four horizontal neighbors on either side.
//	output.texCoord1 = input.tex + float2(texelSize * -4.0f, 0.0f);
//	output.texCoord2 = input.tex + float2(texelSize * -3.0f, 0.0f);
//	output.texCoord3 = input.tex + float2(texelSize * -2.0f, 0.0f);
//	output.texCoord4 = input.tex + float2(texelSize * -1.0f, 0.0f);
//	output.texCoord5 = input.tex + float2(texelSize *  0.0f, 0.0f);
//	output.texCoord6 = input.tex + float2(texelSize *  1.0f, 0.0f);
//	output.texCoord7 = input.tex + float2(texelSize *  2.0f, 0.0f);
//	output.texCoord8 = input.tex + float2(texelSize *  3.0f, 0.0f);
//	output.texCoord9 = input.tex + float2(texelSize *  4.0f, 0.0f);
//
//	return output;
//}

//float4 HorizontalBlurPixelShader(BlurPixelInputType input) : SV_TARGET
//{
//	float weight0, weight1, weight2, weight3, weight4;
//	float normalization;
//	float4 color;
//
//	// Create the weights that each neighbor pixel will contribute to the blur.
//	weight0 = 1.0f;
//	weight1 = 0.9f;
//	weight2 = 0.55f;
//	weight3 = 0.18f;
//	weight4 = 0.1f;
//
//	// Create a normalized value to average the weights out a bit.
//	normalization = (weight0 + 2.0f * (weight1 + weight2 + weight3 + weight4));
//
//	// Normalize the weights.
//	weight0 = weight0 / normalization;
//	weight1 = weight1 / normalization;
//	weight2 = weight2 / normalization;
//	weight3 = weight3 / normalization;
//	weight4 = weight4 / normalization;
//
//	// Initialize the color to black.
//	color = float4(0.0f, 0.0f, 0.0f, 0.0f);
//
//	// Add the nine horizontal pixels to the color by the specific weight of each.
//	color += postProcess.Sample(SampleTypePoint, input.texCoord1) * weight4;
//	color += postProcess.Sample(SampleTypePoint, input.texCoord2) * weight3;
//	color += postProcess.Sample(SampleTypePoint, input.texCoord3) * weight2;
//	color += postProcess.Sample(SampleTypePoint, input.texCoord4) * weight1;
//	color += postProcess.Sample(SampleTypePoint, input.texCoord5) * weight0;
//	color += postProcess.Sample(SampleTypePoint, input.texCoord6) * weight1;
//	color += postProcess.Sample(SampleTypePoint, input.texCoord7) * weight2;
//	color += postProcess.Sample(SampleTypePoint, input.texCoord8) * weight3;
//	color += postProcess.Sample(SampleTypePoint, input.texCoord9) * weight4;
//
//	// Set the alpha channel to one.
//	color.a = 1.0f;
//
//	return color;
//}

//BlurPixelInputType VerticalBlurVertexShader(VertexInputType input)
//{
//	BlurPixelInputType output;
//	float texelSize;
//
//
//	// Change the position vector to be 4 units for proper matrix calculations.
//	//input.position.w = 1.0f;
//
//	// Calculate the position of the vertex against the world, view, and projection matrices.
//	output.position = mul(float4(input.position, 1.0f), g_mProjection);
//
//	// Store the texture coordinates for the pixel shader.
//	output.tex = input.tex;
//
//	// Determine the floating point size of a texel for a screen with this specific height.
//	texelSize = 1.0f / 1024;
//
//	// Create UV coordinates for the pixel and its four vertical neighbors on either side.
//	output.texCoord1 = input.tex + float2(0.0f, texelSize * -4.0f);
//	output.texCoord2 = input.tex + float2(0.0f, texelSize * -3.0f);
//	output.texCoord3 = input.tex + float2(0.0f, texelSize * -2.0f);
//	output.texCoord4 = input.tex + float2(0.0f, texelSize * -1.0f);
//	output.texCoord5 = input.tex + float2(0.0f, texelSize *  0.0f);
//	output.texCoord6 = input.tex + float2(0.0f, texelSize *  1.0f);
//	output.texCoord7 = input.tex + float2(0.0f, texelSize *  2.0f);
//	output.texCoord8 = input.tex + float2(0.0f, texelSize *  3.0f);
//	output.texCoord9 = input.tex + float2(0.0f, texelSize *  4.0f);
//
//	return output;
//}

//float4 VerticalBlurPixelShader(BlurPixelInputType input) : SV_TARGET
//{
//	float weight0, weight1, weight2, weight3, weight4;
//	float normalization;
//	float4 color;
//
//
//	// Create the weights that each neighbor pixel will contribute to the blur.
//	weight0 = 1.0f;
//	weight1 = 0.9f;
//	weight2 = 0.55f;
//	weight3 = 0.18f;
//	weight4 = 0.1f;
//
//	// Create a normalized value to average the weights out a bit.
//	normalization = (weight0 + 2.0f * (weight1 + weight2 + weight3 + weight4));
//
//	// Normalize the weights.
//	weight0 = weight0 / normalization;
//	weight1 = weight1 / normalization;
//	weight2 = weight2 / normalization;
//	weight3 = weight3 / normalization;
//	weight4 = weight4 / normalization;
//
//	// Initialize the color to black.
//	color = float4(0.0f, 0.0f, 0.0f, 0.0f);
//
//	// Add the nine vertical pixels to the color by the specific weight of each.
//	color += postProcess.Sample(SampleTypePoint, input.texCoord1) * weight4;
//	color += postProcess.Sample(SampleTypePoint, input.texCoord2) * weight3;
//	color += postProcess.Sample(SampleTypePoint, input.texCoord3) * weight2;
//	color += postProcess.Sample(SampleTypePoint, input.texCoord4) * weight1;
//	color += postProcess.Sample(SampleTypePoint, input.texCoord5) * weight0;
//	color += postProcess.Sample(SampleTypePoint, input.texCoord6) * weight1;
//	color += postProcess.Sample(SampleTypePoint, input.texCoord7) * weight2;
//	color += postProcess.Sample(SampleTypePoint, input.texCoord8) * weight3;
//	color += postProcess.Sample(SampleTypePoint, input.texCoord9) * weight4;
//
//	// Set the alpha channel to one.
//	color.a = 1.0f;
//
//	return color;
//}