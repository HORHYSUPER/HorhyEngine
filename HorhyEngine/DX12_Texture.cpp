#include "stdafx.h"
#include <DirectXTex.h>
#include "Engine.h"
#include "Render.h"
#include "Util.h"
#include "DX12_RenderTarget.h"
#include "DX12_Texture.h"

#pragma comment(lib,"DirectXTex.lib")

using namespace D3D11Framework;

void DX12_Texture::Release()
{
}

bool DX12_Texture::LoadFromFile(const char *fileName)
{
	strcpy(name, fileName);

	std::wstring filePath = *Engine::GetSystemPath(PATH_TO_TEXTURES);
	filePath.append(ToWChar((char*)fileName));
	if (_access(_bstr_t(filePath.c_str()), 0))
		return false;

	TexMetadata info;
	unique_ptr<ScratchImage> image(new ScratchImage);
	WCHAR ext[_MAX_EXT];
	_wsplitpath_s(ToWChar((char*)fileName), nullptr, 0, nullptr, 0, nullptr, 0, ext, _MAX_EXT);
	if (_wcsicmp(ext, L".dds") == 0)
	{
		ThrowIfFailed(LoadFromDDSFile(filePath.c_str(), DDS_FLAGS_NONE, &info, *image));
	}
	else if (_wcsicmp(ext, L".tga") == 0)
	{
		ThrowIfFailed(LoadFromTGAFile(filePath.c_str(), &info, *image));
	}
	else
	{
		ThrowIfFailed(LoadFromWICFile(filePath.c_str(), WIC_FLAGS_NONE, &info, *image));
	}

	// Describe and create a Texture2D.
	CD3DX12_RESOURCE_DESC texDesc(
		D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		0,
		info.width,
		info.height,
		info.depth,
		static_cast<UINT16>(info.mipLevels),
		info.format,
		1,
		0,
		D3D12_TEXTURE_LAYOUT_UNKNOWN,
		D3D12_RESOURCE_FLAG_NONE);

	ThrowIfFailed(Engine::GetRender()->GetD3D12Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mTexture.ReleaseAndGetAddressOf())));
	mTexture->SetName(ToWChar((char*)fileName));

	D3D12_BOX box = {};
	box.right = info.width;
	box.bottom = info.height;
	box.back = info.depth;
	ThrowIfFailed(mTexture->WriteToSubresource(0, &box, image.get()->GetPixels(), image.get()->GetImages()->rowPitch, image.get()->GetImages()->slicePitch));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = info.format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	const UINT cbvSrvDescriptorSize = Engine::GetRender()->GetD3D12Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvUavDescHandleCpu(Engine::GetRender()->m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvUavDescHandleGpu(Engine::GetRender()->m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart());

	cbvSrvUavDescHandleCpu.Offset(Engine::GetRender()->m_cbvSrvUavHeapElements, cbvSrvDescriptorSize);
	cbvSrvUavDescHandleGpu.Offset(Engine::GetRender()->m_cbvSrvUavHeapElements, cbvSrvDescriptorSize);
	Engine::GetRender()->m_cbvSrvUavHeapElements++;
	return true;
	Engine::GetRender()->GetD3D12Device()->CreateShaderResourceView(mTexture.Get(), NULL, cbvSrvUavDescHandleCpu);

	m_textureSrvHandle = cbvSrvUavDescHandleGpu;

	return true;
}

