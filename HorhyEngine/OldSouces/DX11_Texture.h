#pragma once

#include "DX12_Sampler.h"

namespace D3D11Framework
{
	class DX11_Texture
	{
	public:
		friend class Render;
		friend class DX11_RenderTarget;

		DX11_Texture() :
			numLevels(1),
			texture(NULL),
			shaderResourceView(NULL),
			unorderedAccessView(NULL)
		{
			name[0] = 0;
		}

		~DX11_Texture()
		{
			Release();
		}

		void Release();

		bool LoadFromFile(const char *fileName);

		// creates render-target texture
		bool CreateRenderable(unsigned int width, unsigned int height, unsigned int depth, DXGI_FORMAT format,
			unsigned int sampleCountMSAA, unsigned int numLevels = 1, unsigned int rtFlags = 0);

		void Bind(unsigned short bindingPoint, unsigned short shaderType = 0) const;

		const char* GetName() const
		{
			return name;
		}

		unsigned int GetNumLevels() const
		{
			return numLevels;
		}

		ID3D11UnorderedAccessView *GetUnorderdAccessView() const
		{
			return unorderedAccessView;
		}

		ID3D11ShaderResourceView *GetShaderResourceView() const
		{
			return shaderResourceView;
		}

		void SetShaderResourceView(ID3D11ShaderResourceView *newShaderResourceView)
		{
			shaderResourceView = newShaderResourceView;
		}

		static bool IsSrgbFormat(DXGI_FORMAT texFormat);

		static DXGI_FORMAT ConvertToSrgbFormat(DXGI_FORMAT texFormat);

		static DXGI_FORMAT ConvertFromSrgbFormat(DXGI_FORMAT texFormat);

		static DXGI_FORMAT GetDX11TexFormat(DXGI_FORMAT texFormat, bool typeless = false);

	private:
		char name[DEMO_MAX_FILENAME];
		unsigned int numLevels;

		ID3D11Resource *texture;
		ID3D11ShaderResourceView *shaderResourceView;
		ID3D11UnorderedAccessView *unorderedAccessView;

	};
}