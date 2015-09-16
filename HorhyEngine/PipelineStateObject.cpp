#include "stdafx.h"
#include "Engine.h"
#include "Render.h"
#include "DX12_Shader.h"
#include "DX12_VertexLayout.h"
#include "DX12_UniformBuffer.h"
#include "DX12_Texture.h"
#include "DX12_Sampler.h"
#include "DX12_RenderTarget.h"
#include "Util.h"
#include "PipelineStateObject.h"

using namespace D3D11Framework;

bool PIPELINE_STATE_DESC::operator== (const PIPELINE_STATE_DESC &desc) const
{
	if (this->shader != desc.shader)
		return false;
	if (!this->vertexLayout->IsEqual(desc.vertexLayout->GetVertexElementDesc(), desc.vertexLayout->GetVertexElementDescCount()))
		return false;
	if ((this->rasterizerDesc.AntialiasedLineEnable != desc.rasterizerDesc.AntialiasedLineEnable) ||
		(this->rasterizerDesc.ConservativeRaster != desc.rasterizerDesc.ConservativeRaster) ||
		(this->rasterizerDesc.CullMode != desc.rasterizerDesc.CullMode) ||
		(this->rasterizerDesc.DepthBias != desc.rasterizerDesc.DepthBias) ||
		(this->rasterizerDesc.DepthBiasClamp != desc.rasterizerDesc.DepthBiasClamp) ||
		(this->rasterizerDesc.FillMode != desc.rasterizerDesc.FillMode) || 
		(this->rasterizerDesc.ForcedSampleCount != desc.rasterizerDesc.ForcedSampleCount) ||
		(this->rasterizerDesc.FrontCounterClockwise != desc.rasterizerDesc.FrontCounterClockwise) ||
		(this->rasterizerDesc.MultisampleEnable != desc.rasterizerDesc.MultisampleEnable) ||
		(this->rasterizerDesc.SlopeScaledDepthBias != desc.rasterizerDesc.SlopeScaledDepthBias))
		return false;

	return true;
}

PipelineStateObject::PipelineStateObject()
{
	memset(cbvRootParameterIndex, -1, sizeof(int) * (NUM_UNIFORM_BUFFER_BP + NUM_CUSTOM_UNIFORM_BUFFER_BP) * NUM_SHADER_TYPES);
	memset(srvRootParameterIndex, -1, sizeof(int) * (NUM_TEXTURE_BP + NUM_STRUCTURED_BUFFER_BP) * NUM_SHADER_TYPES);
	memset(samplerRootParameterIndex, -1, sizeof(int) * NUM_TEXTURE_BP * NUM_SHADER_TYPES);
}

PipelineStateObject::~PipelineStateObject()
{
}

