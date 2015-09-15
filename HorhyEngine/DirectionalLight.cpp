#include "stdafx.h"
#include "Camera.h"
#include "PipelineStateObject.h"
#include "DX12_Shader.h"
#include "DX12_VertexLayout.h"
#include "DX12_RenderTarget.h"
#include "DX12_RenderTargetconfig.h"
#include "DX12_UniformBuffer.h"
#include "DX12_Sampler.h"
#include "GameWorld.h"
#include "macros.h"
#include "DirectionalLight.h"

using namespace D3D11Framework;

static const XMVECTORF32 g_vFLTMAX = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
static const XMVECTORF32 g_vFLTMIN = { -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX };
static const XMVECTORF32 g_vHalfVector = { 0.5f, 0.5f, 0.5f, 0.5f };
static const XMVECTORF32 g_vMultiplySetzwToZero = { 1.0f, 1.0f, 0.0f, 0.0f };
static const XMVECTORF32 g_vZero = { 0.0f, 0.0f, 0.0f, 0.0f };

DirectionalLight::DirectionalLight() :
	m_pSamShadowPCF(NULL),
	m_pCascadedShadowMapTexture(NULL),
	m_pCascadedShadowMapDSV(NULL),
	m_pCascadedShadowMapSRV(NULL),
	m_iBlurBetweenCascades(0),
	m_fBlurBetweenCascadesAmount(0.005f),
	//m_prsShadow(NULL),
	//m_prsShadowPancake(NULL),
	//m_prsScene(NULL),
	m_iPCFBlurSize(3),
	m_fPCFOffset(0.002f),
	m_iDerivativeBasedOffset(0),
	m_pCascadedShadowShader(NULL)
{
	for (INT index = 0; index < MAX_CASCADES; ++index)
	{
		m_RenderVP[index].Height = (FLOAT)m_CopyOfCascadeConfig.m_iBufferSize;
		m_RenderVP[index].Width = (FLOAT)m_CopyOfCascadeConfig.m_iBufferSize;
		m_RenderVP[index].MaxDepth = 1.0f;
		m_RenderVP[index].MinDepth = 0.0f;
		m_RenderVP[index].TopLeftX = 0;
		m_RenderVP[index].TopLeftY = 0;
	}
}

DirectionalLight::~DirectionalLight()
{
}

bool DirectionalLight::Init()
{
	lightType = 0;

	m_CascadeConfig.m_nCascadeLevels = 8;
	m_CascadeConfig.m_iBufferSize = 1024;
	m_CascadeConfig.m_ShadowBufferFormat = (SHADOW_TEXTURE_FORMAT)0;

	m_iCascadePartitionsZeroToOne[0] = 2;
	m_iCascadePartitionsZeroToOne[1] = 4;
	m_iCascadePartitionsZeroToOne[2] = 6;
	m_iCascadePartitionsZeroToOne[3] = 9;
	m_iCascadePartitionsZeroToOne[4] = 13;
	m_iCascadePartitionsZeroToOne[5] = 26;
	m_iCascadePartitionsZeroToOne[6] = 36;
	m_iCascadePartitionsZeroToOne[7] = 70;
	m_iCascadePartitionsMax = 1;
	m_bMoveLightTexelSize = true;
	m_eSelectedCascadesFit = FIT_TO_SCENE;
	m_eSelectedNearFarFit = FIT_NEARFAR_SCENE_AABB;
	m_eSelectedCascadeSelection = CASCADE_SELECTION_MAP;

	m_pLightCamera->SetProjection(XMMatrixPerspectiveFovLH(0.5f * 3.141592654f, 1.0f, 0.01f, 10000.0f));
	m_pLightCamera->SetScreenWidth(1024);
	m_pLightCamera->SetScreenHeight(1024);
	m_pLightCamera->SetScreenNearZ(0.0f);
	m_pLightCamera->SetScreenFarZ(1.0f);
	m_pLightCamera->Render(1);

	// SHADOW_MAP_RT
	RenderTargetDesc rtDesc;
	rtDesc.width = 1024;
	rtDesc.height = 1024;
	rtDesc.depth = m_CascadeConfig.m_nCascadeLevels;
	rtDesc.depthStencilBufferDesc.format = DXGI_FORMAT_D32_FLOAT;
	m_pShadowMapRT = Engine::GetRender()->CreateRenderTarget(rtDesc);
	if (!m_pShadowMapRT)
		return false;

	HRESULT hr = S_OK;

	m_CopyOfCascadeConfig = m_CascadeConfig;
	// Initialize m_iBufferSize to 0 to trigger a reallocate on the first frame.   
	m_CopyOfCascadeConfig.m_iBufferSize = 0;
	// Save a pointer to cascade config.  Each frame we check our copy against the pointer.
	m_pCascadeConfig = &m_CascadeConfig;

	XMVECTOR vMeshMin;
	XMVECTOR vMeshMax;

	m_vSceneAABBMin = XMVectorSet(-100, -100, -100, 1);	// m_vSceneAABBMin = g_vFLTMAX;
	m_vSceneAABBMax = XMVectorSet(100, 100, 100, 1);	// m_vSceneAABBMax = g_vFLTMIN;
	// Calcaulte the AABB for the scene by iterating through all the meshes in the SDKMesh file.
	//for (UINT i = 0; i < pMesh->GetNumMeshes(); ++i)
	//{
	//	SDKMESH_MESH* msh = pMesh->GetMesh(i);
	//	vMeshMin = XMVectorSet(msh->BoundingBoxCenter.x - msh->BoundingBoxExtents.x,
	//		msh->BoundingBoxCenter.y - msh->BoundingBoxExtents.y,
	//		msh->BoundingBoxCenter.z - msh->BoundingBoxExtents.z,
	//		1.0f);

	//	vMeshMax = XMVectorSet(msh->BoundingBoxCenter.x + msh->BoundingBoxExtents.x,
	//		msh->BoundingBoxCenter.y + msh->BoundingBoxExtents.y,
	//		msh->BoundingBoxCenter.z + msh->BoundingBoxExtents.z,
	//		1.0f);

	//	m_vSceneAABBMin = XMVectorMin(vMeshMin, m_vSceneAABBMin);
	//	m_vSceneAABBMax = XMVectorMax(vMeshMax, m_vSceneAABBMax);
	//}

	SamplerDesc samplerDesc;
	samplerDesc.filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	samplerDesc.adressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplerDesc.adressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplerDesc.adressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	samplerDesc.lodBias = 0;
	samplerDesc.maxAnisotropy = 0;
	samplerDesc.compareFunc = D3D12_COMPARISON_FUNC_LESS;
	samplerDesc.borderColor[0] = 0.0;
	samplerDesc.borderColor[1] = 0.0;
	samplerDesc.borderColor[2] = 0.0;
	samplerDesc.borderColor[3] = 0.0;
	samplerDesc.minLOD = 0;
	samplerDesc.maxLOD = 0;

	m_pSamShadowPCF = Engine::GetRender()->CreateSampler(samplerDesc);
	if (!m_pSamShadowPCF)
		return false;

	RtConfigDesc rtcDesc;
	rtcDesc.numColorBuffers = 0;
	rtcDesc.flags = DS_READ_ONLY_RTCF;
	m_pSceneRTC = Engine::GetRender()->CreateRenderTargetConfig(rtcDesc);
	if (!m_pSceneRTC)
		return false;

	{
		string msaa = to_string(static_cast<int>(MSAA_COUNT));

		D3D_SHADER_MACRO defines[] =
		{
			"MSAA", msaa.c_str(),
			NULL, NULL
		};
		DX12_Shader *cascadedShadowMapShader = new DX12_Shader;
		cascadedShadowMapShader->Load("CascadedShadow.sdr", defines);

		DX12_VertexLayout *vertexLayout = new DX12_VertexLayout();
		VertexElementDesc vertexElementDescs[] = { POSITION_ELEMENT, 0, R32G32B32_FLOAT_EF, 0,
			TEXCOORDS_ELEMENT, 0, R32G32_FLOAT_EF, D3D12_APPEND_ALIGNED_ELEMENT,
		};
		vertexLayout->Create(vertexElementDescs, _countof(vertexElementDescs));

		PIPELINE_STATE_DESC pointLightPsoDesc;
		pointLightPsoDesc.shader = cascadedShadowMapShader;
		pointLightPsoDesc.vertexLayout = vertexLayout;
		pointLightPsoDesc.renderTartet = Engine::GetRender()->GetRenderTarget(GBUFFERS_RT_ID);

		m_pDirectionalLightPso = Engine::GetRender()->CreatePipelineStateObject(pointLightPsoDesc);
	}

	//m_pCascadedShadowShader = new Shader();
	//if (!m_pCascadedShadowShader)
	//	return false;

	//string msaa = to_string(static_cast<int>(MSAA_COUNT));

	//D3D_SHADER_MACRO defines[] =
	//{
	//	"MSAA", msaa.c_str(),
	//	NULL, NULL
	//};
	//std::wstring shaderName;
	//shaderName = Engine::m_pPaths[PATH_TO_SHADERS] + L"CascadedShadow.hlsl";

	//if (!m_pCascadedShadowShader->CreateShaderPs((WCHAR*)shaderName.c_str(), defines, "main"))
	//	return false;

	//RasterizerDesc rasterDesc;
	//rasterDesc.fillMode = D3D11_FILL_SOLID;
	//rasterDesc.cullMode = D3D11_CULL_NONE;
	//rasterDesc.frontCounterClockwise = FALSE;
	//rasterDesc.depthBias = 0;
	//rasterDesc.depthBiasClamp = 0.0;
	//rasterDesc.slopeScaledDepthBias = 0.0;
	//rasterDesc.depthClipEnable = TRUE;
	//rasterDesc.scissorTest = FALSE;
	//rasterDesc.multisampleEnable = TRUE;
	//rasterDesc.antialiasedLineEnable = TRUE;
	//m_prsScene = Engine::GetRender()->CreateRasterizerState(rasterDesc);
	//if (!m_prsScene)
	//	return false;

	//rasterDesc.slopeScaledDepthBias = 1.0;
	//m_prsShadow = Engine::GetRender()->CreateRasterizerState(rasterDesc);
	//if (!m_prsShadow)
	//	return false;

	//rasterDesc.depthClipEnable = false;
	//m_prsShadowPancake = Engine::GetRender()->CreateRasterizerState(rasterDesc);
	//if (!m_prsShadowPancake)
	//	return false;

	//m_pUniformBuffer = Engine::GetRender()->CreateUniformBuffer(sizeof(CameraBufferData));
	//if (!m_pUniformBuffer)
	//	return false;

	m_pUniformBuffer = Engine::GetRender()->CreateUniformBuffer(sizeof(CB_ALL_SHADOW_DATA));
	if (!m_pUniformBuffer)
		return false;

	m_pCascadesProjectionsUB = Engine::GetRender()->CreateUniformBuffer(sizeof(XMMATRIX) * m_CascadeConfig.m_nCascadeLevels);
	if (!m_pCascadesProjectionsUB)
		return false;

	return hr;
}

