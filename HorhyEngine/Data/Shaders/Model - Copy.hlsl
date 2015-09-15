#include "AdaptiveModel.hlsl"

#define MAX_BONE_MATRICES 64
//#define BLEND_SHAPE_DEFORMER_COUNT 16

struct Influence
{
	vector position;
	vector normal;
};

Texture2D g_baseTexture : register(t0);
Texture2D g_nmhTexture :  register(t1);
Texture2D g_DensityTexture : register( t2 );
Buffer<float4> g_DensityBuffer : register( t0 );  // Density vertex buffer
StructuredBuffer<Influence> g_influenceBuffer[BLEND_SHAPE_DEFORMER_COUNT] : register(t3);

SamplerState g_samLinear : register(s0);

cbuffer cbMain : register(b0)
{
	matrix g_mWorld;                            // World matrix
	matrix g_mView;                             // View matrix
	matrix g_mProjection;                       // Projection matrix
	matrix g_mViewProjection;                   // VP matrix
	vector g_vScreenResolution;                 // Screen resolution
	vector g_vEye;					    	// Camera's location
	vector g_vTessellationFactor;               // Edge, inside, minimum tessellation factor and 1/desired triangle size
	float g_detailTessellationHeightScale;    // Height scale for detail tessellation of grid surface
	vector g_vFrustumPlaneEquation[4];          // View frustum plane equations
	matrix g_mConstBone[MAX_BONE_MATRICES];
	vector g_vDiffuseColor;
	float g_timeElapsed;
};

struct VS_INPUT
{
	float4 inPositionOS : POSITION;
	float2 inTexCoord : TEXCOORD0;
	float3 vInNormalOS : NORMAL;
#if SKINNED_MESH==1
	float4 vBlendIndices1 : BLENDINDICES0;
	float4 vBlendIndices2 : BLENDINDICES1;
	float4 vBlendWeights1 : BLENDWEIGHTS0;
	float4 vBlendWeights2 : BLENDWEIGHTS1;
#endif
};

struct VS_OUTPUT_HS_INPUT
{
	float4 vPosition    : SV_POSITION;
	float4 vWorldPos	: POSITION;
	float2 texCoord		: TEXCOORD0;
	float3 vNormal		: NORMAL;
	uint RTIndex : RTARRAYINDEX;
};

struct HS_CONSTANT_DATA_OUTPUT
{
    float    Edges[3]        : SV_TessFactor;
    float    Inside          : SV_InsideTessFactor;
};

struct HS_CONTROL_POINT_OUTPUT
{
	float4 vWorldPos	: POSITION;
	float2 texCoord		: TEXCOORD0;
	float3 vNormal		: TEXCOORD1;
};

struct DS_OUTPUT
{
	float2 texCoord         : TEXCOORD0;
	float3 vNormal			: NORMAL;
	float4 vPosition        : SV_POSITION;
	float4 vWorldPos		: WORLDPOS;
};

struct PS_INPUT
{
	float2 texCoord			: TEXCOORD0;
	float3 vNormal			: NORMAL;
	float4 vPosition        : SV_POSITION;
	float4 vWorldPos		: WORLDPOS;
};

struct PS_OUTPUT
{
	float4 position : SV_TARGET0;
	float4 color : SV_TARGET1;
	float4 normal : SV_TARGET2;
};

//--------------------------------------------------------------------------------------
// Vertex shader: detail tessellation
//--------------------------------------------------------------------------------------
PS_INPUT VS(VS_INPUT i, uint uVID : SV_VertexID)
{
	PS_INPUT Out;

	float4 vPositionWS;
	float3 vNormalWS;
#if SKINNED_MESH==1
	vPositionWS = mul(g_mConstBone[i.vBlendIndices1.x] * i.vBlendWeights1.x, i.inPositionOS.xzyw);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices1.y] * i.vBlendWeights1.y, i.inPositionOS.xzyw);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices1.z] * i.vBlendWeights1.z, i.inPositionOS.xzyw);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices1.w] * i.vBlendWeights1.w, i.inPositionOS.xzyw);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices2.x] * i.vBlendWeights2.x, i.inPositionOS.xzyw);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices2.y] * i.vBlendWeights2.y, i.inPositionOS.xzyw);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices2.z] * i.vBlendWeights2.z, i.inPositionOS.xzyw);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices2.w] * i.vBlendWeights2.w, i.inPositionOS.xzyw);
	vPositionWS = float4(vPositionWS.x, vPositionWS.z, -vPositionWS.y, vPositionWS.w);
	vPositionWS = mul(vPositionWS, g_mWorld);
	
	vNormalWS = mul(g_mConstBone[i.vBlendIndices1.x] * i.vBlendWeights1.x, i.vInNormalOS.xzy);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices1.y] * i.vBlendWeights1.y, i.vInNormalOS.xzy);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices1.z] * i.vBlendWeights1.z, i.vInNormalOS.xzy);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices1.w] * i.vBlendWeights1.w, i.vInNormalOS.xzy);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices2.x] * i.vBlendWeights2.x, i.vInNormalOS.xzy);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices2.y] * i.vBlendWeights2.y, i.vInNormalOS.xzy);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices2.z] * i.vBlendWeights2.z, i.vInNormalOS.xzy);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices2.w] * i.vBlendWeights2.w, i.vInNormalOS.xzy);
	vNormalWS = float3(vNormalWS.x, vNormalWS.z, -vNormalWS.y);
	vNormalWS = mul(vNormalWS, g_mWorld);

