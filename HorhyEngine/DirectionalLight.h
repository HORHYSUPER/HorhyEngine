#pragma once

#include "ILight.h"
#include "Engine.h"

namespace D3D11Framework
{
#define MAX_CASCADES 8

	class Scene;

	enum SHADOW_TEXTURE_FORMAT
	{
		CASCADE_DXGI_FORMAT_R32_TYPELESS,
		CASCADE_DXGI_FORMAT_R24G8_TYPELESS,
		CASCADE_DXGI_FORMAT_R16_TYPELESS,
		CASCADE_DXGI_FORMAT_R8_TYPELESS
	};

	enum SCENE_SELECTION
	{
		POWER_PLANT_SCENE,
		TEST_SCENE
	};

	enum FIT_PROJECTION_TO_CASCADES
	{
		FIT_TO_CASCADES,
		FIT_TO_SCENE
	};

	enum FIT_TO_NEAR_FAR
	{
		FIT_NEARFAR_PANCAKING,
		FIT_NEARFAR_ZERO_ONE,
		FIT_NEARFAR_AABB,
		FIT_NEARFAR_SCENE_AABB
	};

	enum CASCADE_SELECTION
	{
		CASCADE_SELECTION_MAP,
		CASCADE_SELECTION_INTERVAL
	};

	enum CAMERA_SELECTION
	{
		EYE_CAMERA,
		LIGHT_CAMERA,
		ORTHO_CAMERA1,
		ORTHO_CAMERA2,
		ORTHO_CAMERA3,
		ORTHO_CAMERA4,
		ORTHO_CAMERA5,
		ORTHO_CAMERA6,
		ORTHO_CAMERA7,
		ORTHO_CAMERA8
	};
	// when these paramters change, we must reallocate the shadow resources.
	struct CascadeConfig
	{
		INT m_nCascadeLevels;
		SHADOW_TEXTURE_FORMAT m_ShadowBufferFormat;
		INT m_iBufferSize;
	};


	struct CB_ALL_SHADOW_DATA
	{
		XMFLOAT4 color;
		float multiplier;

		XMMATRIX  m_WorldViewProj;
		XMMATRIX  m_World;
		XMMATRIX  m_WorldView;
		XMMATRIX  m_Shadow;
		XMVECTOR m_vCascadeOffset[8];
		XMVECTOR m_vCascadeScale[8];

		INT         m_nCascadeLevels; // Number of Cascades
		INT         m_iVisualizeCascades; // 1 is to visualize the cascades in different colors. 0 is to just draw the scene.
		INT         m_iPCFBlurForLoopStart; // For loop begin value. For a 5x5 kernal this would be -2.
		INT         m_iPCFBlurForLoopEnd; // For loop end value. For a 5x5 kernel this would be 3.

		// For Map based selection scheme, this keeps the pixels inside of the the valid range.
		// When there is no boarder, these values are 0 and 1 respectivley.
		FLOAT       m_fMinBorderPadding;
		FLOAT       m_fMaxBorderPadding;
		FLOAT       m_fShadowBiasFromGUI;  // A shadow map offset to deal with self shadow artifacts.  
		//These artifacts are aggravated by PCF.
		FLOAT       m_fShadowPartitionSize;
		FLOAT       m_fCascadeBlendArea; // Amount to overlap when blending between cascades.
		FLOAT       m_fTexelSize; // Shadow map texel size.
		FLOAT       m_fNativeTexelSizeInX; // Texel size in native map ( textures are packed ).
		FLOAT       m_fPaddingForCB3;// Padding variables CBs must be a multiple of 16 bytes.
		FLOAT       m_fCascadeFrustumsEyeSpaceDepths[8]; // The values along Z that seperate the cascades.
		XMVECTOR	m_fCascadeFrustumsEyeSpaceDepthsFloat4[8];// the values along Z that separte the cascades.  
		// Wastefully stored in float4 so they are array indexable :(
		XMVECTOR m_vLightDir;
	};