void DirectionalLight::DrawShadowSurface(DrawCmd &drawCmd)
{
	ReleaseAndAllocateNewShadowResources();

	// Copy D3DX matricies into XNA Math matricies.
	const XMMATRIX* mD3DXViewCameraProjection = &Engine::GetRender()->GetCamera()->GetProjection();
	XMMATRIX matViewCameraProjection = XMLoadFloat4x4((XMFLOAT4X4*)&mD3DXViewCameraProjection->r[0].m128_f32[0]);
	const XMMATRIX* mD3DXViewCameraView = &Engine::GetRender()->GetCamera()->GetViewMatrix();
	XMMATRIX matViewCameraView = XMLoadFloat4x4((XMFLOAT4X4*)&mD3DXViewCameraView->r[0].m128_f32[0]);
	const XMMATRIX* mD3DLightView = &m_pLightCamera->GetViewMatrix();
	XMMATRIX matLightCameraView = XMLoadFloat4x4((XMFLOAT4X4*)&mD3DLightView->r[0].m128_f32[0]);

	XMVECTOR det;
	XMMATRIX matInverseViewCamera = XMMatrixInverse(&det, matViewCameraView);

	// Convert from min max representation to center extents represnetation.
	// This will make it easier to pull the points out of the transformation.
	XMVECTOR vSceneCenter = m_vSceneAABBMin + m_vSceneAABBMax;
	vSceneCenter *= g_vHalfVector;
	XMVECTOR vSceneExtents = m_vSceneAABBMax - m_vSceneAABBMin;
	vSceneExtents *= g_vHalfVector;

	XMVECTOR vSceneAABBPointsLightSpace[8];
	// This function simply converts the center and extents of an AABB into 8 points
	CreateAABBPoints(vSceneAABBPointsLightSpace, vSceneCenter, vSceneExtents);
	// Transform the scene AABB to Light space.
	for (int index = 0; index < 8; ++index)
	{
		vSceneAABBPointsLightSpace[index] = XMVector4Transform(vSceneAABBPointsLightSpace[index], matLightCameraView);
	}
	
	FLOAT fFrustumIntervalBegin, fFrustumIntervalEnd;
	XMVECTOR vLightCameraOrthographicMin;  // light space frustrum aabb 
	XMVECTOR vLightCameraOrthographicMax;
	FLOAT fCameraNearFarRange = Engine::GetRender()->GetCamera()->GetScreenFarZ() - Engine::GetRender()->GetCamera()->GetScreenNearZ();

	XMVECTOR vWorldUnitsPerTexel = g_vZero;

	// We loop over the cascades to calculate the orthographic projection for each cascade.
	for (INT iCascadeIndex = 0; iCascadeIndex < m_CopyOfCascadeConfig.m_nCascadeLevels; ++iCascadeIndex)
	{
		// Calculate the interval of the View Frustum that this cascade covers. We measure the interval 
		// the cascade covers as a Min and Max distance along the Z Axis.
		if (m_eSelectedCascadesFit == FIT_TO_CASCADES)
		{
			// Because we want to fit the orthogrpahic projection tightly around the Cascade, we set the Mimiumum cascade 
			// value to the previous Frustum end Interval
			if (iCascadeIndex == 0) fFrustumIntervalBegin = 0.0f;
			else fFrustumIntervalBegin = (FLOAT)m_iCascadePartitionsZeroToOne[iCascadeIndex - 1];
		}
		else
		{
			// In the FIT_TO_SCENE technique the Cascades overlap eachother.  In other words, interval 1 is coverd by
			// cascades 1 to 8, interval 2 is covered by cascades 2 to 8 and so forth.
			fFrustumIntervalBegin = 0.0f;
		}

		// Scale the intervals between 0 and 1. They are now percentages that we can scale with.
		fFrustumIntervalEnd = (FLOAT)m_iCascadePartitionsZeroToOne[iCascadeIndex];
		fFrustumIntervalBegin /= (FLOAT)m_iCascadePartitionsMax;
		fFrustumIntervalEnd /= (FLOAT)m_iCascadePartitionsMax;
		fFrustumIntervalBegin = fFrustumIntervalBegin * fCameraNearFarRange;
		fFrustumIntervalEnd = fFrustumIntervalEnd * fCameraNearFarRange;
		XMVECTOR vFrustumPoints[8];

		// This function takes the began and end intervals along with the projection matrix and returns the 8
		// points that repreresent the cascade Interval
		CreateFrustumPointsFromCascadeInterval(fFrustumIntervalBegin, fFrustumIntervalEnd,
			matViewCameraProjection, vFrustumPoints);

		vLightCameraOrthographicMin = g_vFLTMAX;
		vLightCameraOrthographicMax = g_vFLTMIN;

		XMVECTOR vTempTranslatedCornerPoint;
		// This next section of code calculates the min and max values for the orthographic projection.
		for (int icpIndex = 0; icpIndex < 8; ++icpIndex)
		{
			// Transform the frustum from camera view space to world space.
			vFrustumPoints[icpIndex] = XMVector4Transform(vFrustumPoints[icpIndex], matInverseViewCamera);
			// Transform the point from world space to Light Camera Space.
			vTempTranslatedCornerPoint = XMVector4Transform(vFrustumPoints[icpIndex], matLightCameraView);
			// Find the closest point.
			vLightCameraOrthographicMin = XMVectorMin(vTempTranslatedCornerPoint, vLightCameraOrthographicMin);
			vLightCameraOrthographicMax = XMVectorMax(vTempTranslatedCornerPoint, vLightCameraOrthographicMax);
		}

		// This code removes the shimmering effect along the edges of shadows due to
		// the light changing to fit the camera.
		if (m_eSelectedCascadesFit == FIT_TO_SCENE)
		{
			// Fit the ortho projection to the cascades far plane and a near plane of zero. 
			// Pad the projection to be the size of the diagonal of the Frustum partition. 
			// 
			// To do this, we pad the ortho transform so that it is always big enough to cover 
			// the entire camera view frustum.
			XMVECTOR vDiagonal = vFrustumPoints[0] - vFrustumPoints[6];
			vDiagonal = XMVector3Length(vDiagonal);

			// The bound is the length of the diagonal of the frustum interval.
			FLOAT fCascadeBound = XMVectorGetX(vDiagonal);

			// The offset calculated will pad the ortho projection so that it is always the same size 
			// and big enough to cover the entire cascade interval.
			XMVECTOR vBoarderOffset = (vDiagonal -
				(vLightCameraOrthographicMax - vLightCameraOrthographicMin))
				* g_vHalfVector;
			// Set the Z and W components to zero.
			vBoarderOffset *= g_vMultiplySetzwToZero;

			// Add the offsets to the projection.
			vLightCameraOrthographicMax += vBoarderOffset;
			vLightCameraOrthographicMin -= vBoarderOffset;

			// The world units per texel are used to snap the shadow the orthographic projection
			// to texel sized increments.  This keeps the edges of the shadows from shimmering.
			FLOAT fWorldUnitsPerTexel = fCascadeBound / (float)m_CopyOfCascadeConfig.m_iBufferSize;
			vWorldUnitsPerTexel = XMVectorSet(fWorldUnitsPerTexel, fWorldUnitsPerTexel, 0.0f, 0.0f);


		}
		else if (m_eSelectedCascadesFit == FIT_TO_CASCADES)
		{

			// We calculate a looser bound based on the size of the PCF blur.  This ensures us that we're 
			// sampling within the correct map.
			float fScaleDuetoBlureAMT = ((float)(m_iPCFBlurSize * 2 + 1)
				/ (float)m_CopyOfCascadeConfig.m_iBufferSize);
			XMVECTORF32 vScaleDuetoBlureAMT = { fScaleDuetoBlureAMT, fScaleDuetoBlureAMT, 0.0f, 0.0f };


			float fNormalizeByBufferSize = (1.0f / (float)m_CopyOfCascadeConfig.m_iBufferSize);
			XMVECTOR vNormalizeByBufferSize = XMVectorSet(fNormalizeByBufferSize, fNormalizeByBufferSize, 0.0f, 0.0f);

			// We calculate the offsets as a percentage of the bound.
			XMVECTOR vBoarderOffset = vLightCameraOrthographicMax - vLightCameraOrthographicMin;
			vBoarderOffset *= g_vHalfVector;
			vBoarderOffset *= vScaleDuetoBlureAMT;
			vLightCameraOrthographicMax += vBoarderOffset;
			vLightCameraOrthographicMin -= vBoarderOffset;

			// The world units per texel are used to snap  the orthographic projection
			// to texel sized increments.  
			// Because we're fitting tighly to the cascades, the shimmering shadow edges will still be present when the 
			// camera rotates.  However, when zooming in or strafing the shadow edge will not shimmer.
			vWorldUnitsPerTexel = vLightCameraOrthographicMax - vLightCameraOrthographicMin;
			vWorldUnitsPerTexel *= vNormalizeByBufferSize;

		}
		float fLightCameraOrthographicMinZ = XMVectorGetZ(vLightCameraOrthographicMin);


		if (m_bMoveLightTexelSize)
		{

			// We snape the camera to 1 pixel increments so that moving the camera does not cause the shadows to jitter.
			// This is a matter of integer dividing by the world space size of a texel
			vLightCameraOrthographicMin /= vWorldUnitsPerTexel;
			vLightCameraOrthographicMin = XMVectorFloor(vLightCameraOrthographicMin);
			vLightCameraOrthographicMin *= vWorldUnitsPerTexel;

			vLightCameraOrthographicMax /= vWorldUnitsPerTexel;
			vLightCameraOrthographicMax = XMVectorFloor(vLightCameraOrthographicMax);
			vLightCameraOrthographicMax *= vWorldUnitsPerTexel;

		}

		//These are the unconfigured near and far plane values.  They are purposly awful to show 
		// how important calculating accurate near and far planes is.
		FLOAT fNearPlane = 0.0f;
		FLOAT fFarPlane = 10000.0f;

		if (m_eSelectedNearFarFit == FIT_NEARFAR_AABB)
		{

			XMVECTOR vLightSpaceSceneAABBminValue = g_vFLTMAX;  // world space scene aabb 
			XMVECTOR vLightSpaceSceneAABBmaxValue = g_vFLTMIN;
			// We calculate the min and max vectors of the scene in light space. The min and max "Z" values of the  
			// light space AABB can be used for the near and far plane. This is easier than intersecting the scene with the AABB
			// and in some cases provides similar results.
			for (int index = 0; index < 8; ++index)
			{
				vLightSpaceSceneAABBminValue = XMVectorMin(vSceneAABBPointsLightSpace[index], vLightSpaceSceneAABBminValue);
				vLightSpaceSceneAABBmaxValue = XMVectorMax(vSceneAABBPointsLightSpace[index], vLightSpaceSceneAABBmaxValue);
			}

			// The min and max z values are the near and far planes.
			fNearPlane = XMVectorGetZ(vLightSpaceSceneAABBminValue);
			fFarPlane = XMVectorGetZ(vLightSpaceSceneAABBmaxValue);
		}
		else if (m_eSelectedNearFarFit == FIT_NEARFAR_SCENE_AABB
			|| m_eSelectedNearFarFit == FIT_NEARFAR_PANCAKING)
		{
			// By intersecting the light frustum with the scene AABB we can get a tighter bound on the near and far plane.
			ComputeNearAndFar(fNearPlane, fFarPlane, vLightCameraOrthographicMin,
				vLightCameraOrthographicMax, vSceneAABBPointsLightSpace);
			if (m_eSelectedNearFarFit == FIT_NEARFAR_PANCAKING)
			{
				if (fLightCameraOrthographicMinZ > fNearPlane)
				{
					fNearPlane = fLightCameraOrthographicMinZ;
				}
			}
		}
		else
		{

		}
		// Craete the orthographic projection for this cascade.
		m_matShadowProj[iCascadeIndex] = XMMatrixOrthographicOffCenterLH(
			XMVectorGetX(vLightCameraOrthographicMin),
			XMVectorGetX(vLightCameraOrthographicMax),
			XMVectorGetY(vLightCameraOrthographicMin),
			XMVectorGetY(vLightCameraOrthographicMax),
			fNearPlane, fFarPlane);

		m_fCascadePartitionsFrustum[iCascadeIndex] = fFrustumIntervalEnd;
	}
	m_matShadowView = m_pLightCamera->GetViewMatrix();

	//ID3D11ShaderResourceView *pnullSRV[] = { NULL };
	//_MUTEXLOCK(Engine::m_pMutex);
	//Engine::GetRender()->GetDeviceContext()->CSSetShaderResources(3, 1, pnullSRV);
	//Engine::GetRender()->GetDeviceContext()->PSSetShaderResources(9, 1, pnullSRV);
	//Engine::GetRender()->GetDeviceContext()->ClearDepthStencilView(m_pCascadedShadowMapDSV, D3D11_CLEAR_DEPTH, 1.0, 0);
	//_MUTEXUNLOCK(Engine::m_pMutex);
	//ID3D11RenderTargetView* pnullView = NULL;
	// Set a null render target so as not to render color.
	//_MUTEXLOCK(Engine::m_pMutex);
	//Engine::GetRender()->GetDeviceContext()->OMSetRenderTargets(1, &pnullView, m_pCascadedShadowMapDSV);
	//_MUTEXUNLOCK(Engine::m_pMutex);

	if (m_eSelectedNearFarFit == FIT_NEARFAR_PANCAKING)
	{
		//m_prsShadowPancake->Set();
	}
	else
	{
		//m_prsShadow->Set();
	}
	// Iterate over cascades and render shadows.
	//for (INT currentCascade = 0; currentCascade < m_CopyOfCascadeConfig.m_nCascadeLevels; ++currentCascade)
	{
		// Each cascade has its own viewport because we're storing all the cascades in one large texture.
		//_MUTEXLOCK(Engine::m_pMutex);
		//Engine::GetRender()->GetDeviceContext()->RSSetViewports(1, &m_RenderVP[currentCascade]);
		//_MUTEXUNLOCK(Engine::m_pMutex);
		XMMATRIX ÑMProjMatrices[8];

		for (auto i = 0; i < 8; i++)
		{
			ÑMProjMatrices[i] = XMMatrixTranspose(m_matShadowProj[i]);
		}


		m_pCascadesProjectionsUB->Update(&ÑMProjMatrices);

		//m_pLightCamera->SetProjection(m_matShadowProj[currentCascade]);
		m_pLightCamera->SetProjection(m_matShadowProj[0]);
		m_pLightCamera->Render(1);

		//m_BufferData.viewMatrix = XMMatrixTranspose(m_matShadowView);
		//m_BufferData.projMatrix = XMMatrixTranspose(m_matShadowProj[currentCascade]);
		//m_BufferData.viewProjMatrix = XMMatrixTranspose(m_matShadowView * m_matShadowProj[currentCascade]);
		//m_BufferData.invViewProjMatrix = XMMatrixInverse(&XMMatrixDeterminant(m_BufferData.viewProjMatrix), m_BufferData.viewProjMatrix);
		//m_BufferData.screenResolution = XMVectorSet(
		//	Engine::GetRender()->GetScreenWidth(),
		//	Engine::GetRender()->GetScreenHeight(),
		//	1.0f / Engine::GetRender()->GetScreenWidth(),
		//	1.0f / Engine::GetRender()->GetScreenHeight());
		//m_BufferData.position = m_pLightCamera->GetPosition();
		//m_BufferData.nearClipDistance = m_pLightCamera->GetScreenNearZ();
		//m_BufferData.farClipDistance = m_pLightCamera->GetScreenFarZ();
		//m_BufferData.nearFarClipDistance = m_BufferData.farClipDistance - m_BufferData.nearClipDistance;
		//m_pUniformBuffer->Update(&m_BufferData);

		//m_pUniformBuffer->Bind(CAMERA_UB_BP, VERTEX_SHADER);
		//m_pUniformBuffer->Bind(CAMERA_UB_BP, HULL_SHADER);
		//m_pUniformBuffer->Bind(CAMERA_UB_BP, DOMAIN_SHADER);
		//m_pUniformBuffer->Bind(CAMERA_UB_BP, GEOMETRY_SHADER);
		//m_pUniformBuffer->Bind(CAMERA_UB_BP, PIXEL_SHADER);

		drawCmd.camera = m_pLightCamera;
		drawCmd.numInstances = m_CascadeConfig.m_nCascadeLevels;
		drawCmd.renderTargets[0] = m_pShadowMapRT;
		drawCmd.customUBs[1] = m_pCascadesProjectionsUB;
		//drawCmd.renderTargetConfigs[0] = m_pSceneRTC;
		//drawCmd.customUBs[1] = m_pCubicViewUB;
		//Engine::m_pGameWorld->RenderWorld(CASCADED_SHADOW_RENDER, Engine::GetRender()->GetCamera());
	}

	//_MUTEXLOCK(Engine::m_pMutex);
	//Engine::GetRender()->GetDeviceContext()->RSSetState(NULL);
	//Engine::GetRender()->GetDeviceContext()->OMSetRenderTargets(1, &pnullView, NULL);
	//_MUTEXUNLOCK(Engine::m_pMutex);
}

