#include "stdafx.h"
#include "Shader.h"
#include "DX11_StructuredBuffer.h"
#include "DX11_UniformBuffer.h"
#include "DX11_Texture.h"
#include "macros.h"
#include "Log.h"

using namespace D3D11Framework;

#define MAXLAYOUT 16

Shader::Shader()
{
	//m_pVertexShader[0] = nullptr;
	//m_pHullShader[0] = nullptr;
	//m_pDomainShader[0] = nullptr;
	//m_pPixelShader[0] = nullptr;
	m_pInputLayout = nullptr;
	m_pSamplerState = nullptr;
	m_layoutformat = nullptr;
	m_pHullShaderBuffer = nullptr;
	m_pDomainShaderBuffer = nullptr;
	m_numlayout = 0;
}

void Shader::AddInputElementDesc(const char *SemanticName, UINT semanticIndex, DXGI_FORMAT format)
{
	if (!m_numlayout)
	{
		m_layoutformat = new D3D11_INPUT_ELEMENT_DESC[MAXLAYOUT];
		if (!m_layoutformat)
			return;
	}
	else if (m_numlayout >= MAXLAYOUT)
		return;

	D3D11_INPUT_ELEMENT_DESC &Layout = m_layoutformat[m_numlayout];

	Layout.SemanticName = SemanticName;
	Layout.SemanticIndex = semanticIndex;
	Layout.Format = format;
	Layout.InputSlot = 0;
	if (!m_numlayout)
		Layout.AlignedByteOffset = 0;
	else
		Layout.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	Layout.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	Layout.InstanceDataStepRate = 0;

	m_numlayout++;
}

bool D3D11Framework::Shader::CreateShaderVs(WCHAR *shaderName, CONST D3D_SHADER_MACRO *pDefines, LPCSTR vs)
{
	HRESULT hr = S_OK;
	ID3DBlob *vertexShaderBuffer = nullptr;
	m_pVertexShader.resize(m_pVertexShader.size() + 1);

	hr = m_compileshaderfromfile(shaderName, vs, "vs_5_0", pDefines, &vertexShaderBuffer);
	if (FAILED(hr))
	{
		Log::Get()->Err("Не удалось загрузить вершинный шейдер %ls", shaderName);
		return false;
	}

	hr = Engine::GetRender()->m_pd3dDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &m_pVertexShader[m_pVertexShader.size() - 1]);
	if (FAILED(hr))
	{
		Log::Get()->Err("Не удалось создать вершинный шейдер");
		return false;
	}

	hr = Engine::GetRender()->m_pd3dDevice->CreateInputLayout(m_layoutformat, m_numlayout, vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pInputLayout);
	if (FAILED(hr))
	{
		Log::Get()->Err("Не удалось создать формат ввода");
		return false;
	}

	SAFE_RELEASE(vertexShaderBuffer);

	return true;
}

bool D3D11Framework::Shader::CreateShaderHs(WCHAR *shaderName, CONST D3D_SHADER_MACRO *pDefines, LPCSTR hs)
{
	HRESULT hr = S_OK;
	ID3DBlob *m_pHullShaderBuffer = nullptr;
	m_pHullShader.resize(m_pHullShader.size() + 1);

	hr = m_compileshaderfromfile(shaderName, hs, "hs_5_0", pDefines, &m_pHullShaderBuffer);
	if (FAILED(hr))
	{
		Log::Get()->Err("Не удалось загрузить hull шейдер %ls", shaderName);
		return false;
	}

	hr = Engine::GetRender()->m_pd3dDevice->CreateHullShader(m_pHullShaderBuffer->GetBufferPointer(), m_pHullShaderBuffer->GetBufferSize(), NULL, &m_pHullShader[m_pHullShader.size() - 1]);
	if (FAILED(hr))
	{
		Log::Get()->Err("Не удалось создать HULL шейдер");
		return false;
	}

	SAFE_RELEASE(m_pHullShaderBuffer);

	return true;
}