#else
	if (g_timeElapsed == 0)
	{
		vNormalWS = mul(i.vInNormalOS, g_mWorld);
		vPositionWS = mul(i.inPositionOS, g_mWorld);
	}
	else
	{
		vPositionWS = i.inPositionOS;
		vNormalWS = i.vInNormalOS;
		for (int blendShapeIndex = 0; blendShapeIndex < BLEND_SHAPE_DEFORMER_COUNT; blendShapeIndex++)
		{
			vPositionWS += (g_influenceBuffer[blendShapeIndex][uVID].position - i.inPositionOS) * 1.0f * g_timeElapsed;
			vNormalWS += (g_influenceBuffer[blendShapeIndex][uVID].normal.xyz - i.vInNormalOS) * 1.0f * g_timeElapsed;
		}
		vPositionWS = mul(vPositionWS, g_mWorld);
		vNormalWS = mul(vNormalWS, g_mWorld);
	}

#endif

	// Propagate texture coordinate through:
	Out.texCoord = i.inTexCoord;

	// Transform normal, tangent and binormal vectors from object 
	// space to homogeneous projection space and normalize them

	// Normalize tangent space vectors
	vNormalWS = normalize(vNormalWS);

	// Output normal
	Out.vNormal = vNormalWS;

	// Write world position
	Out.vWorldPos = vPositionWS;
	Out.vPosition = mul(vPositionWS, g_mViewProjection);

	return Out;
}

VS_OUTPUT_HS_INPUT VS_Tessellation(VS_INPUT i, uint uVID : SV_VertexID)
{
	VS_OUTPUT_HS_INPUT Out;

	float4 vPositionWS;
	float3 vNormalWS;
#if SKINNED_MESH==1
	// Compute position in world space
	vPositionWS = mul(g_mConstBone[i.vBlendIndices1.x] * i.vBlendWeights1.x, i.inPositionOS);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices1.y] * i.vBlendWeights1.y, i.inPositionOS);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices1.z] * i.vBlendWeights1.z, i.inPositionOS);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices1.w] * i.vBlendWeights1.w, i.inPositionOS);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices2.x] * i.vBlendWeights2.x, i.inPositionOS);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices2.y] * i.vBlendWeights2.y, i.inPositionOS);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices2.z] * i.vBlendWeights2.z, i.inPositionOS);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices2.w] * i.vBlendWeights2.w, i.inPositionOS);
	vPositionWS = mul(vPositionWS, g_mWorld);

	vNormalWS = mul(g_mConstBone[i.vBlendIndices1.x] * i.vBlendWeights1.x, i.vInNormalOS);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices1.y] * i.vBlendWeights1.y, i.vInNormalOS);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices1.z] * i.vBlendWeights1.z, i.vInNormalOS);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices1.w] * i.vBlendWeights1.w, i.vInNormalOS);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices2.x] * i.vBlendWeights2.x, i.vInNormalOS);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices2.y] * i.vBlendWeights2.y, i.vInNormalOS);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices2.z] * i.vBlendWeights2.z, i.vInNormalOS);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices2.w] * i.vBlendWeights2.w, i.vInNormalOS);
	vNormalWS = mul(vNormalWS, g_mWorld);