bool PipelineStateObject::Create(const PIPELINE_STATE_DESC &inPsoDesc)
{
	this->desc = inPsoDesc;

	std::vector<CD3DX12_ROOT_PARAMETER> parameters;

	CD3DX12_DESCRIPTOR_RANGE cbvRanges[NUM_UNIFORM_BUFFER_BP + NUM_CUSTOM_UNIFORM_BUFFER_BP][NUM_SHADER_TYPES];
	for (auto i = 0; i < NUM_UNIFORM_BUFFER_BP + NUM_CUSTOM_UNIFORM_BUFFER_BP; i++)
	{
		for (auto j = 0; j < NUM_SHADER_TYPES; j++)
		{
			if (inPsoDesc.shader->uniformBufferMasks[i] & (1 << j))
			{
				cbvRanges[i][j].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, i);
				CD3DX12_ROOT_PARAMETER parameter;
				parameter.InitAsDescriptorTable(1, &cbvRanges[i][j], (D3D12_SHADER_VISIBILITY)(j + 1));
				parameters.push_back(parameter);
				cbvRootParameterIndex[i][j] = parameters.size() - 1;
			}
		}
	}

	CD3DX12_DESCRIPTOR_RANGE srvRanges[NUM_TEXTURE_BP + NUM_STRUCTURED_BUFFER_BP][NUM_SHADER_TYPES];
	for (auto i = 0; i < NUM_TEXTURE_BP + NUM_STRUCTURED_BUFFER_BP; i++)
	{
		for (auto j = 0; j < NUM_SHADER_TYPES; j++)
		{
			if (inPsoDesc.shader->textureMasks[i] & (1 << j))
			{
				srvRanges[i][j].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, i);
				CD3DX12_ROOT_PARAMETER parameter;
				parameter.InitAsDescriptorTable(1, &srvRanges[i][j], (D3D12_SHADER_VISIBILITY)(j + 1));
				parameters.push_back(parameter);
				srvRootParameterIndex[i][j] = parameters.size() - 1;
			}
		}
	}

	CD3DX12_DESCRIPTOR_RANGE samplerRanges[NUM_TEXTURE_BP][NUM_SHADER_TYPES];
	for (auto i = 0; i < NUM_TEXTURE_BP; i++)
	{
		for (auto j = 0; j < NUM_SHADER_TYPES; j++)
		{
			if (inPsoDesc.shader->samplerMasks[i] & (1 << j))
			{
				samplerRanges[i][j].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, i);
				CD3DX12_ROOT_PARAMETER parameter;
				parameter.InitAsDescriptorTable(1, &samplerRanges[i][j], (D3D12_SHADER_VISIBILITY)(j + 1));
				parameters.push_back(parameter);
				samplerRootParameterIndex[i][j] = parameters.size() - 1;
			}
		}
	}

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
	descRootSignature.Init(parameters.size(), parameters.data(), 0, nullptr, rootSignatureFlags);

	ComPtr<ID3DBlob> pSignature;
	ComPtr<ID3DBlob> pError;
	ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, pSignature.GetAddressOf(), pError.GetAddressOf()));
	ThrowIfFailed(Engine::GetRender()->GetD3D12Device()->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc(D3D12_DEFAULT);
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	depthStencilDesc.StencilEnable = TRUE;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = inPsoDesc.vertexLayout->GetDX12VertexLayoutDesc();
	psoDesc.pRootSignature = m_rootSignature.Get();
	if (inPsoDesc.shader->byteCodes[ShaderTypes::VERTEX_SHADER])
		psoDesc.VS = { reinterpret_cast<UINT8*>(
			inPsoDesc.shader->byteCodes[ShaderTypes::VERTEX_SHADER]->GetBufferPointer()),
			inPsoDesc.shader->byteCodes[ShaderTypes::VERTEX_SHADER]->GetBufferSize() };

	if (inPsoDesc.shader->byteCodes[ShaderTypes::HULL_SHADER])
		psoDesc.HS = { reinterpret_cast<UINT8*>(
			inPsoDesc.shader->byteCodes[ShaderTypes::HULL_SHADER]->GetBufferPointer()),
			inPsoDesc.shader->byteCodes[ShaderTypes::HULL_SHADER]->GetBufferSize() };

	if (inPsoDesc.shader->byteCodes[ShaderTypes::DOMAIN_SHADER])
		psoDesc.DS = { reinterpret_cast<UINT8*>(
			inPsoDesc.shader->byteCodes[ShaderTypes::DOMAIN_SHADER]->GetBufferPointer()),
			inPsoDesc.shader->byteCodes[ShaderTypes::DOMAIN_SHADER]->GetBufferSize() };

	if (inPsoDesc.shader->byteCodes[ShaderTypes::GEOMETRY_SHADER])
		psoDesc.GS = { reinterpret_cast<UINT8*>(
			inPsoDesc.shader->byteCodes[ShaderTypes::GEOMETRY_SHADER]->GetBufferPointer()),
			inPsoDesc.shader->byteCodes[ShaderTypes::GEOMETRY_SHADER]->GetBufferSize() };

	if (inPsoDesc.shader->byteCodes[ShaderTypes::PIXEL_SHADER])
		psoDesc.PS = { reinterpret_cast<UINT8*>(
			inPsoDesc.shader->byteCodes[ShaderTypes::PIXEL_SHADER]->GetBufferPointer()),
			inPsoDesc.shader->byteCodes[ShaderTypes::PIXEL_SHADER]->GetBufferSize() };
	psoDesc.RasterizerState = inPsoDesc.rasterizerDesc;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = inPsoDesc.shader->renderTargets.size();
	for (auto i = 0; i < inPsoDesc.shader->renderTargets.size(); i++)
	{
		psoDesc.RTVFormats[i] = inPsoDesc.renderTartet ?
			inPsoDesc.renderTartet->GetFormat() :
			StringToDXGIFormat(inPsoDesc.shader->renderTargets[i].format);
	}
	psoDesc.DSVFormat = inPsoDesc.renderTartet ?
		inPsoDesc.renderTartet->GetDepthFormat() :
		DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;

	ThrowIfFailed(Engine::GetRender()->GetD3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));

	return true;
}

void PipelineStateObject::SetUniformBuffer(ID3D12GraphicsCommandList *pCommandList, UniformBufferBP bindingPoint, const DX12_UniformBuffer *uniformBuffer) const
{
	if (!uniformBuffer)
		return;

	for (unsigned int i = 0; i < NUM_SHADER_TYPES; i++)
	{
		if (cbvRootParameterIndex[bindingPoint][i] != -1)
		{
			uniformBuffer->Bind(pCommandList, cbvRootParameterIndex[bindingPoint][i]);
		}
	}
}

void PipelineStateObject::SetStructuredBuffer(StructuredBufferBP bindingPoint, const DX12_StructuredBuffer *structuredBuffer) const
{
	if (!structuredBuffer)
		return;

	for (unsigned int i = 0; i < NUM_SHADER_TYPES; i++)
	{
		//if (srvRootParameterIndex[bindingPoint][i] != -1)
		//	structuredBuffer->Bind(bindingPoint, srvRootParameterIndex[bindingPoint][i]);
	}
}

void PipelineStateObject::SetTexture(ID3D12GraphicsCommandList *pCommandList, TextureBP bindingPoint, const DX12_Texture *texture) const
{
	if (!texture)
		return;

	for (unsigned int i = 0; i < NUM_SHADER_TYPES; i++)
	{
		if (srvRootParameterIndex[bindingPoint][i] != -1)
		{
			texture->Bind(pCommandList, srvRootParameterIndex[bindingPoint][i]);
		}
	}
}

void PipelineStateObject::SetSampler(ID3D12GraphicsCommandList *pCommandList, TextureBP bindingPoint, const DX12_Sampler *sampler) const
{
	if (!sampler)
		return;

	for (unsigned int i = 0; i < NUM_SHADER_TYPES; i++)
	{
		if (samplerRootParameterIndex[bindingPoint][i] != -1)
		{
			sampler->Bind(pCommandList, samplerRootParameterIndex[bindingPoint][i]);
		}
	}
}