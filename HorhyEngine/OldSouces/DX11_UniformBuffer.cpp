#include "stdafx.h"
#include "Engine.h"
#include "Render.h"
#include "DX11_UniformBuffer.h"

using namespace D3D11Framework;

void DX11_UniformBuffer::Release()
{
	SAFE_RELEASE(uniformBuffer);
}

bool DX11_UniformBuffer::Create(unsigned int bufferSize)
{
	if (bufferSize < 4)
		return false;

	size = bufferSize;

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	unsigned int alignedSize = size;
	unsigned int align = alignedSize % 16;
	if (align > 0)
		alignedSize += 16 - align;
	bd.ByteWidth = alignedSize;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if (Engine::GetRender()->GetDevice()->CreateBuffer(&bd, NULL, &uniformBuffer) != S_OK)
		return false;

	return true;
}

bool DX11_UniformBuffer::Update(const void *uniformBufferData)
{
	if (!uniformBufferData)
		return false;

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	_MUTEXLOCK(Engine::m_pMutex);
	Engine::GetRender()->GetDeviceContext()->Map(uniformBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	_MUTEXUNLOCK(Engine::m_pMutex);
	float *resourceData = (float*)MappedResource.pData;
	memcpy(resourceData, uniformBufferData, size);
	_MUTEXLOCK(Engine::m_pMutex);
	Engine::GetRender()->GetDeviceContext()->Unmap(uniformBuffer, 0);
	_MUTEXUNLOCK(Engine::m_pMutex);

	return true;
}

void DX11_UniformBuffer::Bind(UniformBufferBP bindingPoint, ShaderTypes shaderType) const
{
	switch (shaderType)
	{
	case VERTEX_SHADER:
		_MUTEXLOCK(Engine::m_pMutex);
		Engine::GetRender()->GetDeviceContext()->VSSetConstantBuffers(bindingPoint, 1, &uniformBuffer);
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;

	case HULL_SHADER:
		_MUTEXLOCK(Engine::m_pMutex);
		Engine::GetRender()->GetDeviceContext()->HSSetConstantBuffers(bindingPoint, 1, &uniformBuffer);
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;

	case DOMAIN_SHADER:
		_MUTEXLOCK(Engine::m_pMutex);
		Engine::GetRender()->GetDeviceContext()->DSSetConstantBuffers(bindingPoint, 1, &uniformBuffer);
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;

	case GEOMETRY_SHADER:
		_MUTEXLOCK(Engine::m_pMutex);
		Engine::GetRender()->GetDeviceContext()->GSSetConstantBuffers(bindingPoint, 1, &uniformBuffer);
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;

	case PIXEL_SHADER:
		_MUTEXLOCK(Engine::m_pMutex);
		Engine::GetRender()->GetDeviceContext()->PSSetConstantBuffers(bindingPoint, 1, &uniformBuffer);
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;

	case COMPUTE_SHADER:
		_MUTEXLOCK(Engine::m_pMutex);
		Engine::GetRender()->GetDeviceContext()->CSSetConstantBuffers(bindingPoint, 1, &uniformBuffer);
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;
	}
}