void DirectionalLight::DrawScene(RENDER_TYPE renderType)
{
	HRESULT hr = S_OK;

	XMMATRIX dxmatCameraProj = Engine::GetRender()->GetCamera()->GetProjection();
	XMMATRIX dxmatCameraView = Engine::GetRender()->GetCamera()->GetViewMatrix();

	// The user has the option to view the ortho shadow cameras.
	if (m_eSelectedCamera >= ORTHO_CAMERA1)
	{
		// In the CAMERA_SELECTION enumeration, value 0 is EYE_CAMERA
		// value 1 is LIGHT_CAMERA and 2 to 10 are the ORTHO_CAMERA values.
		// Subtract to so that we can use the enum to index.
		dxmatCameraProj = m_matShadowProj[(int)m_eSelectedCamera - 2];
		dxmatCameraView = m_matShadowView;
	}

	XMMATRIX dxmatWorldViewProjection = dxmatCameraView * dxmatCameraProj;

	m_LightBufferData.color = m_color;
	m_LightBufferData.multiplier = 1.0f;

	m_LightBufferData.m_WorldViewProj = XMMatrixTranspose(dxmatWorldViewProjection);
	m_LightBufferData.m_WorldView = XMMatrixTranspose(dxmatCameraView);
	// These are the for loop begin end values. 
	m_LightBufferData.m_iPCFBlurForLoopEnd = m_iPCFBlurSize / 2 + 1;
	m_LightBufferData.m_iPCFBlurForLoopStart = m_iPCFBlurSize / -2;
	// This is a floating point number that is used as the percentage to blur between maps.    
	m_LightBufferData.m_fCascadeBlendArea = m_fBlurBetweenCascadesAmount;
	m_LightBufferData.m_fTexelSize = 1.0f / (float)m_CopyOfCascadeConfig.m_iBufferSize;
	m_LightBufferData.m_fNativeTexelSizeInX = m_LightBufferData.m_fTexelSize / m_CopyOfCascadeConfig.m_nCascadeLevels;
	XMMATRIX dxmatIdentity = XMMatrixIdentity();
	m_LightBufferData.m_World = XMMatrixTranspose(dxmatIdentity);
	XMMATRIX dxmatTextureScale;
	dxmatTextureScale = XMMatrixScaling(0.5f, -0.5f, 1.0f);

	XMMATRIX dxmatTextureTranslation;
	dxmatTextureTranslation = XMMatrixTranslation(.5, .5, 0);

	m_LightBufferData.m_fShadowBiasFromGUI = m_fPCFOffset;
	m_LightBufferData.m_fShadowPartitionSize = 1.0f / (float)m_CopyOfCascadeConfig.m_nCascadeLevels;

	m_LightBufferData.m_Shadow = XMMatrixTranspose(m_matShadowView);
	for (int index = 0; index < m_CopyOfCascadeConfig.m_nCascadeLevels; ++index)
	{
		XMMATRIX mShadowTexture = m_matShadowProj[index] * dxmatTextureScale * dxmatTextureTranslation;
		m_LightBufferData.m_vCascadeScale[index].m128_f32[0] = mShadowTexture.r[0].m128_f32[0];
		m_LightBufferData.m_vCascadeScale[index].m128_f32[1] = mShadowTexture.r[1].m128_f32[1];
		m_LightBufferData.m_vCascadeScale[index].m128_f32[2] = mShadowTexture.r[2].m128_f32[2];
		m_LightBufferData.m_vCascadeScale[index].m128_f32[3] = 1;

		m_LightBufferData.m_vCascadeOffset[index].m128_f32[0] = mShadowTexture.r[3].m128_f32[0];
		m_LightBufferData.m_vCascadeOffset[index].m128_f32[1] = mShadowTexture.r[3].m128_f32[1];
		m_LightBufferData.m_vCascadeOffset[index].m128_f32[2] = mShadowTexture.r[3].m128_f32[2];
		m_LightBufferData.m_vCascadeOffset[index].m128_f32[3] = 0;
	}

	// Copy intervals for the depth interval selection method.
	memcpy(m_LightBufferData.m_fCascadeFrustumsEyeSpaceDepths,
		m_fCascadePartitionsFrustum, MAX_CASCADES * 4);
	for (int index = 0; index < MAX_CASCADES; ++index)
	{
		m_LightBufferData.m_fCascadeFrustumsEyeSpaceDepthsFloat4[index].m128_f32[0] = m_fCascadePartitionsFrustum[index];
	}

	// The border padding values keep the pixel shader from reading the borders during PCF filtering.
	m_LightBufferData.m_fMaxBorderPadding = (float)(m_pCascadeConfig->m_iBufferSize - 1.0f) /
		(float)m_pCascadeConfig->m_iBufferSize;
	m_LightBufferData.m_fMinBorderPadding = (float)(1.0f) /
		(float)m_pCascadeConfig->m_iBufferSize;

	XMVECTOR ep = m_pLightCamera->GetPosition();
	XMVECTOR lp = m_pLightCamera->GetFocusPosition();
	lp -= ep;
	ep = XMVector4Normalize(-lp);

	m_LightBufferData.m_vLightDir = XMVectorSet(ep.m128_f32[0], ep.m128_f32[1], ep.m128_f32[2], 1.0f);
	m_LightBufferData.m_nCascadeLevels = m_CopyOfCascadeConfig.m_nCascadeLevels;
	m_LightBufferData.m_iVisualizeCascades = false;

	m_pUniformBuffer->Update(&m_LightBufferData);

	GpuCmd gpuCmd(DRAW_CM);
	gpuCmd.draw.camera = Engine::GetRender()->GetCamera();
	gpuCmd.draw.light = this;
	gpuCmd.draw.pipelineStateObject = m_pDirectionalLightPso;
	gpuCmd.draw.firstIndex = 0;
	gpuCmd.draw.numElements = 4;
	gpuCmd.draw.numInstances = 1;
	gpuCmd.draw.primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	gpuCmd.draw.renderTargets[0] = Engine::GetRender()->GetRenderTarget(GBUFFERS_RT_ID);
	//gpuCmd.draw.renderTargetConfigs[0] = m_pSceneRTC;
	gpuCmd.draw.textures[CUSTOM0_TEX_BP] = Engine::GetRender()->GetRenderTarget(GBUFFERS_RT_ID)->GetTexture(1);
	gpuCmd.draw.textures[CUSTOM1_TEX_BP] = Engine::GetRender()->GetRenderTarget(GBUFFERS_RT_ID)->GetTexture(2);
	gpuCmd.draw.textures[CUSTOM2_TEX_BP] = Engine::GetRender()->GetRenderTarget(GBUFFERS_RT_ID)->GetTexture(3);
	gpuCmd.draw.textures[CUSTOM3_TEX_BP] = m_pShadowMapRT->GetDepthStencilTexture();
	gpuCmd.draw.samplers[COLOR_TEX_ID] = Engine::GetRender()->GetSampler(LINEAR_SAMPLER_ID);
	gpuCmd.draw.samplers[CUSTOM0_TEX_ID] = m_pSamShadowPCF;
	Engine::GetRender()->AddGpuCmd(gpuCmd);

	switch (renderType)
	{
	case D3D11Framework::GBUFFER_FILL_RENDER:
		break;
	case D3D11Framework::CASCADED_SHADOW_RENDER:
		//m_prsScene->Set();
		//m_pSamShadowPCF->Bind(CUSTOM0_TEX_BP, PIXEL_SHADER);
		//_MUTEXLOCK(Engine::m_pMutex);
		//Engine::GetRender()->GetDeviceContext()->PSSetShader(m_pCascadedShadowShader->GetPixelShader(0), NULL, 0);
		//Engine::GetRender()->GetDeviceContext()->PSSetShaderResources(9, 1, &m_pCascadedShadowMapSRV);
		//_MUTEXUNLOCK(Engine::m_pMutex);
		//m_pUniformBuffer->Bind(LIGHT_UB_BP, VERTEX_SHADER);
		//m_pUniformBuffer->Bind(LIGHT_UB_BP, PIXEL_SHADER);
		break;
	case D3D11Framework::CUBEMAP_DEPTH_RENDER:
		break;
	case D3D11Framework::CUBEMAP_COLOR_RENDER:
		break;
	case D3D11Framework::VOXELIZE_RENDER:
		//m_pSamShadowPCF->Bind(CUSTOM0_TEX_BP, COMPUTE_SHADER);
		//_MUTEXLOCK(Engine::m_pMutex);
		//Engine::GetRender()->GetDeviceContext()->CSSetShaderResources(2, 1, &m_pCascadedShadowMapSRV);
		//_MUTEXUNLOCK(Engine::m_pMutex);
		//m_pUniformBuffer->Bind(LIGHT_UB_BP, COMPUTE_SHADER);
	default:
		break;
	}
}

