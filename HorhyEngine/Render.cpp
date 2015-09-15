#include "stdafx.h"
#include "Util.h"
#include "Buffer.h"
#include "Render.h"
#include "Camera.h"
#include "OrthoWindow.h"
#include "Frustum.h"
#include "macros.h"
#include "Scene.h"
#include "Model.h"
#include "VoxelGlobalIllumination.h"
#include "SkyBox.h"
#include "Timer.h"
#include "BitmapFont.h"
#include "Text.h"
#include "GameWorld.h"
#include "DX12_Sampler.h"
#include "DX12_RenderTarget.h"
#include "DX12_RenderTargetConfig.h"
#include "DX12_VertexLayout.h"
#include "DX12_VertexBuffer.h"
#include "DX12_IndexBuffer.h"
#include "DX12_Texture.h"
#include "DX12_Shader.h"
#include "DX12_UniformBuffer.h"
#include "DirectionalLight.h"
#include "PipelineStateObject.h"
#include "ResourceManager.h"
#include "Engine.h"

using namespace std;
using namespace D3D11Framework;

float g_accumulateTime = 0.0f;
float g_fps;
const float IntermediateClearColor[4] = { 0.0f, 0.2f, 0.3f, 1.0f };

D3D12_GPU_DESCRIPTOR_HANDLE m_sceneCbvHandle[FrameCount];
PipelineStateObject *pFullScreenQuadPso = NULL;
ComPtr<ID3D12Resource> m_intermediateRenderTarget;
D3D12_GPU_DESCRIPTOR_HANDLE m_textureSrvHandle;

Render::Render()
{
	m_pCamera = NULL;
	m_pLightShader = NULL;
	m_font = NULL;
	m_text = NULL;
	m_pd3dDevice = NULL;
	m_pImmediateContext = NULL;
	m_pSwapChain = NULL;
	m_pDepthStencilState = NULL;
	m_pDepthDisabledStencilState = NULL;
	m_pVXGI = NULL;
	m_isWireFrameRender = FALSE;
	m_upcam = FALSE;
	m_downcam = FALSE;
	m_leftcam = FALSE;
	m_rightcam = FALSE;
	m_cbvSrvUavHeapElements = 0;
	m_rtvHeapElements = 0;
	m_dsvHeapElements = 0;
	m_samplerHeapElements = 0;
}

Render::Render(const Render &copy)
{
	this->m_pCamera = copy.m_pCamera;
	this->m_downcam = copy.m_downcam;
}

Render::~Render()
{
	if (m_pImmediateContext)
		m_pImmediateContext->ClearState();

	SAFE_RELEASE(m_pAlphaEnableBlendingState);
	SAFE_RELEASE(m_pAlphaDisableBlendingState);
	SAFE_RELEASE(m_pDepthStencilState);
	SAFE_RELEASE(m_pDepthDisabledStencilState);
	SAFE_RELEASE(m_pSwapChain);
	SAFE_RELEASE(m_pImmediateContext);
	SAFE_RELEASE(m_pd3dDevice);
	SAFE_DELETE(m_pVXGI);
}

Render &Render::operator=(const Render &other)
{
	return *this;
}

