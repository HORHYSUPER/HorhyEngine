#include "stdafx.h"
#include "Engine.h"
#include "Render.h"
#include "DX12_Sampler.h"

using namespace D3D11Framework;

void DX12_Sampler::Release()
{
}

bool DX12_Sampler::Create(const SamplerDesc &desc)
{
	this->desc = desc;
	
	D3D12_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(samplerDesc));
	samplerDesc.Filter = (D3D12_FILTER)desc.filter;
	samplerDesc.AddressU = (D3D12_TEXTURE_ADDRESS_MODE)desc.adressU;
	samplerDesc.AddressV = (D3D12_TEXTURE_ADDRESS_MODE)desc.adressV;
	samplerDesc.AddressW = (D3D12_TEXTURE_ADDRESS_MODE)desc.adressW;
	samplerDesc.MipLODBias = desc.lodBias;
	samplerDesc.MaxAnisotropy = desc.maxAnisotropy;
	samplerDesc.ComparisonFunc = (D3D12_COMPARISON_FUNC)desc.compareFunc;
	samplerDesc.BorderColor[0] = desc.borderColor[0];
	samplerDesc.BorderColor[1] = desc.borderColor[1];
	samplerDesc.BorderColor[2] = desc.borderColor[2];
	samplerDesc.BorderColor[3] = desc.borderColor[3];
	samplerDesc.MinLOD = desc.minLOD;
	samplerDesc.MaxLOD = desc.maxLOD;

	const UINT samplerDescriptorSize = Engine::GetRender()->GetD3D12Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	CD3DX12_CPU_DESCRIPTOR_HANDLE samplerDescHandleCpu(Engine::GetRender()->m_samplerHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE samplerDescHandleGpu(Engine::GetRender()->m_samplerHeap->GetGPUDescriptorHandleForHeapStart());

	samplerDescHandleCpu.Offset(Engine::GetRender()->m_samplerHeapElements, samplerDescriptorSize);
	samplerDescHandleGpu.Offset(Engine::GetRender()->m_samplerHeapElements, samplerDescriptorSize);
	Engine::GetRender()->m_samplerHeapElements++;

	Engine::GetRender()->GetD3D12Device()->CreateSampler(&samplerDesc, samplerDescHandleCpu);
	m_samplerHandle = samplerDescHandleGpu;

	return true;
}

void DX12_Sampler::Bind(ID3D12GraphicsCommandList *pCommandList, unsigned int rootParameterIndex) const
{
	pCommandList->SetGraphicsRootDescriptorTable(rootParameterIndex, m_samplerHandle);
}