#include "stdafx.h"
#include "Render.h"
#include "Camera.h"
#include "Buffer.h"
#include "GameWorld.h"
#include "macros.h"
#include "DX12_Sampler.h"
#include "VoxelGlobalIllumination.h"

using namespace D3D11Framework;

VoxelGlobalIllumination::VoxelGlobalIllumination()
{
	inputLightRTIndices = 0;
	outputLightRTIndices = 1;
	lastInputLightRTIndices = 2;
	m_pVoxelShader = NULL;
}

VoxelGlobalIllumination::~VoxelGlobalIllumination()
{
	SAFE_RELEASE(m_pVoxelGridColorTexture);
	SAFE_RELEASE(m_pVoxelGridColorIlluminatedTexture[0]);
	SAFE_RELEASE(m_pVoxelGridColorIlluminatedTexture[1]);
	SAFE_RELEASE(m_pVoxelGridNormalTexture);
	SAFE_RELEASE(m_pVoxelGridColorSRV);
	SAFE_RELEASE(m_pVoxelGridColorIlluminatedSRV[0]);
	SAFE_RELEASE(m_pVoxelGridColorIlluminatedSRV[1]);
	SAFE_RELEASE(m_pVoxelGridNormalSRV);
	SAFE_RELEASE(m_pVoxelGridColorUAV);
	SAFE_RELEASE(m_pVoxelGridColorIlluminatedUAV[0]);
	SAFE_RELEASE(m_pVoxelGridColorIlluminatedUAV[1]);
	SAFE_RELEASE(m_pVoxelGridNormalUAV);
	SAFE_RELEASE(m_pVoxelizationBuffer);
	SAFE_RELEASE(m_pVoxelSampler);
	//SAFE_CLOSE(m_pVoxelShader);
}