bool Render::Create(HWND hwnd)
{
	//Render *rendr = Framework::GetRender();
	m_hwnd = hwnd;

	RECT rc;
	GetClientRect(m_hwnd, &rc);
	m_width = rc.right - rc.left;
	m_height = rc.bottom - rc.top;

	if (!m_CreateDevice())
	{
		Log::Get()->Err("Не удалось создать DirectX Device");
		return false;
	}

	//if (!m_CreateDepthStencil())
	//{
	//	Log::Get()->Err("Не удалось создать буфер глубины");
	//	return false;
	//}

	//if (!m_CreateBlendingState())
	//{
	//	Log::Get()->Err("Не удалось создать blending state");
	//	return false;
	//}

	//RasterizerDesc rasterDesc;
	//rasterDesc.antialiasedLineEnable = true;
	//rasterDesc.cullMode = D3D11_CULL_NONE;
	//rasterDesc.depthBias = 0;
	//rasterDesc.depthBiasClamp = 0.0f;
	//rasterDesc.depthClipEnable = true;
	//rasterDesc.fillMode = D3D11_FILL_SOLID;
	//rasterDesc.frontCounterClockwise = false;
	//rasterDesc.multisampleEnable = true;
	//rasterDesc.scissorTest = false;
	//rasterDesc.slopeScaledDepthBias = 0.0f;
	//m_pRasterState = CreateRasterizerState(rasterDesc);
	//if (!m_pRasterState)
	//	return false;

	//rasterDesc.fillMode = D3D11_FILL_WIREFRAME;
	//m_pRasterStateWireFrame = CreateRasterizerState(rasterDesc);
	//if (!m_pRasterStateWireFrame)
	//	return false;

	m_viewport.Width = (FLOAT)m_width;
	m_viewport.Height = (FLOAT)m_height;
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;
	m_viewport.TopLeftX = 0;
	m_viewport.TopLeftY = 0;

	//_MUTEXLOCK(m_pFramework->GetMutex());
	//m_pImmediateContext->RSSetViewports(1, &m_viewport);
	//_MUTEXUNLOCK(m_pFramework->GetMutex());

	m_pCamera = new Camera();

	m_InitializeMatrix();

	// create frequently used samplers

	// LINEAR_SAMPLER
	SamplerDesc samplerDesc;
	if (!CreateSampler(samplerDesc))
		return false;

	// TRILINEAR_SAMPLER
	samplerDesc.filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.adressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.adressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.adressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	if (!CreateSampler(samplerDesc))
		return false;

	// SHADOW_MAP_SAMPLER
	samplerDesc.filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	samplerDesc.adressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.adressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.adressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.compareFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	if (!CreateSampler(samplerDesc))
		return false;

	//m_pUniformBuffer = Engine::m_pRender->CreateUniformBuffer(sizeof(CameraBufferData));
	//if (!m_pUniformBuffer)
	//	return false;

	//m_pParamsUB = Engine::m_pRender->CreateUniformBuffer(sizeof(float));
	//if (!m_pParamsUB)
	//	return false;

	// BACK_BUFFER_RT 
	for (auto i = 0; i < FrameCount; i++)
		if (!CreateBackBufferRt(i))
			return false;

	{
		RenderTargetDesc rtDesc;
		rtDesc.width = m_width;
		rtDesc.height = m_height;
		rtDesc.sampleCountMSAA = MSAA_COUNT;
		rtDesc.colorBufferDescs[0].format = DXGI_FORMAT_R32G32B32A32_FLOAT;	// accumulation buffer
		rtDesc.colorBufferDescs[1].format = DXGI_FORMAT_R32G32B32A32_FLOAT;	// xyz - World pos, w - material
		rtDesc.colorBufferDescs[2].format = DXGI_FORMAT_R32G32B32A32_FLOAT;	// rgb - Color, a - specular level
		rtDesc.colorBufferDescs[3].format = DXGI_FORMAT_R32G32B32A32_FLOAT;	// xyz - Normal, w - unused
		rtDesc.depthStencilBufferDesc.format = DXGI_FORMAT_D32_FLOAT;
		if (!CreateRenderTarget(rtDesc))
			return false;
	}

	// Full Screen Quad initialization
	{
		string msaa = to_string(static_cast<int>(MSAA_COUNT));

		D3D_SHADER_MACRO defines[] =
		{
			"MSAA", msaa.c_str(),
			NULL, NULL
		};
		DX12_Shader *fullScreenQuadShader = new DX12_Shader();
		fullScreenQuadShader->Load("FullScreenQuad.sdr", defines);

		DX12_VertexLayout *vertexLayout = new DX12_VertexLayout();

		VertexElementDesc vertexElementDescs[] = { POSITION_ELEMENT, 0, R32G32B32_FLOAT_EF, 0,
			TEXCOORDS_ELEMENT, 0, R32G32_FLOAT_EF, D3D12_APPEND_ALIGNED_ELEMENT,
		};

		vertexLayout->Create(vertexElementDescs, _countof(vertexElementDescs));

		PIPELINE_STATE_DESC FullScreenQuadPsoDesc;
		FullScreenQuadPsoDesc.shader = fullScreenQuadShader;
		FullScreenQuadPsoDesc.vertexLayout = vertexLayout;
		FullScreenQuadPsoDesc.renderTartet = GetRenderTarget(BACK_BUFFER_RT_ID);

		pFullScreenQuadPso = CreatePipelineStateObject(FullScreenQuadPsoDesc);
	}

	m_pFrustum = new Frustum;
	if (!m_pFrustum)
		return false;

	return true;
	//{
	//	RenderTargetDesc rtDesc;
	//	rtDesc.width = m_width;
	//	rtDesc.height = m_height;
	//	rtDesc.sampleCountMSAA = MSAA_COUNT;
	//	rtDesc.colorBufferDescs[0].format = DXGI_FORMAT_R32G32B32A32_FLOAT; // xyz - World pos, w - material
	//	rtDesc.colorBufferDescs[1].format = DXGI_FORMAT_R32G32B32A32_FLOAT; // rgb - Color, a - specular level
	//	rtDesc.colorBufferDescs[2].format = DXGI_FORMAT_R32G32B32A32_FLOAT; // xyz - Normal, w - not use
	//	//rtDesc.colorBufferDescs[3].format = DXGI_FORMAT_R32G32B32A32_FLOAT; // xyz - Tangent, w - not use
	//	rtDesc.depthStencilBufferDesc.format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	//	if (!CreateRenderTarget(rtDesc))
	//		return false;
	//}

	//{
	//	RenderTargetDesc rtDesc;
	//	rtDesc.width = m_width;
	//	rtDesc.height = m_height;
	//	rtDesc.colorBufferDescs[0].format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	//	rtDesc.depthStencilBufferDesc.format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	//	if (!CreateRenderTarget(rtDesc))
	//		return false;
	//}

	m_font = new BitmapFont(this);
	if (!m_font->Init("font.fnt"))
		return false;

	m_text = new Text(this, m_font);
	if (!m_text->Init(L"", false, 20))
		return false;

	m_pFullScreenWindow = new OrthoWindow;
	if (!m_pFullScreenWindow)
	{
		return false;
	}

	// Initialize the full screen ortho window object.
	if (!m_pFullScreenWindow->Initialize(m_pd3dDevice, GetScreenWidth(), GetScreenHeight()))
	{
		MessageBox(hwnd, L"Could not initialize the full screen ortho window object.", L"Error", MB_OK);
		return false;
	}

	if (VXGI_ENABLED)
	{
		m_pVXGI = new VoxelGlobalIllumination();
		if (!m_pVXGI)
			return false;

		m_pVXGI->Init(VOXELIZE_DIMENSION, VOXELIZE_SIZE);
	}

	//m_pLightShader = new Shader();
	//if (!m_pLightShader)
	//	return false;

	//m_pLightShader->AddInputElementDesc("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT);
	//m_pLightShader->AddInputElementDesc("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT);

	//std::wstring shaderName;
	//shaderName = *m_pFramework->GetSystemPath(PATH_TO_SHADERS) + L"Light.hlsl";

	//if (!m_pLightShader->CreateShaderVs((WCHAR*)shaderName.c_str(), NULL, "VS"))
	//	return false;

	//if (!m_pLightShader->CreateShaderVs((WCHAR*)shaderName.c_str(), NULL, "HorizontalBlurVertexShader"))
	//	return false;

	//if (!m_pLightShader->CreateShaderPs((WCHAR*)shaderName.c_str(), NULL, "HorizontalBlurPixelShader"))
	//	return false;

	//if (!m_pLightShader->CreateShaderVs((WCHAR*)shaderName.c_str(), NULL, "VerticalBlurVertexShader"))
	//	return false;

	//if (!m_pLightShader->CreateShaderPs((WCHAR*)shaderName.c_str(), NULL, "VerticalBlurPixelShader"))
	//	return false;

	//string msaa = to_string(static_cast<int>(MSAA_COUNT));

	//D3D_SHADER_MACRO defines[] =
	//{
	//	"VOXEL_GLOBAL_ILLUMINATION", "0",
	//	"MSAA", msaa.c_str(),
	//	NULL, NULL
	//};

	//if (!m_pLightShader->CreateShaderPs((WCHAR*)shaderName.c_str(), defines, "PS"))
	//	return false;

	//defines[0] =
	//{ "VOXEL_GLOBAL_ILLUMINATION", "1" };

	//if (!m_pLightShader->CreateShaderPs((WCHAR*)shaderName.c_str(), defines, "PS"))
	//	return false;

	//VertexElementDesc vertexElementDescs[2] = { POSITION_ELEMENT, 0, R32G32B32_FLOAT_EF, 0,
	//	TEXCOORDS_ELEMENT, 0, R32G32_FLOAT_EF, D3D11_APPEND_ALIGNED_ELEMENT };

	//baseVertexLayout = Engine::m_pRender->CreateVertexLayout(vertexElementDescs, 2);
	//if (!baseVertexLayout)
	//	return false;

	return true;
}

void Render::m_Resize()
{
	//RECT rc;
	//GetClientRect(m_hwnd, &rc);
	//m_width = rc.right - rc.left;
	//m_height = rc.bottom - rc.top;
	//m_viewport.Width = (FLOAT)m_width;
	//m_viewport.Height = (FLOAT)m_height;
	//m_pSwapChain->ResizeBuffers(1, m_width, m_height, DXGI_FORMAT_R8G8B8A8_UNORM, NULL);
	//DXGI_MODE_DESC desc;
	//ZeroMemory(&desc, sizeof(desc));
	//desc.Width = m_width;
	//desc.Height = m_height;
	//m_pSwapChain->ResizeTarget(&desc);
	//m_initmatrix();
	//m_createdepthstencil();
	//m_pFullScreenWindow->Initialize(m_pd3dDevice, m_width, m_height);
}

bool Render::m_CreateDevice()
{

#ifdef _DEBUG
	// Enable the D3D12 debug layer.
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
	}
#endif

	ComPtr<IDXGIFactory4> factory;
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
	m_useWarpDevice = true;
	if (m_useWarpDevice)
	{
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

		DXGI_ADAPTER_DESC adapterDesc;
		// Get the adapter (video card) description.
		warpAdapter->GetDesc(&adapterDesc);

		ThrowIfFailed(D3D12CreateDevice(
			warpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_device)
			));
	}
	else
	{
		ComPtr<IDXGIAdapter> adapter;
		ThrowIfFailed(factory->EnumAdapters(0, &adapter));

		IDXGIOutput* adapterOutput;
		adapter->EnumOutputs(0, &adapterOutput);


		ThrowIfFailed(D3D12CreateDevice(
			adapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_device)
			));
	}

	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.BufferDesc.Width = m_width;
	swapChainDesc.BufferDesc.Height = m_height;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.OutputWindow = m_hwnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = TRUE;

	ComPtr<IDXGISwapChain> swapChain;
	ThrowIfFailed(factory->CreateSwapChain(
		m_commandQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
		&swapChainDesc,
		&swapChain
		));

	ThrowIfFailed(swapChain.As(&m_swapChain));

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount + 100;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		// Describe and create a depth stencil view (DSV) descriptor heap.
		// Each frame has its own depth stencils (to write shadows onto) 
		// and then there is one for the scene itself.
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = FrameCount + 100;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

		D3D12_DESCRIPTOR_HEAP_DESC cbvSrvHeapDesc = {};
		cbvSrvHeapDesc.NumDescriptors = 10000;
		cbvSrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		cbvSrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvSrvHeapDesc, IID_PPV_ARGS(&m_cbvSrvHeap)));

		// Describe and create a sampler descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
		samplerHeapDesc.NumDescriptors = 100;
		samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m_samplerHeap)));

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	m_fenceValue = 1;

	// Create an event handle to use for frame synchronization.
	m_fenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
	if (m_fenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

	m_d3d12viewport.Width = static_cast<float>(m_width);
	m_d3d12viewport.Height = static_cast<float>(m_height);
	m_d3d12viewport.MaxDepth = 1.0f;

	m_scissorRect.right = static_cast<LONG>(m_width);
	m_scissorRect.bottom = static_cast<LONG>(m_height);

	for (unsigned short i = 0; i < FrameCount; i++)
	{
		ThrowIfFailed(Engine::GetRender()->GetD3D12Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator[i])));

		ThrowIfFailed(Engine::GetRender()->GetD3D12Device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator[i].Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList[i])));

		ThrowIfFailed(m_commandList[i]->Close());
	}

	m_fenceValue = 1;

	return true;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = m_width;
	sd.BufferDesc.Height = m_height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = m_hwnd;
	sd.SampleDesc.Count = MSAA_COUNT;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	_MUTEXLOCK(m_pFramework->GetMutex());
	HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &sd, &m_pSwapChain, &m_pd3dDevice, NULL, &m_pImmediateContext);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
	if (FAILED(hr))
		return false;

	return true;
}

