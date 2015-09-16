#pragma once

#include "GpuCmd.h"

using namespace std;
using namespace Microsoft::WRL;
using namespace DirectX;

const float SCREEN_DEPTH = 1000.0f;
const float SCREEN_NEAR = 0.1f;
const int BUFFER_COUNT = 4;

namespace D3D11Framework
{
	class Engine;
	class Text;
	class BitmapFont;
	class OrthoWindow;
	class Frustum;
	class VoxelGlobalIllumination;
	class PipelineStateObject;
	//class DX11_VertexBuffer;
	//class DX11_IndexBuffer;
	//class DX11_StructuredBuffer;
	class DX12_RenderTargetConfig;
	//class DX11_DepthStencilState;
	//class DX11_BlendState;
	//class DX11_Texture;
	class DX12_VertexLayout;
	class DX12_VertexBuffer;
	class DX12_IndexBuffer;
	class DX12_UniformBuffer;
	class DX12_RenderTarget;
	class DX12_Sampler;
	class PIPELINE_STATE_DESC;
	class SamplerDesc;
	class RenderTargetDesc;
	class VertexElementDesc;
	class RasterizerDesc;
	class DepthStencilDesc;
	class BlendDesc;
	class RtConfigDesc;
	enum POST_PROCESSING;

	struct VoxelizationBuffer
	{
		float voxelizeSize;
		float voxelSize;
		float voxelizeDimension;
		XMVECTOR voxelizePosition;
	};


	enum ShaderTypes
	{
		VERTEX_SHADER = 0,
		HULL_SHADER,
		DOMAIN_SHADER,
		GEOMETRY_SHADER,
		PIXEL_SHADER,
		COMPUTE_SHADER
	};

	enum TextureBP
	{
		COLOR_TEX_BP = 0,
		NORMAL_TEX_BP,
		SPECULAR_TEX_BP,
		CUSTOM0_TEX_BP,
		CUSTOM1_TEX_BP,
		CUSTOM2_TEX_BP,
		CUSTOM3_TEX_BP,
		CUSTOM4_TEX_BP,
		CUSTOM5_TEX_BP,
		CUSTOM6_TEX_BP
	};

	enum TextureID
	{
		COLOR_TEX_ID = COLOR_TEX_BP,
		NORMAL_TEX_ID = NORMAL_TEX_BP,
		SPECULAR_TEX_ID = SPECULAR_TEX_BP,
		CUSTOM0_TEX_ID = CUSTOM0_TEX_BP,
		CUSTOM1_TEX_ID = CUSTOM1_TEX_BP,
		CUSTOM2_TEX_ID = CUSTOM2_TEX_BP,
		CUSTOM3_TEX_ID = CUSTOM3_TEX_BP,
		CUSTOM4_TEX_ID = CUSTOM4_TEX_BP,
		CUSTOM5_TEX_ID = CUSTOM5_TEX_BP,
		CUSTOM6_TEX_ID = CUSTOM6_TEX_BP
	};

	enum SamplerID
	{
		LINEAR_SAMPLER_ID = 0,
		TRILINEAR_SAMPLER_ID,
		SHADOW_MAP_SAMPLER_ID
	};

	enum UniformBufferBP
	{
		CAMERA_UB_BP = 0,
		LIGHT_UB_BP,
		CUSTOM0_UB_BP,
		CUSTOM1_UB_BP
	};

	enum StructuredBufferBP
	{
		CUSTOM0_SB_BP = 10,
		CUSTOM1_SB_BP = 11
	};

	enum RenderTargetID
	{
		BACK_BUFFER_RT_ID = 0, // back buffer
		GBUFFERS_RT_ID = FrameCount, // geometry buffers
		SHADOW_MAP_RT_ID // shadow map
	};

	class Render
	{
		friend class Engine;
		friend class Shader;
		friend class ShaderSimple;
		friend class Model;

	public:
		Render();
		Render(const Render &copy);
		~Render();

		void* operator new(size_t i)
		{
			return _aligned_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_aligned_free(p);
		}

		Render & operator = (const Render &other);

		bool Create(HWND hwnd);
		void BeginFrame();
		bool Draw();
		void TurnZBufferOn();
		void TurnZBufferOff();
		void TurnOnAlphaBlending();
		void TurnOffAlphaBlending();
		void EndFrame();
		void AddGpuCmd(const GpuCmd &gpuCmd);

		// set draw states for passed draw command
		void SetDrawStates(const DrawCmd &cmd);

		// set shader states for passed shader command
		void SetShaderStates(const ShaderCmd &cmd, gpuCmdOrders order);

		// set shader params for passed shader command
		void SetShaderParams(const ShaderCmd &cmd);

		void Draww(DrawCmd &cmd);

		//void Dispatch(ComputeCmd &cmd);

		//void UnbindShaderResources();

		//void GenerateMips(GenerateMipsCmd &cmd);

		PipelineStateObject		*CreatePipelineStateObject(const PIPELINE_STATE_DESC &desc);
		//DX11_DepthStencilState	*CreateDepthStencilState(const DepthStencilDesc &desc);
		//DX11_BlendState			*CreateBlendState(const BlendDesc &desc);
		DX12_RenderTargetConfig	*CreateRenderTargetConfig(const RtConfigDesc &desc);
		DX12_RenderTarget		*CreateRenderTarget(const RenderTargetDesc &desc);
		DX12_RenderTarget		*CreateBackBufferRt(UINT bufferIndex);
		DX12_UniformBuffer		*CreateUniformBuffer(unsigned int bufferSize);
		DX12_Sampler			*CreateSampler(const SamplerDesc &desc);
		DX12_VertexLayout		*CreateVertexLayout(const VertexElementDesc *vertexElementDescs, unsigned int numVertexElementDescs);
		DX12_VertexBuffer		*CreateVertexBuffer(unsigned int vertexSize, unsigned int maxVertexCount, bool dynamic);
		DX12_IndexBuffer		*CreateIndexBuffer(unsigned int maxIndexCount, bool dynamic);