bool VoxelGlobalIllumination::Init(float voxelizeDimension, unsigned short voxelizeSize)
{
	HRESULT hr;
	m_voxelizeDimension = voxelizeDimension;
	m_voxelizeSize = voxelizeSize;

	//Create 3dtexture
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MipLevels = 7;
		srvDesc.Texture3D.MostDetailedMip = 0;

		D3D11_TEXTURE3D_DESC textureDesc;
		ZeroMemory(&textureDesc, sizeof(textureDesc));
		textureDesc.Width = m_voxelizeDimension;
		textureDesc.Height = m_voxelizeDimension;
		textureDesc.Depth = m_voxelizeDimension;
		textureDesc.MipLevels = 7;
		textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		textureDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		textureDesc.CPUAccessFlags = 0;
		textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

		hr = Engine::GetRender()->GetDevice()->CreateTexture3D(&textureDesc, NULL, (ID3D11Texture3D**)&m_pVoxelGridColorIlluminatedTexture[0]);
		if (FAILED(hr))
			return false;
		hr = Engine::GetRender()->GetDevice()->CreateShaderResourceView(m_pVoxelGridColorIlluminatedTexture[0], &srvDesc, &m_pVoxelGridColorIlluminatedSRV[0]);
		if (FAILED(hr))
			return false;
		hr = Engine::GetRender()->GetDevice()->CreateUnorderedAccessView(m_pVoxelGridColorIlluminatedTexture[0], NULL, &m_pVoxelGridColorIlluminatedUAV[0]);
		if (FAILED(hr))
			return false;

		hr = Engine::GetRender()->GetDevice()->CreateTexture3D(&textureDesc, NULL, (ID3D11Texture3D**)&m_pVoxelGridColorIlluminatedTexture[1]);
		if (FAILED(hr))
			return false;
		hr = Engine::GetRender()->GetDevice()->CreateShaderResourceView(m_pVoxelGridColorIlluminatedTexture[1], &srvDesc, &m_pVoxelGridColorIlluminatedSRV[1]);
		if (FAILED(hr))
			return false;
		hr = Engine::GetRender()->GetDevice()->CreateUnorderedAccessView(m_pVoxelGridColorIlluminatedTexture[1], NULL, &m_pVoxelGridColorIlluminatedUAV[1]);
		if (FAILED(hr))
			return false;
		
		SamplerDesc samplerDesc;
		//samplerDesc.filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		m_pVoxelSampler = Engine::GetRender()->CreateSampler(samplerDesc);
		if (!m_pVoxelSampler)
			return false;
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MipLevels = 1;
		srvDesc.Texture3D.MostDetailedMip = 0;

		D3D11_TEXTURE3D_DESC textureDesc;
		ZeroMemory(&textureDesc, sizeof(textureDesc));
		textureDesc.Width = m_voxelizeDimension;
		textureDesc.Height = m_voxelizeDimension;
		textureDesc.Depth = m_voxelizeDimension;
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		textureDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		textureDesc.CPUAccessFlags = 0;
		textureDesc.MiscFlags = 0;

		hr = Engine::GetRender()->GetDevice()->CreateTexture3D(&textureDesc, NULL, (ID3D11Texture3D**)&m_pVoxelGridColorTexture);
		if (FAILED(hr))
			return false;
		hr = Engine::GetRender()->GetDevice()->CreateShaderResourceView(m_pVoxelGridColorTexture, &srvDesc, &m_pVoxelGridColorSRV);
		if (FAILED(hr))
			return false;
		hr = Engine::GetRender()->GetDevice()->CreateUnorderedAccessView(m_pVoxelGridColorTexture, NULL, &m_pVoxelGridColorUAV);
		if (FAILED(hr))
			return false;

		hr = Engine::GetRender()->GetDevice()->CreateTexture3D(&textureDesc, NULL, (ID3D11Texture3D**)&m_pVoxelGridNormalTexture);
		if (FAILED(hr))
			return false;
		hr = Engine::GetRender()->GetDevice()->CreateShaderResourceView(m_pVoxelGridNormalTexture, &srvDesc, &m_pVoxelGridNormalSRV);
		if (FAILED(hr))
			return false;
		hr = Engine::GetRender()->GetDevice()->CreateUnorderedAccessView(m_pVoxelGridNormalTexture, NULL, &m_pVoxelGridNormalUAV);
		if (FAILED(hr))
			return false;
	}

	m_pVoxelizationBuffer = Buffer::CreateConstantBuffer(Engine::GetRender()->GetDevice(), sizeof(VoxelizationBuffer), false, NULL);
	if (!m_pVoxelizationBuffer)
		return false;

	//m_pVoxelShader = new Shader();
	//if (!m_pVoxelShader)
	//	return false;

	//std::wstring shaderName;
	//shaderName = Engine::m_pPaths[PATH_TO_SHADERS] + L"gridClear.csh";
	//if (!m_pVoxelShader->CreateShaderCs((WCHAR*)shaderName.c_str(), NULL, "main"))
	//	return false;

	//shaderName = Engine::m_pPaths[PATH_TO_SHADERS] + L"CascadedShadowVoxel.csh";
	//if (!m_pVoxelShader->CreateShaderCs((WCHAR*)shaderName.c_str(), NULL, "main"))
	//	return false;

	//shaderName = Engine::m_pPaths[PATH_TO_SHADERS] + L"CubicShadowVoxel.csh";
	//if (!m_pVoxelShader->CreateShaderCs((WCHAR*)shaderName.c_str(), NULL, "main"))
	//	return false;

	//shaderName = Engine::m_pPaths[PATH_TO_SHADERS] + L"ColoringVoxel.csh";
	//if (!m_pVoxelShader->CreateShaderCs((WCHAR*)shaderName.c_str(), NULL, "main"))
	//	return false;

	//shaderName = Engine::m_pPaths[PATH_TO_SHADERS] + L"LightPropagateVoxel.csh";
	//if (!m_pVoxelShader->CreateShaderCs((WCHAR*)shaderName.c_str(), NULL, "main"))
	//	return false;

	return true;
}

void VoxelGlobalIllumination::PerformVoxelGlobalIllumination()
{
	PerformClearPass();
	PerformVoxelisationPass();
}

void VoxelGlobalIllumination::SwapIndices(unsigned int &indexA, unsigned int &indexB) const
{
	unsigned int tmpIndex = indexA;
	indexA = indexB;
	indexB = tmpIndex;
}

