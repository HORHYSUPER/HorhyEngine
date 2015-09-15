#include "stdafx.h"
#include "Engine.h"
#include "Util.h"
#include "Render.h"
#include "DX12_UniformBuffer.h"

using namespace D3D11Framework;

void DX12_UniformBuffer::Release()
{
	//SAFE_RELEASE(uniformBuffer);
}

bool DX12_UniformBuffer::Create(unsigned int bufferSize)
{
	if (bufferSize < 4)
		return false;

	size = bufferSize;
	c_alignedConstantBufferSize = (bufferSize + 255) & ~255;
	size = c_alignedConstantBufferSize;

	CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(c_alignedConstantBufferSize);

	ThrowIfFailed(Engine::GetRender()->GetD3D12Device()->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&constantBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_uniformBuffer)));

	m_uniformBuffer->SetName(L"Constant Buffer");

	D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = m_uniformBuffer->GetGPUVirtualAddress();
	const UINT cbvSrvDescriptorSize = Engine::GetRender()->GetD3D12Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrcUavDescHandleCpu(Engine::GetRender()->m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrcUavDescHandleGpu(Engine::GetRender()->m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart());
	
	cbvSrcUavDescHandleCpu.Offset(Engine::GetRender()->m_cbvSrvUavHeapElements, cbvSrvDescriptorSize);
	cbvSrcUavDescHandleGpu.Offset(Engine::GetRender()->m_cbvSrvUavHeapElements, cbvSrvDescriptorSize);
	Engine::GetRender()->m_cbvSrvUavHeapElements++;

	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
	desc.BufferLocation = cbvGpuAddress;
	desc.SizeInBytes = c_alignedConstantBufferSize;
	Engine::GetRender()->GetD3D12Device()->CreateConstantBufferView(&desc, cbvSrcUavDescHandleCpu);

	m_cbvHandle = cbvSrcUavDescHandleGpu;

	ThrowIfFailed(m_uniformBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedConstantBuffer)));
	ZeroMemory(m_mappedConstantBuffer, c_alignedConstantBufferSize);

	return true;
}

bool DX12_UniformBuffer::Update(const void *uniformBufferData)
{
	if (!uniformBufferData)
		return false;

	memcpy(m_mappedConstantBuffer, uniformBufferData, size);

	return true;
}

void DX12_UniformBuffer::Bind(ID3D12GraphicsCommandList *pCommandList, unsigned int rootParameterIndex) const
{
	pCommandList->SetGraphicsRootDescriptorTable(rootParameterIndex, m_cbvHandle);
}