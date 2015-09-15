#pragma once

#include "macros.h"
#include "Render.h"

namespace D3D11Framework
{
	// descriptor for setting up DX11_Sampler
	struct SamplerDesc
	{
		SamplerDesc() :
			filter(D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT),
			maxAnisotropy(2),
			adressU(D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
			adressV(D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
			adressW(D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
			minLOD(0.0f),
			maxLOD(FLT_MAX),
			lodBias(0.0f),
			compareFunc(D3D12_COMPARISON_FUNC_LESS_EQUAL)
		{
		}

		bool operator== (const SamplerDesc &desc) const
		{
			if (filter != desc.filter)
				return false;
			if (maxAnisotropy != desc.maxAnisotropy)
				return false;
			if (adressU != desc.adressU)
				return false;
			if (adressV != desc.adressV)
				return false;
			if (adressW != desc.adressW)
				return false;
			if (borderColor != desc.borderColor)
				return false;
			if (!IS_EQUAL(minLOD, desc.minLOD))
				return false;
			if (!IS_EQUAL(maxLOD, desc.maxLOD))
				return false;
			if (!IS_EQUAL(lodBias, desc.lodBias))
				return false;
			if (compareFunc != desc.compareFunc)
				return false;
			return true;
		}

		D3D12_FILTER filter;
		unsigned int maxAnisotropy;
		D3D12_TEXTURE_ADDRESS_MODE adressU;
		D3D12_TEXTURE_ADDRESS_MODE adressV;
		D3D12_TEXTURE_ADDRESS_MODE adressW;
		float borderColor[4];
		float minLOD;
		float maxLOD;
		float lodBias;
		D3D12_COMPARISON_FUNC compareFunc;
	};

	class DX12_Sampler
	{
	public:
		DX12_Sampler()
		{
		}

		~DX12_Sampler()
		{
			Release();
		}

		void Release();

		bool Create(const SamplerDesc &desc);

		void Bind(ID3D12GraphicsCommandList *pCommandList, unsigned int rootParameterIndex) const;

		const SamplerDesc& GetDesc() const
		{
			return desc;
		}

	private:
		SamplerDesc desc;
		CD3DX12_GPU_DESCRIPTOR_HANDLE m_samplerHandle;
	};
}