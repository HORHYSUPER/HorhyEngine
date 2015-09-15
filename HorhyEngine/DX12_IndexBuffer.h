#pragma once

#include "macros.h"

namespace D3D11Framework
{
	class DX12_IndexBuffer
	{
	public:
		DX12_IndexBuffer() :
			m_indexBuffer(NULL),
			m_indexBufferUpload(NULL),
			indices(NULL),
			currentIndexCount(0),
			maxIndexCount(0),
			dynamic(false)
		{
		}

		~DX12_IndexBuffer()
		{
			Release();
		}

		void Release();

		bool Create(unsigned int maxIndexCount, bool dynamic);

		void Clear()
		{
			currentIndexCount = 0;
		}

		unsigned int AddIndices(unsigned int numIndices, const unsigned int *newIndices);

		bool Update();

		void Bind(ID3D12GraphicsCommandList *pCommandList) const;

		unsigned int GetIndexCount() const
		{
			return currentIndexCount;
		}

		bool IsDynamic() const
		{
			return dynamic;
		}

	private:
		D3D12_INDEX_BUFFER_VIEW				m_indexBufferView;
		ComPtr<ID3D12Resource>				m_indexBuffer;
		ComPtr<ID3D12Resource>				m_indexBufferUpload;
		unsigned int *indices;
		unsigned int currentIndexCount; // current count of indices
		unsigned int maxIndexCount; // max count of indices that index buffer can handle
		bool dynamic;

	};
}