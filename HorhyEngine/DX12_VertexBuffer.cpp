#include "stdafx.h"
#include "Engine.h"
#include "Render.h"
#include "Util.h"
#include "DX12_VertexBuffer.h"

using namespace D3D11Framework;

void DX12_VertexBuffer::Release()
{
	SAFE_DELETE_ARRAY(vertices);
}

bool DX12_VertexBuffer::Create(unsigned int vertexSize, unsigned int maxVertexCount, bool dynamic)
{
	if ((vertexSize < 1) || (maxVertexCount < 1))
		return false;

	this->vertexSize = vertexSize;
	this->maxVertexCount = maxVertexCount;
	this->dynamic = dynamic;
	vertices = new char[vertexSize*maxVertexCount];
	if (!vertices)
		return false;

	const UINT vertexBufferSize = vertexSize * maxVertexCount;

	CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

	ThrowIfFailed(Engine::GetRender()->GetD3D12Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(dynamic ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&vertexBufferDesc,
		dynamic ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_vertexBuffer)));

	if (!dynamic)
	{
		ThrowIfFailed(Engine::GetRender()->GetD3D12Device()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&vertexBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_vertexBufferUpload)));
	}

	return true;
}

unsigned int DX12_VertexBuffer::AddVertices(unsigned int numVertices, const float *newVertices)
{
	int firstIndex = currentVertexCount;
	currentVertexCount += numVertices;
	assert(currentVertexCount <= maxVertexCount);
	memcpy(&vertices[vertexSize*firstIndex], newVertices, vertexSize * numVertices);
	return firstIndex;
}

bool DX12_VertexBuffer::Update()
{
	if (currentVertexCount > 0)
	{
		if (dynamic)
		{
			UINT8* pVertexDataBegin;
			ThrowIfFailed(m_vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, vertices, vertexSize * currentVertexCount);
			m_vertexBuffer->Unmap(0, nullptr);
		}
		else
		{
			ComPtr<ID3D12CommandAllocator> m_commandAllocator;
			ThrowIfFailed(Engine::GetRender()->GetD3D12Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

			ComPtr<ID3D12GraphicsCommandList> commandList;
			ThrowIfFailed(Engine::GetRender()->GetD3D12Device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), NULL, IID_PPV_ARGS(&commandList)));

			D3D12_SUBRESOURCE_DATA vertexData = {};
			vertexData.pData = vertices;
			vertexData.RowPitch = vertexSize * currentVertexCount;
			vertexData.SlicePitch = vertexData.RowPitch;

			UpdateSubresources<1>(commandList.Get(), m_vertexBuffer.Get(), m_vertexBufferUpload.Get(), 0, 0, 1, &vertexData);
			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
			commandList->Close();
			ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
			Engine::GetRender()->m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		}
	}

	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.StrideInBytes = vertexSize;
	m_vertexBufferView.SizeInBytes = vertexSize * currentVertexCount;

	return true;
}

void DX12_VertexBuffer::Bind(ID3D12GraphicsCommandList *pCommandList) const
{
	pCommandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
}