//--------------------------------------------------------------------------------------
// These resources must be reallocated based on GUI control settings change.
//--------------------------------------------------------------------------------------
HRESULT DirectionalLight::DestroyAndDeallocateShadowResources()
{

	SAFE_RELEASE(m_pSamShadowPCF);
	SAFE_RELEASE(m_pCascadedShadowMapTexture);
	SAFE_RELEASE(m_pCascadedShadowMapDSV);
	SAFE_RELEASE(m_pCascadedShadowMapSRV);
	//SAFE_RELEASE(m_prsShadow);
	//SAFE_RELEASE(m_prsShadowPancake);
	//SAFE_RELEASE(m_prsScene);

	return S_OK;
}


//--------------------------------------------------------------------------------------
// These settings must be recreated based on GUI control.
//--------------------------------------------------------------------------------------
HRESULT DirectionalLight::ReleaseAndAllocateNewShadowResources()
{
	HRESULT hr = S_OK;
	// If any of these 3 paramaters was changed, we must reallocate the D3D resources.
	if (m_CopyOfCascadeConfig.m_nCascadeLevels != m_pCascadeConfig->m_nCascadeLevels
		|| m_CopyOfCascadeConfig.m_ShadowBufferFormat != m_pCascadeConfig->m_ShadowBufferFormat
		|| m_CopyOfCascadeConfig.m_iBufferSize != m_pCascadeConfig->m_iBufferSize)
	{

		m_CopyOfCascadeConfig = *m_pCascadeConfig;

		//for (INT index = 0; index < m_CopyOfCascadeConfig.m_nCascadeLevels; ++index)
		//{
		//	m_RenderVP[index].Height = (FLOAT)m_CopyOfCascadeConfig.m_iBufferSize;
		//	m_RenderVP[index].Width = (FLOAT)m_CopyOfCascadeConfig.m_iBufferSize;
		//	m_RenderVP[index].MaxDepth = 1.0f;
		//	m_RenderVP[index].MinDepth = 0.0f;
		//	m_RenderVP[index].TopLeftX = (FLOAT)(m_CopyOfCascadeConfig.m_iBufferSize * index);
		//	m_RenderVP[index].TopLeftY = 0;
		//}

		//SAFE_RELEASE(m_pCascadedShadowMapSRV);
		//SAFE_RELEASE(m_pCascadedShadowMapTexture);
		//SAFE_RELEASE(m_pCascadedShadowMapDSV);

		DXGI_FORMAT texturefmt = DXGI_FORMAT_R32_TYPELESS;
		DXGI_FORMAT SRVfmt = DXGI_FORMAT_R32_FLOAT;
		DXGI_FORMAT DSVfmt = DXGI_FORMAT_D32_FLOAT;

		switch (m_CopyOfCascadeConfig.m_ShadowBufferFormat)
		{
		case CASCADE_DXGI_FORMAT_R32_TYPELESS:
			texturefmt = DXGI_FORMAT_R32_TYPELESS;
			SRVfmt = DXGI_FORMAT_R32_FLOAT;
			DSVfmt = DXGI_FORMAT_D32_FLOAT;
			break;
		case CASCADE_DXGI_FORMAT_R24G8_TYPELESS:
			texturefmt = DXGI_FORMAT_R24G8_TYPELESS;
			SRVfmt = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			DSVfmt = DXGI_FORMAT_D24_UNORM_S8_UINT;
			break;
		case CASCADE_DXGI_FORMAT_R16_TYPELESS:
			texturefmt = DXGI_FORMAT_R16_TYPELESS;
			SRVfmt = DXGI_FORMAT_R16_UNORM;
			DSVfmt = DXGI_FORMAT_D16_UNORM;
			break;
		case CASCADE_DXGI_FORMAT_R8_TYPELESS:
			texturefmt = DXGI_FORMAT_R8_TYPELESS;
			SRVfmt = DXGI_FORMAT_R8_UNORM;
			DSVfmt = DXGI_FORMAT_R8_UNORM;
			break;
		}

		//D3D11_TEXTURE2D_DESC dtd =
		//{
		//	m_CopyOfCascadeConfig.m_iBufferSize * m_CopyOfCascadeConfig.m_nCascadeLevels,//UINT Width;
		//	m_CopyOfCascadeConfig.m_iBufferSize,//UINT Height;
		//	1,//UINT MipLevels;
		//	1,//UINT ArraySize;
		//	texturefmt,//DXGI_FORMAT Format;
		//	1,//DXGI_SAMPLE_DESC SampleDesc;
		//	0,
		//	D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
		//	D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE,//UINT BindFlags;
		//	0,//UINT CPUAccessFlags;
		//	0//UINT MiscFlags;    
		//};

		//Engine::GetRender()->GetDevice()->CreateTexture2D(&dtd, NULL, &m_pCascadedShadowMapTexture);

		//D3D11_DEPTH_STENCIL_VIEW_DESC  dsvd =
		//{
		//	DSVfmt,
		//	D3D11_DSV_DIMENSION_TEXTURE2D,
		//	0
		//};
		//Engine::GetRender()->GetDevice()->CreateDepthStencilView(m_pCascadedShadowMapTexture, &dsvd, &m_pCascadedShadowMapDSV);

		//D3D11_SHADER_RESOURCE_VIEW_DESC dsrvd =
		//{
		//	SRVfmt,
		//	D3D11_SRV_DIMENSION_TEXTURE2D,
		//	0,
		//	0
		//};
		//dsrvd.Texture2D.MipLevels = 1;

		//Engine::GetRender()->GetDevice()->CreateShaderResourceView(m_pCascadedShadowMapTexture, &dsrvd, &m_pCascadedShadowMapSRV);
	}
	return hr;

}

