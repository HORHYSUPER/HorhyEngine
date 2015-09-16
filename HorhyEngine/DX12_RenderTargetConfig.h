#pragma once

#define MAX_NUM_VIEWPORTS 8

namespace D3D11Framework
{
	class DX12_RenderTarget;

	enum rtConfigFlags
	{
		DS_READ_ONLY_RTCF = 1, // depth-stencil buffer used as read-only
		COMPUTE_RTCF = 2 // compute shader used
	};

	struct RtConfigDesc
	{
		RtConfigDesc() :
			firstColorBufferIndex(0),
			numColorBuffers(1),
			numUnorderedAccessViews(0),
			flags(0)
		{
			for (unsigned int i = 0; i < MAX_NUM_VIEWPORTS; i++)
			{
				viewports[i].Height = 0.0f;
				viewports[i].Width = 0.0f;
				viewports[i].MaxDepth = 1.0f;
				viewports[i].MinDepth = 0.0f;
				viewports[i].TopLeftX = 0;
				viewports[i].TopLeftY = 0;
				scissorRects[i].right = 0;
				scissorRects[i].bottom = 0;
				scissorRects[i].left = 0;
				scissorRects[i].top = 0;
			}
		}

		bool operator== (const RtConfigDesc &desc) const
		{
			if (firstColorBufferIndex != desc.firstColorBufferIndex)
				return false;
			if (numColorBuffers != desc.numColorBuffers)
				return false;
			if (numUnorderedAccessViews != desc.numUnorderedAccessViews)
				return false;
			if (flags != desc.flags)
				return false;
			return true;
		}

		unsigned int CalcNumViewPorts() const
		{
			unsigned int numViewPorts = 0;
			for (unsigned int i = 0; i < MAX_NUM_VIEWPORTS; i++)
			{
				if (viewports[i].Height != 0)
					numViewPorts++;
				else
					break;
			}
			return numViewPorts;
		}

		unsigned int firstColorBufferIndex; // index of first render-target to render into 
		unsigned int numColorBuffers; // number of render-targets to render into
		unsigned int numUnorderedAccessViews; // number of unordered access views to write into
		unsigned int flags;
		D3D12_VIEWPORT viewports[MAX_NUM_VIEWPORTS];
		D3D12_RECT scissorRects[MAX_NUM_VIEWPORTS];
	};

	class DX12_RenderTargetConfig
	{
	public:
		bool Create(const RtConfigDesc &desc)
		{
			this->desc = desc;
			return true;
		}

		const RtConfigDesc& GetDesc() const
		{
			return desc;
		}

	private:
		RtConfigDesc desc;

	};
}