bool Render::m_CreateDepthStencil()
{
	HRESULT hr;

	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
	depthStencilDesc.StencilEnable = true;
	depthStencilDesc.StencilReadMask = 0xFF;
	depthStencilDesc.StencilWriteMask = 0xFF;
	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	hr = m_pd3dDevice->CreateDepthStencilState(&depthStencilDesc, &m_pDepthStencilState);
	if (FAILED(hr))
		return false;

	depthStencilDesc.DepthEnable = false;
	depthStencilDesc.DepthWriteMask = (D3D11_DEPTH_WRITE_MASK)false;
	depthStencilDesc.StencilEnable = false;
	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NEVER;
	depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_NEVER;
	hr = m_pd3dDevice->CreateDepthStencilState(&depthStencilDesc, &m_pDepthDisabledStencilState);
	if (FAILED(hr))
		return false;

	// Create a texture sampler state description.
	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(samplerDesc));
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samplerDesc.BorderColor[0] = 0;
	samplerDesc.BorderColor[1] = 0;
	samplerDesc.BorderColor[2] = 0;
	samplerDesc.BorderColor[3] = 0;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// Create the texture sampler state.
	hr = m_pd3dDevice->CreateSamplerState(&samplerDesc, &m_pSamplerStatePoint);
	if (FAILED(hr))
		return false;

	samplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;

	hr = m_pd3dDevice->CreateSamplerState(&samplerDesc, &m_pSamplerStateCmp);
	if (FAILED(hr))
		return false;

	return true;
}

bool Render::m_CreateBlendingState()
{
	D3D11_BLEND_DESC blendStateDescription;
	ZeroMemory(&blendStateDescription, sizeof(D3D11_BLEND_DESC));
	blendStateDescription.RenderTarget[0].BlendEnable = TRUE;
	blendStateDescription.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendStateDescription.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendStateDescription.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendStateDescription.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendStateDescription.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendStateDescription.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendStateDescription.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	HRESULT hr = m_pd3dDevice->CreateBlendState(&blendStateDescription, &m_pAlphaEnableBlendingState);
	if (FAILED(hr))
		return false;

	blendStateDescription.RenderTarget[0].BlendEnable = FALSE;
	hr = m_pd3dDevice->CreateBlendState(&blendStateDescription, &m_pAlphaDisableBlendingState);
	if (FAILED(hr))
		return false;

	return true;
}

void Render::m_InitializeMatrix()
{
	m_pCamera->SetPosition(5.0f, 3.5f, 0.0f);
	m_pCamera->SetProjection(XMMatrixPerspectiveFovLH(0.5f * 3.14f, (float)m_width / (float)m_height, 0.01f, 10000.0f));
	m_pCamera->SetRotation(270.0f, 90.0f, 0.0f);
	m_pCamera->SetScreenWidth(m_width);
	m_pCamera->SetScreenHeight(m_height);
	m_pCamera->SetScreenNearZ(0.0f);
	m_pCamera->SetScreenFarZ(1.0f);
}

void Render::m_PostProcessing(POST_PROCESSING postProcessing)
{
	ID3D11ShaderResourceView *pDepthShaderResouceView;
	ID3D11Buffer *buffers[3];
	//buffers[0] = m_pMatrixBuffer;
	//buffers[1] = m_pLightBuffer;
	//buffers[2] = VXGI_ENABLED ? m_pVXGI->m_pVoxelizationBuffer : NULL;

	//m_pCamera->GetUniformBuffer()->Bind(CAMERA_UB_BP, PIXEL_SHADER);
	//baseVertexLayout->Bind();
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//m_pImmediateContext->IASetInputLayout(m_pLightShader->GetInputLayout());
	m_pImmediateContext->VSSetConstantBuffers(0, 1, buffers);
	//m_pImmediateContext->PSSetShaderResources(0, 1, &renderTargets[1]->GetTexture(0)->shaderResourceView);
	//m_pImmediateContext->PSSetShaderResources(1, 1, &renderTargets[1]->GetTexture(1)->shaderResourceView);
	//m_pImmediateContext->PSSetShaderResources(2, 1, &renderTargets[1]->GetTexture(2)->shaderResourceView);
	//m_pImmediateContext->PSSetShaderResources(3, 1, &renderTargets[1]->GetTexture(3)->shaderResourceView);
	//m_pImmediateContext->PSSetShaderResources(0, BUFFER_COUNT, m_pShaderResourceViewArray);
	//m_pImmediateContext->PSSetShaderResources(BUFFER_COUNT, 1, &g_pEnvMapSRV);

	switch (postProcessing)
	{
	case D3D11Framework::SHADOW_RENDER:
		//m_pImmediateContext->VSSetShader(m_pLightShader->GetVertexShader(0), NULL, 0);
		break;
	case D3D11Framework::HORIZONTAL_BLUR:
		//m_pImmediateContext->VSSetShader(m_pLightShader->GetVertexShader(1), NULL, 0);
		//m_pImmediateContext->PSSetShader(m_pLightShader->GetPixelShader(0), NULL, 0);
		//pDepthShaderResouceView = m_pRenderTexture[HORIZONTAL_BLUR].GetShaderResourceView();
		m_pImmediateContext->PSSetShaderResources(BUFFER_COUNT + 1, 1, &pDepthShaderResouceView);
		break;
	case D3D11Framework::VERTICAL_BLUR:
		//m_pImmediateContext->VSSetShader(m_pLightShader->GetVertexShader(2), NULL, 0);
		//m_pImmediateContext->PSSetShader(m_pLightShader->GetPixelShader(1), NULL, 0);
		//pDepthShaderResouceView = m_pRenderTexture[VERTICAL_BLUR].GetShaderResourceView();
		m_pImmediateContext->PSSetShaderResources(BUFFER_COUNT + 1, 1, &pDepthShaderResouceView);
		break;
	case D3D11Framework::FINAL_RENDER:
		//m_pImmediateContext->VSSetShader(m_pLightShader->GetVertexShader(0), NULL, 0);
		//if (vxgi && VXGI_ENABLED)
			//m_pImmediateContext->PSSetShader(m_pLightShader->GetPixelShader(0), NULL, 0);
		//else
			//m_pImmediateContext->PSSetShader(m_pLightShader->GetPixelShader(1), NULL, 0);
		if (VXGI_ENABLED)
			m_pImmediateContext->PSSetConstantBuffers(2, 1, &m_pVXGI->m_pVoxelizationBuffer);
		//pDepthShaderResouceView = m_pRenderTexture[1].GetShaderResourceView();
		//pDepthShaderResouceView = renderTargets[2]->GetTexture()->shaderResourceView;
		m_pImmediateContext->PSSetShaderResources(BUFFER_COUNT + 0, 1, &pDepthShaderResouceView);
		//m_pImmediateContext->PSSetShaderResources(BUFFER_COUNT + 2, 1, &m_pVXGI->m_pVoxelGridNormalSRV);
		if (VXGI_ENABLED)
			m_pImmediateContext->PSSetShaderResources(BUFFER_COUNT + 1, 1, &m_pVXGI->m_pVoxelGridColorIlluminatedSRV[m_pVXGI->outputLightRTIndices]);
		//m_pImmediateContext->PSSetShaderResources(BUFFER_COUNT + 2, 1, &Engine::m_pGameWorld->m_pSkyBox->m_pDiffuseTexture->shaderResourceView);
		if (VXGI_ENABLED)
			//m_pVXGI->m_pVoxelSampler->Bind(CUSTOM0_TEX_BP, PIXEL_SHADER);
			break;
	default:
		break;
	}
	// Set the vertex and pixel shaders that will be used to render.
	m_pImmediateContext->HSSetShader(NULL, NULL, 0);
	m_pImmediateContext->DSSetShader(NULL, NULL, 0);
	m_pImmediateContext->GSSetShader(NULL, NULL, 0);

	// Set the sampler state in the pixel shader.
	m_pImmediateContext->PSSetSamplers(0, 1, &m_pSamplerStatePoint);
	m_pImmediateContext->PSSetSamplers(2, 1, &m_pSamplerStateCmp);

	// Render the geometry.
	m_pImmediateContext->DrawIndexed(m_pFullScreenWindow->GetIndexCount(), 0, 0);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
}

