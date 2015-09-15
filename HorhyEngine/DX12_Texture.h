#pragma once

namespace D3D11Framework
{
	class DX12_Texture
	{
	public:
		friend class DX12_RenderTarget;

		DX12_Texture() :
			numLevels(1)
		{
			name[0] = 0;
		}

		~DX12_Texture()
		{
			Release();
		}

		void Release();

		bool LoadFromFile(const char *fileName);

		// creates render-target texture
		bool CreateRenderable(unsigned int width, unsigned int height, unsigned int depth, DXGI_FORMAT format,
			unsigned int sampleCountMSAA, unsigned int numLevels = 1, unsigned int rtFlags = 0);

		void Bind(ID3D12GraphicsCommandList *pCommandList, unsigned short rootParameterIndex) const;

		const char* GetName() const
		{
			return name;
		}

		unsigned int GetNumLevels() const
		{
			return numLevels;
		}

	private:
		char name[DEMO_MAX_FILENAME];
		unsigned int numLevels;

		D3D12_GPU_DESCRIPTOR_HANDLE m_textureSrvHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE m_textureUavHandle;
		ComPtr<ID3D12Resource> mTexture;
	};
}