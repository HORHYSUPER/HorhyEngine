#pragma once

#include "macros.h"

namespace D3D11Framework
{
	class DX12_UniformBuffer
	{
	public:
		friend class Render;

		DX12_UniformBuffer() :
			size(0),
			m_mappedConstantBuffer(nullptr)
		{
		}

		~DX12_UniformBuffer()
		{
			Release();
		}

		void Release();

		bool Create(unsigned int bufferSize);

		// Please note: uniforms must be aligned according to the HLSL rules, in order to be able
		// to upload data in 1 block.
		bool Update(const void *uniformBufferData);

		void Bind(ID3D12GraphicsCommandList *pCommandList, unsigned int rootParameterIndex) const;

	private:
		unsigned int size; // size of uniform data
		Microsoft::WRL::ComPtr<ID3D12Resource>			m_uniformBuffer;
		D3D12_GPU_DESCRIPTOR_HANDLE						m_cbvHandle;
		UINT8*											m_mappedConstantBuffer;
		UINT											c_alignedConstantBufferSize;

	};
}