void Render::SetDrawStates(const DrawCmd &cmd)
{
	//if (cmd.rasterizerState != lastGpuCmd.draw.rasterizerState)
	//{
	//	assert(cmd.rasterizerState != NULL);
	//	cmd.rasterizerState->Set();
	//	lastGpuCmd.draw.rasterizerState = cmd.rasterizerState;
	//}

	//if (cmd.depthStencilState != lastGpuCmd.draw.depthStencilState)
	//{
	//	assert(cmd.depthStencilState != NULL);
	//	cmd.depthStencilState->Set();
	//	lastGpuCmd.draw.depthStencilState = cmd.depthStencilState;
	//}

	//if (cmd.blendState != lastGpuCmd.draw.blendState)
	//{
	//	assert(cmd.blendState != NULL);
	//	cmd.blendState->Set();
	//	lastGpuCmd.draw.blendState = cmd.blendState;
	//}

	//if (cmd.vertexLayout != lastGpuCmd.draw.vertexLayout)
	//{
	//	if (cmd.vertexLayout)
	//		cmd.vertexLayout->Bind();
	//	lastGpuCmd.draw.vertexLayout = cmd.vertexLayout;
	//}

	//if (cmd.vertexBuffer != lastGpuCmd.draw.vertexBuffer)
	{
		if (cmd.vertexBuffer)
			cmd.vertexBuffer->Bind(m_commandList[m_frameIndex].Get());
		lastGpuCmd.draw.vertexBuffer = cmd.vertexBuffer;
	}

	//if (cmd.indexBuffer != lastGpuCmd.draw.indexBuffer)
	{
		if (cmd.indexBuffer)
			cmd.indexBuffer->Bind(m_commandList[m_frameIndex].Get());
		lastGpuCmd.draw.indexBuffer = cmd.indexBuffer;
	}
}

void Render::SetShaderStates(const ShaderCmd &cmd, gpuCmdOrders order)
{
	m_commandList[m_frameIndex]->SetPipelineState(cmd.pipelineStateObject->m_pipelineState.Get());
	m_commandList[m_frameIndex]->SetGraphicsRootSignature(cmd.pipelineStateObject->m_rootSignature.Get());
	//m_commandList[m_frameIndex]->RSSetViewports(1, &m_d3d12viewport);
	m_commandList[m_frameIndex]->RSSetScissorRects(1, &m_scissorRect);

	// Indicate that the back buffer will be used as a render target.m_pVertexBuffer
	//m_commandList[m_frameIndex]->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	//if ((cmd.renderTarget != lastGpuCmd.draw.renderTarget) || (cmd.renderTargetConfig != lastGpuCmd.draw.renderTargetConfig))
	{
		if (cmd.renderTargets[m_frameIndex])
		{
			cmd.renderTargets[m_frameIndex]->Bind(m_commandList[m_frameIndex].Get(), cmd.renderTargetConfigs[m_frameIndex]);
		}
		else if (cmd.renderTargets[0])
		{
			cmd.renderTargets[0]->Bind(m_commandList[m_frameIndex].Get(), cmd.renderTargetConfigs[0]);
		}
		for (auto i = 0; i < _countof(cmd.renderTargets); i++)
		{
			lastGpuCmd.draw.renderTargets[i] = cmd.renderTargets[i];
			lastGpuCmd.draw.renderTargetConfigs[i] = cmd.renderTargetConfigs[i];
		}
	}

	////if (cmd.shader != lastGpuCmd.draw.shader)
	//{
	//	assert(cmd.shader != NULL);
	//	cmd.shader->Bind();
	//	lastGpuCmd.draw.shader = cmd.shader;
	//}

	////if (cmd.light != lastGpuCmd.draw.light)
	//{
	//	if (cmd.light && cmd.renderTarget)
	//	{
	//		if (order == SHADOW_CO)
	//			cmd.renderTarget->Clear(D3D11_CLEAR_DEPTH);
	//	}
	//	lastGpuCmd.draw.light = cmd.light;
	//}
}

void Render::SetShaderParams(const ShaderCmd &cmd)
{
	if (cmd.camera)
		cmd.pipelineStateObject->SetUniformBuffer(m_commandList[m_frameIndex].Get(), CAMERA_UB_BP, cmd.camera->GetUniformBuffer());

	if (cmd.light)
		cmd.pipelineStateObject->SetUniformBuffer(m_commandList[m_frameIndex].Get(), LIGHT_UB_BP, cmd.light->GetUniformBuffer());

	for (unsigned int i = 0; i < NUM_CUSTOM_UNIFORM_BUFFER_BP; i++)
	{
		if (cmd.customUBs[i])
			cmd.pipelineStateObject->SetUniformBuffer(m_commandList[m_frameIndex].Get(), (UniformBufferBP)(CUSTOM0_UB_BP + i), cmd.customUBs[i]);
	}

	//for (unsigned int i = 0; i < NUM_STRUCTURED_BUFFER_BP; i++)
	//{
	//	if (cmd.customSBs[i])
	//		cmd.shader->SetStructuredBuffer((StructuredBufferBP)(CUSTOM0_SB_BP + i), cmd.customSBs[i]);
	//}

	for (unsigned int i = 0; i < NUM_TEXTURE_BP; i++)
	{
		if (cmd.textures[i])
			cmd.pipelineStateObject->SetTexture(m_commandList[m_frameIndex].Get(), (TextureBP)i, cmd.textures[i]);
	}

	for (unsigned int i = 0; i < NUM_TEXTURE_BP; i++)
	{
		if (cmd.samplers[i])
			cmd.pipelineStateObject->SetSampler(m_commandList[m_frameIndex].Get(), (TextureBP)i, cmd.samplers[i]);
	}
}

