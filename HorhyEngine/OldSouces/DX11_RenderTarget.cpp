#include "stdafx.h"
#include "Engine.h"
#include "Render.h"
#include "DX11_StructuredBuffer.h"
#include "DX11_RenderTarget.h"
#include "DX11_RenderTargetConfig.h"
#include "DX11_Texture.h"

using namespace D3D11Framework;

void DX11_RenderTarget::Release()
{
	if (renderTargetViews)
	{
		for (unsigned int i = 0; i < numColorBuffers; i++)
		{
			SAFE_DELETE(frameBufferTextures[i]);
			SAFE_RELEASE(renderTargetViews[i]);
		}
		SAFE_DELETE_ARRAY(renderTargetViews);
	}
	SAFE_DELETE(depthStencilTexture);
	for (unsigned int i = 0; i < 2; i++)
		SAFE_RELEASE(depthStencilViews[i]);
}

bool DX11_RenderTarget::Create(const RenderTargetDesc &desc)
{
	width = desc.width;
	height = desc.height;
	depth = desc.depth;
	numColorBuffers = desc.CalcNumColorBuffers();

	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = (float)width;
	viewport.Height = (float)height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	if (numColorBuffers > 0)
	{
		clearMask = 4;
		frameBufferTextures.resize(numColorBuffers);
		for (size_t i = 0; i < numColorBuffers; i++)
		{
			frameBufferTextures[i] = new DX11_Texture;
			if (!frameBufferTextures[i])
				return false;
		}
		renderTargetViews = new ID3D11RenderTargetView*[numColorBuffers];
		if (!renderTargetViews)
			return false;
		for (unsigned int i = 0; i < numColorBuffers; i++)
		{
			unsigned int flags = desc.colorBufferDescs[i].rtFlags;
			if (DX11_Texture::IsSrgbFormat(desc.colorBufferDescs[i].format))
				flags |= SRGB_RTF;
			DXGI_FORMAT rtvFormat;
			if (flags & SRGB_RTF)
			{
				if (rtFlags & SRGB_RTF)
					rtvFormat = DX11_Texture::ConvertToSrgbFormat(desc.colorBufferDescs[i].format);
				else
					rtvFormat = DX11_Texture::ConvertFromSrgbFormat(desc.colorBufferDescs[i].format);
			}
			else
			{
				rtvFormat = desc.colorBufferDescs[i].format;
			}
			if (rtvFormat == DXGI_FORMAT_UNKNOWN)
				return false;

			if (!frameBufferTextures[i]->CreateRenderable(width, height, depth, desc.colorBufferDescs[i].format, desc.sampleCountMSAA, desc.numLevels, flags))
			{
				return false;
			}

			D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
			rtvDesc.Format = DX11_Texture::GetDX11TexFormat(rtvFormat);
			if (!(flags & TEXTURE_3D_RTF))
			{
				if (depth == 1)
				{
					rtvDesc.ViewDimension = desc.sampleCountMSAA > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
					rtvDesc.Texture2D.MipSlice = 0;
				}
				else
				{
					rtvDesc.ViewDimension = desc.sampleCountMSAA > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
					rtvDesc.Texture2DArray.MipSlice = 0;
					rtvDesc.Texture2DArray.ArraySize = depth;
					rtvDesc.Texture2DArray.FirstArraySlice = 0;
				}
			}
			else
			{
				rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
				rtvDesc.Texture3D.MipSlice = 0;
				rtvDesc.Texture3D.FirstWSlice = 0;
				rtvDesc.Texture3D.WSize = -1;
			}
			if (Engine::GetRender()->GetDevice()->CreateRenderTargetView(frameBufferTextures[i]->texture, &rtvDesc, &renderTargetViews[i]) != S_OK)
				return false;
		}
	}

	if (desc.depthStencilBufferDesc.format != DXGI_FORMAT_UNKNOWN)
	{
		clearMask |= D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL;
		depthStencilTexture = new DX11_Texture;
		if (!depthStencilTexture)
			return false;
		if (!depthStencilTexture->CreateRenderable(width, height, depth, desc.depthStencilBufferDesc.format, desc.sampleCountMSAA,
			desc.numLevels, desc.depthStencilBufferDesc.rtFlags))
		{
			return false;
		}
		D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
		ZeroMemory(&descDSV, sizeof(descDSV));
		descDSV.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		descDSV.ViewDimension = desc.sampleCountMSAA > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
		descDSV.Texture2D.MipSlice = 0;
		if (Engine::GetRender()->GetDevice()->CreateDepthStencilView(depthStencilTexture->texture, &descDSV, &depthStencilViews[0]) != S_OK)
			return false;
		descDSV.Flags = D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL;
		if (Engine::GetRender()->GetDevice()->CreateDepthStencilView(depthStencilTexture->texture, &descDSV, &depthStencilViews[1]) != S_OK)
			return false;
	}

	return true;
}