#else
	if (g_timeElapsed == 0)
	{
		vNormalWS = mul(i.vInNormalOS, g_mWorld);
		vPositionWS = mul(i.inPositionOS, g_mWorld);
	}
	else
	{
		vPositionWS = i.inPositionOS;
		vNormalWS = i.vInNormalOS;
		for (int blendShapeIndex = 0; blendShapeIndex < BLEND_SHAPE_DEFORMER_COUNT; blendShapeIndex++)
		{
			vPositionWS += (g_influenceBuffer[blendShapeIndex][uVID].position - i.inPositionOS) * 1.0f * g_timeElapsed;
			vNormalWS += (g_influenceBuffer[blendShapeIndex][uVID].normal.xyz - i.vInNormalOS) * 1.0f * g_timeElapsed;
		}
		vPositionWS = mul(vPositionWS, g_mWorld);
		vNormalWS = mul(vNormalWS, g_mWorld);
	}

#endif

	// Propagate texture coordinate through:
	Out.texCoord = i.inTexCoord;

	// Transform normal, tangent and binormal vectors from object 
	// space to homogeneous projection space and normalize them

	// Normalize tangent space vectors
	vNormalWS = normalize(vNormalWS);

	// Output normal
	Out.vNormal = vNormalWS;

	// Write world position
	Out.vWorldPos = vPositionWS;

	return Out;
}

//--------------------------------------------------------------------------------------
// Hull shader
//--------------------------------------------------------------------------------------
HS_CONSTANT_DATA_OUTPUT ConstantsHS( InputPatch<VS_OUTPUT_HS_INPUT, 3> p, uint PatchID : SV_PrimitiveID )
{
    HS_CONSTANT_DATA_OUTPUT output  = (HS_CONSTANT_DATA_OUTPUT)0;
	float4 vEdgeTessellationFactors = g_vTessellationFactor.xxxy;

	// Get the screen space position of each control point
	float2 f2EdgeScreenPosition0 =
		GetScreenSpacePosition(p[0].vWorldPos.xyz, g_mViewProjection, g_vScreenResolution.x, g_vScreenResolution.y);
	float2 f2EdgeScreenPosition1 =
		GetScreenSpacePosition(p[1].vWorldPos.xyz, g_mViewProjection, g_vScreenResolution.x, g_vScreenResolution.y);
	float2 f2EdgeScreenPosition2 =
		GetScreenSpacePosition(p[2].vWorldPos.xyz, g_mViewProjection, g_vScreenResolution.x, g_vScreenResolution.y);
	
	// Calculate edge tessellation factors based on desired screen space tessellation value
	vEdgeTessellationFactors.x = g_vTessellationFactor.w * distance(f2EdgeScreenPosition2, f2EdgeScreenPosition1);
	vEdgeTessellationFactors.y = g_vTessellationFactor.w * distance(f2EdgeScreenPosition2, f2EdgeScreenPosition0);
	vEdgeTessellationFactors.z = g_vTessellationFactor.w * distance(f2EdgeScreenPosition0, f2EdgeScreenPosition1);
	vEdgeTessellationFactors.w = 0.33 * (vEdgeTessellationFactors.x + vEdgeTessellationFactors.y + vEdgeTessellationFactors.z);

	// Retrieve edge density from edge density buffer (swizzle required to match vertex ordering)
	vEdgeTessellationFactors *= g_DensityBuffer.Load(PatchID).yzxw;

	// Assign tessellation levels
	output.Edges[0] = vEdgeTessellationFactors.x;
	output.Edges[1] = vEdgeTessellationFactors.y;
	output.Edges[2] = vEdgeTessellationFactors.z;
	output.Inside = vEdgeTessellationFactors.w;

	// View frustum culling
	bool bViewFrustumCull = ViewFrustumCull(p[0].vWorldPos, p[1].vWorldPos, p[2].vWorldPos, g_vFrustumPlaneEquation, 10);
	if (bViewFrustumCull)
	{
		// Set all tessellation factors to 0 if frustum cull test succeeds
		output.Edges[0] = 0.0;
		output.Edges[1] = 0.0;
		output.Edges[2] = 0.0;
		output.Inside = 0.0;
	}

    return output;
}

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantsHS")]
[maxtessfactor(15.0)]
HS_CONTROL_POINT_OUTPUT HS(InputPatch<VS_OUTPUT_HS_INPUT, 3> inputPatch, uint uCPID : SV_OutputControlPointID)
{
	HS_CONTROL_POINT_OUTPUT    output = (HS_CONTROL_POINT_OUTPUT)0;
	
	// Copy inputs to outputs
	output.vWorldPos = inputPatch[uCPID].vWorldPos;
	output.vNormal = inputPatch[uCPID].vNormal;
	output.texCoord = inputPatch[uCPID].texCoord;

    return output;
}

