#pragma once

using namespace Microsoft::WRL;

namespace D3D11Framework
{
	class DX12_Shader;
	class DX12_UniformBuffer;
	class DX12_VertexLayout;
	class DX12_StructuredBuffer;
	class DX12_Texture;
	class DX12_Sampler;
	
	struct PIPELINE_STATE_DESC
	{
		PIPELINE_STATE_DESC() :
			shader(NULL),
			vertexLayout(NULL),
			renderTartet(NULL),
			rasterizerDesc(D3D12_DEFAULT)
		{}

		bool operator==(const PIPELINE_STATE_DESC &desc) const;

		DX12_Shader *shader;
		DX12_VertexLayout *vertexLayout;
		DX12_RenderTarget *renderTartet;
		CD3DX12_RASTERIZER_DESC rasterizerDesc;
	};
	
	class PipelineStateObject
	{
		friend class Render;
	public:
		PipelineStateObject();
		~PipelineStateObject();

		bool Create(const PIPELINE_STATE_DESC &psoDesc);

		void SetUniformBuffer(ID3D12GraphicsCommandList *pCommandList, UniformBufferBP bindingPoint, const DX12_UniformBuffer *uniformBuffer) const;

		void SetStructuredBuffer(StructuredBufferBP bindingPoint, const DX12_StructuredBuffer *structuredBuffer) const;

		void SetTexture(ID3D12GraphicsCommandList *pCommandList, TextureBP bindingPoint, const DX12_Texture *texture) const;

		void SetSampler(ID3D12GraphicsCommandList *pCommandList, TextureBP bindingPoint, const DX12_Sampler *sampler) const;

		const PIPELINE_STATE_DESC &GetDesc() const
		{
			return desc;
		}
		
	private:
		int cbvRootParameterIndex[NUM_UNIFORM_BUFFER_BP + NUM_CUSTOM_UNIFORM_BUFFER_BP][NUM_SHADER_TYPES];
		int srvRootParameterIndex[NUM_TEXTURE_BP + NUM_STRUCTURED_BUFFER_BP][NUM_SHADER_TYPES];
		int samplerRootParameterIndex[NUM_TEXTURE_BP][NUM_SHADER_TYPES];
		PIPELINE_STATE_DESC desc;
		ComPtr<ID3D12PipelineState> m_pipelineState;
		ComPtr<ID3D12RootSignature> m_rootSignature;

	};
}