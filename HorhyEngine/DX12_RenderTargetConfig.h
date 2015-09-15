
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

		unsigned int firstColorBufferIndex; // index of first render-target to render into 
		unsigned int numColorBuffers; // number of render-targets to render into
		unsigned int numUnorderedAccessViews; // number of unordered access views to write into
		unsigned int flags;
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