DECLSPEC_ALIGN(16) struct Frustumm
{
	XMFLOAT3 Origin;            // Origin of the frustum (and projection).
	XMFLOAT4 Orientation;       // Unit quaternion representing rotation.

	FLOAT RightSlope;           // Positive X slope (X/Z).
	FLOAT LeftSlope;            // Negative X slope.
	FLOAT TopSlope;             // Positive Y slope (Y/Z).
	FLOAT BottomSlope;          // Negative Y slope.
	FLOAT Near, Far;            // Z of the near plane and far plane.
};

VOID ComputeFrustumFromProjection(Frustumm* pOut, XMMATRIX* pProjection)
{
	assert(pOut);
	assert(pProjection);

	// Corners of the projection frustum in homogenous space.
	static XMVECTOR HomogenousPoints[6] =
	{
		{ 1.0f,  0.0f, 1.0f, 1.0f },   // right (at far plane)
		{ -1.0f,  0.0f, 1.0f, 1.0f },   // left
		{ 0.0f,  1.0f, 1.0f, 1.0f },   // top
		{ 0.0f, -1.0f, 1.0f, 1.0f },   // bottom

		{ 0.0f, 0.0f, 0.0f, 1.0f },     // near
		{ 0.0f, 0.0f, 1.0f, 1.0f }      // far
	};

	XMVECTOR Determinant;
	XMMATRIX matInverse = XMMatrixInverse(&Determinant, *pProjection);

	// Compute the frustum corners in world space.
	XMVECTOR Points[6];

	for (INT i = 0; i < 6; i++)
	{
		// Transform point.
		Points[i] = XMVector4Transform(HomogenousPoints[i], matInverse);
	}

	pOut->Origin = XMFLOAT3(0.0f, 0.0f, 0.0f);
	pOut->Orientation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	// Compute the slopes.
	Points[0] = Points[0] * XMVectorReciprocal(XMVectorSplatZ(Points[0]));
	Points[1] = Points[1] * XMVectorReciprocal(XMVectorSplatZ(Points[1]));
	Points[2] = Points[2] * XMVectorReciprocal(XMVectorSplatZ(Points[2]));
	Points[3] = Points[3] * XMVectorReciprocal(XMVectorSplatZ(Points[3]));

	pOut->RightSlope = XMVectorGetX(Points[0]);
	pOut->LeftSlope = XMVectorGetX(Points[1]);
	pOut->TopSlope = XMVectorGetY(Points[2]);
	pOut->BottomSlope = XMVectorGetY(Points[3]);

	// Compute near and far.
	Points[4] = Points[4] * XMVectorReciprocal(XMVectorSplatW(Points[4]));
	Points[5] = Points[5] * XMVectorReciprocal(XMVectorSplatW(Points[5]));

	pOut->Near = XMVectorGetZ(Points[4]);
	pOut->Far = XMVectorGetZ(Points[5]);

	return;
}

