#include "stdafx.h"
#include "Engine.h"
#include "Render.h"
#include "DX11_VertexBuffer.h"

using namespace D3D11Framework;

void DX11_VertexBuffer::Release()
{
	SAFE_RELEASE(vertexBuffer);
	SAFE_DELETE_ARRAY(vertices);
}

bool DX11_VertexBuffer::Create(unsigned int vertexSize, unsigned int maxVertexCount, bool dynamic)
{
	if ((vertexSize < 1) || (maxVertexCount < 1))
		return false;

	this->vertexSize = vertexSize;
	this->maxVertexCount = maxVertexCount;
	this->dynamic = dynamic;
	vertices = new char[vertexSize*maxVertexCount];
	if (!vertices)
		return false;

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.ByteWidth = vertexSize*maxVertexCount;
	if (dynamic)
	{
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}
	else
	{
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.CPUAccessFlags = 0;
	}
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	if (Engine::GetRender()->GetDevice()->CreateBuffer(&bd, NULL, &vertexBuffer) != S_OK)
		return false;

	return true;
}

unsigned int DX11_VertexBuffer::AddVertices(unsigned int numVertices, const float *newVertices)
{
	int firstIndex = currentVertexCount;
	currentVertexCount += numVertices;
	assert(currentVertexCount <= maxVertexCount);
	memcpy(&vertices[vertexSize*firstIndex], newVertices, vertexSize*numVertices);
	return firstIndex;
}

bool DX11_VertexBuffer::Update()
{
	HRESULT hr;
	if (currentVertexCount > 0)
	{
		if (dynamic)
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			_MUTEXLOCK(Engine::m_pMutex);
			hr = Engine::GetRender()->GetDeviceContext()->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			_MUTEXUNLOCK(Engine::m_pMutex);
			if (hr != S_OK)
			{
				return false;
			}
			memcpy(mappedResource.pData, vertices, vertexSize*currentVertexCount);
			_MUTEXLOCK(Engine::m_pMutex);
			Engine::GetRender()->GetDeviceContext()->Unmap(vertexBuffer, 0);
			_MUTEXUNLOCK(Engine::m_pMutex);
		}
		else
		{
			_MUTEXLOCK(Engine::m_pMutex);
			Engine::GetRender()->GetDeviceContext()->UpdateSubresource(vertexBuffer, 0, NULL, vertices, 0, 0);
			_MUTEXUNLOCK(Engine::m_pMutex);
		}
	}
	return true;
}

void DX11_VertexBuffer::Bind() const
{
	UINT stride = vertexSize;
	UINT offset = 0;
	_MUTEXLOCK(Engine::m_pMutex);
	Engine::GetRender()->GetDeviceContext()->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
	_MUTEXUNLOCK(Engine::m_pMutex);
}
