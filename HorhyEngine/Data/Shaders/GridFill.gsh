#include "globals.hlsl"

cbuffer CustomUB: register(CUSTOM0_UB_BP)
{
	struct
	{
		matrix	mWorld;                            // World matrix
		vector	vTessellationFactor;               // Edge, inside, minimum tessellation factor and 1/desired triangle size
		float	vdetailTessellationHeightScale;    // Height scale for detail tessellation of grid surface
		vector	vFrustumPlaneEquation[4];          // View frustum plane equations
		matrix	mConstBone[MAX_BONE_MATRICES];
		vector	vDiffuseColor;
		float	timeElapsed;
	}modelCustomUB;
};

cbuffer CustomUB: register(CUSTOM1_UB_BP)
{
	struct
	{
		float voxelizeSize;
		float voxelSize;
		float voxelizeDimension;
		vector voxelizePosition;
	}voxelCustomUB;
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

struct GS_Output
{
	float4 vPosition	: SV_POSITION;
	float3 vWorldPos	: POSITION;
	float3 vNormal		: NORMAL;
	float3 vBinormal	: BINORMAL;
	float3 vTangent		: TANGENT;
	float2 texCoord		: TEXCOORD;
};

float4 TransformPos(float4 In)
{
	return float4((In.xyz - voxelCustomUB.voxelizePosition.xyz + voxelCustomUB.voxelizeSize / 2) / voxelCustomUB.voxelizeSize, In.w);
}
float4 TransformPosition(float2 In)
{
	return float4((In)* 2 - 1, 0, 1);
}

[maxvertexcount(3)]
void main(triangle VS_OUTPUT input[3], inout TriangleStream<GS_Output> outputStream)
{
	float4 vLastPos[3];

	vLastPos[0] = TransformPos(float4(input[0].vWorldPos.xyz, 0));
	vLastPos[1] = TransformPos(float4(input[1].vWorldPos.xyz, 0));
	vLastPos[2] = TransformPos(float4(input[2].vWorldPos.xyz, 0));

	float3 Delta1 = vLastPos[0].xyz - vLastPos[1].xyz;
	float3 Delta2 = vLastPos[0].xyz - vLastPos[2].xyz;

	float S1 = length(cross(float3(Delta1.xy, 0), float3(Delta2.xy, 0)));
	float S2 = length(cross(float3(Delta1.xz, 0), float3(Delta2.xz, 0)));
	float S3 = length(cross(float3(Delta1.yz, 0), float3(Delta2.yz, 0)));

	GS_Output output[3];

	if (S1 > S2) {
		if (S1 > S3)
		{
			output[0].vPosition = TransformPosition(vLastPos[0].xy);
			output[1].vPosition = TransformPosition(vLastPos[1].xy);
			output[2].vPosition = TransformPosition(vLastPos[2].xy);
		}
		else {
			output[0].vPosition = TransformPosition(vLastPos[0].yz);
			output[1].vPosition = TransformPosition(vLastPos[1].yz);
			output[2].vPosition = TransformPosition(vLastPos[2].yz);
		}
	}
	else
	{
		if (S2 > S3)
		{
			output[0].vPosition = TransformPosition(vLastPos[0].xz);
			output[1].vPosition = TransformPosition(vLastPos[1].xz);
			output[2].vPosition = TransformPosition(vLastPos[2].xz);
		}
		else {
			output[0].vPosition = TransformPosition(vLastPos[0].yz);
			output[1].vPosition = TransformPosition(vLastPos[1].yz);
			output[2].vPosition = TransformPosition(vLastPos[2].yz);
		}
	}

	[unroll]
	for (uint i = 0; i < 3; i++)
	{
		output[i].vWorldPos = input[i].vWorldPos;
		output[i].vNormal = input[i].vNormal;
		output[i].vBinormal = input[i].vBinormal;
		output[i].vTangent = input[i].vTangent;
		output[i].texCoord = input[i].texCoord;
	}

	// Bloat triangle in normalized device space with the texel size of the currently bound 
	// render-target. In this way pixels, which would have been discarded due to the low 
	// resolution of the currently bound render-target, will still be rasterized. 
	float2 side0N = normalize(output[1].vPosition.xy - output[0].vPosition.xy);
	float2 side1N = normalize(output[2].vPosition.xy - output[1].vPosition.xy);
	float2 side2N = normalize(output[0].vPosition.xy - output[2].vPosition.xy);
	const float texelSize = 1.0f / voxelCustomUB.voxelizeDimension;
	output[0].vPosition.xy += normalize(-side0N + side2N)*texelSize;
	output[1].vPosition.xy += normalize(side0N - side1N)*texelSize;
	output[2].vPosition.xy += normalize(side1N - side2N)*texelSize;

	outputStream.Append(output[0]);
	outputStream.Append(output[1]);
	outputStream.Append(output[2]);
	outputStream.RestartStrip();
}

/*static const float3 viewDirections[3] =
{
	float3(0.0f, 0.0f, -1.0f), // back to front
	float3(-1.0f, 0.0f, 0.0f), // right to left
	float3(0.0f, -1.0f, 0.0f)  // top to down
};

uint GetViewIndex(in float3 normal)
{
	float3x3 directionMatrix;
	directionMatrix[0] = -viewDirections[0];
	directionMatrix[1] = -viewDirections[1];
	directionMatrix[2] = -viewDirections[2];
	float3 dotProducts = abs(mul(directionMatrix, normal));
	float maximum = max(max(dotProducts.x, dotProducts.y), dotProducts.z);
	uint index;
	if (maximum == dotProducts.x)
		index = 0;
	else if (maximum == dotProducts.y)
		index = 1;
	else
		index = 2;

	return index;
}

[maxvertexcount(3)]
void main(triangle VS_Output input[3], inout TriangleStream<GS_Output> outputStream)
{
	float3 faceNormal = normalize(input[0].normal + input[1].normal + input[2].normal);

	// Get view, at which the current triangle is most visible, in order to achieve highest
	// possible rasterization of the primitive.
	uint viewIndex = GetViewIndex(faceNormal);

	GS_Output output[3];
	[unroll]
	for (uint i = 0; i < 3; i++)
	{
		output[i].vPosition = mul(voxelCustomUB.gridViewProjMatrices[0 + viewIndex], float4(input[i].vPosition, 1.0f));
		output[i].vPositionWS = input[i].vPosition; // position in world space
		output[i].texCoords = input[i].texCoords;
		output[i].normal = input[i].normal;
	}

	// Bloat triangle in normalized device space with the texel size of the currently bound 
	// render-target. In this way pixels, which would have been discarded due to the low 
	// resolution of the currently bound render-target, will still be rasterized. 
	float2 side0N = normalize(output[1].vPosition.xy - output[0].vPosition.xy);
	float2 side1N = normalize(output[2].vPosition.xy - output[1].vPosition.xy);
	float2 side2N = normalize(output[0].vPosition.xy - output[2].vPosition.xy);
	const float texelSize = 1.0f / 512.0f;
	output[0].vPosition.xy += normalize(-side0N + side2N)*texelSize;
	output[1].vPosition.xy += normalize(side0N - side1N)*texelSize;
	output[2].vPosition.xy += normalize(side1N - side2N)*texelSize;

	[unroll]
	for (uint j = 0; j < 3; j++)
		outputStream.Append(output[j]);

	outputStream.RestartStrip();
}*/