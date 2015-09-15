#pragma once

#include "Engine.h"
#include "Render.h"

namespace D3D11Framework
{
	class Shader
	{
		friend class Model;
	public:
		Shader();

		void Bind() const;

		void SetUniformBuffer(UniformBufferBP bindingPoint, const DX11_UniformBuffer *uniformBuffer) const;

		void SetStructuredBuffer(StructuredBufferBP bindingPoint, const DX11_StructuredBuffer *structuredBuffer) const;

		void SetTexture(TextureBP bindingPoint, const DX11_Texture *texture, const DX12_Sampler *sampler) const;

		void AddInputElementDesc(const char *SemanticName, UINT inputSlot, DXGI_FORMAT format);
		bool CreateShaderVs(WCHAR *shaderName, CONST D3D_SHADER_MACRO* pDefines, LPCSTR vs);
		bool CreateShaderHs(WCHAR *shaderName, CONST D3D_SHADER_MACRO* pDefines, LPCSTR hs);
		bool CreateShaderDs(WCHAR *shaderName, CONST D3D_SHADER_MACRO* pDefines, LPCSTR ds);
		bool CreateShaderGs(WCHAR *shaderName, CONST D3D_SHADER_MACRO* pDefines, LPCSTR gs);
		bool CreateShaderPs(WCHAR *shaderName, CONST D3D_SHADER_MACRO* pDefines, LPCSTR ps);
		bool CreateShaderCs(WCHAR *shaderName, CONST D3D_SHADER_MACRO* pDefines, LPCSTR cs);
		bool CreateSamplerState();

		ID3D11VertexShader		*GetVertexShader(int index);
		ID3D11HullShader		*GetHullShader(int index);
		ID3D11DomainShader		*GetDomainShader(int index);
		ID3D11PixelShader		*GetPixelShader(int index);
		ID3D11GeometryShader	*GetGeometryShader(int index);
		ID3D11ComputeShader		*GetComputeShader(int index);
		ID3D11InputLayout		*GetInputLayout();

		void Close();

	protected:
		HRESULT m_compileshaderfromfile(WCHAR* FileName, LPCSTR EntryPoint, LPCSTR ShaderModel, CONST D3D_SHADER_MACRO* pDefines, ID3DBlob** ppBlobOut);
		
		std::vector<ID3D11VertexShader*>	m_pVertexShader;
		std::vector<ID3D11HullShader*>		m_pHullShader;
		std::vector<ID3D11DomainShader*>	m_pDomainShader;
		std::vector<ID3D11GeometryShader*>	m_pGeometryShader;
		std::vector<ID3D11PixelShader*>		m_pPixelShader;
		std::vector<ID3D11ComputeShader*>	m_pComputeShader;
		ID3D11InputLayout					*m_pInputLayout;
		ID3D11SamplerState					*m_pSamplerState;
		ID3D10Blob							*m_pHullShaderBuffer;
		ID3D10Blob							*m_pDomainShaderBuffer;

		D3D11_INPUT_ELEMENT_DESC *m_layoutformat;
		unsigned int m_numlayout;
	};
}