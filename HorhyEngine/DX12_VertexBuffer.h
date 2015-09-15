#pragma once

#include "macros.h"

namespace D3D11Framework
{
	class DX12_VertexBuffer
	{
	public:
		DX12_VertexBuffer() :
			m_vertexBuffer(NULL),
			m_vertexBufferUpload(NULL),
			vertices(NULL),
			vertexSize(0),
			currentVertexCount(0),
			maxVertexCount(0),
			dynamic(false)
		{
		}

		~DX12_VertexBuffer()
		{
			Release();
		}

		void Release();

		bool Create(unsigned int vertexSize, unsigned int maxVertexCount, bool dynamic);

		void Clear()
		{
			currentVertexCount = 0;
		}

		unsigned int AddVertices(unsigned int numVertices, const float *newVertices);

		bool Update();

		void Bind(ID3D12GraphicsCommandList *pCommandList) const;

		unsigned int GetVertexSize() const
		{
			return vertexSize;
		}

		unsigned int GetVertexCount() const
		{
			return currentVertexCount;
		}

		bool IsDynamic() const
		{
			return dynamic;
		}

	private:
		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
		ComPtr<ID3D12Resource> m_vertexBuffer;
		ComPtr<ID3D12Resource> m_vertexBufferUpload;
		char *vertices;
		unsigned int vertexSize; // size of 1 vertex
		unsigned int currentVertexCount; // current count of vertices 
		unsigned int maxVertexCount; // max count of vertices that vertex buffer can handle
		bool dynamic;

	};
}