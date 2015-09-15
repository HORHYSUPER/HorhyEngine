#pragma once

#include "macros.h"

namespace D3D11Framework
{
	enum vertexElements
	{
		POSITION_ELEMENT = 0,
		TEXCOORDS_ELEMENT,
		NORMAL_ELEMENT,
		BINORMAL_ELEMENT,
		TANGENT_ELEMENT,
		COLOR_ELEMENT,
		BONE_INDICES_ELEMENT,
		BONE_WEIGHTS_ELEMENT
	};

	enum ElementFormats
	{
		R32_FLOAT_EF = 0,
		R32G32_FLOAT_EF,
		R32G32B32_FLOAT_EF,
		R32G32B32A32_FLOAT_EF
	};

	struct VertexElementDesc
	{
		vertexElements vertexElement;
		unsigned int semanticIndex;
		ElementFormats format;
		unsigned int offset;
	};

	class DX11_VertexLayout
	{
	public:
		DX11_VertexLayout() :
			inputLayout(NULL),
			vertexElementDescs(NULL),
			numVertexElementDescs(0)
		{
		}

		~DX11_VertexLayout()
		{
			Release();
		}

		void Release();

		bool Create(const VertexElementDesc *vertexElementDescs, unsigned int numVertexElementDescs);

		void Bind() const;

		unsigned int CalcVertexSize() const;

		bool IsEqual(const VertexElementDesc *vertexElementDescs, unsigned int numVertexElementDescs) const;

	private:
		ID3D11InputLayout *inputLayout;
		VertexElementDesc *vertexElementDescs;
		unsigned int numVertexElementDescs;

	};
}