bool D3D11Framework::Shader::CreateShaderDs(WCHAR *shaderName, CONST D3D_SHADER_MACRO *pDefines, LPCSTR ds)
{
	HRESULT hr = S_OK;
	ID3DBlob *m_pDomainShaderBuffer = nullptr;
	m_pDomainShader.resize(m_pDomainShader.size() + 1);

	hr = m_compileshaderfromfile(shaderName, ds, "ds_5_0", pDefines, &m_pDomainShaderBuffer);
	if (FAILED(hr))
	{
		Log::Get()->Err("Не удалось загрузить domain шейдер %ls", shaderName);
		return false;
	}

	hr = Engine::GetRender()->m_pd3dDevice->CreateDomainShader(m_pDomainShaderBuffer->GetBufferPointer(), m_pDomainShaderBuffer->GetBufferSize(), NULL, &m_pDomainShader[m_pDomainShader.size() - 1]);
	if (FAILED(hr))
	{
		Log::Get()->Err("Не удалось создать DOMAIN шейдер");
		return false;
	}

	SAFE_RELEASE(m_pDomainShaderBuffer);

	return true;
}

bool D3D11Framework::Shader::CreateShaderGs(WCHAR *shaderName, CONST D3D_SHADER_MACRO *pDefines, LPCSTR gs)
{
	HRESULT hr = S_OK;
	ID3DBlob *geometryShaderBuffer = nullptr;
	m_pGeometryShader.resize(m_pGeometryShader.size() + 1);

	hr = m_compileshaderfromfile(shaderName, gs, "gs_5_0", pDefines, &geometryShaderBuffer);
	if (FAILED(hr))
	{
		Log::Get()->Err("Не удалось загрузить геометрический шейдер %ls", shaderName);
		return false;
	}

	hr = Engine::GetRender()->m_pd3dDevice->CreateGeometryShader(geometryShaderBuffer->GetBufferPointer(), geometryShaderBuffer->GetBufferSize(), NULL, &m_pGeometryShader[m_pGeometryShader.size() - 1]);
	if (FAILED(hr))
	{
		Log::Get()->Err("Не удалось создать геометрический шейдер");
		return false;
	}

	SAFE_RELEASE(geometryShaderBuffer);

	return true;
}

bool D3D11Framework::Shader::CreateShaderPs(WCHAR *shaderName, CONST D3D_SHADER_MACRO *pDefines, LPCSTR ps)
{
	HRESULT hr = S_OK;
	ID3DBlob *pixelShaderBuffer = nullptr;
	m_pPixelShader.resize(m_pPixelShader.size() + 1);

	hr = m_compileshaderfromfile(shaderName, ps, "ps_5_0", pDefines, &pixelShaderBuffer);
	if (FAILED(hr))
	{
		Log::Get()->Err("Не удалось загрузить пиксельный шейдер %ls", shaderName);
		return false;
	}

	hr = Engine::GetRender()->m_pd3dDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader[m_pPixelShader.size() - 1]);
	if (FAILED(hr))
	{
		Log::Get()->Err("Не удалось создать пиксельный шейдер");
		return false;
	}

	SAFE_RELEASE(pixelShaderBuffer);

	return true;
}

bool D3D11Framework::Shader::CreateShaderCs(WCHAR *shaderName, CONST D3D_SHADER_MACRO *pDefines, LPCSTR cs)
{
	HRESULT hr = S_OK;
	ID3DBlob *computeShaderBuffer = nullptr;
	m_pComputeShader.resize(m_pComputeShader.size() + 1);

	hr = m_compileshaderfromfile(shaderName, cs, "cs_5_0", pDefines, &computeShaderBuffer);
	if (FAILED(hr))
	{
		Log::Get()->Err("Не удалось загрузить вычислительный шейдер %ls", shaderName);
		return false;
	}

	hr = Engine::GetRender()->m_pd3dDevice->CreateComputeShader(computeShaderBuffer->GetBufferPointer(), computeShaderBuffer->GetBufferSize(), NULL, &m_pComputeShader[m_pComputeShader.size() - 1]);
	if (FAILED(hr))
	{
		Log::Get()->Err("Не удалось создать вычислительный шейдер");
		return false;
	}

	SAFE_RELEASE(computeShaderBuffer);

	return true;
}

HRESULT Shader::m_compileshaderfromfile(WCHAR *FileName, LPCSTR EntryPoint, LPCSTR ShaderModel, CONST D3D_SHADER_MACRO *pDefines, ID3DBlob **ppBlobOut)
{
	HRESULT hr = S_OK;

	DWORD ShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	ShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob *pErrorBlob = nullptr;
	hr = D3DCompileFromFile(FileName, pDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, EntryPoint, ShaderModel, ShaderFlags, 0, ppBlobOut, &pErrorBlob);
	if (FAILED(hr) && pErrorBlob)
		OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
	
	SAFE_RELEASE(pErrorBlob);
	return hr;
}