//--------------------------------------------------------------------------------------
// This function takes the camera's projection matrix and returns the 8
// points that make up a view frustum.
// The frustum is scaled to fit within the Begin and End interval paramaters.
//--------------------------------------------------------------------------------------
void DirectionalLight::CreateFrustumPointsFromCascadeInterval(float fCascadeIntervalBegin,
	FLOAT fCascadeIntervalEnd,
	XMMATRIX &vProjection,
	XMVECTOR* pvCornerPointsWorld)
{

	Frustumm vViewFrust;
	ComputeFrustumFromProjection(&vViewFrust, &vProjection);
	vViewFrust.Near = fCascadeIntervalBegin;
	vViewFrust.Far = fCascadeIntervalEnd;

	static const XMVECTORU32 vGrabY = { 0x00000000,0xFFFFFFFF,0x00000000,0x00000000 };
	static const XMVECTORU32 vGrabX = { 0xFFFFFFFF,0x00000000,0x00000000,0x00000000 };

	XMVECTORF32 vRightTop = { vViewFrust.RightSlope,vViewFrust.TopSlope,1.0f,1.0f };
	XMVECTORF32 vLeftBottom = { vViewFrust.LeftSlope,vViewFrust.BottomSlope,1.0f,1.0f };
	XMVECTORF32 vNear = { vViewFrust.Near,vViewFrust.Near,vViewFrust.Near,1.0f };
	XMVECTORF32 vFar = { vViewFrust.Far,vViewFrust.Far,vViewFrust.Far,1.0f };
	XMVECTOR vRightTopNear = XMVectorMultiply(vRightTop, vNear);
	XMVECTOR vRightTopFar = XMVectorMultiply(vRightTop, vFar);
	XMVECTOR vLeftBottomNear = XMVectorMultiply(vLeftBottom, vNear);
	XMVECTOR vLeftBottomFar = XMVectorMultiply(vLeftBottom, vFar);

	pvCornerPointsWorld[0] = vRightTopNear;
	pvCornerPointsWorld[1] = XMVectorSelect(vRightTopNear, vLeftBottomNear, vGrabX);
	pvCornerPointsWorld[2] = vLeftBottomNear;
	pvCornerPointsWorld[3] = XMVectorSelect(vRightTopNear, vLeftBottomNear, vGrabY);

	pvCornerPointsWorld[4] = vRightTopFar;
	pvCornerPointsWorld[5] = XMVectorSelect(vRightTopFar, vLeftBottomFar, vGrabX);
	pvCornerPointsWorld[6] = vLeftBottomFar;
	pvCornerPointsWorld[7] = XMVectorSelect(vRightTopFar, vLeftBottomFar, vGrabY);

}

