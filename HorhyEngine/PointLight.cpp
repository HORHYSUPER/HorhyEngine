#include "stdafx.h"
#include "Render.h"
#include "GameWorld.h"
#include "Camera.h"
#include "DX12_UniformBuffer.h"
#include "DX12_RenderTarget.h"
#include "DX12_RenderTargetConfig.h"
#include "DX12_Shader.h"
#include "DX12_VertexLayout.h"
#include "PipelineStateObject.h"
#include "GpuCmd.h"
#include "PointLight.h"

using namespace D3D11Framework;

PointLight::~PointLight()
{
}

bool PointLight::Init()
{
	lightType = 1;

	m_pLightCamera->SetProjection(XMMatrixPerspectiveFovLH(0.5f * 3.141592654f, 1.0f, 0.01f, 10000.0f));
	m_pLightCamera->SetScreenWidth(1024);
	m_pLightCamera->SetScreenHeight(1024);
	m_pLightCamera->SetScreenNearZ(0.0f);
	m_pLightCamera->SetScreenFarZ(1.0f);
	m_pLightCamera->Render(1);

	RenderTargetDesc rtDesc;
	rtDesc.width = 1024;
	rtDesc.height = 1024;
	rtDesc.depth = 6;
	rtDesc.sampleCountMSAA = MSAA_COUNT;
	rtDesc.colorBufferDescs[0].format = DXGI_FORMAT_R32_FLOAT;
	rtDesc.colorBufferDescs[0].rtFlags = TEXTURE_CUBE_RTF;
	rtDesc.depthStencilBufferDesc.format = DXGI_FORMAT_D32_FLOAT;
	rtDesc.depthStencilBufferDesc.rtFlags = TEXTURE_CUBE_RTF;
	m_pShadowMapRT = Engine::GetRender()->CreateRenderTarget(rtDesc);
	if (!m_pShadowMapRT)
		return false;

	RtConfigDesc rtcDesc;
	rtcDesc.numColorBuffers = 1;
	rtcDesc.flags = DS_READ_ONLY_RTCF;
	m_pSceneRTC = Engine::GetRender()->CreateRenderTargetConfig(rtcDesc);
	if (!m_pSceneRTC)
		return false;

	m_pUniformBuffer = Engine::GetRender()->CreateUniformBuffer(sizeof(BufferData));
	if (!m_pUniformBuffer)
		return false;

	m_pCubicViewUB = Engine::GetRender()->CreateUniformBuffer(sizeof(XMMATRIX) * 6);
	if (!m_pCubicViewUB)
		return false;

	// Full Screen Quad initialization
	{
		string msaa = to_string(static_cast<int>(MSAA_COUNT));

		D3D_SHADER_MACRO defines[] =
		{
			"MSAA", msaa.c_str(),
			NULL, NULL
		};
		DX12_Shader *fullScreenQuadShader = new DX12_Shader();
		fullScreenQuadShader->Load("CubicShadow.sdr", defines);

		DX12_VertexLayout *vertexLayout = new DX12_VertexLayout();
		VertexElementDesc vertexElementDescs[] = { POSITION_ELEMENT, 0, R32G32B32_FLOAT_EF, 0,
			TEXCOORDS_ELEMENT, 0, R32G32_FLOAT_EF, D3D12_APPEND_ALIGNED_ELEMENT,
		};
		vertexLayout->Create(vertexElementDescs, _countof(vertexElementDescs));
		
		PIPELINE_STATE_DESC pointLightPsoDesc;
		pointLightPsoDesc.shader = fullScreenQuadShader;
		pointLightPsoDesc.vertexLayout = vertexLayout;
		pointLightPsoDesc.renderTartet = Engine::GetRender()->GetRenderTarget(GBUFFERS_RT_ID);

		m_pPointLightPso = Engine::GetRender()->CreatePipelineStateObject(pointLightPsoDesc);
	}

	return true;
}