bool Shader::CreateSamplerState()
{
	HRESULT hr;

	D3D11_SAMPLER_DESC SSDesc;
	SSDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SSDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	SSDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	SSDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	SSDesc.MipLODBias = 0.0f;
	SSDesc.MaxAnisotropy = 1;
	SSDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	SSDesc.BorderColor[0] = 0;
	SSDesc.BorderColor[1] = 0;
	SSDesc.BorderColor[2] = 0;
	SSDesc.BorderColor[3] = 0;
	SSDesc.MinLOD = 0;
	SSDesc.MaxLOD = D3D11_FLOAT32_MAX;

	hr = Engine::GetRender()->m_pd3dDevice->CreateSamplerState(&SSDesc, &m_pSamplerState);
	if (FAILED(hr))
	{
		Log::Get()->Err("Не удалось создать sample state");
		return false;
	}

	return true;
}

void Shader::Bind() const
{
	Engine::GetRender()->GetDeviceContext()->VSSetShader(m_pVertexShader[0], NULL, 0);
	//Framework::Engine::GetRender()->GetDeviceContext()->HSSetShader(m_pHullShader[0], NULL, 0);
	//Framework::Engine::GetRender()->GetDeviceContext()->DSSetShader(m_pDomainShader[0], NULL, 0);
	//Framework::Engine::GetRender()->GetDeviceContext()->GSSetShader(m_pGeometryShader[0], NULL, 0);
	Engine::GetRender()->GetDeviceContext()->PSSetShader(m_pPixelShader[0], NULL, 0);
	//Framework::Engine::GetRender()->GetDeviceContext()->CSSetShader(m_pComputeShader[0], NULL, 0);
}
void Shader::SetUniformBuffer(UniformBufferBP bindingPoint, const DX11_UniformBuffer *uniformBuffer) const
{
	if (!uniformBuffer)
		return;
	for (unsigned int i = 0; i < NUM_SHADER_TYPES; i++)
	{
		uniformBuffer->Bind(bindingPoint, (ShaderTypes)i);
	}
}

void Shader::SetStructuredBuffer(StructuredBufferBP bindingPoint, const DX11_StructuredBuffer *structuredBuffer) const
{
	if (!structuredBuffer)
		return;
	for (unsigned int i = 0; i < NUM_SHADER_TYPES; i++)
	{
		structuredBuffer->Bind(bindingPoint, (ShaderTypes)i);
	}
}

void Shader::SetTexture(TextureBP bindingPoint, const DX11_Texture *texture, const DX12_Sampler *sampler) const
{
	if (!texture)
		return;
	for (unsigned int i = 0; i < NUM_SHADER_TYPES; i++)
	{
		texture->Bind(bindingPoint, (ShaderTypes)i);
		//if (sampler)
			//sampler->Bind(bindingPoint, (ShaderTypes)i);
	}
}

ID3D11VertexShader *Shader::GetVertexShader(int index)
{
	return m_pVertexShader[index];
}

ID3D11HullShader *Shader::GetHullShader(int index)
{
	return m_pHullShader[index];
}

ID3D11DomainShader *Shader::GetDomainShader(int index)
{
	return m_pDomainShader[index];
}

ID3D11PixelShader *Shader::GetPixelShader(int index)
{
	return m_pPixelShader[index];
}

ID3D11GeometryShader *Shader::GetGeometryShader(int index)
{
	return m_pGeometryShader[index];
}

ID3D11ComputeShader *Shader::GetComputeShader(int index)
{
	return m_pComputeShader[index];
}

ID3D11InputLayout *Shader::GetInputLayout()
{
	return m_pInputLayout;
}

void Shader::Close()
{
	SAFE_RELEASE(m_pVertexShader[0]);
	SAFE_RELEASE(m_pHullShader[0]);
	SAFE_RELEASE(m_pDomainShader[0]);
	SAFE_RELEASE(m_pGeometryShader[0]);
	SAFE_RELEASE(m_pPixelShader[0]);
	SAFE_RELEASE(m_pComputeShader[0]);
	SAFE_RELEASE(m_pInputLayout);
	SAFE_DELETE_ARRAY(m_layoutformat);
	SAFE_RELEASE(m_pSamplerState);
}