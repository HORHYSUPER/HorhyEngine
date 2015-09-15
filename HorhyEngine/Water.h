#pragma once

#ifndef _OCEAN_WAVE_H
#define _OCEAN_WAVE_H
#define PAD16(n) (((n)+15)/16*16)

#include "Render.h"
#include "Framework.h"

namespace D3D11Framework
{
	// Quadtree structures & routines
	struct QuadNode
	{
		XMVECTOR bottom_left;
		float length;
		int lod;

		int sub_node[4];
	};

	struct OceanParameter
	{
		// Must be power of 2.
		int dmap_dim;
		// Typical value is 1000 ~ 2000
		float patch_length;

		// Adjust the time interval for simulation.
		float time_scale;
		// Amplitude for transverse wave. Around 1.0
		float wave_amplitude;
		// Wind direction. Normalization not required.
		XMVECTOR wind_dir;
		// Around 100 ~ 1000
		float wind_speed;
		// This value damps out the waves against the wind direction.
		// Smaller value means higher wind dependency.
		float wind_dependency;
		// The amplitude for longitudinal wave. Must be positive.
		float choppy_scale;
	};

	class Engine;

	class Water
	{
	public:
		Water(OceanParameter& params, ID3D11Device* pd3dDevice, Engine *framework);
		~Water();

		// -------------------------- Initialization & simulation routines ------------------------
		bool initRenderResource(const OceanParameter& ocean_param, ID3D11Device* pd3dDevice);
		// Update ocean wave when tick arrives.
		void updateDisplacementMap(float time);
		void RenderWater(Camera *camera, RENDER_TYPE rendertype, ID3D11ShaderResourceView* displacemnet_map, ID3D11ShaderResourceView* gradient_map, float time, ID3D11DeviceContext* pd3dContext);

		// Texture access
		ID3D11ShaderResourceView* getD3D11DisplacementMap();
		ID3D11ShaderResourceView* getD3D11GradientMap();

		const OceanParameter& getParameters();


	protected:
		struct ocean_vertex
		{
			float index_x;
			float index_y;
		};

		// Constant buffer
		struct Const_Per_Call
		{
			XMMATRIX	mWorld;                         // World matrix
			XMMATRIX	mView;                          // View matrix
			XMMATRIX	mProjection;                    // Projection matrix
			XMMATRIX	mWorldViewProjection;                // VP matrix
			XMMATRIX	MatLocal;
			XMVECTOR	UVBase;
			XMVECTOR	PerlinMovement;
			XMVECTOR	LocalEye[6];
		};

		struct QuadRenderParam
		{
			UINT num_inner_verts;
			UINT num_inner_faces;
			UINT inner_start_index;

			UINT num_boundary_verts;
			UINT num_boundary_faces;
			UINT boundary_start_index;
		};

		typedef struct CSFFT_512x512_Data_t
		{
			// More than one array can be transformed at same time
			UINT slices;

			// For 512x512 config, we need 6 constant buffers
			ID3D11Buffer* pRadix008A_CB[6];

			// Temporary buffers
			ID3D11Buffer* pBuffer_Tmp;
			ID3D11UnorderedAccessView* pUAV_Tmp;
			ID3D11ShaderResourceView* pSRV_Tmp;
		} CSFFT512x512_Plan;