void Render::Draww(DrawCmd &cmd)
{
	m_commandList[m_frameIndex]->IASetPrimitiveTopology((D3D12_PRIMITIVE_TOPOLOGY)cmd.primitiveType);

	if (cmd.indexBuffer != NULL)
	{
		m_commandList[m_frameIndex]->DrawIndexedInstanced(cmd.numElements, cmd.numInstances, cmd.firstIndex, cmd.vertexOffset, 0);
	}
	else
	{
		m_commandList[m_frameIndex]->DrawInstanced(cmd.numElements, cmd.numInstances, cmd.vertexOffset, 0);
	}

	return;
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pImmediateContext->IASetPrimitiveTopology((D3D12_PRIMITIVE_TOPOLOGY)cmd.primitiveType);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
	if (cmd.indexBuffer != NULL)
	{
		if (cmd.numInstances < 2)
		{
			_MUTEXLOCK(m_pFramework->GetMutex());
			m_pImmediateContext->DrawIndexed(cmd.numElements, cmd.firstIndex, 0);
			_MUTEXUNLOCK(m_pFramework->GetMutex());
		}
		else
		{
			_MUTEXLOCK(m_pFramework->GetMutex());
			m_pImmediateContext->DrawIndexedInstanced(cmd.numElements, cmd.numInstances, cmd.firstIndex, 0, 0);
			_MUTEXUNLOCK(m_pFramework->GetMutex());
		}
	}
	else
	{
		if (cmd.numInstances < 2)
		{
			_MUTEXLOCK(m_pFramework->GetMutex());
			m_pImmediateContext->Draw(cmd.numElements, cmd.firstIndex);
			_MUTEXUNLOCK(m_pFramework->GetMutex());
		}
		else
		{
			_MUTEXLOCK(m_pFramework->GetMutex());
			m_pImmediateContext->DrawInstanced(cmd.numElements, cmd.numInstances, cmd.firstIndex, 0);
			_MUTEXUNLOCK(m_pFramework->GetMutex());
		}
	}
	//UnbindShaderResources();
}
//
//void Render::Dispatch(ComputeCmd &cmd)
//{
//	m_pImmediateContext->Dispatch(cmd.numThreadGroupsX, cmd.numThreadGroupsY, cmd.numThreadGroupsZ);
//	UnbindShaderResources();
//
//	ID3D11UnorderedAccessView *unorderedAccessViews[MAX_NUM_COLOR_BUFFERS] =
//	{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
//	m_pImmediateContext->CSSetUnorderedAccessViews(0, MAX_NUM_COLOR_BUFFERS, unorderedAccessViews, NULL);
//}
//
//void Render::GenerateMips(GenerateMipsCmd &cmd)
//{
//	for (unsigned int i = 0; i < NUM_TEXTURE_BP; i++)
//	{
//		if (cmd.textures[i] && (cmd.textures[i]->GetNumLevels() > 1))
//		{
//			m_pImmediateContext->GenerateMips(cmd.textures[i]->shaderResourceView);
//		}
//	}
//}

//void Render::UnbindShaderResources()
//{
//	const unsigned int numViews = NUM_TEXTURE_BP + NUM_STRUCTURED_BUFFER_BP;
//	ID3D11ShaderResourceView *shaderResourceViews[numViews] =
//	{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
//	_MUTEXLOCK(Engine::m_pMutex);
//	m_pImmediateContext->VSSetShaderResources(0, numViews, shaderResourceViews);
//	m_pImmediateContext->HSSetShaderResources(0, numViews, shaderResourceViews);
//	m_pImmediateContext->DSSetShaderResources(0, numViews, shaderResourceViews);
//	m_pImmediateContext->GSSetShaderResources(0, numViews, shaderResourceViews);
//	m_pImmediateContext->PSSetShaderResources(0, numViews, shaderResourceViews);
//	m_pImmediateContext->CSSetShaderResources(0, numViews, shaderResourceViews);
//	_MUTEXUNLOCK(Engine::m_pMutex);
//}

void Render::BeginFrame()
{
	gpuCmds.clear();
	m_objectListOnRender.erase(m_objectListOnRender.begin(), m_objectListOnRender.end());

	for (auto i = 0; i < renderTargets.size(); i++)
	{
		renderTargets[i]->Reset();
	}

	return;

	ID3D11SamplerState *lSamplers[3] = { NULL, NULL, NULL };
	ID3D11ShaderResourceView *lClearSRVArray[BUFFER_COUNT + 3] = { NULL, NULL, NULL, NULL, NULL, NULL };

	TurnZBufferOn();
	float ClearColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };

	_MUTEXLOCK(m_pFramework->GetMutex());
	//m_pImmediateContext->ClearRenderTargetView(m_pRenderTargetViewLight, ClearColor);
	//m_pImmediateContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	m_pImmediateContext->PSSetSamplers(0, 3, lSamplers);
	m_pImmediateContext->PSSetShaderResources(0, BUFFER_COUNT + 3, lClearSRVArray);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
}

