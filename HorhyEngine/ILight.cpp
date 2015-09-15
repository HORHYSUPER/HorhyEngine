#include "stdafx.h"
#include "Camera.h"
#include "ILight.h"

using namespace D3D11Framework;

ILight::ILight()
{
	m_pLightCamera = new Camera();
	m_color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_width = 1024;
	m_height = 1024;
}


ILight::ILight(const ILight& other)
{
}

ILight::~ILight()
{
}

Camera *ILight::GetLightCamera()
{
	return m_pLightCamera;
}

DX12_UniformBuffer *ILight::GetUniformBuffer()
{
	return m_pUniformBuffer;
}

int ILight::GetLightType()
{
	return lightType;
}

XMVECTOR ILight::GetPosition()
{
	return m_pLightCamera->GetPosition();
}

XMVECTOR ILight::GetRotation()
{
	return m_pLightCamera->GetRotation();
}

void ILight::SetPosition(float x, float y, float z)
{
	m_pLightCamera->SetPosition(x, y, z);
}

void ILight::SetRotation(float x, float y, float z)
{
	m_pLightCamera->SetRotation(x, y, z);
}

void ILight::SetColor(float r, float g, float b)
{
	m_color.x = r;
	m_color.y = g;
	m_color.z = b;
}