		DX12_RenderTarget* GetRenderTarget(unsigned int ID) const
		{
			assert(ID < renderTargets.size());
			return renderTargets[ID];
		}

		DX12_Sampler* GetSampler(unsigned int ID) const
		{
			assert(ID < samplers.size());
			return samplers[ID];
		}

		ComPtr<IDXGISwapChain3> GetSwapChain() const
		{
			return m_swapChain;
		}

		VoxelGlobalIllumination *GetVoxelGlobalIllumination() const
		{
			return m_pVXGI;
		}

		UINT					GetScreenWidth();
		UINT					GetScreenHeight();
		Engine					*GetFramework();
		ID3D11Device			*GetDevice();
		ComPtr<ID3D12Device>	GetD3D12Device();
		ID3D11DeviceContext		*GetDeviceContext();
		D3D11_VIEWPORT			*GetViewport();
		Camera					*GetCamera();
		std::vector<Model*>		m_objectListOnRender;

		bool vxgi = false;
		float coneSlope = 0.05f;
		// DX 12 ---------------------------
		ComPtr<ID3D12CommandQueue> m_commandQueue;
		ComPtr<ID3D12CommandAllocator> m_commandAllocator[3];
		ComPtr<ID3D12GraphicsCommandList> m_commandList[3];

		// Adapter info.
		bool m_useWarpDevice;

		UINT64 m_fenceValue;
		ComPtr<ID3D12Fence> m_fence;
		HANDLE m_fenceEvent;
		UINT m_frameIndex;
		UINT m_rtvDescriptorSize;

		// Directx 12 pipeline objects.
		ComPtr<IDXGISwapChain3> m_swapChain;
		ComPtr<ID3D12Device> m_device;
		ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
		ComPtr<ID3D12Resource> m_depthStencil;
		ComPtr<ID3D12RootSignature> m_rootSignature;
		ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
		ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
		ComPtr<ID3D12DescriptorHeap> m_cbvSrvHeap;
		ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
		ComPtr<ID3D12PipelineState> m_pipelineState;
		ComPtr<ID3D12PipelineState> m_pipelineStateShadowMap;
		UINT												m_cbvDescriptorSize;
		UINT8*												m_mappedConstantBuffer;
		UINT												m_cbvSrvUavHeapElements;
		UINT												m_rtvHeapElements;
		UINT												m_dsvHeapElements;
		UINT												m_samplerHeapElements;
		// DX 12 ---------------------------

	protected:
		void m_PostProcessing(POST_PROCESSING postProcessing);
		bool m_CreateDevice();
		bool m_CreateDepthStencil();
		bool m_CreateBlendingState();
		void m_InitializeMatrix();
		void m_Resize();

		D3D11_VIEWPORT			m_viewport;
		ID3D11Device			*m_pd3dDevice;
		ID3D11DeviceContext		*m_pImmediateContext;
		IDXGISwapChain			*m_pSwapChain;
		ID3D11SamplerState		*m_pSamplerStatePoint;
		ID3D11SamplerState		*m_pSamplerStateCmp;
		ID3D11DepthStencilState	*m_pDepthStencilState;
		ID3D11DepthStencilState	*m_pDepthDisabledStencilState;
		ID3D11BlendState		*m_pAlphaEnableBlendingState;
		ID3D11BlendState		*m_pAlphaDisableBlendingState;

		vector<GpuCmd>					gpuCmds;
		vector<PipelineStateObject*>	m_pipelineStateObjects;
		GpuCmd							lastGpuCmd;
		vector<DX12_VertexLayout*>		vertexLayouts;
		vector<DX12_VertexBuffer*>		vertexBuffers;
		vector<DX12_IndexBuffer*>		indexBuffers;
		vector<DX12_UniformBuffer*>		uniformBuffers;
		vector<DX12_RenderTarget*>		renderTargets;
		vector<DX12_RenderTargetConfig*>renderTargetConfigs;
		vector<DX12_Sampler*>			samplers;
		//vector<DX11_DepthStencilState*>	depthStencilStates;
		//vector<DX11_BlendState*>		blendStates;
		DX12_VertexLayout				*baseVertexLayout;

		OrthoWindow					*m_pFullScreenWindow;
		VoxelGlobalIllumination		*m_pVXGI;
		Shader						*m_pLightShader;
		BitmapFont					*m_font;
		Text						*m_text;
		Engine						*m_pFramework;
		Camera						*m_pCamera;
		Frustum						*m_pFrustum;
		//DX11_RasterizerState		*m_pRasterState, *m_pRasterStateWireFrame;
		DX12_UniformBuffer			*m_pUniformBuffer;
		DX12_UniformBuffer			*m_pParamsUB;
		//CameraBufferData			m_BufferData;
		HWND m_hwnd;
		bool m_upcam;
		bool m_downcam;
		bool m_leftcam;
		bool m_rightcam;
		bool m_isWireFrameRender;
		UINT m_width;
		UINT m_height;
	};
}