//--------------------------------------------------------------------------------------
// Domain Shader
//--------------------------------------------------------------------------------------
[domain("tri")]
DS_OUTPUT DS(HS_CONSTANT_DATA_OUTPUT input, float3 BarycentricCoordinates : SV_DomainLocation,
	const OutputPatch<HS_CONTROL_POINT_OUTPUT, 3> TrianglePatch)
{
	DS_OUTPUT output = (DS_OUTPUT)0;

	// Interpolate world space position with barycentric coordinates
	float3 vWorldPos = BarycentricCoordinates.x * TrianglePatch[0].vWorldPos +
		BarycentricCoordinates.y * TrianglePatch[1].vWorldPos +
		BarycentricCoordinates.z * TrianglePatch[2].vWorldPos;
	
	// Interpolate world space normal and renormalize it
	float3 vNormal = BarycentricCoordinates.x * TrianglePatch[0].vNormal +
		BarycentricCoordinates.y * TrianglePatch[1].vNormal +
		BarycentricCoordinates.z * TrianglePatch[2].vNormal;
	vNormal = normalize(vNormal);

	// Interpolate other inputs with barycentric coordinates
	output.texCoord = BarycentricCoordinates.x * TrianglePatch[0].texCoord +
		BarycentricCoordinates.y * TrianglePatch[1].texCoord +
		BarycentricCoordinates.z * TrianglePatch[2].texCoord;

	// Calculate MIP level to fetch normal from
	float fHeightMapMIPLevel = clamp((distance(vWorldPos, g_vEye) - 100.0f) / 100.0f, 0.0f, 3.0f);

	// Sample normal and height map
	float3 vNormalHeight = g_nmhTexture.SampleLevel(g_samLinear, output.texCoord, fHeightMapMIPLevel).xyz;
		
	// Displace vertex along normal
	vWorldPos += vNormal*(g_detailTessellationHeightScale * (vNormalHeight - 1.0f));
	// Transform world position with viewprojection matrix
	output.vPosition = mul(float4(vWorldPos, 1.0), g_mViewProjection);
#if DEPTH_ONLY==1
	//output.vPosition.x = output.vPosition.x / (abs(output.vPosition.x) + 1.0f) / 2;
	//output.vPosition.y = output.vPosition.y / (abs(output.vPosition.y) + 1.0f) / 2;
#endif
	output.vWorldPos = float4(vWorldPos, 1.0);

	// Per-vertex lighting
	float3 vNormalTS = normalize(vNormalHeight * 2.0f - 1.0f);
	output.vNormal = normalize(vNormal);
	
    return output;
}

//--------------------------------------------------------------------------------------
// Bump mapping shader
//--------------------------------------------------------------------------------------
PS_OUTPUT PS(PS_INPUT i, float4 vWVPPosition : SV_POSITION) : SV_TARGET
{
	PS_OUTPUT output;

	output.position = float4(i.vWorldPos.xyz, 2.0f);
	output.color = g_vDiffuseColor.w < 0 ? g_baseTexture.Sample(g_samLinear, i.texCoord) : g_vDiffuseColor;
	output.normal = float4(i.vNormal, 1.0f);

	return output;
}


//--------------------------------------------------------------------------------------
// Cubemap via Instancing
//--------------------------------------------------------------------------------------
cbuffer cbPerCubeRender : register(b1)
{
	matrix g_mViewCM[6]; // View matrices for cube map rendering
	vector g_vLightPosition;
};

struct VS_OUTPUT_GS_INPUT
{
	float4 vPosition    : SV_POSITION;
	float4 vWorldPos	: POSITION;
	float2 texCoord		: TEXCOORD0;
	float3 vNormal		: NORMAL;
	uint RTIndex : RTARRAYINDEX;
};

struct PS_INPUT_INSTCUBEMAP
{
	float4 vPosition        : SV_POSITION;
	float4 vWorldPos		: POSITION;
	float2 texCoord			: TEXCOORD0;
	float3 vNormal			: NORMAL;
	uint RTIndex : SV_RenderTargetArrayIndex;
};

struct PS_OUTPUT_INSTCUBEMAP
{
#if DEPTH_ONLY==1
	float depth : SV_TARGET;
#else
	float4 color : SV_TARGET;
#endif
};


