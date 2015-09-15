#pragma once

#include "macros.h"

namespace D3D11Framework
{
	class DX11_VertexBuffer
	{
	public:
		DX11_VertexBuffer() :
			vertexBuffer(NULL),
			vertices(NULL),
			vertexSize(0),
			currentVertexCount(0),
			maxVertexCount(0),
			dynamic(false)
		{
		}

		~DX11_VertexBuffer()
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

		void Bind() const;

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
		ID3D11Buffer *vertexBuffer;
		char *vertices;
		unsigned int vertexSize; // size of 1 vertex
		unsigned int currentVertexCount; // current count of vertices 
		unsigned int maxVertexCount; // max count of vertices that vertex buffer can handle
		bool dynamic;

	};
}