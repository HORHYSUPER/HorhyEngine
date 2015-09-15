//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

struct PSInput
{
	float4 position : SV_POSITION;
};

PSInput VSMain(float4 position : POSITION, float4 color : COLOR, uint uVID : SV_VertexID)
{
	PSInput result;

	float2 texcoord = float2(uVID & 1, uVID >> 1); //you can use these for texture coordinates later

	result.position = float4((texcoord.x - 0.5f) * 2, -(texcoord.y - 0.5f) * 2, 0, 1);

	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return float4(1, 0, 0, 1);
}
