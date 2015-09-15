#include "globals.hlsl"

// This flag uses the derivative information to map the texels in a shadow map to the
// view space plane of the primitive being rendred.  This depth is then used as the 
// comparison depth and reduces self shadowing aliases.  This  technique is expensive
// and is only valid when objects are planer ( such as a ground plane ).
#ifndef USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG
#define USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG 0
#endif

// This flag enables the shadow to blend between cascades.  This is most useful when the 
// the shadow maps are small and artifact can be seen between the various cascade layers.
#ifndef BLEND_BETWEEN_CASCADE_LAYERS_FLAG
#define BLEND_BETWEEN_CASCADE_LAYERS_FLAG 0
#endif

// There are two methods for selecting the proper cascade a fragment lies in.  Interval selection
// compares the depth of the fragment against the frustum's depth partition.
// Map based selection compares the texture coordinates against the acutal cascade maps.
// Map based selection gives better coverage.  
// Interval based selection is easier to extend and understand.
#ifndef SELECT_CASCADE_BY_INTERVAL_FLAG
#define SELECT_CASCADE_BY_INTERVAL_FLAG 0
#endif

// The number of cascades 
#ifndef CASCADE_COUNT_FLAG
#define CASCADE_COUNT_FLAG 8
#endif

Texture3D<float4> g_VoxelGridShadedInputTexture: register(t0);
Texture3D<float4> g_VoxelGridNormalTexture: register(t1);
Texture2D			g_txShadow			: register(t2);

RWTexture3D<float4> g_VoxelGridShadedOutputTexture: register(u0);

SamplerComparisonState g_samShadow          : register(CUSTOM0_SAM_BP);

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

//cbuffer cbAllShadowData : register(b3)
//{
//	matrix          dirLightUB.mWorldViewProjection;
//	matrix          dirLightUB.mWorld;
//	matrix          dirLightUB.mWorldView;
//	matrix          dirLightUB.mShadow;
//	float4          dirLightUB.vCascadeOffset[8];
//	float4          dirLightUB.vCascadeScale[8];
//	int             dirLightUB.nCascadeLevels; // Number of Cascades
//	int             dirLightUB.iVisualizeCascades; // 1 is to visualize the cascades in different colors. 0 is to just draw the scene
//	int             dirLightUB.iPCFBlurForLoopStart; // For loop begin value. For a 5x5 Kernal this would be -2.
//	int             dirLightUB.iPCFBlurForLoopEnd; // For loop end value. For a 5x5 kernel this would be 3.
//
//										  // For Map based selection scheme, this keeps the pixels inside of the the valid range.
//										  // When there is no boarder, these values are 0 and 1 respectivley.
//	float           dirLightUB.fMinBorderPadding;
//	float           dirLightUB.fMaxBorderPadding;
//	float           dirLightUB.fShadowBiasFromGUI;  // A shadow map offset to deal with self shadow artifacts.  
//										   //These artifacts are aggravated by PCF.
//	float           dirLightUB.fShadowPartitionSize;
//	float           dirLightUB.fCascadeBlendArea; // Amount to overlap when blending between cascades.
//	float           dirLightUB.fTexelSize;
//	float           dirLightUB.fNativeTexelSizeInX;
//	float           dirLightUB.fPaddingForCB3; // Padding variables exist because CBs must be a multiple of 16 bytes.
//	float4          dirLightUB.fCascadeFrustumsEyeSpaceDepthsFloat[2];  // The values along Z that seperate the cascades.
//	float4          dirLightUB.fCascadeFrustumsEyeSpaceDepthsFloat4[8];  // the values along Z that separte the cascades.  
//																// Wastefully stored in float4 so they are array indexable. 
//	float3          dirLightUB.vLightDir;
//	float           dirLightUB.fPaddingCB4;
//};

GLOBAL_DIR_LIGHT_UB(dirLightUB);

static const float4 vCascadeColorsMultiplier[8] =
{
	float4 (1.5f, 0.0f, 0.0f, 1.0f),
	float4 (0.0f, 1.5f, 0.0f, 1.0f),
	float4 (0.0f, 0.0f, 5.5f, 1.0f),
	float4 (1.5f, 0.0f, 5.5f, 1.0f),
	float4 (1.5f, 1.5f, 0.0f, 1.0f),
	float4 (1.0f, 1.0f, 1.0f, 1.0f),
	float4 (0.0f, 1.0f, 5.5f, 1.0f),
	float4 (0.5f, 3.5f, 0.75f, 1.0f)
};


