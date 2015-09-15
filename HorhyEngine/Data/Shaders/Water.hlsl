// Copyright (c) 2011 NVIDIA Corporation. All rights reserved.
//
// TO  THE MAXIMUM  EXTENT PERMITTED  BY APPLICABLE  LAW, THIS SOFTWARE  IS PROVIDED
// *AS IS*  AND NVIDIA AND  ITS SUPPLIERS DISCLAIM  ALL WARRANTIES,  EITHER  EXPRESS
// OR IMPLIED, INCLUDING, BUT NOT LIMITED  TO, NONINFRINGEMENT,IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL  NVIDIA 
// OR ITS SUPPLIERS BE  LIABLE  FOR  ANY  DIRECT, SPECIAL,  INCIDENTAL,  INDIRECT,  OR  
// CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION,  DAMAGES FOR LOSS 
// OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR ANY 
// OTHER PECUNIARY LOSS) ARISING OUT OF THE  USE OF OR INABILITY  TO USE THIS SOFTWARE, 
// EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
// Please direct any bugs or questions to SDKFeedback@nvidia.com

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

#define PATCH_BLEND_BEGIN		800
#define PATCH_BLEND_END			20000

//-----------------------------------------------------------------------------------
// Texture & Samplers
//-----------------------------------------------------------------------------------
Texture2D	g_texDisplacement	: register(t0); // FFT wave displacement map in VS
Texture2D	g_texPerlin			: register(t1); // FFT wave gradient map in PS
Texture2D	g_texGradient		: register(t2); // Perlin wave displacement & gradient map in both VS & PS
Texture1D	g_texFresnel		: register(t3); // Fresnel factor lookup table
TextureCube	g_texReflectCube	: register(t4); // A small skybox cube texture for reflection

// FFT wave displacement map in VS, XY for choppy field, Z for height field
SamplerState g_samplerDisplacement	: register(s0);

// Perlin noise for composing distant waves, W for height field, XY for gradient
SamplerState g_samplerPerlin	: register(s1);

// FFT wave gradient map, converted to normal value in PS
SamplerState g_samplerGradient	: register(s2);

// Fresnel factor lookup table
SamplerState g_samplerFresnel	: register(s3);

// A small sky cubemap for reflection
SamplerState g_samplerCube		: register(s4);


// Shading parameters
cbuffer cbShading : register(b2)
{
	// Water-reflected sky color
	vector		g_SkyColor;
	// The color of bottomless water body
	vector		g_WaterbodyColor;

	// The strength, direction and color of sun streak
	float		g_Shineness;
	vector		g_SunDir;
	vector		g_SunColor;
	
	// The parameter is used for fixing an artifact
	vector		g_BendParam;

	// Perlin noise for distant wave crest
	float		g_PerlinSize;
	vector		g_PerlinAmplitude;
	vector		g_PerlinOctave;
	vector		g_PerlinGradient;

	// Constants for calculating texcoord from position
	float		g_TexelLength_x2;
	float		g_UVScale;
	float		g_UVOffset;
};

// Per draw call constants
cbuffer cbChangePerCall : register(b4)
{
	// Transform matrices
	matrix	g_mWorld;
	matrix	g_mView;
	matrix	g_mProjection;
	matrix	g_mWorldViewProjection;
	matrix	g_mLocal;

	// Misc per draw call constants
	float4		g_UVBase;
	float4		g_PerlinMovement;
	float4		g_LocalEye[6];
};

struct VS_OUTPUT
{
	float4 vPosition		: SV_POSITION;
	float4 vWorldPosition	: POSITION;
	float2 texCoord			: TEXCOORD0;
	float3 vLocalPosition	: TEXCOORD1;
};

struct PS_OUTPUT
{
#if DEPTH_ONLY==1
	float depth : SV_TARGET0;
#else
	float4 position : SV_TARGET0;
	float4 color : SV_TARGET1;
	float4 normal : SV_TARGET2;
#endif
};

