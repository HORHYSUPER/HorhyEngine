#include "stdafx.h"
#include "Engine.h"
#include "Render.h"
#include "DX11_IndexBuffer.h"

using namespace D3D11Framework;

void DX11_IndexBuffer::Release()
{
	SAFE_RELEASE(indexBuffer);
	SAFE_DELETE_ARRAY(indices);
}

bool DX11_IndexBuffer::Create(unsigned int maxIndexCount, bool dynamic)
{
	HRESULT hr;
	this->maxIndexCount = maxIndexCount;
	this->dynamic = dynamic;
	indices = new unsigned int[maxIndexCount];
	if (!indices)
		return false;

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.ByteWidth = sizeof(unsigned int)*maxIndexCount;
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
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	_MUTEXLOCK(Engine::m_pMutex);
	hr = Engine::GetRender()->GetDevice()->CreateBuffer(&bd, NULL, &indexBuffer);
	_MUTEXUNLOCK(Engine::m_pMutex);
	if (hr != S_OK)
		return false;

	return true;
}

unsigned int DX11_IndexBuffer::AddIndices(unsigned int numIndices, const unsigned int *newIndices)
{
	int firstIndex = currentIndexCount;
	currentIndexCount += numIndices;
	assert(currentIndexCount <= maxIndexCount);
	memcpy(&indices[firstIndex], newIndices, sizeof(unsigned int)*numIndices);
	return firstIndex;
}

bool DX11_IndexBuffer::Update()
{
	HRESULT hr;
	if (currentIndexCount > 0)
	{
		if (dynamic)
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			_MUTEXLOCK(Engine::m_pMutex);
			hr = Engine::GetRender()->GetDeviceContext()->Map(indexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			_MUTEXUNLOCK(Engine::m_pMutex);
			if (hr != S_OK)
				return false;
			memcpy(mappedResource.pData, indices, sizeof(unsigned int)*currentIndexCount);
			_MUTEXLOCK(Engine::m_pMutex);
			Engine::GetRender()->GetDeviceContext()->Unmap(indexBuffer, 0);
			_MUTEXUNLOCK(Engine::m_pMutex);
		}
		else
		{
			_MUTEXLOCK(Engine::m_pMutex);
			Engine::GetRender()->GetDeviceContext()->UpdateSubresource(indexBuffer, 0, NULL, indices, 0, 0);
			_MUTEXUNLOCK(Engine::m_pMutex);
		}
	}
	return true;
}

void DX11_IndexBuffer::Bind() const
{
	_MUTEXLOCK(Engine::m_pMutex);
	Engine::GetRender()->GetDeviceContext()->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	_MUTEXUNLOCK(Engine::m_pMutex);
}
