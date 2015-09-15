#pragma once

#include "macros.h"

namespace D3D11Framework
{
	struct RasterizerDesc
	{
		RasterizerDesc() :
			fillMode(D3D11_FILL_SOLID),
			cullMode(D3D11_CULL_NONE),
			frontCounterClockwise(true),
			depthBias(D3D11_DEFAULT_DEPTH_BIAS),
			depthBiasClamp(D3D11_DEFAULT_DEPTH_BIAS_CLAMP),
			slopeScaledDepthBias(D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS),
			depthClipEnable(true),
			scissorTest(false),
			multisampleEnable(false),
			antialiasedLineEnable(false)
		{
		}

		bool operator== (const RasterizerDesc &desc) const
		{
			if (fillMode != desc.fillMode)
				return false;
			if (cullMode != desc.cullMode)
				return false;
			if (scissorTest != desc.scissorTest)
				return false;
			if (multisampleEnable != desc.multisampleEnable)
				return false;
			return true;
		}

		D3D11_FILL_MODE fillMode;
		D3D11_CULL_MODE cullMode;
		bool frontCounterClockwise;
		INT depthBias;
		FLOAT depthBiasClamp;
		FLOAT slopeScaledDepthBias;
		bool depthClipEnable;
		bool scissorTest;
		bool multisampleEnable;
		bool antialiasedLineEnable;
	};

	class DX11_RasterizerState
	{
	public:
		DX11_RasterizerState() :
			rasterizerState(NULL)
		{
		}

		~DX11_RasterizerState()
		{
			Release();
		}

		void Release();

		bool Create(const RasterizerDesc &desc);

		void Set() const;

		const RasterizerDesc& GetDesc() const
		{
			return desc;
		}

	private:
		RasterizerDesc desc;
		ID3D11RasterizerState *rasterizerState;

	};
}