VS_OUTPUT VS(float2 vPos : POSITION)
{
	VS_OUTPUT Output;

	// Local position
	float4 pos_local = mul(float4(vPos, 0, 1), g_mLocal);
	// UV
	float2 uv_local = pos_local.xy * g_UVScale + g_UVOffset;

	// Blend displacement to avoid tiling artifact
	float3 eye_vec = pos_local.xyz - g_LocalEye[0];
	float dist_2d = length(eye_vec.xy);
	float blend_factor = (PATCH_BLEND_END - dist_2d) / (PATCH_BLEND_END - PATCH_BLEND_BEGIN);
	blend_factor = clamp(blend_factor, 0, 1);

	// Add perlin noise to distant patches
	float perlin = 0;
	if (blend_factor < 1)
	{
		float2 perlin_tc = uv_local * g_PerlinSize + g_UVBase;
		float perlin_0 = g_texPerlin.SampleLevel(g_samplerPerlin, perlin_tc * g_PerlinOctave.x + g_PerlinMovement, 0).w;
		float perlin_1 = g_texPerlin.SampleLevel(g_samplerPerlin, perlin_tc * g_PerlinOctave.y + g_PerlinMovement, 0).w;
		float perlin_2 = g_texPerlin.SampleLevel(g_samplerPerlin, perlin_tc * g_PerlinOctave.z + g_PerlinMovement, 0).w;
		
		perlin = perlin_0 * g_PerlinAmplitude.x + perlin_1 * g_PerlinAmplitude.y + perlin_2 * g_PerlinAmplitude.z;
	}

	// Displacement map
	float3 displacement = 0;
	if (blend_factor > 0)
		displacement = g_texDisplacement.SampleLevel(g_samplerDisplacement, uv_local, 0).xyz;
	displacement = lerp(float3(0, 0, perlin), displacement, blend_factor);
	pos_local.xyz += displacement;

	// Transform
	Output.vPosition = mul(float4(pos_local.xyz, 1.0f), g_mWorldViewProjection);
#if DEPTH_ONLY==1
	//Output.Position.x = Output.Position.x / (abs(Output.Position.x) + 1.0f) / 2;
	//Output.Position.y = Output.Position.y / (abs(Output.Position.y) + 1.0f) / 2;
#endif
	Output.vLocalPosition = pos_local.xyz;
	Output.vWorldPosition = mul(float4(pos_local.xyz, 1.0f), g_mWorld);
	// Pass thru texture coordinate
	Output.texCoord = uv_local;

	return Output; 
}


//-----------------------------------------------------------------------------
// Name: OceanSurfPS
// Type: Pixel shader                                      
// Desc: Ocean shading pixel shader. Check SDK document for more details
//-----------------------------------------------------------------------------
PS_OUTPUT PS(VS_OUTPUT In, float4 vWVPPosition : SV_POSITION) : SV_Target
{
	PS_OUTPUT output;

	// Calculate eye vector.
	float3 eye_vec = g_LocalEye[0] - In.vLocalPosition;
	float3 eye_dir = normalize(eye_vec);
	

	// --------------- Blend perlin noise for reducing the tiling artifacts

	// Blend displacement to avoid tiling artifact
	float dist_2d = length(eye_vec.xy);
	float blend_factor = (PATCH_BLEND_END - dist_2d) / (PATCH_BLEND_END - PATCH_BLEND_BEGIN);
	blend_factor = clamp(blend_factor * blend_factor * blend_factor, 0, 1);

	// Compose perlin waves from three octaves
	float2 perlin_tc = In.texCoord * g_PerlinSize + g_UVBase;
	float2 perlin_tc0 = (blend_factor < 1) ? perlin_tc * g_PerlinOctave.x + g_PerlinMovement : 0;
	float2 perlin_tc1 = (blend_factor < 1) ? perlin_tc * g_PerlinOctave.y + g_PerlinMovement : 0;
	float2 perlin_tc2 = (blend_factor < 1) ? perlin_tc * g_PerlinOctave.z + g_PerlinMovement : 0;

	float2 perlin_0 = g_texPerlin.Sample(g_samplerPerlin, perlin_tc0).xy;
	float2 perlin_1 = g_texPerlin.Sample(g_samplerPerlin, perlin_tc1).xy;
	float2 perlin_2 = g_texPerlin.Sample(g_samplerPerlin, perlin_tc2).xy;
	
	float2 perlin = (perlin_0 * g_PerlinGradient.x + perlin_1 * g_PerlinGradient.y + perlin_2 * g_PerlinGradient.z);


	// --------------- Water body color

	// Texcoord mash optimization: Texcoord of FFT wave is not required when blend_factor > 1
	float2 fft_tc = (blend_factor > 0) ? In.texCoord : 0;

	float2 grad = g_texGradient.Sample(g_samplerGradient, fft_tc).xy;
	grad = lerp(perlin, grad, blend_factor);

	// Calculate normal here.
	float3 normal = normalize(float3(grad, g_TexelLength_x2));
	// Reflected ray
	float3 reflect_vec = reflect(-eye_dir, normal);
	// dot(N, V)
	float cos_angle = dot(normal, eye_dir);

	// A coarse way to handle transmitted light
	float3 body_color = g_WaterbodyColor;


	// --------------- Reflected color

	// ramp.x for fresnel term. ramp.y for sky blending
	float4 ramp = g_texFresnel.Sample(g_samplerFresnel, cos_angle).xyzw;
	// A workaround to deal with "indirect reflection vectors" (which are rays requiring multiple
	// reflections to reach the sky).
	if (reflect_vec.z < g_BendParam.x)
		ramp = lerp(ramp, g_BendParam.z, (g_BendParam.x - reflect_vec.z)/(g_BendParam.x - g_BendParam.y));
	reflect_vec.z = max(0, reflect_vec.z);

	float3 reflection = g_texReflectCube.Sample(g_samplerCube, reflect_vec).xyz;
	// Hack bit: making higher contrast
	reflection = reflection * reflection * 2.5f;

	// Blend with predefined sky color
	float3 reflected_color = lerp(g_SkyColor, reflection, ramp.y);

	// Combine waterbody color and reflected color
	float3 water_color = lerp(body_color, reflected_color, ramp.x);


	// --------------- Sun spots

	float cos_spec = clamp(dot(reflect_vec, g_SunDir), 0, 1);
	float sun_spot = pow(cos_spec, g_Shineness);
	water_color += g_SunColor * sun_spot;

	normal = mul(float3(grad, g_TexelLength_x2), (float3x3) g_mWorld);

#if DEPTH_ONLY==1
	output.depth = vWVPPosition.z;
#else
	output.position = In.vWorldPosition;
	output.color = float4(water_color, 1.0f);
	output.normal = float4(normalize(normal), 1.0f);
#endif

	return output;
}


