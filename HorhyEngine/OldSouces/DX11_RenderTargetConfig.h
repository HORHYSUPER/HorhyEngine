
namespace D3D11Framework
{
	class DX11_RenderTarget;
	class DX11_IUAVBuffer;

#define NUM_UAV_VIEW 4

	enum rtConfigFlags
	{
		DS_READ_ONLY_RTCF = 1, // depth-stencil buffer used as read-only
		COMPUTE_RTCF = 2 // compute shader used
	};

	// descriptor for setting up DX11_RenderTargetConfig
	struct RtConfigDesc
	{
		RtConfigDesc() :
			firstColorBufferIndex(0),
			numColorBuffers(1),
			numUnorderedAccessViews(0),
			flags(0)
		{
			memset(unorderedAccessViews, 0, sizeof(ID3D11UnorderedAccessView*)*NUM_UAV_VIEW);
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
			for (unsigned int i = 0; i < NUM_UAV_VIEW; i++)
			{
				if (unorderedAccessViews[i] != desc.unorderedAccessViews[i])
					return false;
			}
			return true;
		}

		unsigned int firstColorBufferIndex; // index of first render-target to render into 
		unsigned int numColorBuffers; // number of render-targets to render into
		unsigned int numUnorderedAccessViews; // number of unordered access views to write into
		ID3D11UnorderedAccessView *unorderedAccessViews[NUM_UAV_VIEW]; // unordered access views to write into
		unsigned int flags;
	};

	// DX11_RenderTargetConfig
	//
	// Offers possibility to configure a render-target.
	class DX11_RenderTargetConfig
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