void ComputeCoordinatesTransform(in int iCascadeIndex,
	in float4 InterpolatedPosition,
	in out float4 vShadowTexCoord,
	in out float4 vShadowTexCoordViewSpace)
{
	// Now that we know the correct map, we can transform the world space position of the current fragment                
	if (SELECT_CASCADE_BY_INTERVAL_FLAG)
	{
		vShadowTexCoord = vShadowTexCoordViewSpace * dirLightUB.vCascadeScale[iCascadeIndex];
		vShadowTexCoord += dirLightUB.vCascadeOffset[iCascadeIndex];
	}

	vShadowTexCoord.x *= dirLightUB.fShadowPartitionSize;  // precomputed (float)iCascadeIndex / (float)CASCADE_CNT
	vShadowTexCoord.x += (dirLightUB.fShadowPartitionSize * (float)iCascadeIndex);
}


//--------------------------------------------------------------------------------------
// This function calculates the screen space depth for shadow space texels
//--------------------------------------------------------------------------------------
void CalculateRightAndUpTexelDepthDeltas(in float3 vShadowTexDDX,
	in float3 vShadowTexDDY,
	out float fUpTextDepthWeight,
	out float fRightTextDepthWeight
	) {

	// We use the derivatives in X and Y to create a transformation matrix.  Because these derivives give us the 
	// transformation from screen space to shadow space, we need the inverse matrix to take us from shadow space 
	// to screen space.  This new matrix will allow us to map shadow map texels to screen space.  This will allow 
	// us to find the screen space depth of a corresponding depth pixel.
	// This is not a perfect solution as it assumes the underlying geometry of the scene is a plane.  A more 
	// accureate way of finding the actual depth would be to do a deferred rendering approach and actually 
	//sample the depth.

	// Using an offset, or using variance shadow maps is a better approach to reducing these artifacts in most cases.

	float2x2 matScreentoShadow = float2x2(vShadowTexDDX.xy, vShadowTexDDY.xy);
	float fDeterminant = determinant(matScreentoShadow);

	float fInvDeterminant = 1.0f / fDeterminant;

	float2x2 matShadowToScreen = float2x2 (
		matScreentoShadow._22 * fInvDeterminant, matScreentoShadow._12 * -fInvDeterminant,
		matScreentoShadow._21 * -fInvDeterminant, matScreentoShadow._11 * fInvDeterminant);

	float2 vRightShadowTexelLocation = float2(dirLightUB.fTexelSize, 0.0f);
	float2 vUpShadowTexelLocation = float2(0.0f, dirLightUB.fTexelSize);

	// Transform the right pixel by the shadow space to screen space matrix.
	float2 vRightTexelDepthRatio = mul(vRightShadowTexelLocation, matShadowToScreen);
	float2 vUpTexelDepthRatio = mul(vUpShadowTexelLocation, matShadowToScreen);

	// We can now caculate how much depth changes when you move up or right in the shadow map.
	// We use the ratio of change in x and y times the dervivite in X and Y of the screen space 
	// depth to calculate this change.
	fUpTextDepthWeight =
		vUpTexelDepthRatio.x * vShadowTexDDX.z
		+ vUpTexelDepthRatio.y * vShadowTexDDY.z;
	fRightTextDepthWeight =
		vRightTexelDepthRatio.x * vShadowTexDDX.z
		+ vRightTexelDepthRatio.y * vShadowTexDDY.z;
}


