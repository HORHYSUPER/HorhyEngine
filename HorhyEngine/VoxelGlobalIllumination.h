#pragma once

#include "Render.h"
#include "Engine.h"

namespace D3D11Framework
{
	class VoxelGlobalIllumination
	{
	public:
		VoxelGlobalIllumination();
		~VoxelGlobalIllumination();

		bool					Init(float voxelizeDimension, unsigned short voxelizeSize);
		void					PerformVoxelGlobalIllumination();
		float					GetVoxelizeSize();
		unsigned short			GetVoxelizeDimension();

	//private:
		void SwapIndices(unsigned int &indexA, unsigned int &indexB) const;
		void PerformVoxelisationPass();
		void PerformGridLightingPass(int lightType);
		void PerformLightPropagatePass();
		void PerformClearPass();
		
		ID3D11Texture3D				*m_pVoxelGridColorTexture, *m_pVoxelGridColorIlluminatedTexture[2], *m_pVoxelGridNormalTexture;
		ID3D11ShaderResourceView	*m_pVoxelGridColorSRV, *m_pVoxelGridColorIlluminatedSRV[2], *m_pVoxelGridNormalSRV;
		ID3D11UnorderedAccessView	*m_pVoxelGridColorUAV, *m_pVoxelGridColorIlluminatedUAV[2], *m_pVoxelGridNormalUAV;
		DX12_Sampler				*m_pVoxelSampler;
		Shader						*m_pVoxelShader;
		ID3D11Buffer				*m_pVoxelizationBuffer;

		unsigned int inputLightRTIndices;
		unsigned int outputLightRTIndices;
		unsigned int lastInputLightRTIndices;
		float m_voxelizeSize;
		unsigned short m_voxelizeDimension;
	};
}