//--------------------------------------------------------------------------------------
// Cubemap via Instancing
//--------------------------------------------------------------------------------------
cbuffer cbPerCubeRender : register(b5)
{
	matrix g_mViewCM[6]; // View matrices for cube map rendering
	vector g_vLightPosition;
};

struct VS_OUTPUT_GS_INPUT
{
	float4 vPosition		: SV_POSITION;
	float4 vWorldPosition	: POSITION;
	float2 texCoord			: TEXCOORD0;
	float3 vLocalPosition	: TEXCOORD1;
	uint RTIndex : RTARRAYINDEX;
};

struct PS_INPUT_INSTCUBEMAP
{
	float4 vPosition        : SV_POSITION;
	float4 vWorldPosition	: POSITION;
	float2 texCoord			: TEXCOORD0;
	float3 vLocalPosition	: TEXCOORD1;
	uint RTIndex : SV_RenderTargetArrayIndex;
};

struct PS_OUTPUT_INSTCUBEMAP
{
#if DEPTH_ONLY==1
	float4 depth : SV_TARGET;
#else
	float4 color : SV_TARGET;
#endif
};


VS_OUTPUT_GS_INPUT VS_CubeMap_Inst(float2 vPos : POSITION, uint CubeSize : SV_InstanceID)
{
	VS_OUTPUT_GS_INPUT Output;

	// Local position
	float4 pos_local = mul(float4(vPos, 0, 1), g_mLocal);
	// UV
	float2 uv_local = pos_local.xy * g_UVScale + g_UVOffset;

	// Blend displacement to avoid tiling artifact
	float3 eye_vec = pos_local.xyz - g_LocalEye[CubeSize];
	float dist_2d = length(eye_vec.xy);
	float blend_factor = (PATCH_BLEND_END - dist_2d) / (PATCH_BLEND_END - PATCH_BLEND_BEGIN);
	blend_factor = clamp(blend_factor, 0, 1);

	// Add perlin noise to distant patches
	float perlin = 0;
	if (blend_factor < 1)
	{
		float2 perlin_tc = uv_local * g_PerlinSize + g_UVBase;
		float perlin_0 = g_texPerlin.SampleLevel(g_samplerPerlin, perlin_tc * g_PerlinOctave.x + g_PerlinMovement, 0).w;
		float perlin_1 = g_texPerlin.SampleLevel(g_samplerPerlin, perlin_tc * g_PerlinOctave.y + g_PerlinMovement, 0).w;
		float perlin_2 = g_texPerlin.SampleLevel(g_samplerPerlin, perlin_tc * g_PerlinOctave.z + g_PerlinMovement, 0).w;

		perlin = perlin_0 * g_PerlinAmplitude.x + perlin_1 * g_PerlinAmplitude.y + perlin_2 * g_PerlinAmplitude.z;
	}

	// Displacement map
	float3 displacement = 0;
	if (blend_factor > 0)
		displacement = g_texDisplacement.SampleLevel(g_samplerDisplacement, uv_local, 0).xyz;
	displacement = lerp(float3(0, 0, perlin), displacement, blend_factor);
	pos_local.xyz += displacement;

	// Transform
	Output.vPosition = mul(float4(pos_local.xyz, 1.0f), g_mWorld);
	Output.vPosition = mul(Output.vPosition, g_mViewCM[CubeSize]);
	Output.vPosition = mul(Output.vPosition, g_mProjection);

	Output.vLocalPosition = pos_local.xyz;
	Output.vWorldPosition = mul(float4(pos_local.xyz, 1.0f), g_mWorld);
	// Pass thru texture coordinate
	Output.texCoord = uv_local;
	Output.RTIndex = CubeSize;

	return Output;
}