bool DX12_Texture::CreateRenderable(unsigned int width, unsigned int height, unsigned int depth, DXGI_FORMAT format,
	unsigned int sampleCountMSAA, unsigned int numLevels, unsigned int rtFlags)
{
	strcpy(name, "renderTargetTexture");
	const bool useUAV = rtFlags & UAV_RTF;
	const bool texture3D = rtFlags & TEXTURE_3D_RTF;
	this->numLevels = numLevels;

	if (!texture3D)
	{
		CD3DX12_RESOURCE_DESC renderTargetDesc(
			D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			0,
			static_cast<UINT>(width),
			static_cast<UINT>(height),
			static_cast<UINT>(depth),
			static_cast<UINT>(numLevels),
			GetDepthResourceFormat(format),
			sampleCountMSAA,
			0,
			D3D12_TEXTURE_LAYOUT_UNKNOWN,
			IsDepthFormat(format) ?
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL :
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		D3D12_CLEAR_VALUE clearValue;
		if (IsDepthFormat(format))
		{
			clearValue.DepthStencil.Depth = 1.0f;
			clearValue.DepthStencil.Stencil = 0;
			clearValue.Format = format;
		}
		else
		{
			const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			memcpy(clearValue.Color, clearColor, sizeof(clearColor));
			clearValue.Format = format;
		}

		ThrowIfFailed(Engine::GetRender()->GetD3D12Device()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&renderTargetDesc,
			IsDepthFormat(format) ?
			D3D12_RESOURCE_STATE_DEPTH_WRITE :
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			&clearValue,
			IID_PPV_ARGS(&mTexture)));

		// Create a SRV of the intermediate render target.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = GetDepthSRVFormat(format);

		if (rtFlags & TEXTURE_CUBE_RTF)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MipLevels = numLevels;
			srvDesc.TextureCube.MostDetailedMip = 0;
		}
		else if (true)
		{
			if (depth == 1)
			{
				srvDesc.ViewDimension = (sampleCountMSAA > 1) ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Texture2D.MipLevels = numLevels;
				srvDesc.Texture2D.MostDetailedMip = 0;
			}
			else
			{
				srvDesc.ViewDimension = (sampleCountMSAA > 1) ? D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				srvDesc.Texture2DArray.MipLevels = numLevels;
				srvDesc.Texture2DArray.MostDetailedMip = 0;
				srvDesc.Texture2DArray.ArraySize = depth;
				srvDesc.Texture2DArray.FirstArraySlice = 0;
			}
		}

		const UINT cbvSrvDescriptorSize = Engine::GetRender()->GetD3D12Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvUavDescHandleCpu(Engine::GetRender()->m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart());
		CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvUavDescHandleGpu(Engine::GetRender()->m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart());

		cbvSrvUavDescHandleCpu.Offset(Engine::GetRender()->m_cbvSrvUavHeapElements, cbvSrvDescriptorSize);
		cbvSrvUavDescHandleGpu.Offset(Engine::GetRender()->m_cbvSrvUavHeapElements, cbvSrvDescriptorSize);
		Engine::GetRender()->m_cbvSrvUavHeapElements++;

		Engine::GetRender()->GetD3D12Device()->CreateShaderResourceView(mTexture.Get(), &srvDesc, cbvSrvUavDescHandleCpu);
		m_textureSrvHandle = cbvSrvUavDescHandleGpu;

		if (useUAV)
		{
			//D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			//uavDesc.Format = DXGI_FORMAT_UNKNOWN;
			//uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			//uavDesc.Buffer.FirstElement = 0;
			//uavDesc.Buffer.NumElements = TriangleCount;
			//uavDesc.Buffer.StructureByteStride = sizeof(IndirectCommand);
			//uavDesc.Buffer.CounterOffsetInBytes = 0;
			//uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

			//cbvSrvUavDescHandleCpu.Offset(Engine::GetRender()->m_cbvSrvUavHeapElements, cbvSrvDescriptorSize);
			//cbvSrvUavDescHandleGpu.Offset(Engine::GetRender()->m_cbvSrvUavHeapElements, cbvSrvDescriptorSize);
			//Engine::GetRender()->m_cbvSrvUavHeapElements++;

			//Engine::GetRender()->GetD3D12Device()->CreateUnorderedAccessView(mTexture.Get(), NULL, NULL, cbvSrvUavDescHandleCpu);
			//m_textureUavHandle = cbvSrvUavDescHandleGpu;
		}
	}
	else
	{
		// TO DO : create 3d texture
	}

	return true;
}

void DX12_Texture::Bind(ID3D12GraphicsCommandList *pCommandList, unsigned short rootParameterIndex) const
{
	pCommandList->SetGraphicsRootDescriptorTable(rootParameterIndex, m_textureSrvHandle);
}