void VoxelGlobalIllumination::PerformVoxelisationPass()
{
	ID3D11UnorderedAccessView* cs_uavs[] = { NULL, NULL };
	ID3D11SamplerState *lSamplers[3] = { NULL, NULL, NULL };
	VoxelizationBuffer vCB;
	vCB.voxelizeSize = m_voxelizeSize;
	vCB.voxelSize = m_voxelizeSize / m_voxelizeDimension;
	vCB.voxelizeDimension = m_voxelizeDimension;
	int x, y, z, voxelSize;
	voxelSize = m_voxelizeSize / m_voxelizeDimension;
	XMVECTOR dir = XMVector4Normalize(Engine::GetRender()->GetCamera()->GetFocusPosition() - Engine::GetRender()->GetCamera()->GetPosition());
	x = Engine::GetRender()->GetCamera()->GetPosition().m128_f32[0]/* + (dir.m128_f32[0] * m_voxelizeSize * 0.25)*/;
	y = Engine::GetRender()->GetCamera()->GetPosition().m128_f32[1]/* + (dir.m128_f32[1] * m_voxelizeSize * 0.25)*/;
	z = Engine::GetRender()->GetCamera()->GetPosition().m128_f32[2]/* + (dir.m128_f32[2] * m_voxelizeSize * 0.25)*/;
	//x = x / voxelSize * voxelSize;
	//y = y / voxelSize * voxelSize;
	//z = z / voxelSize * voxelSize;

	vCB.voxelizePosition = XMVectorSet(x - 0, y - 0, z - 0, 0);

	_MUTEXLOCK(Engine::m_pMutex);
	Engine::GetRender()->GetDeviceContext()->UpdateSubresource(m_pVoxelizationBuffer, 0, NULL, &vCB, 0, 0);
	_MUTEXUNLOCK(Engine::m_pMutex);

	D3D11_VIEWPORT old_viewport;
	UINT num_viewport = 1;
	_MUTEXLOCK(Engine::m_pMutex);
	Engine::GetRender()->GetDeviceContext()->RSGetViewports(&num_viewport, &old_viewport);
	_MUTEXUNLOCK(Engine::m_pMutex);

	D3D11_VIEWPORT new_vp = { 0, 0, (float)m_voxelizeDimension, (float)m_voxelizeDimension, 0.0f, 1.0f };

	cs_uavs[0] = { m_pVoxelGridColorUAV };
	cs_uavs[1] = { m_pVoxelGridNormalUAV };

	_MUTEXLOCK(Engine::m_pMutex);
	Engine::GetRender()->GetDeviceContext()->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 0, 2, cs_uavs, (UINT*)(&cs_uavs[0]));
	Engine::GetRender()->GetDeviceContext()->RSSetViewports(1, &new_vp);
	Engine::GetRender()->GetDeviceContext()->GSSetConstantBuffers(CUSTOM1_UB_BP, 1, &m_pVoxelizationBuffer);
	Engine::GetRender()->GetDeviceContext()->PSSetConstantBuffers(CUSTOM1_UB_BP, 1, &m_pVoxelizationBuffer);
	_MUTEXUNLOCK(Engine::m_pMutex);

	Engine::m_pGameWorld->RenderWorld(VOXELIZE_RENDER, Engine::GetRender()->GetCamera());

	cs_uavs[0] = { NULL };
	cs_uavs[1] = { NULL };

	_MUTEXLOCK(Engine::m_pMutex);
	Engine::GetRender()->GetDeviceContext()->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 0, 2, cs_uavs, (UINT*)(&cs_uavs[0]));
	Engine::GetRender()->GetDeviceContext()->RSSetViewports(num_viewport, &old_viewport);
	_MUTEXUNLOCK(Engine::m_pMutex);
}

