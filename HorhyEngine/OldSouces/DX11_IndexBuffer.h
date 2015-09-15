#pragma once

#include "macros.h"

namespace D3D11Framework
{
	class DX11_IndexBuffer
	{
	public:
		DX11_IndexBuffer() :
			indexBuffer(NULL),
			indices(NULL),
			currentIndexCount(0),
			maxIndexCount(0),
			dynamic(false)
		{
		}

		~DX11_IndexBuffer()
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

		void Bind() const;

		unsigned int GetIndexCount() const
		{
			return currentIndexCount;
		}

		bool IsDynamic() const
		{
			return dynamic;
		}

	private:
		ID3D11Buffer *indexBuffer;
		unsigned int *indices;
		unsigned int currentIndexCount; // current count of indices
		unsigned int maxIndexCount; // max count of indices that index buffer can handle
		bool dynamic;

	};
}