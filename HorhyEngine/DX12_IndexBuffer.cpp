#include "stdafx.h"
#include "Engine.h"
#include "Render.h"
#include "Util.h"
#include "DX12_IndexBuffer.h"

using namespace D3D11Framework;

void DX12_IndexBuffer::Release()
{
	SAFE_DELETE_ARRAY(indices);
}

bool DX12_IndexBuffer::Create(unsigned int maxIndexCount, bool dynamic)
{
	HRESULT hr;
	this->maxIndexCount = maxIndexCount;
	this->dynamic = dynamic;
	indices = new unsigned int[maxIndexCount];
	if (!indices)
		return false;

	CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(unsigned int) * maxIndexCount);

	ThrowIfFailed(Engine::GetRender()->GetD3D12Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(dynamic ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&indexBufferDesc,
		dynamic ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_indexBuffer)));

	if (!dynamic)
	{
		ThrowIfFailed(Engine::GetRender()->GetD3D12Device()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&indexBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_indexBufferUpload)));
	}

	return true;
}

unsigned int DX12_IndexBuffer::AddIndices(unsigned int numIndices, const unsigned int *newIndices)
{
	int firstIndex = currentIndexCount;
	currentIndexCount += numIndices;
	assert(currentIndexCount <= maxIndexCount);
	memcpy(&indices[firstIndex], newIndices, sizeof(unsigned int)*numIndices);
	return firstIndex;
}

bool DX12_IndexBuffer::Update()
{
	if (currentIndexCount > 0)
	{
		if (dynamic)
		{
			UINT8* pIndexDataBegin;
			ThrowIfFailed(m_indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pIndexDataBegin)));
			memcpy(pIndexDataBegin, indices, sizeof(unsigned int) * currentIndexCount);
			m_indexBuffer->Unmap(0, nullptr);
		}
		else
		{
			ComPtr<ID3D12CommandAllocator> m_commandAllocator;
			ThrowIfFailed(Engine::GetRender()->GetD3D12Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

			ComPtr<ID3D12GraphicsCommandList> commandList;
			ThrowIfFailed(Engine::GetRender()->GetD3D12Device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), NULL, IID_PPV_ARGS(&commandList)));

			D3D12_SUBRESOURCE_DATA vertexData = {};
			vertexData.pData = indices;
			vertexData.RowPitch = sizeof(unsigned int) * currentIndexCount;
			vertexData.SlicePitch = vertexData.RowPitch;

			UpdateSubresources<1>(commandList.Get(), m_indexBuffer.Get(), m_indexBufferUpload.Get(), 0, 0, 1, &vertexData);
			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));
			commandList->Close();
			ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
			Engine::GetRender()->m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		}
	}

	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.SizeInBytes = sizeof(unsigned int) * currentIndexCount;
	m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	return true;
}

void DX12_IndexBuffer::Bind(ID3D12GraphicsCommandList *pCommandList) const
{
	pCommandList->IASetIndexBuffer(&m_indexBufferView);
}
