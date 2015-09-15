#pragma once

#include "macros.h"

namespace D3D11Framework
{
	struct DepthStencilDesc
	{
		DepthStencilDesc() :
			depthFunc(D3D11_COMPARISON_LESS_EQUAL),
			stencilRef(0),
			stencilMask(~0),
			stencilFailOp(D3D11_STENCIL_OP_KEEP),
			stencilDepthFailOp(D3D11_STENCIL_OP_INCR_SAT),
			stencilPassOp(D3D11_STENCIL_OP_INCR_SAT),
			stencilFunc(D3D11_COMPARISON_ALWAYS),
			depthTest(true),
			depthMask(true),
			stencilTest(false)
		{
		}

		bool operator== (const DepthStencilDesc &desc) const
		{
			if (depthFunc != desc.depthFunc)
				return false;
			if (stencilRef != desc.stencilRef)
				return false;
			if (stencilMask != desc.stencilMask)
				return false;
			if (stencilFailOp != desc.stencilFailOp)
				return false;
			if (stencilDepthFailOp != desc.stencilDepthFailOp)
				return false;
			if (stencilPassOp != desc.stencilPassOp)
				return false;
			if (stencilFunc != desc.stencilFunc)
				return false;
			if (depthTest != desc.depthTest)
				return false;
			if (depthMask != desc.depthMask)
				return false;
			if (stencilTest != desc.stencilTest)
				return false;
			return true;
		}

		D3D11_COMPARISON_FUNC depthFunc;
		unsigned int stencilRef;
		unsigned int stencilMask;
		D3D11_STENCIL_OP stencilFailOp;
		D3D11_STENCIL_OP stencilDepthFailOp;
		D3D11_STENCIL_OP stencilPassOp;
		D3D11_COMPARISON_FUNC stencilFunc;
		bool depthTest;
		bool depthMask;
		bool stencilTest;
	};

	class DX11_DepthStencilState
	{
	public:
		DX11_DepthStencilState() :
			depthStencilState(NULL)
		{
		}

		~DX11_DepthStencilState()
		{
			Release();
		}

		void Release();

		bool Create(const DepthStencilDesc &desc);

		void Set() const;

		const DepthStencilDesc& GetDesc() const
		{
			return desc;
		}

	private:
		DepthStencilDesc desc;
		ID3D11DepthStencilState *depthStencilState;

	};
}