void VoxelGlobalIllumination::PerformGridLightingPass(int lightType)
{
	ID3D11UnorderedAccessView* cs_uavs[] = { NULL };
	ID3D11ShaderResourceView *pDepthShaderResouceView[] = { NULL, NULL, NULL };
	VoxelizationBuffer vCB;

	// Shaded of cascaded light---------------
	cs_uavs[0] = { m_pVoxelGridColorIlluminatedUAV[outputLightRTIndices] };

	pDepthShaderResouceView[0] = m_pVoxelGridColorIlluminatedSRV[inputLightRTIndices];
	pDepthShaderResouceView[1] = m_pVoxelGridNormalSRV;

	switch (lightType)
	{
	case 0:
		_MUTEXLOCK(Engine::m_pMutex);
		//Engine::GetRender()->GetDeviceContext()->CSSetShader(m_pVoxelShader->GetComputeShader(1), NULL, 0);
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;
	case 1:
		//m_pVoxelSampler->Bind(CUSTOM0_TEX_BP, COMPUTE_SHADER);
		_MUTEXLOCK(Engine::m_pMutex);
		//Engine::GetRender()->GetDeviceContext()->CSSetShader(m_pVoxelShader->GetComputeShader(2), NULL, 0);
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;
	default:
		break;
	}
	_MUTEXLOCK(Engine::m_pMutex);
	Engine::GetRender()->GetDeviceContext()->CSSetUnorderedAccessViews(0, 1, cs_uavs, (UINT*)(&cs_uavs[0]));
	Engine::GetRender()->GetDeviceContext()->CSSetShaderResources(0, 2, pDepthShaderResouceView);
	Engine::GetRender()->GetDeviceContext()->Dispatch(m_voxelizeDimension / 8, m_voxelizeDimension / 8, m_voxelizeDimension / 8);
	_MUTEXUNLOCK(Engine::m_pMutex);

	cs_uavs[0] = { NULL };
	pDepthShaderResouceView[0] = NULL;
	pDepthShaderResouceView[1] = NULL;
	pDepthShaderResouceView[2] = NULL;

	_MUTEXLOCK(Engine::m_pMutex);
	Engine::GetRender()->GetDeviceContext()->CSSetUnorderedAccessViews(0, 1, cs_uavs, (UINT*)(&cs_uavs[0]));
	Engine::GetRender()->GetDeviceContext()->CSSetShaderResources(0, 3, pDepthShaderResouceView);
	//Framework::GetRender()->GetDeviceContext()->CSSetSamplers(0, 1, { NULL });
	_MUTEXUNLOCK(Engine::m_pMutex);

	SwapIndices(inputLightRTIndices, outputLightRTIndices);
}

