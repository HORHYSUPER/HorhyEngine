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

	class DX12_VertexLayout
	{
	public:
		DX12_VertexLayout() :
			vertexElementDescs(NULL),
			numVertexElementDescs(0)
		{
		}

		~DX12_VertexLayout()
		{
			Release();
		}

		void Release();

		bool Create(const VertexElementDesc *vertexElementDescs, unsigned int numVertexElementDescs);

		const D3D12_INPUT_LAYOUT_DESC &GetDX12VertexLayoutDesc() const
		{
			return m_inputLayoutDesc;
		}

		unsigned int CalcVertexSize() const;

		bool IsEqual(const VertexElementDesc *vertexElementDescs, unsigned int numVertexElementDescs) const;

		const VertexElementDesc *GetVertexElementDesc() const
		{
			return vertexElementDescs;
		}

		const unsigned int GetVertexElementDescCount() const
		{
			return numVertexElementDescs;
		}

	private:
		D3D12_INPUT_ELEMENT_DESC *m_pVertexDescription;
		D3D12_INPUT_LAYOUT_DESC m_inputLayoutDesc;
		VertexElementDesc *vertexElementDescs;
		unsigned int numVertexElementDescs;

	};
}