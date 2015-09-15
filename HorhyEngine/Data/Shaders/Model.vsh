#include "globals.hlsl"
#include "AdaptiveModel.hlsl"

#ifndef MAX_BONE_MATRICES
#define MAX_BONE_MATRICES 16
#endif

#ifndef BLEND_SHAPE_DEFORMER_COUNT
#define BLEND_SHAPE_DEFORMER_COUNT 16
#endif

struct Influence
{
	vector position;
	vector normal;
};

StructuredBuffer<Influence> g_influenceBuffer[BLEND_SHAPE_DEFORMER_COUNT] : register(t3);

GLOBAL_CAMERA_UB(cameraUB);

cbuffer CustomUB: register(CUSTOM0_UB_BP)
{
	struct
	{
		matrix	mWorld;
		float	material;							// Obfect ID, or anything else ID
		vector	vTessellationFactor;               // Edge, inside, minimum tessellation factor and 1/desired triangle size
		float	vdetailTessellationHeightScale;    // Height scale for detail tessellation of grid surface
		vector	vFrustumPlaneEquation[4];          // View frustum plane equations
		matrix	mConstBone[MAX_BONE_MATRICES];
		vector	vDiffuseColor;
		float	specularLevel;
		float	timeElapsed;
		bool	hasDiffuseTexture;
		bool	hasSpecularTexture;
		bool	hasNormalTexture;
	}modelCustomUB;
};

struct VS_INPUT
{
	float3 inPositionOS : POSITION;
	float3 vInNormalOS    : NORMAL;
	float3 vInBinormalOS  : BINORMAL;
	float3 vInTangentOS   : TANGENT;
	float2 inTexCoord : TEXCOORD0;
#if SKINNED_MESH==1
	float4 vBlendIndices1 : BLENDINDICES0;
	float4 vBlendIndices2 : BLENDINDICES1;
	float4 vBlendWeights1 : BLENDWEIGHTS0;
	float4 vBlendWeights2 : BLENDWEIGHTS1;
#endif
};

struct VS_OUTPUT
{
	float4 vPosition    : SV_POSITION;
	float4 vWorldPos	: POSITION;
	float3 vNormal		: NORMAL;
	float3 vBinormal	: BINORMAL;
	float3 vTangent		: TANGENT;
	float2 texCoord		: TEXCOORD0;
	uint RTIndex : RTARRAYINDEX;
};

VS_OUTPUT main(VS_INPUT i, uint uVID : SV_VertexID, uint instanceID : SV_InstanceID)
{
	VS_OUTPUT Out;

	float4 vPositionWS;
	float3 vNormalWS;
	float3 vBinormalWS;
	float3 vTangentWS;

	float4 vPositionOS = float4(i.inPositionOS, 1.0f);
	float3 vNormalOS = i.vInNormalOS;
	if (modelCustomUB.timeElapsed != 0)
	{
		for (int blendShapeIndex = 0; blendShapeIndex < BLEND_SHAPE_DEFORMER_COUNT; blendShapeIndex++)
		{
			//vPositionOS += (g_influenceBuffer[blendShapeIndex][uVID].position - float4(i.inPositionOS, 1.0f)) * 1.0f * modelCustomUB.timeElapsed;
			//vNormalOS += (g_influenceBuffer[blendShapeIndex][uVID].normal.xyz - i.vInNormalOS) * 1.0f * modelCustomUB.timeElapsed;
		}
	}

#if SKINNED_MESH==1
	float4 vPositionSkinDeform;
	vPositionSkinDeform = mul(modelCustomUB.mConstBone[i.vBlendIndices1.x] * i.vBlendWeights1.x, vPositionOS);
	vPositionSkinDeform += mul(modelCustomUB.mConstBone[i.vBlendIndices1.y] * i.vBlendWeights1.y, vPositionOS);
	vPositionSkinDeform += mul(modelCustomUB.mConstBone[i.vBlendIndices1.z] * i.vBlendWeights1.z, vPositionOS);
	vPositionSkinDeform += mul(modelCustomUB.mConstBone[i.vBlendIndices1.w] * i.vBlendWeights1.w, vPositionOS);
	vPositionSkinDeform += mul(modelCustomUB.mConstBone[i.vBlendIndices2.x] * i.vBlendWeights2.x, vPositionOS);
	vPositionSkinDeform += mul(modelCustomUB.mConstBone[i.vBlendIndices2.y] * i.vBlendWeights2.y, vPositionOS);
	vPositionSkinDeform += mul(modelCustomUB.mConstBone[i.vBlendIndices2.z] * i.vBlendWeights2.z, vPositionOS);
	vPositionSkinDeform += mul(modelCustomUB.mConstBone[i.vBlendIndices2.w] * i.vBlendWeights2.w, vPositionOS);
	vPositionOS = vPositionSkinDeform;

	float3 vNormalSkinDeform;
	vNormalSkinDeform = mul(modelCustomUB.mConstBone[i.vBlendIndices1.x] * i.vBlendWeights1.x, vNormalOS);
	vNormalSkinDeform += mul(modelCustomUB.mConstBone[i.vBlendIndices1.y] * i.vBlendWeights1.y, vNormalOS);
	vNormalSkinDeform += mul(modelCustomUB.mConstBone[i.vBlendIndices1.z] * i.vBlendWeights1.z, vNormalOS);
	vNormalSkinDeform += mul(modelCustomUB.mConstBone[i.vBlendIndices1.w] * i.vBlendWeights1.w, vNormalOS);
	vNormalSkinDeform += mul(modelCustomUB.mConstBone[i.vBlendIndices2.x] * i.vBlendWeights2.x, vNormalOS);
	vNormalSkinDeform += mul(modelCustomUB.mConstBone[i.vBlendIndices2.y] * i.vBlendWeights2.y, vNormalOS);
	vNormalSkinDeform += mul(modelCustomUB.mConstBone[i.vBlendIndices2.z] * i.vBlendWeights2.z, vNormalOS);
	vNormalSkinDeform += mul(modelCustomUB.mConstBone[i.vBlendIndices2.w] * i.vBlendWeights2.w, vNormalOS);
	vNormalOS = vNormalSkinDeform;

#endif
	vPositionWS = mul(vPositionOS, modelCustomUB.mWorld);
	vNormalWS = mul(vNormalOS, modelCustomUB.mWorld);
	vBinormalWS = mul(i.vInBinormalOS, modelCustomUB.mWorld);
	vTangentWS = mul(i.vInTangentOS, modelCustomUB.mWorld);

	Out.vPosition = mul(vPositionWS, cameraUB.viewProjMatrix);
	Out.vWorldPos = vPositionWS;
	Out.vNormal = normalize(vNormalWS);
	Out.vBinormal = normalize(vBinormalWS);
	Out.vTangent = normalize(vTangentWS);
	Out.texCoord = i.inTexCoord;
	Out.RTIndex = instanceID;

	return Out;
}