bool DX11_RenderTarget::CreateBackBuffer()
{
	width = Engine::GetRender()->GetScreenWidth()/*SCREEN_WIDTH*/;
	height = Engine::GetRender()->GetScreenHeight()/*SCREEN_HEIGHT*/;
	depth = 1;
	clearMask = 4 | D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL;
	numColorBuffers = 1;

	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = (float)width;
	viewport.Height = (float)height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	ID3D11Texture2D* backBufferTexture = NULL;
	if (Engine::GetRender()->GetSwapChain()->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBufferTexture) != S_OK)
		return false;
	renderTargetViews = new ID3D11RenderTargetView*[numColorBuffers];
	if (!renderTargetViews)
		return false;
	if (Engine::GetRender()->GetDevice()->CreateRenderTargetView(backBufferTexture, NULL, &renderTargetViews[0]) != S_OK)
	{
		backBufferTexture->Release();
		return false;
	}
	backBufferTexture->Release();

	depthStencilTexture = new DX11_Texture;
	if (!depthStencilTexture)
		return false;
	if (!depthStencilTexture->CreateRenderable(width, height, depth, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, MSAA_COUNT))
		return false;
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDSV.ViewDimension = MSAA_COUNT > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	if (Engine::GetRender()->GetDevice()->CreateDepthStencilView(depthStencilTexture->texture, &descDSV, &depthStencilViews[0]) != S_OK)
		return false;
	descDSV.Flags = D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL;
	if (Engine::GetRender()->GetDevice()->CreateDepthStencilView(depthStencilTexture->texture, &descDSV, &depthStencilViews[1]) != S_OK)
		return false;

	return true;
}

void DX11_RenderTarget::Bind(const DX11_RenderTargetConfig *rtConfig)
{
	if (renderTargetViews || depthStencilViews[0])
	{
		_MUTEXLOCK(Engine::m_pMutex);
		Engine::GetRender()->GetDeviceContext()->RSSetViewports(1, &viewport);
		_MUTEXUNLOCK(Engine::m_pMutex);
	}

	if (!rtConfig)
	{
		_MUTEXLOCK(Engine::m_pMutex);
		Engine::GetRender()->GetDeviceContext()->OMSetRenderTargets(numColorBuffers, renderTargetViews, depthStencilViews[0]);
		_MUTEXUNLOCK(Engine::m_pMutex);
	}
	else
	{
		const RtConfigDesc &rtConfigDesc = rtConfig->GetDesc();
		ID3D11DepthStencilView *setDSV = (rtConfigDesc.flags & DS_READ_ONLY_RTCF) ? depthStencilViews[1] : depthStencilViews[0];
		if (!(rtConfigDesc.flags & COMPUTE_RTCF))
		{
			if (rtConfigDesc.numUnorderedAccessViews == 0)
			{
				_MUTEXLOCK(Engine::m_pMutex);
				Engine::GetRender()->GetDeviceContext()->OMSetRenderTargets(rtConfigDesc.numColorBuffers,
					&renderTargetViews[rtConfigDesc.firstColorBufferIndex], setDSV);
				_MUTEXUNLOCK(Engine::m_pMutex);
			}
			else
			{
				assert(rtConfigDesc.numUnorderedAccessViews <= NUM_UAV_VIEW);
				_MUTEXLOCK(Engine::m_pMutex);
				Engine::GetRender()->GetDeviceContext()->OMSetRenderTargetsAndUnorderedAccessViews(numColorBuffers,
					&renderTargetViews[rtConfigDesc.firstColorBufferIndex], setDSV, rtConfigDesc.numColorBuffers,
					rtConfigDesc.numUnorderedAccessViews, rtConfigDesc.unorderedAccessViews, NULL);
				_MUTEXUNLOCK(Engine::m_pMutex);
			}
		}
		else
		{
			_MUTEXLOCK(Engine::m_pMutex);
			Engine::GetRender()->GetDeviceContext()->OMSetRenderTargets(0, NULL, NULL);
			_MUTEXUNLOCK(Engine::m_pMutex);
			if (rtConfigDesc.numUnorderedAccessViews == 0)
			{
				assert(rtConfigDesc.numColorBuffers <= MAX_NUM_COLOR_BUFFERS);
				ID3D11UnorderedAccessView *sbUnorderedAccessViews[MAX_NUM_COLOR_BUFFERS];
				for (unsigned int i = 0; i < rtConfigDesc.numColorBuffers; i++)
					sbUnorderedAccessViews[i] = frameBufferTextures[i]->GetUnorderdAccessView();
				{
					_MUTEXLOCK(Engine::m_pMutex);
					Engine::GetRender()->GetDeviceContext()->CSSetUnorderedAccessViews(0, rtConfigDesc.numColorBuffers,
						&sbUnorderedAccessViews[rtConfigDesc.firstColorBufferIndex], NULL);
					_MUTEXUNLOCK(Engine::m_pMutex);
				}
			}
			else
			{
				assert(rtConfigDesc.numUnorderedAccessViews <= NUM_UAV_VIEW);
				_MUTEXLOCK(Engine::m_pMutex);
				Engine::GetRender()->GetDeviceContext()->CSSetUnorderedAccessViews(0, rtConfigDesc.numUnorderedAccessViews, rtConfigDesc.unorderedAccessViews, NULL);
				_MUTEXUNLOCK(Engine::m_pMutex);
			}
		}
	}

	if (clearTarget)
	{
		Clear(clearMask);
		clearTarget = false;
	}
}

void DX11_RenderTarget::Clear(unsigned int newClearMask) const
{
	float ClearColor[4] = { 0.0, 0.0, 0.0, 0.0 };
	if (renderTargetViews)
	{
		if (newClearMask & 4)
		{
			for (unsigned int i = 0; i < numColorBuffers; i++)
			{
				_MUTEXLOCK(Engine::m_pMutex);
				Engine::GetRender()->GetDeviceContext()->ClearRenderTargetView(renderTargetViews[i], ClearColor);
				_MUTEXUNLOCK(Engine::m_pMutex);
			}
		}
	}
	if (depthStencilViews[0])
	{
		newClearMask &= ~4;
		if (newClearMask != 0)
		{
			_MUTEXLOCK(Engine::m_pMutex);
			Engine::GetRender()->GetDeviceContext()->ClearDepthStencilView(depthStencilViews[0], newClearMask, 1.0f, 0);
			_MUTEXUNLOCK(Engine::m_pMutex);
		}
	}
}