//--------------------------------------------------------------------------------------
// Use PCF to sample the depth map and return a percent lit value.
//--------------------------------------------------------------------------------------
void CalculatePCFPercentLit(in float4 vShadowTexCoord,
	in float fRightTexelDepthDelta,
	in float fUpTexelDepthDelta,
	in float fBlurRowSize,
	out float fPercentLit
	)
{
	fPercentLit = 0.0f;
	// This loop could be unrolled, and texture immediate offsets could be used if the kernel size were fixed.
	// This would be performance improvment.
	for (int x = dirLightUB.iPCFBlurForLoopStart; x < dirLightUB.iPCFBlurForLoopEnd; ++x)
	{
		for (int y = dirLightUB.iPCFBlurForLoopStart; y < dirLightUB.iPCFBlurForLoopEnd; ++y)
		{
			float depthcompare = vShadowTexCoord.z;
			// A very simple solution to the depth bias problems of PCF is to use an offset.
			// Unfortunately, too much offset can lead to Peter-panning (shadows near the base of object disappear )
			// Too little offset can lead to shadow acne ( objects that should not be in shadow are partially self shadowed ).
			depthcompare -= dirLightUB.fShadowBiasFromGUI;
			if (USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG)
			{
				// Add in derivative computed depth scale based on the x and y pixel.
				depthcompare += fRightTexelDepthDelta * ((float)x) + fUpTexelDepthDelta * ((float)y);
			}
			// Compare the transformed pixel depth to the depth read from the map.
			fPercentLit += g_txShadow.SampleCmpLevelZero(g_samShadow,
				float2(
					vShadowTexCoord.x + (((float)x) * dirLightUB.fNativeTexelSizeInX),
					vShadowTexCoord.y + (((float)y) * dirLightUB.fTexelSize)
					),
				depthcompare);
		}
	}
	fPercentLit /= (float)fBlurRowSize;
}

//--------------------------------------------------------------------------------------
// Calculate amount to blend between two cascades and the band where blending will occure.
//--------------------------------------------------------------------------------------
void CalculateBlendAmountForInterval(in int iCurrentCascadeIndex,
	in out float fPixelDepth,
	in out float fCurrentPixelsBlendBandLocation,
	out float fBlendBetweenCascadesAmount
	)
{

	// We need to calculate the band of the current shadow map where it will fade into the next cascade.
	// We can then early out of the expensive PCF for loop.
	// 
	float fBlendInterval = dirLightUB.fCascadeFrustumsEyeSpaceDepthsFloat4[iCurrentCascadeIndex].x;
	//if( iNextCascadeIndex > 1 ) 
	int fBlendIntervalbelowIndex = min(0, iCurrentCascadeIndex - 1);
	fPixelDepth -= dirLightUB.fCascadeFrustumsEyeSpaceDepthsFloat4[fBlendIntervalbelowIndex].x;
	fBlendInterval -= dirLightUB.fCascadeFrustumsEyeSpaceDepthsFloat4[fBlendIntervalbelowIndex].x;

	// The current pixel's blend band location will be used to determine when we need to blend and by how much.
	fCurrentPixelsBlendBandLocation = fPixelDepth / fBlendInterval;
	fCurrentPixelsBlendBandLocation = 1.0f - fCurrentPixelsBlendBandLocation;
	// The fBlendBetweenCascadesAmount is our location in the blend band.
	fBlendBetweenCascadesAmount = fCurrentPixelsBlendBandLocation / dirLightUB.fCascadeBlendArea;
}



//--------------------------------------------------------------------------------------
// Calculate amount to blend between two cascades and the band where blending will occure.
//--------------------------------------------------------------------------------------
void CalculateBlendAmountForMap(in float4 vShadowMapTextureCoord,
	in out float fCurrentPixelsBlendBandLocation,
	out float fBlendBetweenCascadesAmount)
{
	// Calcaulte the blend band for the map based selection.
	float2 distanceToOne = float2 (1.0f - vShadowMapTextureCoord.x, 1.0f - vShadowMapTextureCoord.y);
	fCurrentPixelsBlendBandLocation = min(vShadowMapTextureCoord.x, vShadowMapTextureCoord.y);
	float fCurrentPixelsBlendBandLocation2 = min(distanceToOne.x, distanceToOne.y);
	fCurrentPixelsBlendBandLocation =
		min(fCurrentPixelsBlendBandLocation, fCurrentPixelsBlendBandLocation2);
	fBlendBetweenCascadesAmount = fCurrentPixelsBlendBandLocation / dirLightUB.fCascadeBlendArea;
}