		Engine					*m_pFramework;
		OceanParameter				m_param;
		ID3D11Buffer				*m_pShadingCB;
		ID3D11Buffer				*m_pPerCallCB;
		ID3D11Buffer				*m_pPerFrameCB;
		ID3D11Buffer				*m_pMeshVB;
		ID3D11Buffer				*m_pMeshIB;
		ID3D11Buffer				*m_pBuffer_Float2_H0;
		ID3D11UnorderedAccessView	*m_pUAV_H0;
		ID3D11ShaderResourceView	*m_pSRV_H0;
		ID3D11Buffer				*m_pBuffer_Float_Omega;
		ID3D11UnorderedAccessView	*m_pUAV_Omega;
		ID3D11ShaderResourceView	*m_pSRV_Omega;
		ID3D11Buffer				*m_pBuffer_Float2_Ht;
		ID3D11UnorderedAccessView	*m_pUAV_Ht;
		ID3D11ShaderResourceView	*m_pSRV_Ht;
		ID3D11Buffer				*m_pBuffer_Float_Dxyz;
		ID3D11UnorderedAccessView	*m_pUAV_Dxyz;
		ID3D11ShaderResourceView	*m_pSRV_Dxyz;
		ID3D11Texture1D				*m_pFresnelMap;
		ID3D11Texture2D				*m_pDisplacementMap;
		ID3D11ShaderResourceView	*m_pDisplacementSRV;
		ID3D11RenderTargetView		*m_pDisplacementRTV;
		ID3D11Texture2D				*m_pGradientMap;
		ID3D11ShaderResourceView	*m_pGradientSRV;
		ID3D11RenderTargetView		*m_pGradientRTV;
		ID3D11ShaderResourceView	*m_pSRV_Fresnel;
		ID3D11ShaderResourceView	*m_pSRV_Perlin;
		ID3D11ShaderResourceView	*m_pSRV_ReflectCube;
		ID3D11SamplerState			*m_pPointSamplerState;
		ID3D11SamplerState			*m_pHeightSampler;
		ID3D11SamplerState			*m_pGradientSampler;
		ID3D11SamplerState			*m_pFresnelSampler;
		ID3D11SamplerState			*m_pPerlinSampler;
		ID3D11SamplerState			*m_pCubeSampler;
		ID3D11RasterizerState		*m_pRSState_Solid;
		Shader						*m_pSurfaceShader;
		Shader						*m_pDepthShader;
		std::vector<QuadNode>		m_render_list;
		QuadRenderParam				m_mesh_patterns[9][3][3][3][3];
		float						m_UpperGridCoverage;
		int							m_FurthestCover;
		int							m_Lods;
		int							m_MeshDim;
		float						m_PerlinSize;
		float						m_PerlinSpeed;
		XMVECTOR					m_PerlinAmplitude;
		XMVECTOR					m_PerlinGradient;
		XMVECTOR					m_PerlinOctave;
		XMVECTOR					m_BendParam;
		XMVECTOR					m_SunDir;
		XMVECTOR					m_SunColor;
		float						m_Shineness;
		float						m_SkyBlending;

		// Quad-tree LOD, 0 to 9 (1x1 ~ 512x512) 


		// Pattern lookup array. Filled at init time.
		// Pick a proper mesh pattern according to the adjacent patches.
		QuadRenderParam& selectMeshPattern(const QuadNode& quad_node);

		// Samplers

		// Initialize the vector field.
		void initHeightMap(OceanParameter& params, XMVECTOR* out_h0, float* out_omega);
		float estimateGridCoverage(const QuadNode& quad_node, float screen_area, Camera *camera);
		bool checkNodeVisibility(const QuadNode& quad_node, Camera *camera);
		int buildNodeList(QuadNode& quad_node, Camera *camera, RENDER_TYPE rendertype);
		void fft_512x512_c2c(CSFFT512x512_Plan* fft_plan,
			ID3D11UnorderedAccessView* pUAV_Dst,
			ID3D11ShaderResourceView* pSRV_Dst,
			ID3D11ShaderResourceView* pSRV_Src);
		void radix008A(CSFFT512x512_Plan* fft_plan,
			ID3D11UnorderedAccessView* pUAV_Dst,
			ID3D11ShaderResourceView* pSRV_Src,
			UINT thread_count,
			UINT istride);
		void fft512x512_create_plan(CSFFT512x512_Plan* plan, ID3D11Device* pd3dDevice, UINT slices);
		void createFresnelMap();
		void createSurfaceMesh();
		void createTextureAndViews(UINT width, UINT height, DXGI_FORMAT format,
			ID3D11Texture2D** ppTex, ID3D11ShaderResourceView** ppSRV, ID3D11RenderTargetView** ppRTV);
		void CreateBufferAndUAV(void* data, UINT byte_width, UINT byte_stride,
			ID3D11Buffer** ppBuffer, ID3D11UnorderedAccessView** ppUAV, ID3D11ShaderResourceView** ppSRV);
		int generateInnerMesh(RECT vert_rect, DWORD* output);
		int generateBoundaryMesh(int left_degree, int right_degree, int bottom_degree, int top_degree,
			RECT vert_rect, DWORD* output);
		void create_cbuffers_512x512(CSFFT512x512_Plan* plan, ID3D11Device* pd3dDevice, UINT slices);
		void fft512x512_destroy_plan(CSFFT512x512_Plan* plan);


		ID3D11Buffer* m_pQuadVB;

		// Shaders, layouts and constants
		ID3D11ComputeShader* m_pUpdateSpectrumCS;

		//ID3D11VertexShader* m_pQuadVS;
		ID3D11PixelShader* m_pUpdateDisplacementPS;
		ID3D11PixelShader* m_pGenGradientFoldingPS;

		//ID3D11InputLayout* m_pQuadLayout;

		ID3D11Buffer* m_pImmutableCB;
		//ID3D11Buffer* m_pPerFrameCB;

		// FFT wrap-up
		CSFFT512x512_Plan m_fft_plan;
	};
	}
#endif	// _OCEAN_WAVE_H
