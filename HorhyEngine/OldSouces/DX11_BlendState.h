#pragma once

#include "macros.h"

namespace D3D11Framework
{
	struct BlendDesc
	{
		BlendDesc() :
			srcColorBlend(D3D11_BLEND_ONE),
			dstColorBlend(D3D11_BLEND_ONE),
			blendColorOp(D3D11_BLEND_OP_ADD),
			srcAlphaBlend(D3D11_BLEND_ONE),
			dstAlphaBlend(D3D11_BLEND_ONE),
			blendAlphaOp(D3D11_BLEND_OP_ADD),
			blend(false),
			colorMask(D3D11_COLOR_WRITE_ENABLE_ALL)
		{
		}

		bool operator== (const BlendDesc &desc) const
		{
			if (srcColorBlend != desc.srcColorBlend)
				return false;
			if (dstColorBlend != desc.dstColorBlend)
				return false;
			if (blendColorOp != desc.blendColorOp)
				return false;
			if (srcAlphaBlend != desc.srcAlphaBlend)
				return false;
			if (dstAlphaBlend != desc.dstAlphaBlend)
				return false;
			if (blendAlphaOp != desc.blendAlphaOp)
				return false;
			if (constBlendColor != desc.constBlendColor)
				return false;
			if (blend != desc.blend)
				return false;
			if (colorMask != desc.colorMask)
				return false;
			return true;
		}

		D3D11_BLEND srcColorBlend;
		D3D11_BLEND dstColorBlend;
		D3D11_BLEND_OP blendColorOp;
		D3D11_BLEND srcAlphaBlend;
		D3D11_BLEND dstAlphaBlend;
		D3D11_BLEND_OP blendAlphaOp;
		float constBlendColor[4];
		bool blend;
		unsigned char colorMask;
	};

	class DX11_BlendState
	{
	public:
		DX11_BlendState() :
			blendState(NULL)
		{
		}

		~DX11_BlendState()
		{
			Release();
		}

		void Release();

		bool Create(const BlendDesc &desc);

		void Set() const;

		const BlendDesc& GetDesc() const
		{
			return desc;
		}

	private:
		BlendDesc desc;
		ID3D11BlendState *blendState;

	};
}