[numthreads(8, 8, 8)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	float4 normal = g_VoxelGridNormalTexture[dispatchThreadID.xyz];
	if (normal.w == 0)
		return;

	float4 outputColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float3 position = dispatchThreadID * customUB.voxelSize + customUB.voxelizePosition.xyz - customUB.voxelizeSize / 2;
	float diffuse;
	float4 vShadowMapTextureCoord = 0.0f;
	float4 vShadowMapTextureCoord_blend = 0.0f;
	float4 vVisualizeCascadeColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
	float fPercentLit = 0.0f;
	float fPercentLit_blend = 0.0f;
	float fUpTextDepthWeight = 0;
	float fRightTextDepthWeight = 0;
	float fUpTextDepthWeight_blend = 0;
	float fRightTextDepthWeight_blend = 0;
	int iCascadeFound = 0;
	int iNextCascadeIndex = 1;
	float fCurrentPixelDepth;

	int iBlurRowSize = dirLightUB.iPCFBlurForLoopEnd - dirLightUB.iPCFBlurForLoopStart;
	iBlurRowSize *= iBlurRowSize;
	float fBlurRowSize = (float)iBlurRowSize;

	// The interval based selection technique compares the pixel's depth against the frustum's cascade divisions.
	fCurrentPixelDepth = mul(float4(position, 1.0f), dirLightUB.mWorldView).z;

	// This for loop is not necessary when the frustum is uniformaly divided and interval based selection is used.
	// In this case fCurrentPixelDepth could be used as an array lookup into the correct frustum. 
	int iCurrentCascadeIndex;

	float4 vShadowMapTextureCoordViewSpace = mul(float4(position, 1.0f), dirLightUB.mShadow);
	if (SELECT_CASCADE_BY_INTERVAL_FLAG)
	{
		iCurrentCascadeIndex = 0;
		if (CASCADE_COUNT_FLAG > 1)
		{
			float4 vCurrentPixelDepth = mul(float4(position, 1.0f), dirLightUB.mWorldView).z;
			float4 fComparison = (vCurrentPixelDepth > dirLightUB.fCascadeFrustumsEyeSpaceDepthsFloat[0]);
			float4 fComparison2 = (vCurrentPixelDepth > dirLightUB.fCascadeFrustumsEyeSpaceDepthsFloat[1]);
			float fIndex = dot(
				float4(CASCADE_COUNT_FLAG > 0,
					CASCADE_COUNT_FLAG > 1,
					CASCADE_COUNT_FLAG > 2,
					CASCADE_COUNT_FLAG > 3)
				, fComparison)
				+ dot(
					float4(
						CASCADE_COUNT_FLAG > 4,
						CASCADE_COUNT_FLAG > 5,
						CASCADE_COUNT_FLAG > 6,
						CASCADE_COUNT_FLAG > 7)
					, fComparison2);

			fIndex = min(fIndex, CASCADE_COUNT_FLAG - 1);
			iCurrentCascadeIndex = (int)fIndex;
		}
	}

	if (!SELECT_CASCADE_BY_INTERVAL_FLAG)
	{
		iCurrentCascadeIndex = 0;
		if (CASCADE_COUNT_FLAG == 1)
		{
			vShadowMapTextureCoord = vShadowMapTextureCoordViewSpace * dirLightUB.vCascadeScale[0];
			vShadowMapTextureCoord += dirLightUB.vCascadeOffset[0];
		}
		if (CASCADE_COUNT_FLAG > 1) {
			for (int iCascadeIndex = 0; iCascadeIndex < CASCADE_COUNT_FLAG && iCascadeFound == 0; ++iCascadeIndex)
			{
				vShadowMapTextureCoord = vShadowMapTextureCoordViewSpace * dirLightUB.vCascadeScale[iCascadeIndex];
				vShadowMapTextureCoord += dirLightUB.vCascadeOffset[iCascadeIndex];

				if (min(vShadowMapTextureCoord.x, vShadowMapTextureCoord.y) > dirLightUB.fMinBorderPadding
					&& max(vShadowMapTextureCoord.x, vShadowMapTextureCoord.y) < dirLightUB.fMaxBorderPadding)
				{
					iCurrentCascadeIndex = iCascadeIndex;
					iCascadeFound = 1;
				}
			}
		}
	}

	if (BLEND_BETWEEN_CASCADE_LAYERS_FLAG)
	{
		// Repeat text coord calculations for the next cascade. 
		// The next cascade index is used for blurring between maps.
		iNextCascadeIndex = min(CASCADE_COUNT_FLAG - 1, iCurrentCascadeIndex + 1);
	}

	float fBlendBetweenCascadesAmount = 1.0f;
	float fCurrentPixelsBlendBandLocation = 1.0f;

	if (SELECT_CASCADE_BY_INTERVAL_FLAG)
	{
		if (BLEND_BETWEEN_CASCADE_LAYERS_FLAG && CASCADE_COUNT_FLAG > 1)
		{
			CalculateBlendAmountForInterval(iCurrentCascadeIndex, fCurrentPixelDepth,
				fCurrentPixelsBlendBandLocation, fBlendBetweenCascadesAmount);
		}
	}
	else
	{

		if (BLEND_BETWEEN_CASCADE_LAYERS_FLAG)
		{
			CalculateBlendAmountForMap(vShadowMapTextureCoord,
				fCurrentPixelsBlendBandLocation, fBlendBetweenCascadesAmount);
		}
	}

	float3 vShadowMapTextureCoordDDX;
	float3 vShadowMapTextureCoordDDY;
	// The derivatives are used to find the slope of the current plane.
	// The derivative calculation has to be inside of the loop in order to prevent divergent flow control artifacts.
	if (USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG)
	{
		vShadowMapTextureCoordDDX = ddx(vShadowMapTextureCoordViewSpace);
		vShadowMapTextureCoordDDY = ddy(vShadowMapTextureCoordViewSpace);

		vShadowMapTextureCoordDDX *= dirLightUB.vCascadeScale[iCurrentCascadeIndex];
		vShadowMapTextureCoordDDY *= dirLightUB.vCascadeScale[iCurrentCascadeIndex];
	}

	ComputeCoordinatesTransform(iCurrentCascadeIndex,
		float4(position, 1.0f),
		vShadowMapTextureCoord,
		vShadowMapTextureCoordViewSpace);


	vVisualizeCascadeColor = vCascadeColorsMultiplier[iCurrentCascadeIndex];

	if (USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG)
	{
		CalculateRightAndUpTexelDepthDeltas(vShadowMapTextureCoordDDX, vShadowMapTextureCoordDDY,
			fUpTextDepthWeight, fRightTextDepthWeight);
	}

	CalculatePCFPercentLit(vShadowMapTextureCoord, fRightTextDepthWeight,
		fUpTextDepthWeight, fBlurRowSize, fPercentLit);

	if (BLEND_BETWEEN_CASCADE_LAYERS_FLAG && CASCADE_COUNT_FLAG > 1)
	{
		if (fCurrentPixelsBlendBandLocation < dirLightUB.fCascadeBlendArea)
		{  // the current pixel is within the blend band.

		   // Repeat text coord calculations for the next cascade. 
		   // The next cascade index is used for blurring between maps.
			if (!SELECT_CASCADE_BY_INTERVAL_FLAG)
			{
				vShadowMapTextureCoord_blend = vShadowMapTextureCoordViewSpace * dirLightUB.vCascadeScale[iNextCascadeIndex];
				vShadowMapTextureCoord_blend += dirLightUB.vCascadeOffset[iNextCascadeIndex];
			}

			ComputeCoordinatesTransform(iNextCascadeIndex,
				float4(position, 1.0f),
				vShadowMapTextureCoord_blend,
				vShadowMapTextureCoordViewSpace);

			// We repeat the calcuation for the next cascade layer, when blending between maps.
			if (fCurrentPixelsBlendBandLocation < dirLightUB.fCascadeBlendArea)
			{  // the current pixel is within the blend band.
				if (USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG)
				{

					CalculateRightAndUpTexelDepthDeltas(vShadowMapTextureCoordDDX,
						vShadowMapTextureCoordDDY,
						fUpTextDepthWeight_blend,
						fRightTextDepthWeight_blend);
				}
				CalculatePCFPercentLit(vShadowMapTextureCoord_blend, fRightTextDepthWeight_blend,
					fUpTextDepthWeight_blend, fBlurRowSize, fPercentLit_blend);
				fPercentLit = lerp(fPercentLit_blend, fPercentLit, fBlendBetweenCascadesAmount);
				// Blend the two calculated shadows by the blend amount.
			}
		}
	}

	if (!dirLightUB.iVisualizeCascades) vVisualizeCascadeColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

	diffuse = saturate(dot(normalize(dirLightUB.vLightDir), normal.xyz));
	if (diffuse > 0.0f)
	{
		outputColor += dirLightUB.color * dirLightUB.multiplier * diffuse;
	}

	outputColor = saturate(outputColor * fPercentLit);

	g_VoxelGridShadedOutputTexture[dispatchThreadID.xyz] = outputColor + g_VoxelGridShadedInputTexture[dispatchThreadID.xyz];
}