[maxvertexcount(3)]
void GS_CubeMap_Inst(triangle VS_OUTPUT_GS_INPUT input[3], inout TriangleStream<PS_INPUT_INSTCUBEMAP> CubeMapStream)
{
	PS_INPUT_INSTCUBEMAP output;
	for (int v = 0; v < 3; v++)
	{
		output.vPosition = input[v].vPosition;
		output.vWorldPosition = input[v].vWorldPosition;
		output.vLocalPosition = input[v].vLocalPosition;
		output.texCoord = input[v].texCoord;
		output.RTIndex = input[v].RTIndex;
		CubeMapStream.Append(output);
	}
}

PS_OUTPUT_INSTCUBEMAP PS_CubeMap_Inst(PS_INPUT_INSTCUBEMAP In) : SV_Target
{
	PS_OUTPUT_INSTCUBEMAP output;

	// Calculate eye vector.
	float3 eye_vec = g_LocalEye[In.RTIndex] - In.vLocalPosition;
	float3 eye_dir = normalize(eye_vec);
	
	// --------------- Blend perlin noise for reducing the tiling artifacts

	// Blend displacement to avoid tiling artifact
	float dist_2d = length(eye_vec.xy);
	float blend_factor = (PATCH_BLEND_END - dist_2d) / (PATCH_BLEND_END - PATCH_BLEND_BEGIN);
	blend_factor = clamp(blend_factor * blend_factor * blend_factor, 0, 1);

	// Compose perlin waves from three octaves
	float2 perlin_tc = In.texCoord * g_PerlinSize + g_UVBase;
	float2 perlin_tc0 = (blend_factor < 1) ? perlin_tc * g_PerlinOctave.x + g_PerlinMovement : 0;
	float2 perlin_tc1 = (blend_factor < 1) ? perlin_tc * g_PerlinOctave.y + g_PerlinMovement : 0;
	float2 perlin_tc2 = (blend_factor < 1) ? perlin_tc * g_PerlinOctave.z + g_PerlinMovement : 0;

	float2 perlin_0 = g_texPerlin.Sample(g_samplerPerlin, perlin_tc0).xy;
	float2 perlin_1 = g_texPerlin.Sample(g_samplerPerlin, perlin_tc1).xy;
	float2 perlin_2 = g_texPerlin.Sample(g_samplerPerlin, perlin_tc2).xy;

	float2 perlin = (perlin_0 * g_PerlinGradient.x + perlin_1 * g_PerlinGradient.y + perlin_2 * g_PerlinGradient.z);


	// --------------- Water body color

	// Texcoord mash optimization: Texcoord of FFT wave is not required when blend_factor > 1
	float2 fft_tc = (blend_factor > 0) ? In.texCoord : 0;

	float2 grad = g_texGradient.Sample(g_samplerGradient, fft_tc).xy;
	grad = lerp(perlin, grad, blend_factor);

	// Calculate normal here.
	float3 normal = normalize(float3(grad, g_TexelLength_x2));
	// Reflected ray
	float3 reflect_vec = reflect(-eye_dir, normal);
	// dot(N, V)
	float cos_angle = dot(normal, eye_dir);

	// A coarse way to handle transmitted light
	float3 body_color = g_WaterbodyColor;

	// --------------- Reflected color

	// ramp.x for fresnel term. ramp.y for sky blending
	float4 ramp = g_texFresnel.Sample(g_samplerFresnel, cos_angle).xyzw;
	// A workaround to deal with "indirect reflection vectors" (which are rays requiring multiple
	// reflections to reach the sky).
	if (reflect_vec.z < g_BendParam.x)
		ramp = lerp(ramp, g_BendParam.z, (g_BendParam.x - reflect_vec.z) / (g_BendParam.x - g_BendParam.y));
	reflect_vec.z = max(0, reflect_vec.z);

	float3 reflection = g_texReflectCube.Sample(g_samplerCube, reflect_vec).xyz;
	// Hack bit: making higher contrast
	reflection = reflection * reflection * 2.5f;

	// Blend with predefined sky color
	float3 reflected_color = lerp(g_SkyColor, reflection, ramp.y);

	// Combine waterbody color and reflected color
	float3 water_color = lerp(body_color, reflected_color, ramp.x);


	// --------------- Sun spots

	float cos_spec = clamp(dot(reflect_vec, g_SunDir), 0, 1);
	float sun_spot = pow(cos_spec, g_Shineness);
	water_color += g_SunColor * sun_spot;

	normal = mul(float3(grad, g_TexelLength_x2), (float3x3) g_mWorld);

#if DEPTH_ONLY==1
	output.depth = length(g_vLightPosition.xyz - In.vWorldPosition.xyz);
#else
	output.color = float4(water_color, 1.0f);
#endif

	return output;
}