	class DirectionalLight :
		public ILight
	{
	public:
		DirectionalLight();
		~DirectionalLight();

		bool Init();
		void DrawShadowSurface(DrawCmd &);
		void DrawScene(RENDER_TYPE);

		HRESULT DestroyAndDeallocateShadowResources();

		XMVECTOR GetSceneAABBMin() { return m_vSceneAABBMin; };
		XMVECTOR GetSceneAABBMax() { return m_vSceneAABBMax; };


		INT                                 m_iCascadePartitionsMax;
		FLOAT                               m_fCascadePartitionsFrustum[MAX_CASCADES]; // Values are  between near and far
		INT                                 m_iCascadePartitionsZeroToOne[MAX_CASCADES]; // Values are 0 to 100 and represent a percent of the frstum
		INT                                 m_iPCFBlurSize;
		FLOAT                               m_fPCFOffset;
		INT                                 m_iDerivativeBasedOffset;
		INT                                 m_iBlurBetweenCascades;
		FLOAT                               m_fBlurBetweenCascadesAmount;

		BOOL                                m_bMoveLightTexelSize;
		CAMERA_SELECTION                    m_eSelectedCamera;
		FIT_PROJECTION_TO_CASCADES          m_eSelectedCascadesFit;
		FIT_TO_NEAR_FAR                     m_eSelectedNearFarFit;
		CASCADE_SELECTION                   m_eSelectedCascadeSelection;
		DX12_RenderTarget					*GetRT()
		{
			return m_pShadowMapRT;
		}

	private:

		// Compute the near and far plane by intersecting an Ortho Projection with the Scenes AABB.
		void ComputeNearAndFar(FLOAT& fNearPlane,
			FLOAT& fFarPlane,
			FXMVECTOR vLightCameraOrthographicMin,
			FXMVECTOR vLightCameraOrthographicMax,
			XMVECTOR* pvPointsInCameraView
			);
		
		void CreateFrustumPointsFromCascadeInterval(float fCascadeIntervalBegin,
			FLOAT fCascadeIntervalEnd,
			XMMATRIX &vProjection,
			XMVECTOR* pvCornerPointsWorld);
		
		void CreateAABBPoints(XMVECTOR* vAABBPoints, FXMVECTOR vCenter, FXMVECTOR vExtents);
		
		HRESULT ReleaseAndAllocateNewShadowResources();  // This is called when cascade config changes. 

		DX12_RenderTarget					*m_pShadowMapRT;
		XMVECTOR                            m_vSceneAABBMin;
		XMVECTOR                            m_vSceneAABBMax;
		// For example: when the shadow buffer size changes.
		XMMATRIX							m_matShadowProj[MAX_CASCADES];
		XMMATRIX							m_matShadowView;
		CascadeConfig						m_CopyOfCascadeConfig;      // This copy is used to determine when settings change. 
		//Some of these settings require new buffer allocations.
		CascadeConfig*						m_pCascadeConfig;           // Pointer to the most recent setting.

		// D3D11 variables
		PipelineStateObject					*m_pDirectionalLightPso;
		DX12_RenderTargetConfig				*m_pSceneRTC;
		DX12_RenderTargetConfig				*m_pCSMRTC;
		ID3D11Texture2D*                    m_pCascadedShadowMapTexture;
		ID3D11DepthStencilView*             m_pCascadedShadowMapDSV;
		ID3D11ShaderResourceView*           m_pCascadedShadowMapSRV;
		Shader								*m_pCascadedShadowShader;
		// An actual title would break this up into multiple 
		// buffers updated based on frequency of variable changes
		//DX11_RasterizerState				*m_prsScene;
		//DX11_RasterizerState				*m_prsShadow;
		//DX11_RasterizerState				*m_prsShadowPancake;
		DX12_Sampler						*m_pSamShadowPCF;
		DX12_UniformBuffer					*m_pCascadesProjectionsUB;
		D3D11_VIEWPORT                      m_RenderVP[MAX_CASCADES];
		CB_ALL_SHADOW_DATA					m_LightBufferData;
		CascadeConfig						m_CascadeConfig;
	};
}