void VoxelGlobalIllumination::PerformLightPropagatePass()
{
	ID3D11UnorderedAccessView* cs_uavs[] = { NULL, NULL, NULL, NULL };
	ID3D11ShaderResourceView *pDepthShaderResouceView[] = { NULL, NULL, NULL, NULL };
	VoxelizationBuffer vCB;

	// Coloring voxel---------------
	cs_uavs[0] = { m_pVoxelGridColorIlluminatedUAV[outputLightRTIndices] };

	pDepthShaderResouceView[0] = m_pVoxelGridColorIlluminatedSRV[inputLightRTIndices];
	pDepthShaderResouceView[1] = m_pVoxelGridColorSRV;

	_MUTEXLOCK(Engine::m_pMutex);
	//Engine::GetRender()->GetDeviceContext()->CSSetShader(m_pVoxelShader->GetComputeShader(3), NULL, 0);
	Engine::GetRender()->GetDeviceContext()->CSSetUnorderedAccessViews(0, 1, cs_uavs, (UINT*)(&cs_uavs[0]));
	Engine::GetRender()->GetDeviceContext()->CSSetShaderResources(0, 2, pDepthShaderResouceView);
	Engine::GetRender()->GetDeviceContext()->Dispatch(m_voxelizeDimension / 8, m_voxelizeDimension / 8, m_voxelizeDimension / 8);
	_MUTEXUNLOCK(Engine::m_pMutex);

	cs_uavs[0] = { NULL };

	pDepthShaderResouceView[0] = NULL;
	pDepthShaderResouceView[1] = NULL;

	_MUTEXLOCK(Engine::m_pMutex);
	Engine::GetRender()->GetDeviceContext()->CSSetUnorderedAccessViews(0, 1, cs_uavs, (UINT*)(&cs_uavs[0]));
	Engine::GetRender()->GetDeviceContext()->CSSetShaderResources(0, 2, pDepthShaderResouceView);
	_MUTEXUNLOCK(Engine::m_pMutex);
	// Coloring voxel---------------

	_MUTEXLOCK(Engine::m_pMutex);
	Engine::GetRender()->GetDeviceContext()->GenerateMips(m_pVoxelGridColorIlluminatedSRV[outputLightRTIndices]);
	_MUTEXUNLOCK(Engine::m_pMutex);

	for (size_t i = 0; i < 2; i++)
	{
		//Light propagate second bounce---------------
		cs_uavs[0] = { m_pVoxelGridColorIlluminatedUAV[inputLightRTIndices] };

		pDepthShaderResouceView[0] = m_pVoxelGridColorSRV;
		pDepthShaderResouceView[1] = m_pVoxelGridNormalSRV;
		pDepthShaderResouceView[2] = m_pVoxelGridColorIlluminatedSRV[outputLightRTIndices];

		//m_pVoxelSampler->Bind(CUSTOM0_TEX_BP, COMPUTE_SHADER);
		_MUTEXLOCK(Engine::m_pMutex);
		//Engine::GetRender()->GetDeviceContext()->CSSetShader(m_pVoxelShader->GetComputeShader(4), NULL, 0);
		Engine::GetRender()->GetDeviceContext()->CSSetUnorderedAccessViews(0, 1, cs_uavs, (UINT*)(&cs_uavs[0]));
		Engine::GetRender()->GetDeviceContext()->CSSetShaderResources(0, 3, pDepthShaderResouceView);
		Engine::GetRender()->GetDeviceContext()->Dispatch(m_voxelizeDimension / 8, m_voxelizeDimension / 8, m_voxelizeDimension / 8);
		_MUTEXUNLOCK(Engine::m_pMutex);

		cs_uavs[0] = { NULL };
		pDepthShaderResouceView[0] = NULL;
		pDepthShaderResouceView[1] = NULL;
		pDepthShaderResouceView[2] = NULL;
		_MUTEXLOCK(Engine::m_pMutex);
		Engine::GetRender()->GetDeviceContext()->CSSetUnorderedAccessViews(0, 1, cs_uavs, (UINT*)(&cs_uavs[0]));
		Engine::GetRender()->GetDeviceContext()->CSSetShaderResources(0, 3, pDepthShaderResouceView);
		_MUTEXUNLOCK(Engine::m_pMutex);
		//Light propagate second bounce---------------

		_MUTEXLOCK(Engine::m_pMutex);
		Engine::GetRender()->GetDeviceContext()->GenerateMips(m_pVoxelGridColorIlluminatedSRV[inputLightRTIndices]);
		_MUTEXUNLOCK(Engine::m_pMutex);

		SwapIndices(inputLightRTIndices, outputLightRTIndices);
	}
}

float VoxelGlobalIllumination::GetVoxelizeSize()
{
	return m_voxelizeSize;
}

unsigned short VoxelGlobalIllumination::GetVoxelizeDimension()
{
	return m_voxelizeDimension;
}

void VoxelGlobalIllumination::PerformClearPass()
{
	ID3D11UnorderedAccessView* cs_uavs[] = { m_pVoxelGridColorUAV, m_pVoxelGridNormalUAV, m_pVoxelGridColorIlluminatedUAV[0], m_pVoxelGridColorIlluminatedUAV[1] };
	_MUTEXLOCK(Engine::m_pMutex);
	Engine::GetRender()->GetDeviceContext()->CSSetUnorderedAccessViews(0, 4, cs_uavs, (UINT*)(&cs_uavs[0]));
	//Engine::GetRender()->GetDeviceContext()->CSSetShader(m_pVoxelShader->GetComputeShader(0), NULL, 0);
	Engine::GetRender()->GetDeviceContext()->Dispatch(m_voxelizeDimension / 8, m_voxelizeDimension / 8, m_voxelizeDimension / 8);
	_MUTEXUNLOCK(Engine::m_pMutex);

	cs_uavs[0] = { NULL };
	cs_uavs[1] = { NULL };
	cs_uavs[2] = { NULL };
	cs_uavs[3] = { NULL };
	_MUTEXLOCK(Engine::m_pMutex);
	Engine::GetRender()->GetDeviceContext()->CSSetUnorderedAccessViews(0, 4, cs_uavs, (UINT*)(&cs_uavs[0]));
	_MUTEXUNLOCK(Engine::m_pMutex);
}