bool Render::Draw()
{
	m_pCamera->TurnUp(m_upcam);
	m_pCamera->TurnDown(m_downcam);
	m_pCamera->TurnLeft(m_leftcam);
	m_pCamera->TurnRight(m_rightcam);
	m_pCamera->Render(m_pFramework->GetTimer()->getTimeInterval());
	m_pFramework->m_pGameWorld->UpdateWorld(m_pFramework->GetTimer()->getTimeInterval());
	m_pFrustum->ConstructFrustum(1000.0f, m_pCamera->GetProjection(), m_pCamera->GetViewMatrix());

	UINT visibleObjcts = 0;
	for (int i = 0; i < Engine::m_pGameWorld->GetScenesCount(); i++)
		for (int j = 0; j < Engine::m_pGameWorld->GetSceneByIndex(i)->GetModelsCount(); j++)
		{
			PxBounds3 bounds = Engine::m_pGameWorld->GetSceneByIndex(i)->GetModelByIndex(j)->GetWorldBounds();
			PxVec3 center = bounds.getCenter();
			PxVec3 dimensions = bounds.getDimensions();
			if (m_pFrustum->CheckRectangle(center.x, center.y, center.z, dimensions.x, dimensions.y, dimensions.z, false))
			{
				m_objectListOnRender.push_back(Engine::m_pGameWorld->GetSceneByIndex(i)->GetModelByIndex(j));
				visibleObjcts++;
			}
		}

	cout << visibleObjcts << endl;

	g_accumulateTime += m_pFramework->GetTimer()->getTimeInterval();
	if (g_accumulateTime > 0.1f)
	{
		g_fps = 1.f / m_pFramework->GetTimer()->getTimeInterval();
		g_accumulateTime = 0.0f;
	}

	cout << g_fps << endl;

	for (auto j = 0; j < m_objectListOnRender.size(); j++)
	{
		m_objectListOnRender[j]->Draw(m_pCamera, GBUFFER_FILL_RENDER);
	}

	for (auto i = 0; i < Engine::m_pGameWorld->GetLightsCount(); i++)
	{
		for (auto j = 0; j < m_objectListOnRender.size(); j++)
			m_objectListOnRender[j]->AddShadowMapSurfaces(i);
		Engine::m_pGameWorld->GetLight(i)->DrawScene(GBUFFER_FILL_RENDER);
	}

	GpuCmd gpuCmd(DRAW_CM);
	gpuCmd.draw.camera = m_pCamera;
	gpuCmd.draw.pipelineStateObject = pFullScreenQuadPso;
	gpuCmd.draw.firstIndex = 0;
	gpuCmd.draw.numElements = 4;
	gpuCmd.draw.numInstances = 1;
	gpuCmd.draw.primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	for (auto i = 0; i < FrameCount; i++)
		gpuCmd.draw.renderTargets[i] = GetRenderTarget(BACK_BUFFER_RT_ID + i);
	gpuCmd.draw.textures[0] = renderTargets[GBUFFERS_RT_ID]->GetTexture(0);
	gpuCmd.draw.textures[1] = renderTargets[GBUFFERS_RT_ID]->GetTexture(1);
	gpuCmd.draw.textures[2] = renderTargets[GBUFFERS_RT_ID]->GetTexture(2);
	gpuCmd.draw.textures[3] = renderTargets[GBUFFERS_RT_ID]->GetTexture(3);
	gpuCmd.draw.textures[4] = dynamic_cast<DirectionalLight*>(Engine::m_pGameWorld->GetLight(0))->GetRT()->GetDepthStencilTexture();
	gpuCmd.draw.samplers[COLOR_TEX_ID] = Engine::GetRender()->GetSampler(TRILINEAR_SAMPLER_ID);
	AddGpuCmd(gpuCmd);

	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	ThrowIfFailed(m_commandAllocator[m_frameIndex]->Reset());

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	ThrowIfFailed(m_commandList[m_frameIndex]->Reset(m_commandAllocator[m_frameIndex].Get(), NULL));

	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvHeap.Get(), m_samplerHeap.Get() };
	m_commandList[m_frameIndex]->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	for (unsigned int i = 0; i < gpuCmds.size(); i++)
	{
		switch (gpuCmds[i].mode)
		{
		case DRAW_CM:
			SetDrawStates(gpuCmds[i].draw);
			SetShaderStates(gpuCmds[i].draw, gpuCmds[i].order);
			SetShaderParams(gpuCmds[i].draw);
			Draww(gpuCmds[i].draw);
			break;
		}
	}
	// Indicate that the back buffer will now be used to present.
	//m_commandList[m_frameIndex]->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[m_frameIndex]->m_renderTarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_commandList[m_frameIndex]->Close());

	//// Command list allocators can only be reset when the associated 
	//// command lists have finished execution on the GPU; apps should use 
	//// fences to determine GPU execution progress.
	//ThrowIfFailed(m_commandAllocator->Reset());

	//// However, when ExecuteCommandList() is called on a particular command 
	//// list, that command list can then be reset at any time and must be before 
	//// re-recording.
	//ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), pPso->m_pipelineState.Get()));

	//// Set necessary state.
	//m_commandList->SetGraphicsRootSignature(pPso->m_rootSignature.Get());
	//m_commandList->RSSetViewports(1, &m_d3d12viewport);
	//m_commandList->RSSetScissorRects(1, &m_scissorRect);

	//// Indicate that the back buffer will be used as a render target.m_pVertexBuffer
	//m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	//CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	//m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	//m_angle += static_cast<float>(m_pFramework->GetTimer()->getTimeInterval()) * XM_PIDIV4;
	//XMStoreFloat4x4(&m_constantBufferData.model, XMMatrixTranspose(Engine::m_pGameWorld->GetSceneByIndex(0)->GetModelByIndex(0)->m_objectMatrix));
	//XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(m_pCamera->GetViewMatrix()));
	//XMStoreFloat4x4(&m_constantBufferData.projection, XMMatrixTranspose(m_pCamera->GetProjection()));

	//uniformBuffer->Update(&m_constantBufferData);

	//ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvHeap.Get(), m_samplerHeap.Get() };
	//m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//m_commandList->SetGraphicsRootDescriptorTable(0, uniformBuffer->m_cbvHandle);
	//m_commandList->SetGraphicsRootDescriptorTable(1, texture->m_textureSrvHandle);
	//m_commandList->SetGraphicsRootDescriptorTable(2, m_samplerHeap->GetGPUDescriptorHandleForHeapStart());

	//// Record commands.
	//const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	//m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	//m_pFramework->m_pGameWorld->RenderWorld(NORMAL_RENDER, m_pCamera);

	//// Indicate that the back buffer will now be used to present.
	//m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	//ThrowIfFailed(m_commandList->Close());

	//// Execute the command list.
	//ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	//m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	//// Present the frame.
	//ThrowIfFailed(m_swapChain->Present(1, 0));

	//{
	//	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	//	// This is code implemented as such for simplicity. More advanced samples 
	//	// illustrate how to use fences for efficient resource usage.

	//	// Signal and increment the fence value.
	//	const UINT64 fence = m_fenceValue;
	//	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
	//	m_fenceValue++;

	//	// Wait until the previous frame is finished.
	//	if (m_fence->GetCompletedValue() < fence)
	//	{
	//		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
	//		WaitForSingleObject(m_fenceEvent, INFINITE);
	//	}

	//	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	//}

	return true;

	//MatrixBufferType mCB;
	//LightBufferType lCB;
	//ID3D11SamplerState *lSamplers[3];

	//mCB.projection = XMMatrixTranspose(m_pCamera->GetOrthoMatrix());
	//mCB.camVP = XMMatrixTranspose(m_pCamera->GetViewMatrix() * m_pCamera->GetProjection());
	//mCB.eye = XMVectorSet(m_pCamera->GetPosition().m128_f32[0],
	//	m_pCamera->GetPosition().m128_f32[1],
	//	m_pCamera->GetPosition().m128_f32[2], 0.0f);
	//mCB.ambientColor = XMVectorSet(m_pFramework->m_pGameWorld->GetAmbientColor()->x,
	//	m_pFramework->m_pGameWorld->GetAmbientColor()->y,
	//	m_pFramework->m_pGameWorld->GetAmbientColor()->z, 1.0f);

	//_MUTEXLOCK(m_pFramework->GetMutex());
	//m_pImmediateContext->UpdateSubresource(m_pMatrixBuffer, 0, NULL, &mCB, 0, 0);
	//_MUTEXUNLOCK(m_pFramework->GetMutex());

	//renderTargets[1]->Bind();
	//if (!m_isWireFrameRender)
	//	m_pRasterState->Set();
	//else
	//	m_pRasterStateWireFrame->Set();

	//m_pCamera->GetUniformBuffer()->Bind(CAMERA_UB_BP, VERTEX_SHADER);
	//m_pCamera->GetUniformBuffer()->Bind(CAMERA_UB_BP, HULL_SHADER);
	//m_pCamera->GetUniformBuffer()->Bind(CAMERA_UB_BP, DOMAIN_SHADER);
	//m_pCamera->GetUniformBuffer()->Bind(CAMERA_UB_BP, GEOMETRY_SHADER);
	//m_pCamera->GetUniformBuffer()->Bind(CAMERA_UB_BP, PIXEL_SHADER);

	TurnOffAlphaBlending();
	//GpuCmd gpuCmd(DRAW_CM);
	m_pFramework->m_pGameWorld->RenderWorld(GBUFFER_FILL_RENDER, m_pCamera);

	//UnbindShaderResources();

	//m_pUniformBuffer->Bind(CAMERA_UB_BP, VERTEX_SHADER);
	//m_pUniformBuffer->Bind(CAMERA_UB_BP, HULL_SHADER);
	//m_pUniformBuffer->Bind(CAMERA_UB_BP, DOMAIN_SHADER);
	//m_pUniformBuffer->Bind(CAMERA_UB_BP, GEOMETRY_SHADER);
	//m_pUniformBuffer->Bind(CAMERA_UB_BP, PIXEL_SHADER);

	if (VXGI_ENABLED)
		m_pVXGI->PerformVoxelGlobalIllumination();

	//for (int i = 0; i < NUM_LIGHTS; i++)
	{
		//m_pFramework->m_pGameWorld->lights[i]->DrawShadowSurface();
	}

	m_viewport.Width = (FLOAT)m_width;
	m_viewport.Height = (FLOAT)m_height;

	TurnZBufferOff();
	TurnOnAlphaBlending();

	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFullScreenWindow->Render(m_pImmediateContext);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	//renderTargets[2]->Bind();

	//for (int i = 0; i < NUM_LIGHTS; i++)
	{
		//lSamplers[0] = NULL;
		//lSamplers[1] = NULL;
		//lSamplers[2] = NULL;
		//m_pImmediateContext->CSSetSamplers(0, 3, lSamplers);
		//m_pFramework->m_pGameWorld->lights[i]->DrawScene(CASCADED_SHADOW_RENDER);
		m_PostProcessing(SHADOW_RENDER);
	}
	//ID3D11ShaderResourceView *lClearSRVArray[] = { NULL };

	//_MUTEXLOCK(m_pFramework->GetMutex());
	////m_pImmediateContext->PSSetShaderResources(4, 1, lClearSRVArray);
	//m_pImmediateContext->PSSetShader(m_pLightShader->GetPixelShader(0), NULL, 0);
	//_MUTEXUNLOCK(m_pFramework->GetMutex());

	//m_PostProcessing(SHADOW_RENDER);

	//m_pRenderTexture[2].SetRenderTarget();
	//m_pRenderTexture[2].ClearRenderTarget(0.0f, 0.0f, 0.0f, 1.0f);

	//m_PostProcessing(HORIZONTAL_BLUR);

	//m_pRenderTexture[0].SetRenderTarget();
	//m_pRenderTexture[0].ClearRenderTarget(0.0f, 0.0f, 0.0f, 1.0f);

	//m_PostProcessing(VERTICAL_BLUR);

	if (VXGI_ENABLED)
	{
		ID3D11ShaderResourceView *pDepthShaderResouceView[3];
		ID3D11Buffer *buffers[3];
		//buffers[0] = m_pMatrixBuffer;
		buffers[1] = NULL;
		buffers[2] = m_pVXGI->m_pVoxelizationBuffer;

		//for (int i = 0; i < NUM_LIGHTS; i++)
		{
			_MUTEXLOCK(m_pFramework->GetMutex());
			m_pImmediateContext->CSSetConstantBuffers(2, 1, &m_pVXGI->m_pVoxelizationBuffer);
			_MUTEXUNLOCK(m_pFramework->GetMutex());
			//m_pFramework->m_pGameWorld->lights[i]->DrawScene(VOXELIZE_RENDER);
			//m_pVXGI->PerformGridLightingPass(m_pFramework->m_pGameWorld->lights[i]->GetLightType());
			pDepthShaderResouceView[0] = NULL;
			_MUTEXLOCK(m_pFramework->GetMutex());
			Engine::m_pRender->GetDeviceContext()->CSSetShaderResources(2, 1, pDepthShaderResouceView);
			_MUTEXUNLOCK(m_pFramework->GetMutex());
		}

		//m_pUniformBuffer->Bind(CAMERA_UB_BP, VERTEX_SHADER);
		//m_pUniformBuffer->Bind(CAMERA_UB_BP, HULL_SHADER);
		//m_pUniformBuffer->Bind(CAMERA_UB_BP, DOMAIN_SHADER);
		//m_pUniformBuffer->Bind(CAMERA_UB_BP, GEOMETRY_SHADER);
		//m_pUniformBuffer->Bind(CAMERA_UB_BP, PIXEL_SHADER);

		_MUTEXLOCK(m_pFramework->GetMutex());
		m_pImmediateContext->CSSetConstantBuffers(2, 1, &m_pVXGI->m_pVoxelizationBuffer);
		_MUTEXUNLOCK(m_pFramework->GetMutex());
		m_pVXGI->PerformLightPropagatePass();
	}

	//renderTargets[0]->Bind();

	m_pParamsUB->Update(&coneSlope);
	//m_pParamsUB->Bind(CUSTOM1_UB_BP, PIXEL_SHADER);

	//if (m_pFramework->m_pInputMgr->GetCursorX() >= 0 && m_pFramework->m_pInputMgr->GetCursorX() <= m_width &&
	//	m_pFramework->m_pInputMgr->GetCursorY() >= 0 && m_pFramework->m_pInputMgr->GetCursorY() <= m_height)
	//{
	//	// Get description of source texture
	//	ID3D11Texture2D *tex2d = NULL;
	//	HRESULT hr = renderTargets[1]->GetTexture(0)->texture->QueryInterface(IID_ID3D11Texture2D, (void **)&tex2d);
	//	if (FAILED(hr))
	//		cout << "error" << endl;

	//	D3D11_TEXTURE2D_DESC Desc;
	//	tex2d->GetDesc(&Desc);

	//	// Create STAGING height map texture
	//	ID3D11Texture2D* pMaterialMapStaging;
	//	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	//	Desc.Usage = D3D11_USAGE_STAGING;
	//	Desc.BindFlags = 0;
	//	Desc.Width = 1;
	//	Desc.Height = 1;
	//	Desc.SampleDesc.Count = 1;
	//	m_pd3dDevice->CreateTexture2D(&Desc, NULL, &pMaterialMapStaging);

	//	D3D11_BOX srcBox;
	//	srcBox.left = m_pFramework->m_pInputMgr->GetCursorX();
	//	srcBox.right = srcBox.left + 1;
	//	srcBox.top = m_pFramework->m_pInputMgr->GetCursorY();
	//	srcBox.bottom = srcBox.top + 1;
	//	srcBox.front = 0;
	//	srcBox.back = 1;

	//	_MUTEXLOCK(m_pFramework->GetMutex());
	//	m_pImmediateContext->CopySubresourceRegion(pMaterialMapStaging, 0, 0, 0, 0, tex2d, 0, &srcBox);
	//	_MUTEXUNLOCK(m_pFramework->GetMutex());

	//	D3D11_MAPPED_SUBRESOURCE MappedSubResourceRead;
	//	_MUTEXLOCK(m_pFramework->GetMutex());
	//	m_pImmediateContext->Map(pMaterialMapStaging, 0, D3D11_MAP_READ, 0, &MappedSubResourceRead);
	//	_MUTEXUNLOCK(m_pFramework->GetMutex());
	//	XMFLOAT4 CurrentPixel = *(XMFLOAT4 *)MappedSubResourceRead.pData;

	//	printf("r=%f, g=%f, b=%f, a=%f\n", CurrentPixel.x, CurrentPixel.y, CurrentPixel.z, CurrentPixel.w);

	//	_MUTEXLOCK(m_pFramework->GetMutex());
	//	m_pImmediateContext->Unmap(pMaterialMapStaging, 0);
	//	_MUTEXUNLOCK(m_pFramework->GetMutex());
	//}

	m_PostProcessing(FINAL_RENDER);

	std::wstring text = L"FPS: " + to_wstring((int)(g_fps));
	m_text->SetText(text);
	m_text->Draw(1.0f, 1.0f, 0.0f, 20.0f, 20.0f);

	text = L"Camera Position: ";
	m_text->SetText(text);
	m_text->Draw(1.0f, 1.0f, 0.0f, 20.0f, 50.0f);

	text = L"X: " + to_wstring(m_pCamera->GetPosition().m128_f32[0]);
	m_text->SetText(text);
	m_text->Draw(1.0f, 1.0f, 0.0f, 20.0f, 80.0f);

	text = L"Y: " + to_wstring(m_pCamera->GetPosition().m128_f32[1]);
	m_text->SetText(text);
	m_text->Draw(1.0f, 1.0f, 0.0f, 20.0f, 110.0f);

	text = L"Z: " + to_wstring(m_pCamera->GetPosition().m128_f32[2]);
	m_text->SetText(text);
	m_text->Draw(1.0f, 1.0f, 0.0f, 20.0f, 140.0f);

	TurnZBufferOn();
	TurnOffAlphaBlending();

	return true;
}