VS_OUTPUT_GS_INPUT VS_CubeMap_Inst(VS_INPUT i, uint uVID : SV_VertexID, uint CubeSize : SV_InstanceID)
{
	VS_OUTPUT_GS_INPUT Out;

	float4 vPositionWS;
	float3 vNormalWS;
#if SKINNED_MESH==1
	vPositionWS = mul(g_mConstBone[i.vBlendIndices1.x] * i.vBlendWeights1.x, i.inPositionOS.xzyw);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices1.y] * i.vBlendWeights1.y, i.inPositionOS.xzyw);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices1.z] * i.vBlendWeights1.z, i.inPositionOS.xzyw);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices1.w] * i.vBlendWeights1.w, i.inPositionOS.xzyw);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices2.x] * i.vBlendWeights2.x, i.inPositionOS.xzyw);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices2.y] * i.vBlendWeights2.y, i.inPositionOS.xzyw);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices2.z] * i.vBlendWeights2.z, i.inPositionOS.xzyw);
	vPositionWS += mul(g_mConstBone[i.vBlendIndices2.w] * i.vBlendWeights2.w, i.inPositionOS.xzyw);
	vPositionWS = float4(vPositionWS.x, vPositionWS.z, -vPositionWS.y, vPositionWS.w);
	vPositionWS = mul(vPositionWS, g_mWorld);
	
	vNormalWS = mul(g_mConstBone[i.vBlendIndices1.x] * i.vBlendWeights1.x, i.vInNormalOS.xzy);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices1.y] * i.vBlendWeights1.y, i.vInNormalOS.xzy);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices1.z] * i.vBlendWeights1.z, i.vInNormalOS.xzy);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices1.w] * i.vBlendWeights1.w, i.vInNormalOS.xzy);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices2.x] * i.vBlendWeights2.x, i.vInNormalOS.xzy);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices2.y] * i.vBlendWeights2.y, i.vInNormalOS.xzy);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices2.z] * i.vBlendWeights2.z, i.vInNormalOS.xzy);
	vNormalWS += mul(g_mConstBone[i.vBlendIndices2.w] * i.vBlendWeights2.w, i.vInNormalOS.xzy);
	vNormalWS = float3(vNormalWS.x, vNormalWS.z, -vNormalWS.y);
	vNormalWS = mul(vNormalWS, g_mWorld);

#else
	if (g_timeElapsed == 0)
	{
		vNormalWS = mul(i.vInNormalOS, g_mWorld);
		vPositionWS = mul(i.inPositionOS, g_mWorld);
	}
	else
	{
		vPositionWS = i.inPositionOS;
		vNormalWS = i.vInNormalOS;
		for (int blendShapeIndex = 0; blendShapeIndex < BLEND_SHAPE_DEFORMER_COUNT; blendShapeIndex++)
		{
			vPositionWS += (g_influenceBuffer[blendShapeIndex][uVID].position - i.inPositionOS) * 1.0f * g_timeElapsed;
			vNormalWS += (g_influenceBuffer[blendShapeIndex][uVID].normal.xyz - i.vInNormalOS) * 1.0f * g_timeElapsed;
		}
		vPositionWS = mul(vPositionWS, g_mWorld);
		vNormalWS = mul(vNormalWS, g_mWorld);
	}

#endif

	// Propagate texture coordinate through:
	Out.texCoord = i.inTexCoord;

	// Transform normal, tangent and binormal vectors from object 
	// space to homogeneous projection space and normalize them

	// Normalize tangent space vectors
	vNormalWS = normalize(vNormalWS);

	// Output normal
	Out.vNormal = vNormalWS;

	// Write world position
	Out.RTIndex = CubeSize;
	Out.vPosition = mul(vPositionWS, g_mViewCM[CubeSize]);
	Out.vPosition = mul(Out.vPosition, g_mProjection);
	Out.vPosition = vPositionWS;
	Out.vWorldPos = vPositionWS;

	return Out;
}

[maxvertexcount(3)]
void GS_CubeMap_Inst(triangle VS_OUTPUT_GS_INPUT input[3], inout TriangleStream<PS_INPUT_INSTCUBEMAP> CubeMapStream)
{
	PS_INPUT_INSTCUBEMAP output;
	for (int v = 0; v < 3; v++)
	{
		output.vPosition = mul(input[v].vWorldPos, g_mViewCM[input[v].RTIndex]);
		output.vPosition = mul(output.vPosition, g_mProjection);
		//output.vPosition = input[v].vPosition;
		output.vWorldPos = input[v].vWorldPos;
		output.texCoord = input[v].texCoord;
		output.vNormal = input[v].vNormal;
		output.RTIndex = input[v].RTIndex;
		CubeMapStream.Append(output);
	}
}

PS_OUTPUT_INSTCUBEMAP PS_CubeMap_Inst(PS_INPUT_INSTCUBEMAP i) : SV_TARGET
{
	PS_OUTPUT_INSTCUBEMAP output;

#if DEPTH_ONLY==1
	output.depth = length(g_vLightPosition.xyz - i.vWorldPos.xyz);
#else
	output.color = g_baseTexture.Sample(g_samLinear, i.texCoord);
#endif

	return output;
}