void PointLight::DrawShadowSurface(DrawCmd &drawCmd)
{
	XMMATRIX cubeMapViewMatrices[6];
	XMVECTOR vPos;
	XMVECTOR vLookDir;
	XMVECTOR vUpDir;

	vPos = m_pLightCamera->GetPosition();
	vLookDir = XMVectorSet(vPos.m128_f32[0] + 1.0f, vPos.m128_f32[1], vPos.m128_f32[2], 0.0f);
	vUpDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	cubeMapViewMatrices[0] = XMMatrixTranspose(XMMatrixLookAtLH(vPos, vLookDir, vUpDir));

	vLookDir = XMVectorSet(vPos.m128_f32[0] - 1.0f, vPos.m128_f32[1], vPos.m128_f32[2], 0.0f);
	vUpDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	cubeMapViewMatrices[1] = XMMatrixTranspose(XMMatrixLookAtLH(vPos, vLookDir, vUpDir));

	vLookDir = XMVectorSet(vPos.m128_f32[0], vPos.m128_f32[1] + 1.0f, vPos.m128_f32[2], 0.0f);
	vUpDir = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
	cubeMapViewMatrices[2] = XMMatrixTranspose(XMMatrixLookAtLH(vPos, vLookDir, vUpDir));

	vLookDir = XMVectorSet(vPos.m128_f32[0], vPos.m128_f32[1] - 1.0f, vPos.m128_f32[2], 0.0f);
	vUpDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	cubeMapViewMatrices[3] = XMMatrixTranspose(XMMatrixLookAtLH(vPos, vLookDir, vUpDir));

	vLookDir = XMVectorSet(vPos.m128_f32[0], vPos.m128_f32[1], vPos.m128_f32[2] + 1.0f, 0.0f);
	vUpDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	cubeMapViewMatrices[4] = XMMatrixTranspose(XMMatrixLookAtLH(vPos, vLookDir, vUpDir));

	vLookDir = XMVectorSet(vPos.m128_f32[0], vPos.m128_f32[1], vPos.m128_f32[2] - 1.0f, 0.0f);
	vUpDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	cubeMapViewMatrices[5] = XMMatrixTranspose(XMMatrixLookAtLH(vPos, vLookDir, vUpDir));

	m_pCubicViewUB->Update(&cubeMapViewMatrices);

	drawCmd.camera = m_pLightCamera;
	drawCmd.numInstances = 6;
	drawCmd.renderTargets[0] = m_pShadowMapRT;
	drawCmd.customUBs[1] = m_pCubicViewUB;
}

void PointLight::DrawScene(RENDER_TYPE renderType)
{
	bufferData.position = XMFLOAT3(m_pLightCamera->GetPosition().m128_f32[0], m_pLightCamera->GetPosition().m128_f32[1], m_pLightCamera->GetPosition().m128_f32[2]);
	bufferData.radius = 30;
	bufferData.color = m_color;
	bufferData.multiplier = 1.0f;
	bufferData.worldMatrix = XMMatrixIdentity();
	m_pUniformBuffer->Update(&bufferData);

	GpuCmd gpuCmd(DRAW_CM);
	gpuCmd.draw.camera = Engine::GetRender()->GetCamera();
	gpuCmd.draw.light = this;
	gpuCmd.draw.pipelineStateObject = m_pPointLightPso;
	gpuCmd.draw.firstIndex = 0;
	gpuCmd.draw.numElements = 4;
	gpuCmd.draw.numInstances = 1;
	gpuCmd.draw.primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	gpuCmd.draw.renderTargets[0] = Engine::GetRender()->GetRenderTarget(GBUFFERS_RT_ID);
	gpuCmd.draw.renderTargetConfigs[0] = m_pSceneRTC;
	gpuCmd.draw.textures[CUSTOM0_TEX_BP] = Engine::GetRender()->GetRenderTarget(GBUFFERS_RT_ID)->GetTexture(1);
	gpuCmd.draw.textures[CUSTOM1_TEX_BP] = Engine::GetRender()->GetRenderTarget(GBUFFERS_RT_ID)->GetTexture(2);
	gpuCmd.draw.textures[CUSTOM2_TEX_BP] = Engine::GetRender()->GetRenderTarget(GBUFFERS_RT_ID)->GetTexture(3);
	gpuCmd.draw.textures[CUSTOM3_TEX_BP] = m_pShadowMapRT->GetTexture();
	gpuCmd.draw.samplers[COLOR_TEX_ID] = Engine::GetRender()->GetSampler(LINEAR_SAMPLER_ID);
	gpuCmd.draw.samplers[CUSTOM0_TEX_ID] = Engine::GetRender()->GetSampler(SHADOW_MAP_SAMPLER_ID);
	Engine::GetRender()->AddGpuCmd(gpuCmd);
}