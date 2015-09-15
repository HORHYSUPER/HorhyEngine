#pragma once

using namespace std;
using namespace Microsoft::WRL;
using namespace DirectX;

namespace D3D11Framework
{
	class DX12_RenderTargetConfig;
	class DX12_Texture;
	class DX12_Shader;
	class DX12_VertexBuffer;
	class DX12_IndexBuffer;
	class DX12_UniformBuffer;
	class DX12_Sampler;

	// For every material, record the offsets in every VBO and triangle counts
	struct SubMesh
	{
		SubMesh() : IndexOffset(0), VertexOffeset(0), TriangleCount(0), diffuseColor(XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)) {}

		int IndexOffset;
		int VertexOffeset;
		int TriangleCount;
		XMFLOAT4 ambientColor;
		XMFLOAT4 diffuseColor;
		XMFLOAT4 specularColor;
		string ambientTexture;
		string SpecularTexture;
		string DiffuseTexture;
		string NormalTexture;
		string DensityTexture;
		string GridDensityTexture;
		std::vector<SimpleVertex> vertexListSimple;
		std::vector<int> indexList;
	};

	struct Influence
	{
		Influence() : position(XMVECTOR{ 0.0f, 0.0f, 0.0f, 0.0f }), normal(XMVECTOR{ 0.0f, 0.0f, 0.0f, 0.0f }) {}

		XMVECTOR position;
		XMVECTOR normal;
	};

	struct AnimationCurves
	{
		XMFLOAT3 position;
	};

	struct ModelBufferData
	{
		ModelBufferData() {}
		XMMATRIX	mWorld;
		FLOAT		material;
		XMVECTOR	vTessellationFactor;            // Edge, inside, minimum tessellation factor and 1/desired triangle size
		FLOAT		detailTessellationHeightScale;	// Height scale for detail tessellation of grid surface
		XMVECTOR	vFrustumPlaneEquation[4];       // View frustum plane equations
		XMMATRIX	mBone[64];
		XMVECTOR	vDiffuseColor;
		FLOAT		specularLevel;
		FLOAT		timeElapsed;
		BOOL		hasDiffuseTexture;
		BOOL		hasSpecularTexture;
		BOOL		hasNormalTexture;
	};

	class Model
	{
		friend class Render;
		friend class Scene;
		friend class Photons;

	public:
		Model();
		Model(const Model &copy);
		~Model();

		Model &operator = (const Model &other);

		void* operator new(size_t i)
		{
			return _aligned_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_aligned_free(p);
		}

		bool Initialize(Scene *scene, FbxScene *pScene, FbxNode* pNode);
		bool animateModel(float keyFrame);
		void StepPhysX();
		void Draw(Camera *camer, RENDER_TYPE rendertype);
		void AddShadowMapSurfaces(unsigned int lightIndex);
		int GetPhysicalType();
		void SetPhysicalType(int pPhysicalType);
		void ApplyForce(PxVec3 force);
		XMFLOAT3 GetPosition();
		void SetPosition(float x, float y, float z);
		PxBounds3 GetWorldBounds();

	private:
		bool m_LoadFbxCache();
		bool m_LoadFbxAnimation(FbxAnimLayer* pAnimLayer);
		bool m_LoadShaders();
		bool m_LoadPhysics();
		bool m_UpdateInfluenceVB();
		void m_RenderBuffers(unsigned int submeshNo, RENDER_TYPE rendertype, GpuCmd &gpuCmd);
		void m_SetShaderParameters(unsigned int submeshNo, Camera *camera, RENDER_TYPE rendertype, GpuCmd &gpuCmd);
		void m_RenderShader(unsigned int submeshNo, RENDER_TYPE rendertype, GpuCmd &gpuCmd);
		void CreateStagingBufferFromBuffer(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pContext, ID3D11Buffer* pInputBuffer, ID3D11Buffer **ppStagingBuffer);
		HRESULT CreateDensityMapTexture(ID3D11ShaderResourceView *detailTessellationHeightTextureRV,
			WCHAR *pszDensityMapFilename, float fDensityScale, float fMeaningfulDifference);
		void CreateDensityMapFromHeightMap(ID3D11Device* pd3dDevice, ID3D11DeviceContext *pDeviceContext, ID3D11Texture2D* pHeightMap,
			float fDensityScale, ID3D11Texture2D** ppDensityMap, ID3D11Texture2D** ppDensityMapStaging = NULL,
			float fMeaningfulDifference = 0.0f);
		HRESULT CreateDensity(ID3D11ShaderResourceView **pGridDensityVBSRV, ID3D11Buffer *vertexBuffer, ID3D11Buffer *indexBuffer,
			unsigned int submeshNo, DWORD dwVertexStride, unsigned long numIndices, float fBaseTextureRepeat, DWORD dwGridTessellation);
		void CreateEdgeDensityVertexStream(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pDeviceContext, XMVECTOR* pTexcoord,
			DWORD dwVertexStride, void *pIndex, DWORD dwIndexSize, DWORD dwNumIndices,
			ID3D11Texture2D* pDensityMap, ID3D11Buffer** ppEdgeDensityVertexStream,
			float fBaseTextureRepeat);
		FbxDouble3 GetMaterialProperty(const FbxSurfaceMaterial * pMaterial,
			const char *pPropertyName,
			const char *pFactorPropertyName,
			string &pTextureName);

		PxRigidStatic							*rigidStaticActor;
		PxRigidDynamic							*rigidDynamicActor;
		XMMATRIX								m_objectMatrix;
		XMMATRIX								m_boneMatrix[255];
		DX12_VertexBuffer						*m_pDX12VertexBuffer;
		DX12_IndexBuffer						*m_pDX12IndexBuffer;
		FbxArray<SubMesh*>						m_subMeshes;
		PxVec3									*m_pPxVertices;
		Vertex									*vertices;
		unsigned int							*indices;
		std::vector<AnimationCurves>			animationCurves;
		std::vector<Influence>					*m_pInfluenceVertex;
		unsigned int							lBlendShapeChannelCount;
		int										physicalType;
		unsigned int							lMaterialCount;
		unsigned int							nVertices;
		unsigned int							nIndices;
		std::vector<ID3D11ShaderResourceView*>	m_pInfluenceVBSRV;
		Scene									*m_pScene;
		FbxMesh									*m_pMesh;
		BOOL									isSkinMesh;
		BOOL									isMorphMesh;
		ModelBufferData							bufferData;
		FbxScene								*m_pFbxScene;
		FbxNode									*m_pNode;
		FbxAnimStack							*m_pAnimStack;
		FbxAnimLayer							*m_pAnimLayer;
		ID3D11Buffer							*m_pSimpleVertexBuffer;
		ID3D11Buffer							*m_pSimpleIndexBuffer;
		FbxAMatrix								m_globallOffPosition;
		PipelineStateObject						*pPso;
		PipelineStateObject						*m_pCubeMapPso;
		PipelineStateObject						*m_pCSMPso;
		DX12_UniformBuffer						*m_pUniformBuffer;
		DX12_Texture *m_pDiffuseTexture, *m_pNormalTexture, *m_pSpecularTexture, *m_pDensityTexture, *m_pGridDensityTexture;
		DX12_RenderTargetConfig					*multiRTC;
		DX12_Shader								*m_pGBufferFillShader;
		DX12_Shader								*m_pGBufferFillShaderWOTess;
		DX12_Shader								*m_pCSMShader;
		DX12_Shader								*m_pDX11ShaderCubeMap[2];
		DX12_Shader								*m_pDX11ShaderVoxelize;
		//DX11_VertexLayout *baseVertexLayout;
		//DX11_VertexBuffer *m_pVertexBuffer;
		//DX11_IndexBuffer *m_pIndexBuffer;
		//DX11_RasterizerState *rasterizerState;
		//DX11_DepthStencilState *depthStencilState;
		//DX11_BlendState *blendState;		
		//RasterizerDesc rasterDesc;
		//DepthStencilDesc depthStencilDesc;
		//BlendDesc blendDesc;
	};
}