//--------------------------------------------------------------------------------------
// Used to compute an intersection of the orthographic projection and the Scene AABB
//--------------------------------------------------------------------------------------
struct Triangle
{
	XMVECTOR pt[3];
	BOOL culled;
};

//--------------------------------------------------------------------------------------
// Computing an accurate near and flar plane will decrease surface acne and Peter-panning.
// Surface acne is the term for erroneous self shadowing.  Peter-panning is the effect where
// shadows disappear near the base of an object.
// As offsets are generally used with PCF filtering due self shadowing issues, computing the
// correct near and far planes becomes even more important.
// This concept is not complicated, but the intersection code is.
//--------------------------------------------------------------------------------------
void DirectionalLight::ComputeNearAndFar(FLOAT& fNearPlane,
	FLOAT& fFarPlane,
	FXMVECTOR vLightCameraOrthographicMin,
	FXMVECTOR vLightCameraOrthographicMax,
	XMVECTOR* pvPointsInCameraView)
{

	// Initialize the near and far planes
	fNearPlane = FLT_MAX;
	fFarPlane = -FLT_MAX;

	Triangle triangleList[16];
	INT iTriangleCnt = 1;

	triangleList[0].pt[0] = pvPointsInCameraView[0];
	triangleList[0].pt[1] = pvPointsInCameraView[1];
	triangleList[0].pt[2] = pvPointsInCameraView[2];
	triangleList[0].culled = false;

	// These are the indices used to tesselate an AABB into a list of triangles.
	static const INT iAABBTriIndexes[] =
	{
		0,1,2,  1,2,3,
		4,5,6,  5,6,7,
		0,2,4,  2,4,6,
		1,3,5,  3,5,7,
		0,1,4,  1,4,5,
		2,3,6,  3,6,7
	};

	INT iPointPassesCollision[3];

	// At a high level: 
	// 1. Iterate over all 12 triangles of the AABB.  
	// 2. Clip the triangles against each plane. Create new triangles as needed.
	// 3. Find the min and max z values as the near and far plane.

	//This is easier because the triangles are in camera spacing making the collisions tests simple comparisions.

	float fLightCameraOrthographicMinX = XMVectorGetX(vLightCameraOrthographicMin);
	float fLightCameraOrthographicMaxX = XMVectorGetX(vLightCameraOrthographicMax);
	float fLightCameraOrthographicMinY = XMVectorGetY(vLightCameraOrthographicMin);
	float fLightCameraOrthographicMaxY = XMVectorGetY(vLightCameraOrthographicMax);

	for (INT AABBTriIter = 0; AABBTriIter < 12; ++AABBTriIter)
	{

		triangleList[0].pt[0] = pvPointsInCameraView[iAABBTriIndexes[AABBTriIter * 3 + 0]];
		triangleList[0].pt[1] = pvPointsInCameraView[iAABBTriIndexes[AABBTriIter * 3 + 1]];
		triangleList[0].pt[2] = pvPointsInCameraView[iAABBTriIndexes[AABBTriIter * 3 + 2]];
		iTriangleCnt = 1;
		triangleList[0].culled = FALSE;

		// Clip each invidual triangle against the 4 frustums.  When ever a triangle is clipped into new triangles, 
		//add them to the list.
		for (INT frustumPlaneIter = 0; frustumPlaneIter < 4; ++frustumPlaneIter)
		{

			FLOAT fEdge;
			INT iComponent;

			if (frustumPlaneIter == 0)
			{
				fEdge = fLightCameraOrthographicMinX; // todo make float temp
				iComponent = 0;
			}
			else if (frustumPlaneIter == 1)
			{
				fEdge = fLightCameraOrthographicMaxX;
				iComponent = 0;
			}
			else if (frustumPlaneIter == 2)
			{
				fEdge = fLightCameraOrthographicMinY;
				iComponent = 1;
			}
			else
			{
				fEdge = fLightCameraOrthographicMaxY;
				iComponent = 1;
			}

			for (INT triIter = 0; triIter < iTriangleCnt; ++triIter)
			{
				// We don't delete triangles, so we skip those that have been culled.
				if (!triangleList[triIter].culled)
				{
					INT iInsideVertCount = 0;
					XMVECTOR tempOrder;
					// Test against the correct frustum plane.
					// This could be written more compactly, but it would be harder to understand.

					if (frustumPlaneIter == 0)
					{
						for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
						{
							if (XMVectorGetX(triangleList[triIter].pt[triPtIter]) >
								XMVectorGetX(vLightCameraOrthographicMin))
							{
								iPointPassesCollision[triPtIter] = 1;
							}
							else
							{
								iPointPassesCollision[triPtIter] = 0;
							}
							iInsideVertCount += iPointPassesCollision[triPtIter];
						}
					}
					else if (frustumPlaneIter == 1)
					{
						for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
						{
							if (XMVectorGetX(triangleList[triIter].pt[triPtIter]) <
								XMVectorGetX(vLightCameraOrthographicMax))
							{
								iPointPassesCollision[triPtIter] = 1;
							}
							else
							{
								iPointPassesCollision[triPtIter] = 0;
							}
							iInsideVertCount += iPointPassesCollision[triPtIter];
						}
					}
					else if (frustumPlaneIter == 2)
					{
						for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
						{
							if (XMVectorGetY(triangleList[triIter].pt[triPtIter]) >
								XMVectorGetY(vLightCameraOrthographicMin))
							{
								iPointPassesCollision[triPtIter] = 1;
							}
							else
							{
								iPointPassesCollision[triPtIter] = 0;
							}
							iInsideVertCount += iPointPassesCollision[triPtIter];
						}
					}
					else
					{
						for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
						{
							if (XMVectorGetY(triangleList[triIter].pt[triPtIter]) <
								XMVectorGetY(vLightCameraOrthographicMax))
							{
								iPointPassesCollision[triPtIter] = 1;
							}
							else
							{
								iPointPassesCollision[triPtIter] = 0;
							}
							iInsideVertCount += iPointPassesCollision[triPtIter];
						}
					}

					// Move the points that pass the frustum test to the begining of the array.
					if (iPointPassesCollision[1] && !iPointPassesCollision[0])
					{
						tempOrder = triangleList[triIter].pt[0];
						triangleList[triIter].pt[0] = triangleList[triIter].pt[1];
						triangleList[triIter].pt[1] = tempOrder;
						iPointPassesCollision[0] = TRUE;
						iPointPassesCollision[1] = FALSE;
					}
					if (iPointPassesCollision[2] && !iPointPassesCollision[1])
					{
						tempOrder = triangleList[triIter].pt[1];
						triangleList[triIter].pt[1] = triangleList[triIter].pt[2];
						triangleList[triIter].pt[2] = tempOrder;
						iPointPassesCollision[1] = TRUE;
						iPointPassesCollision[2] = FALSE;
					}
					if (iPointPassesCollision[1] && !iPointPassesCollision[0])
					{
						tempOrder = triangleList[triIter].pt[0];
						triangleList[triIter].pt[0] = triangleList[triIter].pt[1];
						triangleList[triIter].pt[1] = tempOrder;
						iPointPassesCollision[0] = TRUE;
						iPointPassesCollision[1] = FALSE;
					}

					if (iInsideVertCount == 0)
					{ // All points failed. We're done,  
						triangleList[triIter].culled = true;
					}
					else if (iInsideVertCount == 1)
					{// One point passed. Clip the triangle against the Frustum plane
						triangleList[triIter].culled = false;

						// 
						XMVECTOR vVert0ToVert1 = triangleList[triIter].pt[1] - triangleList[triIter].pt[0];
						XMVECTOR vVert0ToVert2 = triangleList[triIter].pt[2] - triangleList[triIter].pt[0];

						// Find the collision ratio.
						FLOAT fHitPointTimeRatio = fEdge - XMVectorGetByIndex(triangleList[triIter].pt[0], iComponent);
						// Calculate the distance along the vector as ratio of the hit ratio to the component.
						FLOAT fDistanceAlongVector01 = fHitPointTimeRatio / XMVectorGetByIndex(vVert0ToVert1, iComponent);
						FLOAT fDistanceAlongVector02 = fHitPointTimeRatio / XMVectorGetByIndex(vVert0ToVert2, iComponent);
						// Add the point plus a percentage of the vector.
						vVert0ToVert1 *= fDistanceAlongVector01;
						vVert0ToVert1 += triangleList[triIter].pt[0];
						vVert0ToVert2 *= fDistanceAlongVector02;
						vVert0ToVert2 += triangleList[triIter].pt[0];

						triangleList[triIter].pt[1] = vVert0ToVert2;
						triangleList[triIter].pt[2] = vVert0ToVert1;

					}
					else if (iInsideVertCount == 2)
					{ // 2 in  // tesselate into 2 triangles


					  // Copy the triangle\(if it exists) after the current triangle out of
					  // the way so we can override it with the new triangle we're inserting.
						triangleList[iTriangleCnt] = triangleList[triIter + 1];

						triangleList[triIter].culled = false;
						triangleList[triIter + 1].culled = false;

						// Get the vector from the outside point into the 2 inside points.
						XMVECTOR vVert2ToVert0 = triangleList[triIter].pt[0] - triangleList[triIter].pt[2];
						XMVECTOR vVert2ToVert1 = triangleList[triIter].pt[1] - triangleList[triIter].pt[2];

						// Get the hit point ratio.
						FLOAT fHitPointTime_2_0 = fEdge - XMVectorGetByIndex(triangleList[triIter].pt[2], iComponent);
						FLOAT fDistanceAlongVector_2_0 = fHitPointTime_2_0 / XMVectorGetByIndex(vVert2ToVert0, iComponent);
						// Calcaulte the new vert by adding the percentage of the vector plus point 2.
						vVert2ToVert0 *= fDistanceAlongVector_2_0;
						vVert2ToVert0 += triangleList[triIter].pt[2];

						// Add a new triangle.
						triangleList[triIter + 1].pt[0] = triangleList[triIter].pt[0];
						triangleList[triIter + 1].pt[1] = triangleList[triIter].pt[1];
						triangleList[triIter + 1].pt[2] = vVert2ToVert0;

						//Get the hit point ratio.
						FLOAT fHitPointTime_2_1 = fEdge - XMVectorGetByIndex(triangleList[triIter].pt[2], iComponent);
						FLOAT fDistanceAlongVector_2_1 = fHitPointTime_2_1 / XMVectorGetByIndex(vVert2ToVert1, iComponent);
						vVert2ToVert1 *= fDistanceAlongVector_2_1;
						vVert2ToVert1 += triangleList[triIter].pt[2];
						triangleList[triIter].pt[0] = triangleList[triIter + 1].pt[1];
						triangleList[triIter].pt[1] = triangleList[triIter + 1].pt[2];
						triangleList[triIter].pt[2] = vVert2ToVert1;
						// Cncrement triangle count and skip the triangle we just inserted.
						++iTriangleCnt;
						++triIter;


					}
					else
					{ // all in
						triangleList[triIter].culled = false;

					}
				}// end if !culled loop            
			}
		}
		for (INT index = 0; index < iTriangleCnt; ++index)
		{
			if (!triangleList[index].culled)
			{
				// Set the near and far plan and the min and max z values respectivly.
				for (int vertind = 0; vertind < 3; ++vertind)
				{
					float fTriangleCoordZ = XMVectorGetZ(triangleList[index].pt[vertind]);
					if (fNearPlane > fTriangleCoordZ)
					{
						fNearPlane = fTriangleCoordZ;
					}
					if (fFarPlane < fTriangleCoordZ)
					{
						fFarPlane = fTriangleCoordZ;
					}
				}
			}
		}
	}

}


//--------------------------------------------------------------------------------------
// This function converts the "center, extents" version of an AABB into 8 points.
//--------------------------------------------------------------------------------------
void DirectionalLight::CreateAABBPoints(XMVECTOR* vAABBPoints, FXMVECTOR vCenter, FXMVECTOR vExtents)
{
	//This map enables us to use a for loop and do vector math.
	static const XMVECTORF32 vExtentsMap[] =
	{
		{ 1.0f, 1.0f, -1.0f, 1.0f },
		{ -1.0f, 1.0f, -1.0f, 1.0f },
		{ 1.0f, -1.0f, -1.0f, 1.0f },
		{ -1.0f, -1.0f, -1.0f, 1.0f },
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		{ -1.0f, 1.0f, 1.0f, 1.0f },
		{ 1.0f, -1.0f, 1.0f, 1.0f },
		{ -1.0f, -1.0f, 1.0f, 1.0f }
	};

	for (INT index = 0; index < 8; ++index)
	{
		vAABBPoints[index] = XMVectorMultiplyAdd(vExtentsMap[index], vExtents, vCenter);
	}

}