void Render::EndFrame()
{
	ID3D12CommandList* ppCommandLists[] = { m_commandList[m_frameIndex].Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame.
	ThrowIfFailed(m_swapChain->Present(VSYNC_ENABLED, 0));

	{
		// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
		// This is code implemented as such for simplicity. More advanced samples 
		// illustrate how to use fences for efficient resource usage.

		// Signal and increment the fence value.
		const UINT64 fence = m_fenceValue;
		ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
		m_fenceValue++;

		// Wait until the previous frame is finished.
		if (m_fence->GetCompletedValue() < fence)
		{
			ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
			WaitForSingleObject(m_fenceEvent, INFINITE);
		}

		m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	}

	return;

	m_pSwapChain->Present(VSYNC_ENABLED, 0);
}

void Render::AddGpuCmd(const GpuCmd &gpuCmd)
{
	int index = gpuCmds.size();
	gpuCmds.push_back(gpuCmd);
	gpuCmds[gpuCmds.size() - 1].ID = index;
}

void Render::TurnZBufferOn()
{
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pImmediateContext->OMSetDepthStencilState(m_pDepthStencilState, 1);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
}

void Render::TurnZBufferOff()
{
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pImmediateContext->OMSetDepthStencilState(m_pDepthDisabledStencilState, 1);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
}

void Render::TurnOnAlphaBlending()
{
	float blendFactor[4];
	blendFactor[0] = 0.0f;
	blendFactor[1] = 0.0f;
	blendFactor[2] = 0.0f;
	blendFactor[3] = 0.0f;
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pImmediateContext->OMSetBlendState(m_pAlphaEnableBlendingState, blendFactor, 0xffffffff);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
}

void Render::TurnOffAlphaBlending()
{
	float blendFactor[4];
	blendFactor[0] = 0.0f;
	blendFactor[1] = 0.0f;
	blendFactor[2] = 0.0f;
	blendFactor[3] = 0.0f;
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pImmediateContext->OMSetBlendState(m_pAlphaDisableBlendingState, blendFactor, 0xffffffff);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
}

Engine *Render::GetFramework()
{
	return m_pFramework;
}

ID3D11Device *Render::GetDevice()
{
	return m_pd3dDevice;
}

ComPtr<ID3D12Device> Render::GetD3D12Device()
{
	return m_device;
}

ID3D11DeviceContext *Render::GetDeviceContext()
{
	return m_pImmediateContext;
}

D3D11_VIEWPORT *Render::GetViewport()
{
	return &m_viewport;
}

UINT Render::GetScreenHeight()
{
	return m_height;
}

UINT Render::GetScreenWidth()
{
	return m_width;
}

Camera *Render::GetCamera()
{
	return m_pCamera;
}

DX12_UniformBuffer* Render::CreateUniformBuffer(unsigned int bufferSize)
{
	DX12_UniformBuffer *uniformBuffer = new DX12_UniformBuffer;
	if (!uniformBuffer)
		return NULL;
	if (!uniformBuffer->Create(bufferSize))
	{
		SAFE_DELETE(uniformBuffer);
		return NULL;
	}
	uniformBuffers.push_back(uniformBuffer);
	return uniformBuffer;
}

DX12_VertexLayout *Render::CreateVertexLayout(const VertexElementDesc *vertexElementDescs, unsigned int numVertexElementDescs)
{
	for (unsigned int i = 0; i < vertexLayouts.size(); i++)
	{
		if (vertexLayouts[i]->IsEqual(vertexElementDescs, numVertexElementDescs))
			return vertexLayouts[i];
	}
	DX12_VertexLayout *vertexLayout = new DX12_VertexLayout;
	if (!vertexLayout)
		return NULL;
	if (!vertexLayout->Create(vertexElementDescs, numVertexElementDescs))
	{
		SAFE_DELETE(vertexLayout);
		return NULL;
	}
	vertexLayouts.push_back(vertexLayout);
	return vertexLayout;
	return NULL;
}

DX12_VertexBuffer *Render::CreateVertexBuffer(unsigned int vertexSize, unsigned int maxVertexCount, bool dynamic)
{
	DX12_VertexBuffer *vertexBuffer = new DX12_VertexBuffer();
	if (!vertexBuffer)
		return NULL;
	if (!vertexBuffer->Create(vertexSize, maxVertexCount, dynamic))
	{
		SAFE_DELETE(vertexBuffer);
		return NULL;
	}
	vertexBuffers.push_back(vertexBuffer);
	return vertexBuffer;
}

DX12_IndexBuffer *Render::CreateIndexBuffer(unsigned int maxIndexCount, bool dynamic)
{
	DX12_IndexBuffer *indexBuffer = new DX12_IndexBuffer;
	if (!indexBuffer)
		return NULL;
	if (!indexBuffer->Create(maxIndexCount, dynamic))
	{
		SAFE_DELETE(indexBuffer);
		return NULL;
	}
	indexBuffers.push_back(indexBuffer);
	return indexBuffer;
}

DX12_Sampler* Render::CreateSampler(const SamplerDesc &desc)
{
	for (unsigned int i = 0; i < samplers.size(); i++)
	{
		if (samplers[i]->GetDesc() == desc)
			return samplers[i];
	}
	DX12_Sampler *sampler = new DX12_Sampler;
	if (!sampler)
		return NULL;
	if (!sampler->Create(desc))
	{
		SAFE_DELETE(sampler);
		return NULL;
	}
	samplers.push_back(sampler);
	return sampler;
}

PipelineStateObject *Render::CreatePipelineStateObject(const PIPELINE_STATE_DESC &desc)
{
	for (unsigned int i = 0; i < m_pipelineStateObjects.size(); i++)
	{
		//if (m_pipelineStateObjects[i]->GetDesc() == desc)
		//	return m_pipelineStateObjects[i];
	}
	PipelineStateObject *pipelineStateObject = new PipelineStateObject;
	if (!pipelineStateObject)
		return NULL;
	if (!pipelineStateObject->Create(desc))
	{
		SAFE_DELETE(pipelineStateObject);
		return NULL;
	}
	m_pipelineStateObjects.push_back(pipelineStateObject);
	return pipelineStateObject;
}

//DX11_DepthStencilState *Render::CreateDepthStencilState(const DepthStencilDesc &desc)
//{
//	for (unsigned int i = 0; i < depthStencilStates.size(); i++)
//	{
//		if (depthStencilStates[i]->GetDesc() == desc)
//			return depthStencilStates[i];
//	}
//	DX11_DepthStencilState *depthStencilState = new DX11_DepthStencilState;
//	if (!depthStencilState)
//		return NULL;
//	if (!depthStencilState->Create(desc))
//	{
//		SAFE_DELETE(depthStencilState);
//		return NULL;
//	}
//	depthStencilStates.push_back(depthStencilState);
//	return depthStencilState;
//}
//
//DX11_BlendState *Render::CreateBlendState(const BlendDesc &desc)
//{
//	for (unsigned int i = 0; i < blendStates.size(); i++)
//	{
//		if (blendStates[i]->GetDesc() == desc)
//			return blendStates[i];
//	}
//	DX11_BlendState *blendState = new DX11_BlendState;
//	if (!blendState)
//		return NULL;
//	if (!blendState->Create(desc))
//	{
//		SAFE_DELETE(blendState);
//		return NULL;
//	}
//	blendStates.push_back(blendState);
//	return blendState;
//}

DX12_RenderTarget *Render::CreateRenderTarget(const RenderTargetDesc &desc)
{
	DX12_RenderTarget *renderTarget = new DX12_RenderTarget;
	if (!renderTarget)
		return NULL;
	if (!renderTarget->Create(desc))
	{
		SAFE_DELETE(renderTarget);
		return NULL;
	}
	renderTargets.push_back(renderTarget);
	return renderTarget;
}

DX12_RenderTarget* Render::CreateBackBufferRt(UINT bufferIndex)
{
	DX12_RenderTarget *backBuffer = new DX12_RenderTarget;
	if (!backBuffer)
		return NULL;

	if (!backBuffer->CreateBackBuffer(bufferIndex))
	{
		SAFE_DELETE(backBuffer);
		return NULL;
	}
	renderTargets.push_back(backBuffer);
	return backBuffer;
}

DX12_RenderTargetConfig *Render::CreateRenderTargetConfig(const RtConfigDesc &desc)
{
	for (unsigned int i = 0; i < renderTargetConfigs.size(); i++)
	{
		if (renderTargetConfigs[i]->GetDesc() == desc)
			return renderTargetConfigs[i];
	}
	DX12_RenderTargetConfig *renderTargetConfig = new DX12_RenderTargetConfig;
	if (!renderTargetConfig)
		return NULL;
	if (!renderTargetConfig->Create(desc))
	{
		SAFE_DELETE(renderTargetConfig);
		return NULL;
	}
	renderTargetConfigs.push_back(renderTargetConfig);
	return renderTargetConfig;
}