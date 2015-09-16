#pragma once

// max number of color-buffers, which can be attached to 1 render-target
#define MAX_NUM_COLOR_BUFFERS 8

using namespace Microsoft::WRL;
using namespace DirectX;

namespace D3D11Framework
{
	enum renderTargetFlags
	{
		TEXTURE_CUBE_RTF = 1,
		TEXTURE_3D_RTF = 2, // use 3D texture for render-target
		UAV_RTF = 3, // create unordered access view
		SRGB_RTF = 4 // convert color from linear to SRGB space
	};

	class DX12_Texture;
	class DX12_Sampler;
	class DX12_RenderTargetConfig;

	// descriptor for a render-target buffer
	struct RtBufferDesc
	{
		RtBufferDesc() :
			format(DXGI_FORMAT_UNKNOWN),
			rtFlags(0)
		{
		}

		DXGI_FORMAT format;
		unsigned int rtFlags;
	};

	// descriptor for setting up DX11_RenderTarget
	struct RenderTargetDesc
	{
		RenderTargetDesc() :
			width(0),
			height(0),
			depth(1),
			numLevels(1),
			sampleCountMSAA(1)
		{
		}

		unsigned int CalcNumColorBuffers() const
		{
			unsigned int numColorBuffers = 0;
			for (unsigned int i = 0; i < MAX_NUM_COLOR_BUFFERS; i++)
			{
				if (colorBufferDescs[i].format != DXGI_FORMAT_UNKNOWN)
					numColorBuffers++;
				else
					break;
			}
			return numColorBuffers;
		}

		unsigned int width, height, depth;
		unsigned int numLevels;
		unsigned int sampleCountMSAA;
		RtBufferDesc colorBufferDescs[MAX_NUM_COLOR_BUFFERS];
		RtBufferDesc depthStencilBufferDesc;
	};

	class DX12_RenderTarget
	{
		friend class Render;
	public:
		DX12_RenderTarget() :
			width(0),
			height(0),
			depth(0),
			numColorBuffers(0),
			rtFlags(0),
			clearMask(0),
			frameBufferTextures(NULL),
			depthStencilTexture(NULL),
			clearTarget(true)
		{
			memset(dsvHandles, 0, sizeof(CD3DX12_CPU_DESCRIPTOR_HANDLE*) * 2);
		}

		~DX12_RenderTarget()
		{
			Release();
		}

		void Release();

		bool Create(const RenderTargetDesc &desc);

		bool CreateBackBuffer(UINT bufferIndex);

		void Bind(ID3D12GraphicsCommandList *pCommandList, const DX12_RenderTargetConfig *rtConfig = NULL);

		// indicate, that render-target should be cleared
		void Reset()
		{
			clearTarget = true;
		}

		void Clear(ID3D12GraphicsCommandList *pCommandList, unsigned int newClearMask) const;

		DX12_Texture* GetTexture(unsigned int index = 0) const
		{
			assert(index < numColorBuffers);
			return frameBufferTextures[index];
		}

		DX12_Texture* GetDepthStencilTexture() const
		{
			return depthStencilTexture;
		}

		unsigned int GetWidth() const
		{
			return width;
		}

		unsigned int GetHeight() const
		{
			return height;
		}

		unsigned int GetDepth() const
		{
			return depth;
		}

		DXGI_FORMAT GetFormat(unsigned int index = 0) const
		{
			assert(index < numColorBuffers);
			return rtFormats[index];
		}

		DXGI_FORMAT GetDepthFormat() const
		{
			return depthFormat;
		}

	private:
		unsigned int width, height, depth;
		unsigned int numColorBuffers;
		unsigned int rtFlags;
		unsigned int clearMask;
		CD3DX12_CPU_DESCRIPTOR_HANDLE *rtvHandles;
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandles[2];
		ComPtr<ID3D12Resource>	m_renderTarget;
		vector<DX12_Texture*>	frameBufferTextures;
		DXGI_FORMAT				*rtFormats;
		DXGI_FORMAT				depthFormat;
		DX12_Texture			*depthStencilTexture;
		D3D12_VIEWPORT			m_viewport;
		D3D12_RECT				m_scissorRect;
		bool					clearTarget;

	};
}