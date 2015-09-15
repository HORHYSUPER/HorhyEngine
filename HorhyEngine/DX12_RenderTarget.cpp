#include "stdafx.h"
#include "Engine.h"
#include "Render.h"
#include "Util.h"
#include "DX12_Texture.h"
#include "DX12_RenderTargetConfig.h"
#include "DX12_RenderTarget.h"

using namespace D3D11Framework;

void DX12_RenderTarget::Release()
{
	//if (renderTargetViews)
	//{
	//	for (unsigned int i = 0; i < numColorBuffers; i++)
	//	{
	//		SAFE_DELETE(frameBufferTextures[i]);
	//		SAFE_RELEASE(renderTargetViews[i]);
	//	}
	//	SAFE_DELETE_ARRAY(renderTargetViews);
	//}
	//SAFE_DELETE(depthStencilTexture);
	//for (unsigned int i = 0; i < 2; i++)
	//	SAFE_RELEASE(depthStencilViews[i]);
}

bool DX12_RenderTarget::Create(const RenderTargetDesc &desc)
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
		rtFormats = new DXGI_FORMAT[numColorBuffers];
		frameBufferTextures.resize(numColorBuffers);
		for (auto i = 0; i < numColorBuffers; i++)
		{
			frameBufferTextures[i] = new DX12_Texture;
			if (!frameBufferTextures[i])
				return false;
		}

		rtvHandles = new CD3DX12_CPU_DESCRIPTOR_HANDLE[numColorBuffers];
		if (!rtvHandles)
			return false;

		for (unsigned int i = 0; i < numColorBuffers; i++)
		{
			unsigned int flags = desc.colorBufferDescs[i].rtFlags;
			rtFormats[i] = desc.colorBufferDescs[i].format;

			if (!frameBufferTextures[i]->CreateRenderable(width, height, depth, desc.colorBufferDescs[i].format, desc.sampleCountMSAA, desc.numLevels, flags))
			{
				return false;
			}

			const UINT rtvDescriptorSize = Engine::GetRender()->GetD3D12Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescHandleCpu(Engine::GetRender()->m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
			CD3DX12_GPU_DESCRIPTOR_HANDLE rtvDescHandleGpu(Engine::GetRender()->m_rtvHeap->GetGPUDescriptorHandleForHeapStart());

			rtvDescHandleCpu.Offset(Engine::GetRender()->m_rtvHeapElements, rtvDescriptorSize);
			rtvDescHandleGpu.Offset(Engine::GetRender()->m_rtvHeapElements, rtvDescriptorSize);
			Engine::GetRender()->m_rtvHeapElements++;

			D3D12_RENDER_TARGET_VIEW_DESC descRT;
			ZeroMemory(&descRT, sizeof(D3D12_RENDER_TARGET_VIEW_DESC));
			descRT.Format = desc.colorBufferDescs[i].format;
			descRT.ViewDimension = desc.colorBufferDescs[i].rtFlags & TEXTURE_CUBE_RTF ? D3D12_RTV_DIMENSION_TEXTURE2DARRAY : D3D12_RTV_DIMENSION_TEXTURE2D;
			descRT.Texture2DArray.FirstArraySlice = 0;
			descRT.Texture2DArray.ArraySize = depth;
			descRT.Texture2DArray.MipSlice = 0;

			Engine::GetRender()->GetD3D12Device()->CreateRenderTargetView(frameBufferTextures[i]->mTexture.Get(), &descRT, rtvDescHandleCpu);
			rtvHandles[i] = rtvDescHandleCpu;
		}
	}

	// Create the depth stencil.
	if (desc.depthStencilBufferDesc.format != DXGI_FORMAT_UNKNOWN)
	{
		clearMask |= D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL;
		depthStencilTexture = new DX12_Texture;
		if (!depthStencilTexture)
			return false;
		if (!depthStencilTexture->CreateRenderable(width, height, depth, DXGI_FORMAT_R32G8X24_TYPELESS, desc.sampleCountMSAA,
			desc.numLevels, desc.depthStencilBufferDesc.rtFlags))
		{
			return false;
		}

		const UINT dsvDescriptorSize = Engine::GetRender()->GetD3D12Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvDescHandleCpu(Engine::GetRender()->m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
		CD3DX12_GPU_DESCRIPTOR_HANDLE dsvDescHandleGpu(Engine::GetRender()->m_dsvHeap->GetGPUDescriptorHandleForHeapStart());

		dsvDescHandleCpu.Offset(Engine::GetRender()->m_dsvHeapElements, dsvDescriptorSize);
		dsvDescHandleGpu.Offset(Engine::GetRender()->m_dsvHeapElements, dsvDescriptorSize);
		Engine::GetRender()->m_dsvHeapElements++;

		D3D12_DEPTH_STENCIL_VIEW_DESC descDSV;
		ZeroMemory(&descDSV, sizeof(descDSV));
		descDSV.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		if (depth == 1)
		{
			descDSV.ViewDimension = (desc.sampleCountMSAA > 1) ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
			descDSV.Texture2D.MipSlice = 0;
		}
		else
		{
			descDSV.ViewDimension = (desc.sampleCountMSAA > 1) ? D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY : D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			descDSV.Texture2DArray.MipSlice = 0;
			descDSV.Texture2DArray.ArraySize = depth;
			descDSV.Texture2DArray.FirstArraySlice = 0;
		}
		Engine::GetRender()->GetD3D12Device()->CreateDepthStencilView(depthStencilTexture->mTexture.Get(), &descDSV, dsvDescHandleCpu);
		dsvHandles[0] = dsvDescHandleCpu;

		dsvDescHandleCpu.Offset(Engine::GetRender()->m_dsvHeapElements, dsvDescriptorSize);
		dsvDescHandleGpu.Offset(Engine::GetRender()->m_dsvHeapElements, dsvDescriptorSize);
		Engine::GetRender()->m_dsvHeapElements++;

		descDSV.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH | D3D12_DSV_FLAG_READ_ONLY_STENCIL;
		Engine::GetRender()->GetD3D12Device()->CreateDepthStencilView(depthStencilTexture->mTexture.Get(), &descDSV, dsvDescHandleCpu);
		dsvHandles[1] = dsvDescHandleCpu;
	}

	return true;
}

bool DX12_RenderTarget::CreateBackBuffer(UINT bufferIndex)
{
	width = Engine::GetRender()->GetScreenWidth()/*SCREEN_WIDTH*/;
	height = Engine::GetRender()->GetScreenHeight()/*SCREEN_HEIGHT*/;
	depth = 1;
	clearMask = 4 | D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL;
	numColorBuffers = 1;
	rtFormats = new DXGI_FORMAT[numColorBuffers];

	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = (float)width;
	viewport.Height = (float)height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	rtvHandles = new CD3DX12_CPU_DESCRIPTOR_HANDLE[numColorBuffers];
	if (!rtvHandles)
		return false;

	ThrowIfFailed(Engine::GetRender()->GetSwapChain()->GetBuffer(bufferIndex, IID_PPV_ARGS(&m_renderTarget)));
	rtFormats[0] = m_renderTarget.Get()->GetDesc().Format;

	const UINT rtvDescriptorSize = Engine::GetRender()->GetD3D12Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescHandleCpu(Engine::GetRender()->m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE rtvDescHandleGpu(Engine::GetRender()->m_rtvHeap->GetGPUDescriptorHandleForHeapStart());

	rtvDescHandleCpu.Offset(Engine::GetRender()->m_rtvHeapElements, rtvDescriptorSize);
	rtvDescHandleGpu.Offset(Engine::GetRender()->m_rtvHeapElements, rtvDescriptorSize);
	Engine::GetRender()->m_rtvHeapElements++;

	Engine::GetRender()->GetD3D12Device()->CreateRenderTargetView(m_renderTarget.Get(), nullptr, rtvDescHandleCpu);
	rtvHandles[0] = rtvDescHandleCpu;

	depthStencilTexture = new DX12_Texture;
	if (!depthStencilTexture)
		return false;
	if (!depthStencilTexture->CreateRenderable(width, height, depth, DXGI_FORMAT_R32G8X24_TYPELESS, MSAA_COUNT))
		return false;

	const UINT dsvDescriptorSize = Engine::GetRender()->GetD3D12Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvDescHandleCpu(Engine::GetRender()->m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE dsvDescHandleGpu(Engine::GetRender()->m_dsvHeap->GetGPUDescriptorHandleForHeapStart());

	dsvDescHandleCpu.Offset(Engine::GetRender()->m_dsvHeapElements, dsvDescriptorSize);
	dsvDescHandleGpu.Offset(Engine::GetRender()->m_dsvHeapElements, dsvDescriptorSize);
	Engine::GetRender()->m_dsvHeapElements++;
	
	D3D12_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	descDSV.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	Engine::GetRender()->GetD3D12Device()->CreateDepthStencilView(depthStencilTexture->mTexture.Get(), &descDSV, dsvDescHandleCpu);
	dsvHandles[0] = dsvDescHandleCpu;

	dsvDescHandleCpu.Offset(Engine::GetRender()->m_dsvHeapElements, dsvDescriptorSize);
	dsvDescHandleGpu.Offset(Engine::GetRender()->m_dsvHeapElements, dsvDescriptorSize);
	Engine::GetRender()->m_dsvHeapElements++;

	descDSV.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH | D3D12_DSV_FLAG_READ_ONLY_STENCIL;
	Engine::GetRender()->GetD3D12Device()->CreateDepthStencilView(depthStencilTexture->mTexture.Get(), &descDSV, dsvDescHandleCpu);
	dsvHandles[1] = dsvDescHandleCpu;

	return true;
}

void DX12_RenderTarget::Bind(ID3D12GraphicsCommandList *pCommandList, const DX12_RenderTargetConfig *rtConfig)
{
	if (rtvHandles || dsvHandles)
	{
		pCommandList->RSSetViewports(1, &viewport);
	}

	if (!rtConfig)
	{
		pCommandList->OMSetRenderTargets(numColorBuffers, rtvHandles, FALSE, &dsvHandles[0]);
	}
	else
	{
		const RtConfigDesc &rtConfigDesc = rtConfig->GetDesc();
		CD3DX12_CPU_DESCRIPTOR_HANDLE setDSV = (rtConfigDesc.flags & DS_READ_ONLY_RTCF) ? dsvHandles[1] : dsvHandles[0];
		if (!(rtConfigDesc.flags & COMPUTE_RTCF))
		{
			if (rtConfigDesc.numUnorderedAccessViews == 0)
			{
				pCommandList->OMSetRenderTargets(rtConfigDesc.numColorBuffers, &rtvHandles[rtConfigDesc.firstColorBufferIndex], FALSE, &setDSV);
			}
			else
			{
				//assert(rtConfigDesc.numUnorderedAccessViews <= NUM_UAV_VIEW);
				//_MUTEXLOCK(Engine::m_pMutex);
				//Engine::GetRender()->GetDeviceContext()->OMSetRenderTargetsAndUnorderedAccessViews(numColorBuffers,
				//	&renderTargetViews[rtConfigDesc.firstColorBufferIndex], setDSV, rtConfigDesc.numColorBuffers,
				//	rtConfigDesc.numUnorderedAccessViews, rtConfigDesc.unorderedAccessViews, NULL);
				//_MUTEXUNLOCK(Engine::m_pMutex);
			}
		}
		else
		{
			//_MUTEXLOCK(Engine::m_pMutex);
			//Engine::GetRender()->GetDeviceContext()->OMSetRenderTargets(0, NULL, NULL);
			//_MUTEXUNLOCK(Engine::m_pMutex);
			//if (rtConfigDesc.numUnorderedAccessViews == 0)
			//{
			//	assert(rtConfigDesc.numColorBuffers <= MAX_NUM_COLOR_BUFFERS);
			//	ID3D11UnorderedAccessView *sbUnorderedAccessViews[MAX_NUM_COLOR_BUFFERS];
			//	for (unsigned int i = 0; i < rtConfigDesc.numColorBuffers; i++)
			//		sbUnorderedAccessViews[i] = frameBufferTextures[i]->GetUnorderdAccessView();
			//	{
			//		_MUTEXLOCK(Engine::m_pMutex);
			//		Engine::GetRender()->GetDeviceContext()->CSSetUnorderedAccessViews(0, rtConfigDesc.numColorBuffers,
			//			&sbUnorderedAccessViews[rtConfigDesc.firstColorBufferIndex], NULL);
			//		_MUTEXUNLOCK(Engine::m_pMutex);
			//	}
			//}
			//else
			//{
			//	assert(rtConfigDesc.numUnorderedAccessViews <= NUM_UAV_VIEW);
			//	_MUTEXLOCK(Engine::m_pMutex);
			//	Engine::GetRender()->GetDeviceContext()->CSSetUnorderedAccessViews(0, rtConfigDesc.numUnorderedAccessViews, rtConfigDesc.unorderedAccessViews, NULL);
			//	_MUTEXUNLOCK(Engine::m_pMutex);
			//}
		}
	}

	if (clearTarget)
	{
		Clear(pCommandList, clearMask);
		clearTarget = false;
	}
}

void DX12_RenderTarget::Clear(ID3D12GraphicsCommandList *pCommandList, unsigned int newClearMask) const
{
	float ClearColor[4] = { 0.0, 0.0, 0.0, 0.0 };
	if (rtvHandles)
	{
		if (newClearMask & 4)
		{
			for (unsigned int i = 0; i < numColorBuffers; i++)
			{
				pCommandList->ClearRenderTargetView(rtvHandles[i], ClearColor, 0, nullptr);
			}
		}
	}
	if (dsvHandles)
	{
		newClearMask &= ~4;
		if (newClearMask != 0)
		{
			pCommandList->ClearDepthStencilView(dsvHandles[0], D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		}
	}
}




