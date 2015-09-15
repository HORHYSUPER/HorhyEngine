#include "stdafx.h"
#include "Engine.h"
#include "GameWorld.h"
#include "Render.h"
#include "Camera.h"
#include "ILight.h"
#include "Scene.h"
#include "Buffer.h"
#include "macros.h"
#include "Util.h"
#include "GetPosition.h"
#include "VoxelGlobalIllumination.h"
#include "DX12_Sampler.h"
#include "DX12_Texture.h"
#include "PipelineStateObject.h"
#include "DX12_VertexLayout.h"
#include "DX12_VertexBuffer.h"
#include "DX12_IndexBuffer.h"
#include "DX12_UniformBuffer.h"
#include "DX12_RenderTargetConfig.h"
#include "ResourceManager.h"
#include "Model.h"

using namespace D3D11Framework;
using namespace DirectX;
using namespace physx;
using namespace std;

float g_time = 0;
const unsigned int MAX_BLEND_SHAPE_DEFORMER_COUNT = 32;

#define FROUND(x)    ( (int)( (x) + 0.5 ) )
void ExtractPlanesFromFrustum(XMVECTOR* pPlaneEquation, const XMMATRIX* pMatrix, bool bNormalize = TRUE);

//--------------------------------------------------------------------------------------
// Helper function to normalize a plane
//--------------------------------------------------------------------------------------
void NormalizePlane(XMVECTOR* pPlaneEquation)
{
	float mag;

	mag = sqrt(pPlaneEquation->m128_f32[0] * pPlaneEquation->m128_f32[0] +
		pPlaneEquation->m128_f32[1] * pPlaneEquation->m128_f32[1] +
		pPlaneEquation->m128_f32[2] * pPlaneEquation->m128_f32[2]);

	pPlaneEquation->m128_f32[0] = pPlaneEquation->m128_f32[0] / mag;
	pPlaneEquation->m128_f32[1] = pPlaneEquation->m128_f32[1] / mag;
	pPlaneEquation->m128_f32[2] = pPlaneEquation->m128_f32[2] / mag;
	pPlaneEquation->m128_f32[3] = pPlaneEquation->m128_f32[3] / mag;
}


//--------------------------------------------------------------------------------------
// Extract all 6 plane equations from frustum denoted by supplied matrix
//--------------------------------------------------------------------------------------
void ExtractPlanesFromFrustum(XMVECTOR* pPlaneEquation, const XMMATRIX* pMatrix, bool bNormalize)
{
	// Left clipping plane
	pPlaneEquation[0].m128_f32[0] = pMatrix->r[0].m128_f32[3] + pMatrix->r[0].m128_f32[0];
	pPlaneEquation[0].m128_f32[1] = pMatrix->r[1].m128_f32[3] + pMatrix->r[1].m128_f32[0];
	pPlaneEquation[0].m128_f32[2] = pMatrix->r[2].m128_f32[3] + pMatrix->r[2].m128_f32[0];
	pPlaneEquation[0].m128_f32[3] = pMatrix->r[3].m128_f32[3] + pMatrix->r[3].m128_f32[0];

	// Right clipping plane
	pPlaneEquation[1].m128_f32[0] = pMatrix->r[0].m128_f32[3] - pMatrix->r[0].m128_f32[0];
	pPlaneEquation[1].m128_f32[1] = pMatrix->r[1].m128_f32[3] - pMatrix->r[1].m128_f32[0];
	pPlaneEquation[1].m128_f32[2] = pMatrix->r[2].m128_f32[3] - pMatrix->r[2].m128_f32[0];
	pPlaneEquation[1].m128_f32[3] = pMatrix->r[3].m128_f32[3] - pMatrix->r[3].m128_f32[0];

	// Top clipping plane
	pPlaneEquation[2].m128_f32[0] = pMatrix->r[0].m128_f32[3] - pMatrix->r[0].m128_f32[1];
	pPlaneEquation[2].m128_f32[1] = pMatrix->r[1].m128_f32[3] - pMatrix->r[1].m128_f32[1];
	pPlaneEquation[2].m128_f32[2] = pMatrix->r[2].m128_f32[3] - pMatrix->r[2].m128_f32[1];
	pPlaneEquation[2].m128_f32[3] = pMatrix->r[3].m128_f32[3] - pMatrix->r[3].m128_f32[1];

	// Bottom clipping plane
	pPlaneEquation[3].m128_f32[0] = pMatrix->r[0].m128_f32[3] + pMatrix->r[0].m128_f32[1];
	pPlaneEquation[3].m128_f32[1] = pMatrix->r[1].m128_f32[3] + pMatrix->r[1].m128_f32[1];
	pPlaneEquation[3].m128_f32[2] = pMatrix->r[2].m128_f32[3] + pMatrix->r[2].m128_f32[1];
	pPlaneEquation[3].m128_f32[3] = pMatrix->r[3].m128_f32[3] + pMatrix->r[3].m128_f32[1];

	// Near clipping plane
	pPlaneEquation[4].m128_f32[0] = pMatrix->r[0].m128_f32[2];
	pPlaneEquation[4].m128_f32[1] = pMatrix->r[1].m128_f32[2];
	pPlaneEquation[4].m128_f32[2] = pMatrix->r[2].m128_f32[2];
	pPlaneEquation[4].m128_f32[3] = pMatrix->r[3].m128_f32[2];

	// Far clipping plane
	pPlaneEquation[5].m128_f32[0] = pMatrix->r[0].m128_f32[3] - pMatrix->r[0].m128_f32[2];
	pPlaneEquation[5].m128_f32[1] = pMatrix->r[1].m128_f32[3] - pMatrix->r[1].m128_f32[2];
	pPlaneEquation[5].m128_f32[2] = pMatrix->r[2].m128_f32[3] - pMatrix->r[2].m128_f32[2];
	pPlaneEquation[5].m128_f32[3] = pMatrix->r[3].m128_f32[3] - pMatrix->r[3].m128_f32[2];

	// Normalize the plane equations, if requested
	if (bNormalize)
	{
		NormalizePlane(&pPlaneEquation[0]);
		NormalizePlane(&pPlaneEquation[1]);
		NormalizePlane(&pPlaneEquation[2]);
		NormalizePlane(&pPlaneEquation[3]);
		NormalizePlane(&pPlaneEquation[4]);
		NormalizePlane(&pPlaneEquation[5]);
	}
}

XMVECTOR						g_pWorldSpaceFrustumPlaneEquation[6];

Model::Model()
{
	//m_pInfluenceVBSRV = NULL;
	pPso = NULL;
	m_pMesh = NULL;
	m_pCSMShader = NULL;
	m_pCSMPso = NULL;
	m_pDiffuseTexture = NULL;
	m_pSpecularTexture = NULL;
	m_pNormalTexture = NULL;
	m_pDensityTexture = NULL;
	m_pGridDensityTexture = NULL;
	isMorphMesh = false;
	isSkinMesh = false;
}

Model::Model(const Model &copy)
{
	operator=(copy);
}

Model::~Model()
{
	/*_RELEASE(m_pVertexBuffer);
	_RELEASE(m_pIndexBuffer);
	_RELEASE(m_pSimpleVertexBuffer);
	_RELEASE(m_pSimpleIndexBuffer);
	_CLOSE(m_pShader);*/
}

Model &Model::operator=(const Model &other)
{
	if (this != &other)
	{
		this->pPso = other.pPso;
		this->m_pCubeMapPso = other.m_pCubeMapPso;
		this->animationCurves = other.animationCurves;
		//this->buf[0] = other.buf[0];
		//this->flipV = other.flipV;
		this->m_pInfluenceVBSRV = other.m_pInfluenceVBSRV;
		this->indices = other.indices;
		this->isMorphMesh = other.isMorphMesh;
		this->lMaterialCount = other.lMaterialCount;
		this->m_pGBufferFillShader = other.m_pGBufferFillShader;
		this->m_pCSMShader = other.m_pCSMShader;
		this->m_pCSMPso = other.m_pCSMPso;
		this->m_pGBufferFillShaderWOTess = other.m_pGBufferFillShaderWOTess;
		this->m_pDX11ShaderCubeMap[0] = other.m_pDX11ShaderCubeMap[0];
		this->m_pDX11ShaderCubeMap[1] = other.m_pDX11ShaderCubeMap[1];
		this->m_pDX11ShaderVoxelize = other.m_pDX11ShaderVoxelize;
		this->m_pDiffuseTexture = other.m_pDiffuseTexture;
		this->m_pNormalTexture = other.m_pNormalTexture;
		this->m_pSpecularTexture = other.m_pSpecularTexture;
		this->m_pDensityTexture = other.m_pDensityTexture;
		this->m_pGridDensityTexture = other.m_pGridDensityTexture;
		for (size_t i = 0; i < 64; i++)
			this->m_boneMatrix[i] = other.m_boneMatrix[i];
		this->m_objectMatrix = other.m_objectMatrix;
		this->m_pAnimLayer = other.m_pAnimLayer;
		this->m_pAnimStack = other.m_pAnimStack;
		this->m_pFbxScene = other.m_pFbxScene;
		this->m_pInfluenceVertex = other.m_pInfluenceVertex;
		this->m_pMesh = other.m_pMesh;
		this->m_pNode = other.m_pNode;
		this->m_pScene = other.m_pScene;
		this->m_pSimpleIndexBuffer = other.m_pSimpleIndexBuffer;
		this->m_pSimpleVertexBuffer = other.m_pSimpleVertexBuffer;
		//this->m_pIndexBuffer = other.m_pIndexBuffer;
		//this->m_pVertexBuffer = other.m_pVertexBuffer;
		this->m_subMeshes = other.m_subMeshes;
		this->multiRTC = other.multiRTC;
		this->m_globallOffPosition = other.m_globallOffPosition;
		this->nIndices = other.nIndices;
		this->nVertices = other.nVertices;
		this->physicalType = other.physicalType;
		this->rigidDynamicActor = other.rigidDynamicActor;
		this->rigidStaticActor = other.rigidStaticActor;
		this->isSkinMesh = other.isSkinMesh;
		this->vertices = other.vertices;
		this->m_pUniformBuffer = other.m_pUniformBuffer;
		this->m_pDX12VertexBuffer = other.m_pDX12VertexBuffer;
		this->m_pDX12IndexBuffer = other.m_pDX12IndexBuffer;
	}

	return *this;
}

//--------------------------------------------------------------------------------------
// Helper function to create a staging buffer from a buffer resource
//--------------------------------------------------------------------------------------
void Model::CreateStagingBufferFromBuffer(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pContext,
	ID3D11Buffer* pInputBuffer, ID3D11Buffer **ppStagingBuffer)
{
	D3D11_BUFFER_DESC BufferDesc;

	// Get buffer description
	pInputBuffer->GetDesc(&BufferDesc);

	// Modify description to create STAGING buffer
	BufferDesc.BindFlags = 0;
	BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
	BufferDesc.Usage = D3D11_USAGE_STAGING;

	// Create staging buffer
	pd3dDevice->CreateBuffer(&BufferDesc, NULL, ppStagingBuffer);

	// Copy buffer into STAGING buffer
	_MUTEXLOCK(Engine::m_pMutex);
	Engine::m_pRender->GetDeviceContext()->CopyResource(*ppStagingBuffer, pInputBuffer);
	_MUTEXUNLOCK(Engine::m_pMutex);
}

//--------------------------------------------------------------------------------------
// Sample density map along specified edge and return maximum value
//--------------------------------------------------------------------------------------
float GetMaximumDensityAlongEdge(DWORD *pTexture2D, DWORD dwStride, DWORD dwWidth, DWORD dwHeight,
	XMVECTOR fTexcoord0, XMVECTOR fTexcoord1)
{
#define GETTEXEL(x, y)       ( *(RGBQUAD *)( ( (BYTE *)pTexture2D ) + ( (y)*dwStride ) + ( (x)*sizeof(DWORD) ) ) )
#define LERP(x, y, a)        ( (x)*(1.0f-(a)) + (y)*(a) )

	// Convert texture coordinates to texel coordinates using round-to-nearest
	int nU0 = FROUND(fTexcoord0.m128_f32[0] * (dwWidth - 1));
	int nV0 = FROUND(fTexcoord0.m128_f32[1] * (dwHeight - 1));
	int nU1 = FROUND(fTexcoord1.m128_f32[0] * (dwWidth - 1));
	int nV1 = FROUND(fTexcoord1.m128_f32[1] * (dwHeight - 1));

	// Wrap texture coordinates
	nU0 = nU0 % dwWidth;
	nV0 = nV0 % dwHeight;
	nU1 = nU1 % dwWidth;
	nV1 = nV1 % dwHeight;

	// Find how many texels on edge
	int nNumTexelsOnEdge = max(abs(nU1 - nU0), abs(nV1 - nV0)) + 1;

	float fU, fV;
	float fMaxDensity = 0.0f;
	for (int i = 0; i < nNumTexelsOnEdge; ++i)
	{
		// Calculate current texel coordinates
		fU = LERP((float)nU0, (float)nU1, ((float)i) / ((nNumTexelsOnEdge == 1 ? 2 : nNumTexelsOnEdge) - 1));
		fV = LERP((float)nV0, (float)nV1, ((float)i) / ((nNumTexelsOnEdge == 1 ? 2 : nNumTexelsOnEdge) - 1));

		// Round texel texture coordinates to nearest
		int nCurrentU = FROUND(fU);
		int nCurrentV = FROUND(fV);

		// Update max density along edge
		fMaxDensity = max(fMaxDensity, GETTEXEL(nCurrentU, nCurrentV).rgbBlue / 255.0f);
	}

	return fMaxDensity;
}

//--------------------------------------------------------------------------------------
// Calculate the maximum density along a triangle edge and
// store it in the resulting vertex stream.
//--------------------------------------------------------------------------------------
void Model::CreateEdgeDensityVertexStream(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pDeviceContext, XMVECTOR* pTexcoord,
	DWORD dwVertexStride, void *pIndex, DWORD dwIndexSize, DWORD dwNumIndices,
	ID3D11Texture2D* pDensityMap, ID3D11Buffer** ppEdgeDensityVertexStream,
	float fBaseTextureRepeat)
{
	HRESULT                hr;
	D3D11_SUBRESOURCE_DATA InitData;
	ID3D11Texture2D*       pDensityMapReadFrom;
	DWORD                  dwNumTriangles = dwNumIndices / 3;

	// Create host memory buffer
	XMVECTOR *pEdgeDensityBuffer = new XMVECTOR[dwNumTriangles];

	// Retrieve density map description
	D3D11_TEXTURE2D_DESC Tex2DDesc;
	pDensityMap->GetDesc(&Tex2DDesc);

	// Check if provided texture can be Mapped() for reading directly
	BOOL bCanBeRead = Tex2DDesc.CPUAccessFlags & D3D11_CPU_ACCESS_READ;
	if (!bCanBeRead)
	{
		// Texture cannot be read directly, need to create STAGING texture and copy contents into it
		Tex2DDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		Tex2DDesc.Usage = D3D11_USAGE_STAGING;
		Tex2DDesc.BindFlags = 0;
		pd3dDevice->CreateTexture2D(&Tex2DDesc, NULL, &pDensityMapReadFrom);

		// Copy contents of height map into staging version
		_MUTEXLOCK(Engine::m_pMutex);
		pDeviceContext->CopyResource(pDensityMapReadFrom, pDensityMap);
		_MUTEXUNLOCK(Engine::m_pMutex);
	}
	else
	{
		pDensityMapReadFrom = pDensityMap;
	}

	// Map density map for reading
	D3D11_MAPPED_SUBRESOURCE MappedSubResource;
	_MUTEXLOCK(Engine::m_pMutex);
	pDeviceContext->Map(pDensityMapReadFrom, 0, D3D11_MAP_READ, 0, &MappedSubResource);
	_MUTEXUNLOCK(Engine::m_pMutex);

	// Loop through all triangles
	for (DWORD i = 0; i < dwNumTriangles; ++i)
	{
		DWORD wIndex0, wIndex1, wIndex2;

		// Retrieve indices of current triangle
		if (dwIndexSize == sizeof(WORD))
		{
			wIndex0 = (DWORD)((WORD *)pIndex)[3 * i + 0];
			wIndex1 = (DWORD)((WORD *)pIndex)[3 * i + 1];
			wIndex2 = (DWORD)((WORD *)pIndex)[3 * i + 2];
		}
		else
		{
			wIndex0 = ((DWORD *)pIndex)[3 * i + 0];
			wIndex1 = ((DWORD *)pIndex)[3 * i + 1];
			wIndex2 = ((DWORD *)pIndex)[3 * i + 2];
		}

		// Retrieve texture coordinates of each vertex making up current triangle
		XMVECTOR    vTexcoord0 = *(XMVECTOR *)((BYTE *)pTexcoord + wIndex0 * dwVertexStride);
		XMVECTOR    vTexcoord1 = *(XMVECTOR *)((BYTE *)pTexcoord + wIndex1 * dwVertexStride);
		XMVECTOR    vTexcoord2 = *(XMVECTOR *)((BYTE *)pTexcoord + wIndex2 * dwVertexStride);

		// Adjust texture coordinates with texture repeat
		vTexcoord0 *= fBaseTextureRepeat;
		vTexcoord1 *= fBaseTextureRepeat;
		vTexcoord2 *= fBaseTextureRepeat;

		// Sample density map at vertex location
		float fEdgeDensity0 = GetMaximumDensityAlongEdge((DWORD *)MappedSubResource.pData, MappedSubResource.RowPitch,
			Tex2DDesc.Width, Tex2DDesc.Height, vTexcoord0, vTexcoord1);
		float fEdgeDensity1 = GetMaximumDensityAlongEdge((DWORD *)MappedSubResource.pData, MappedSubResource.RowPitch,
			Tex2DDesc.Width, Tex2DDesc.Height, vTexcoord1, vTexcoord2);
		float fEdgeDensity2 = GetMaximumDensityAlongEdge((DWORD *)MappedSubResource.pData, MappedSubResource.RowPitch,
			Tex2DDesc.Width, Tex2DDesc.Height, vTexcoord2, vTexcoord0);

		// Store edge density in x,y,z and store maximum density in .w
		pEdgeDensityBuffer[i] = XMVectorSet(fEdgeDensity0, fEdgeDensity1, fEdgeDensity2,
			max(max(fEdgeDensity0, fEdgeDensity1), fEdgeDensity2));
	}

	// Unmap density map
	_MUTEXLOCK(Engine::m_pMutex);
	pDeviceContext->Unmap(pDensityMapReadFrom, 0);
	_MUTEXUNLOCK(Engine::m_pMutex);
	// Release staging density map if we had to create it
	if (!bCanBeRead)
	{
		SAFE_RELEASE(pDensityMapReadFrom);
	}

	// Set source buffer for initialization data
	InitData.pSysMem = (void *)pEdgeDensityBuffer;

	// Create density vertex stream buffer: 1 float per entry
	D3D11_BUFFER_DESC bd;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = dwNumTriangles * sizeof(XMVECTOR);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;
	hr = pd3dDevice->CreateBuffer(&bd, &InitData, ppEdgeDensityVertexStream);
	if (FAILED(hr))
		OutputDebugString(L"Failed to create vertex buffer.\n");

	// Free host memory buffer
	delete[] pEdgeDensityBuffer;
}

//--------------------------------------------------------------------------------------
// Create a density texture map from a height map
//--------------------------------------------------------------------------------------
void Model::CreateDensityMapFromHeightMap(ID3D11Device* pd3dDevice, ID3D11DeviceContext *pDeviceContext, ID3D11Texture2D* pHeightMap,
	float fDensityScale, ID3D11Texture2D** ppDensityMap, ID3D11Texture2D** ppDensityMapStaging,
	float fMeaningfulDifference)
{
#define ReadPixelFromMappedSubResource(x, y)       ( *( (RGBQUAD *)((BYTE *)MappedSubResourceRead.pData + (y)*MappedSubResourceRead.RowPitch + (x)*sizeof(DWORD)) ) )
#define WritePixelToMappedSubResource(x, y, value) ( ( *( (DWORD *)((BYTE *)MappedSubResourceWrite.pData + (y)*MappedSubResourceWrite.RowPitch + (x)*sizeof(DWORD)) ) ) = (DWORD)(value) )

	// Get description of source texture
	D3D11_TEXTURE2D_DESC Desc;
	pHeightMap->GetDesc(&Desc);

	// Create density map with the same properties
	pd3dDevice->CreateTexture2D(&Desc, NULL, ppDensityMap);

	// Create STAGING height map texture
	ID3D11Texture2D* pHeightMapStaging;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	Desc.Usage = D3D11_USAGE_STAGING;
	Desc.BindFlags = 0;
	pd3dDevice->CreateTexture2D(&Desc, NULL, &pHeightMapStaging);

	// Create STAGING density map
	ID3D11Texture2D* pDensityMapStaging;
	pd3dDevice->CreateTexture2D(&Desc, NULL, &pDensityMapStaging);

	// Copy contents of height map into staging version
	_MUTEXLOCK(Engine::m_pMutex);
	pDeviceContext->CopyResource(pHeightMapStaging, pHeightMap);
	_MUTEXUNLOCK(Engine::m_pMutex);
	// Compute density map and store into staging version
	D3D11_MAPPED_SUBRESOURCE MappedSubResourceRead, MappedSubResourceWrite;
	_MUTEXLOCK(Engine::m_pMutex);
	pDeviceContext->Map(pHeightMapStaging, 0, D3D11_MAP_READ, 0, &MappedSubResourceRead);
	pDeviceContext->Map(pDensityMapStaging, 0, D3D11_MAP_WRITE, 0, &MappedSubResourceWrite);
	_MUTEXUNLOCK(Engine::m_pMutex);
	// Loop map and compute derivatives
	for (int j = 0; j <= (int)Desc.Height - 1; ++j)
	{
		for (int i = 0; i <= (int)Desc.Width - 1; ++i)
		{
			// Edges are set to the minimum value
			if ((j == 0) ||
				(i == 0) ||
				(j == ((int)Desc.Height - 1)) ||
				(i == ((int)Desc.Width - 1)))
			{
				WritePixelToMappedSubResource(i, j, (DWORD)1);
				continue;
			}

			// Get current pixel
			RGBQUAD CurrentPixel = ReadPixelFromMappedSubResource(i, j);

			// Get left pixel
			RGBQUAD LeftPixel = ReadPixelFromMappedSubResource(i - 1, j);

			// Get right pixel
			RGBQUAD RightPixel = ReadPixelFromMappedSubResource(i + 1, j);

			// Get top pixel
			RGBQUAD TopPixel = ReadPixelFromMappedSubResource(i, j - 1);

			// Get bottom pixel
			RGBQUAD BottomPixel = ReadPixelFromMappedSubResource(i, j + 1);

			// Get top-right pixel
			RGBQUAD TopRightPixel = ReadPixelFromMappedSubResource(i + 1, j - 1);

			// Get bottom-right pixel
			RGBQUAD BottomRightPixel = ReadPixelFromMappedSubResource(i + 1, j + 1);

			// Get top-left pixel
			RGBQUAD TopLeftPixel = ReadPixelFromMappedSubResource(i - 1, j - 1);

			// Get bottom-left pixel
			RGBQUAD BottomLeft = ReadPixelFromMappedSubResource(i - 1, j + 1);

			float fCurrentPixelHeight = (CurrentPixel.rgbReserved / 255.0f);
			float fCurrentPixelLeftHeight = (LeftPixel.rgbReserved / 255.0f);
			float fCurrentPixelRightHeight = (RightPixel.rgbReserved / 255.0f);
			float fCurrentPixelTopHeight = (TopPixel.rgbReserved / 255.0f);
			float fCurrentPixelBottomHeight = (BottomPixel.rgbReserved / 255.0f);
			float fCurrentPixelTopRightHeight = (TopRightPixel.rgbReserved / 255.0f);
			float fCurrentPixelBottomRightHeight = (BottomRightPixel.rgbReserved / 255.0f);
			float fCurrentPixelTopLeftHeight = (TopLeftPixel.rgbReserved / 255.0f);
			float fCurrentPixelBottomLeftHeight = (BottomLeft.rgbReserved / 255.0f);

			float fDensity = 0.0f;
			float fHorizontalVariation = fabs((fCurrentPixelRightHeight - fCurrentPixelHeight) -
				(fCurrentPixelHeight - fCurrentPixelLeftHeight));
			float fVerticalVariation = fabs((fCurrentPixelBottomHeight - fCurrentPixelHeight) -
				(fCurrentPixelHeight - fCurrentPixelTopHeight));
			float fFirstDiagonalVariation = fabs((fCurrentPixelTopRightHeight - fCurrentPixelHeight) -
				(fCurrentPixelHeight - fCurrentPixelBottomLeftHeight));
			float fSecondDiagonalVariation = fabs((fCurrentPixelBottomRightHeight - fCurrentPixelHeight) -
				(fCurrentPixelHeight - fCurrentPixelTopLeftHeight));
			if (fHorizontalVariation >= fMeaningfulDifference) fDensity += 0.293f * fHorizontalVariation;
			if (fVerticalVariation >= fMeaningfulDifference) fDensity += 0.293f * fVerticalVariation;
			if (fFirstDiagonalVariation >= fMeaningfulDifference) fDensity += 0.207f * fFirstDiagonalVariation;
			if (fSecondDiagonalVariation >= fMeaningfulDifference) fDensity += 0.207f * fSecondDiagonalVariation;

			// Scale density with supplied scale                
			fDensity *= fDensityScale;

			// Clamp density
			fDensity = max(min(fDensity, 1.0f), 1.0f / 255.0f);

			// Write density into density map
			WritePixelToMappedSubResource(i, j, (DWORD)(fDensity * 255.0f));
		}
	}
	_MUTEXLOCK(Engine::m_pMutex);
	pDeviceContext->Unmap(pDensityMapStaging, 0);
	pDeviceContext->Unmap(pHeightMapStaging, 0);
	_MUTEXUNLOCK(Engine::m_pMutex);
	SAFE_RELEASE(pHeightMapStaging);

	// Copy contents of density map into DEFAULT density version
	_MUTEXLOCK(Engine::m_pMutex);
	pDeviceContext->CopyResource(*ppDensityMap, pDensityMapStaging);
	_MUTEXUNLOCK(Engine::m_pMutex);
	// If a pointer to a staging density map was provided then return it with staging density map
	if (ppDensityMapStaging)
	{
		*ppDensityMapStaging = pDensityMapStaging;
	}
	else
	{
		SAFE_RELEASE(pDensityMapStaging);
	}
}

HRESULT Model::CreateDensityMapTexture(ID3D11ShaderResourceView *detailTessellationHeightTextureRV, WCHAR *pszDensityMapFilename, float fDensityScale, float fMeaningfulDifference)
{
	HRESULT hr = S_OK;

	// Get description of source texture
	ID3D11Texture2D* pHeightMap;
	ID3D11Texture2D* pDensityMap;
	detailTessellationHeightTextureRV->GetResource((ID3D11Resource**)&pHeightMap);

	// Create density map from height map
	_MUTEXLOCK(Engine::m_pMutex);
	CreateDensityMapFromHeightMap(Engine::m_pRender->GetDevice(), Engine::m_pRender->GetDeviceContext(), pHeightMap,
		fDensityScale, &pDensityMap, NULL,
		fMeaningfulDifference);

	//ScratchImage image;
	//hr = CaptureTexture(Engine::m_pRender->GetDevice(), Engine::m_pRender->GetDeviceContext(), pDensityMap, image);
	//if (SUCCEEDED(hr))
	//{
	//	hr = SaveToDDSFile(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
	//		DDS_FLAGS_NONE, pszDensityMapFilename);
	//	if (FAILED(hr))
	//	{
	//		Log::Get()->Err("Не удалось сохранить текстуру плотности в файл");
	//	}
	//}
	_MUTEXUNLOCK(Engine::m_pMutex);

	SAFE_RELEASE(pDensityMap);
	SAFE_RELEASE(pHeightMap);

	return hr;
}

HRESULT Model::CreateDensity(ID3D11ShaderResourceView **pGridDensityVBSRV, ID3D11Buffer *vertexBuffer, ID3D11Buffer *indexBuffer,
	unsigned int submeshNo, DWORD dwVertexStride, unsigned long numIndices, float fBaseTextureRepeat, DWORD dwGridTessellation)
{
	HRESULT hr;
	ID3D11Buffer *pGridDensityVB;

	// Create STAGING versions of VB and IB
	ID3D11Buffer* pGridVBSTAGING = NULL;
	CreateStagingBufferFromBuffer(Engine::m_pRender->GetDevice(), Engine::m_pRender->GetDeviceContext(), vertexBuffer, &pGridVBSTAGING);

	ID3D11Buffer* pGridIBSTAGING = NULL;
	CreateStagingBufferFromBuffer(Engine::m_pRender->GetDevice(), Engine::m_pRender->GetDeviceContext(), indexBuffer, &pGridIBSTAGING);

	ID3D11Texture2D* pDensityMap = NULL;
	//m_pDensityTexture->GetShaderResourceView()->GetResource((ID3D11Resource **)&pDensityMap);

	// Map those buffers for reading
	D3D11_MAPPED_SUBRESOURCE VBRead;
	D3D11_MAPPED_SUBRESOURCE IBRead;
	_MUTEXLOCK(Engine::m_pMutex);
	Engine::m_pRender->GetDeviceContext()->Map(pGridVBSTAGING, 0, D3D11_MAP_READ, 0, &VBRead);
	Engine::m_pRender->GetDeviceContext()->Map(pGridIBSTAGING, 0, D3D11_MAP_READ, 0, &IBRead);
	_MUTEXUNLOCK(Engine::m_pMutex);
	// Set up a pointer to texture coordinates
	XMVECTOR* pTexcoord = (XMVECTOR *)(&((float *)VBRead.pData)[3]);

	// Create vertex buffer containing edge density data
	CreateEdgeDensityVertexStream(Engine::m_pRender->GetDevice(), Engine::m_pRender->GetDeviceContext(), pTexcoord, dwVertexStride,
		IBRead.pData, sizeof(int), numIndices,
		pDensityMap, &pGridDensityVB, fBaseTextureRepeat);

	pDensityMap->Release();
	_MUTEXLOCK(Engine::m_pMutex);
	Engine::m_pRender->GetDeviceContext()->Unmap(pGridIBSTAGING, 0);
	Engine::m_pRender->GetDeviceContext()->Unmap(pGridVBSTAGING, 0);
	_MUTEXUNLOCK(Engine::m_pMutex);
	SAFE_RELEASE(pGridIBSTAGING);
	SAFE_RELEASE(pGridVBSTAGING);

	// Create SRV for density vertex buffer
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVBufferDesc;
	SRVBufferDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SRVBufferDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	SRVBufferDesc.Buffer.ElementOffset = 0;
	SRVBufferDesc.Buffer.ElementWidth = numIndices / 3;
	hr = Engine::m_pRender->GetDevice()->CreateShaderResourceView(pGridDensityVB, &SRVBufferDesc, pGridDensityVBSRV);
	if (FAILED(hr))
		return hr;

	return hr;
}

void ComputeClusterDeformation(FbxAMatrix& pGlobalPosition,
	FbxMesh* m_pMesh,
	FbxCluster* pCluster,
	FbxAMatrix& pVertexTransformMatrix,
	FbxTime pTime,
	FbxPose* pPose)
{
	FbxCluster::ELinkMode lClusterMode = pCluster->GetLinkMode();

	FbxAMatrix lReferenceGlobalInitPosition;
	FbxAMatrix lReferenceGlobalCurrentPosition;
	FbxAMatrix lAssociateGlobalInitPosition;
	FbxAMatrix lAssociateGlobalCurrentPosition;
	FbxAMatrix lClusterGlobalInitPosition;
	FbxAMatrix lClusterGlobalCurrentPosition;

	FbxAMatrix lReferenceGeometry;
	FbxAMatrix lAssociateGeometry;
	FbxAMatrix lClusterGeometry;

	FbxAMatrix lClusterRelativeInitPosition;
	FbxAMatrix lClusterRelativeCurrentPositionInverse;

	FbxAMatrix lClusterGlobalCurrentPositionTrans;

	pCluster->GetTransformMatrix(lReferenceGlobalInitPosition);

	lReferenceGlobalCurrentPosition = pGlobalPosition;
	// Multiply lReferenceGlobalInitPosition by Geometric Transformation
	lReferenceGeometry = GetGeometry(m_pMesh->GetNode());
	lReferenceGlobalInitPosition *= lReferenceGeometry;

	// Get the link initial global position and the link current global position.
	pCluster->GetTransformLinkMatrix(lClusterGlobalInitPosition);
	lClusterGlobalCurrentPosition = GetGlobalPosition(pCluster->GetLink(), pTime, pPose);

	// Compute the initial position of the link relative to the reference.
	lClusterRelativeInitPosition = lClusterGlobalInitPosition.Inverse() * lReferenceGlobalInitPosition;

	// Compute the current position of the link relative to the reference.
	lClusterRelativeCurrentPositionInverse = lReferenceGlobalCurrentPosition.Inverse() * lClusterGlobalCurrentPosition;

	// Compute the shift of the link relative to the reference.
	pVertexTransformMatrix = lClusterRelativeCurrentPositionInverse * lClusterRelativeInitPosition;
}

bool Model::m_LoadFbxAnimation(FbxAnimLayer* pAnimLayer)
{
	FbxAnimCurve* lAnimCurve = NULL;

	// Display curves specific to a light or marker.
	FbxNodeAttribute* lNodeAttribute = m_pNode->GetNodeAttribute();
	if (lNodeAttribute)
	{
		// Display curves specific to a geometry.
		if (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh ||
			lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eNurbs ||
			lNodeAttribute->GetAttributeType() == FbxNodeAttribute::ePatch)
		{
			FbxGeometry* lGeometry = (FbxGeometry*)lNodeAttribute;

			int lBlendShapeDeformerCount = lGeometry->GetDeformerCount(FbxDeformer::eBlendShape);
			for (int lBlendShapeIndex = 0; lBlendShapeIndex < lBlendShapeDeformerCount; ++lBlendShapeIndex)
			{
				FbxBlendShape* lBlendShape = (FbxBlendShape*)lGeometry->GetDeformer(lBlendShapeIndex, FbxDeformer::eBlendShape);

				lBlendShapeChannelCount = lBlendShape->GetBlendShapeChannelCount();
				m_pInfluenceVBSRV.resize(lBlendShapeChannelCount);

				m_pInfluenceVertex = new std::vector<Influence>[lBlendShapeChannelCount];
				if (!m_pInfluenceVertex)
					return false;

				for (int lChannelIndex = 0; lChannelIndex < lBlendShapeChannelCount; ++lChannelIndex)
				{
					FbxBlendShapeChannel* lChannel = lBlendShape->GetBlendShapeChannel(lChannelIndex);
					const char* lChannelName = lChannel->GetName();

					lAnimCurve = lGeometry->GetShapeChannel(lBlendShapeIndex, lChannelIndex, pAnimLayer, true);
					if (lChannel)
					{
						if (!lAnimCurve) continue;

						FbxTime pTime;
						pTime.SetTimeString("100");
						double lWeight = lAnimCurve->Evaluate(pTime);

						int lShapeCount = lChannel->GetTargetShapeCount();
						double* lFullWeights = lChannel->GetTargetShapeFullWeights();

						// Find out which scope the lWeight falls in.
						int lStartIndex = -1;
						int lEndIndex = -1;
						for (int lShapeIndex = 0; lShapeIndex < lShapeCount; ++lShapeIndex)
						{
							if (lWeight > 0 && lWeight <= lFullWeights[0])
							{
								lEndIndex = 0;
								break;
							}
							if (lWeight > lFullWeights[lShapeIndex] && lWeight < lFullWeights[lShapeIndex + 1])
							{
								lStartIndex = lShapeIndex;
								lEndIndex = lShapeIndex + 1;
								break;
							}
						}

						FbxShape* lStartShape = NULL;
						FbxShape* lEndShape = NULL;
						if (lStartIndex > -1)
						{
							lStartShape = lChannel->GetTargetShape(lStartIndex);
						}
						if (lEndIndex > -1)
						{
							lEndShape = lChannel->GetTargetShape(lEndIndex);
						}

						//The weight percentage falls between base geometry and the first target shape.
						if (lStartIndex == -1 && lEndShape)
						{
							isMorphMesh = true;
							double lEndWeight = lFullWeights[0];
							// Calculate the real weight.
							lWeight = (lWeight / lEndWeight) * 100;
							for (size_t i = 0; i < nVertices; i++)
							{
								m_pInfluenceVertex[lChannelIndex].resize(m_pInfluenceVertex[lChannelIndex].size() + 1);
								m_pInfluenceVertex[lChannelIndex][i].position.m128_f32[0] = (float)lEndShape->GetControlPointAt(vertices[i].vertIndex).mData[0];
								m_pInfluenceVertex[lChannelIndex][i].position.m128_f32[1] = (float)lEndShape->GetControlPointAt(vertices[i].vertIndex).mData[1];
								m_pInfluenceVertex[lChannelIndex][i].position.m128_f32[2] = (float)lEndShape->GetControlPointAt(vertices[i].vertIndex).mData[2];
								m_pInfluenceVertex[lChannelIndex][i].position.m128_f32[3] = 1.0f;
								m_pInfluenceVertex[lChannelIndex][i].normal.m128_f32[0] = (float)lEndShape->GetElementNormal(0)->GetDirectArray().GetAt(vertices[i].vertIndex).mData[0];
								m_pInfluenceVertex[lChannelIndex][i].normal.m128_f32[1] = (float)lEndShape->GetElementNormal(0)->GetDirectArray().GetAt(vertices[i].vertIndex).mData[1];
								m_pInfluenceVertex[lChannelIndex][i].normal.m128_f32[2] = (float)lEndShape->GetElementNormal(0)->GetDirectArray().GetAt(vertices[i].vertIndex).mData[2];
								m_pInfluenceVertex[lChannelIndex][i].normal.m128_f32[3] = 1.0f;
							}
						}
						//The weight percentage falls between two target shapes.
						else if (lStartShape && lEndShape)
						{
							isMorphMesh = true;
							double lStartWeight = lFullWeights[lStartIndex];
							double lEndWeight = lFullWeights[lEndIndex];
							// Calculate the real weight.
							lWeight = ((lWeight - lStartWeight) / (lEndWeight - lStartWeight)) * 100;
							// Initialize the lDstVertexArray with vertex of the previous target shape geometry.
							/*memcpy(lDstVertexArray, lStartShape->GetControlPoints(), lVertexCount * sizeof(FbxVector4));
							for (int j = 0; j < lVertexCount; j++)
							{
							// Add the influence of the shape vertex to the previous shape vertex.
							FbxVector4 lInfluence = (lEndShape->GetControlPoints()[j] - lStartShape->GetControlPoints()[j]) * lWeight * 0.01;
							lDstVertexArray[j] += lInfluence;
							}*/
						}
						HRESULT hr;
						ID3D11Buffer *pInfluenceVB;
						D3D11_SUBRESOURCE_DATA initData;

						initData.pSysMem = m_pInfluenceVertex[lChannelIndex].data();

						D3D11_BUFFER_DESC bufferDesc;
						bufferDesc.Usage = D3D11_USAGE_DEFAULT;
						bufferDesc.ByteWidth = nVertices * sizeof(Influence);
						bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
						bufferDesc.CPUAccessFlags = 0;
						bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
						bufferDesc.StructureByteStride = sizeof(Influence);
						hr = Engine::m_pRender->GetDevice()->CreateBuffer(&bufferDesc, &initData, &pInfluenceVB);
						if (FAILED(hr))
						{
							OutputDebugString(L"Failed to create vertex buffer.\n");
							return hr;
						}

						D3D11_SHADER_RESOURCE_VIEW_DESC SRVBufferDesc;
						SRVBufferDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
						SRVBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
						SRVBufferDesc.Buffer.ElementOffset = 0;
						SRVBufferDesc.Buffer.ElementWidth = nVertices;
						hr = Engine::m_pRender->GetDevice()->CreateShaderResourceView(pInfluenceVB, &SRVBufferDesc, &m_pInfluenceVBSRV[lChannelIndex]);
						if (FAILED(hr))
						{
							OutputDebugString(L"Failed to create vertex buffer shader resource view.\n");
							return hr;
						}
					}
				}
			}
		}
	}

	return true;
}

bool Model::m_LoadFbxCache()
{
#define TRIANGULATE FALSE
	if (TRIANGULATE)
	{
		FbxGeometryConverter GeometryConverter(m_pNode->GetFbxManager());
		if (!GeometryConverter.Triangulate(m_pNode->GetNodeAttribute(), true, false))
			return false;
	}

	m_pMesh = m_pNode->GetMesh();
	if (!m_pMesh)
		return false;

	const int lPolygonCount = m_pMesh->GetPolygonCount();

	// Count the polygon count of each material
	FbxLayerElementArrayTemplate<int> *lMaterialIndice = NULL;
	FbxGeometryElement::EMappingMode lMaterialMappingMode = FbxGeometryElement::eNone;
	if (m_pMesh->GetElementMaterial())
	{
		lMaterialIndice = &m_pMesh->GetElementMaterial()->GetIndexArray();
		lMaterialMappingMode = m_pMesh->GetElementMaterial()->GetMappingMode();
		if (lMaterialIndice && lMaterialMappingMode == FbxGeometryElement::eByPolygon)
		{
			FBX_ASSERT(lMaterialIndice->GetCount() == lPolygonCount);
			if (lMaterialIndice->GetCount() == lPolygonCount)
			{
				// Count the faces of each material
				for (int lPolygonIndex = 0; lPolygonIndex < lPolygonCount; ++lPolygonIndex)
				{
					const int lMaterialIndex = lMaterialIndice->GetAt(lPolygonIndex);
					if (m_subMeshes.GetCount() < lMaterialIndex + 1)
					{
						m_subMeshes.Resize(lMaterialIndex + 1);
					}
					if (m_subMeshes[lMaterialIndex] == NULL)
					{
						m_subMeshes[lMaterialIndex] = new SubMesh;
					}
					m_subMeshes[lMaterialIndex]->TriangleCount += 1;
				}

				// Make sure we have no "holes" (NULL) in the m_subMeshes table. This can happen
				// if, in the loop above, we resized the m_subMeshes by more than one slot.
				for (int i = 0; i < m_subMeshes.GetCount(); i++)
				{
					if (m_subMeshes[i] == NULL)
						m_subMeshes[i] = new SubMesh;
				}

				// Record the offset (how many vertex)
				const int lMaterialCount = m_subMeshes.GetCount();
				int lOffset = 0;
				for (int lIndex = 0; lIndex < lMaterialCount; ++lIndex)
				{
					m_subMeshes[lIndex]->IndexOffset = lOffset;
					lOffset += m_subMeshes[lIndex]->TriangleCount * 3;
					// This will be used as counter in the following procedures, reset to zero
					m_subMeshes[lIndex]->TriangleCount = 0;
				}
				FBX_ASSERT(lOffset == lPolygonCount * 3);
			}
		}
	}

	// All faces will use the same material.
	if (m_subMeshes.GetCount() == 0)
	{
		m_subMeshes.Resize(1);
		m_subMeshes[0] = new SubMesh();
	}

	FbxStringList uvSetsNames;
	m_pMesh->GetUVSetNames(uvSetsNames);
	const FbxVector4 * lControlPoints = m_pMesh->GetControlPoints();
	FbxVector4 lCurrentVertex;
	FbxVector4 lCurrentNormal;
	FbxVector4 lCurrentTangent;
	FbxVector4 lCurrentBinormal;
	FbxVector2 lCurrentUV;
	float * lUVs = NULL;
	FbxStringList lUVNames;
	m_pMesh->GetUVSetNames(lUVNames);
	const char * lUVName = NULL;
	if (lUVNames.GetCount())
	{
		lUVs = new float[lPolygonCount * 2];
		lUVName = lUVNames[0];
	}
	nVertices = lPolygonCount * 3;
	nIndices = lPolygonCount * 3;
	m_pPxVertices = new PxVec3[nVertices];

	vertices = new Vertex[nVertices];
	if (!vertices)
		return false;

	indices = new unsigned int[nIndices];
	if (!indices)
		return false;

	int lVertexCount = 0;
	for (int lPolygonIndex = 0; lPolygonIndex < lPolygonCount; lPolygonIndex++)
	{
		if (3 != m_pMesh->GetPolygonSize(lPolygonIndex))
		{
			ERROR(0);
		}

		// The material for current face.
		int lMaterialIndex = 0;
		if (lMaterialIndice && lMaterialMappingMode == FbxGeometryElement::eByPolygon)
		{
			lMaterialIndex = lMaterialIndice->GetAt(lPolygonIndex);
		}

		// Where should I save the vertex attribute index, according to the material
		const int lIndexOffset = m_subMeshes[lMaterialIndex]->IndexOffset +
			m_subMeshes[lMaterialIndex]->TriangleCount * 3;

		for (size_t lVerticeIndex = 0; lVerticeIndex < 3; lVerticeIndex++)
		{
			const int lControlPointIndex = m_pMesh->GetPolygonVertex(lPolygonIndex, lVerticeIndex);

			indices[lIndexOffset + lVerticeIndex] = static_cast<unsigned int>(lVertexCount);

			vertices[lVertexCount].vertIndex = lControlPointIndex;

			lCurrentVertex = lControlPoints[lControlPointIndex];
			vertices[lVertexCount].pos.x = m_pPxVertices[lVertexCount].x = static_cast<float>(lCurrentVertex[0]);
			vertices[lVertexCount].pos.y = m_pPxVertices[lVertexCount].y = static_cast<float>(lCurrentVertex[1]);
			vertices[lVertexCount].pos.z = m_pPxVertices[lVertexCount].z = static_cast<float>(lCurrentVertex[2]);

			if (m_pMesh->GetPolygonVertexNormal(lPolygonIndex, lVerticeIndex, lCurrentNormal))
			{
				vertices[lVertexCount].norm.x = static_cast<float>(lCurrentNormal[0]);
				vertices[lVertexCount].norm.y = static_cast<float>(lCurrentNormal[1]);
				vertices[lVertexCount].norm.z = static_cast<float>(lCurrentNormal[2]);
			}

			XMVECTOR normal = XMVectorSet(lCurrentNormal[0], lCurrentNormal[1], lCurrentNormal[2], lCurrentNormal[3]);
			XMVECTOR tangent;
			XMVECTOR binormal;
			XMVECTOR c1 = XMVector3Cross(normal, XMVectorSet(0.0, 0.0, 1.0, 1.0));
			XMVECTOR c2 = XMVector3Cross(normal, XMVectorSet(0.0, 1.0, 0.0, 1.0));
			if (XMVector3Length(c1).m128_f32[0] > XMVector3Length(c2).m128_f32[0])
				tangent = c1;
			else
				tangent = c2;
			tangent = XMVector3Normalize(tangent);
			binormal = XMVector3Normalize(XMVector3Cross(normal, tangent));

			vertices[lVertexCount].binorm.x = static_cast<float>(binormal.m128_f32[0]);
			vertices[lVertexCount].binorm.y = static_cast<float>(binormal.m128_f32[1]);
			vertices[lVertexCount].binorm.z = static_cast<float>(binormal.m128_f32[2]);

			vertices[lVertexCount].tan.x = static_cast<float>(tangent.m128_f32[0]);
			vertices[lVertexCount].tan.y = static_cast<float>(tangent.m128_f32[1]);
			vertices[lVertexCount].tan.z = static_cast<float>(tangent.m128_f32[2]);

			//if (m_pMesh->GetElementBinormalCount() > 0)
			//{
			//	lCurrentBinormal = m_pMesh->GetElementBinormal(0)->GetDirectArray().GetAt(lControlPointIndex);
			//	vertices[lVertexCount].binorm.x = static_cast<float>(lCurrentBinormal[0]);
			//	vertices[lVertexCount].binorm.y = static_cast<float>(lCurrentBinormal[1]);
			//	vertices[lVertexCount].binorm.z = static_cast<float>(lCurrentBinormal[2]);
			//}

			//if (m_pMesh->GetElementTangentCount() > 0)
			//{
			//	lCurrentTangent = m_pMesh->GetElementTangent(0)->GetDirectArray().GetAt(lControlPointIndex);
			//	vertices[lVertexCount].tan.x = static_cast<float>(lCurrentTangent[0]);
			//	vertices[lVertexCount].tan.y = static_cast<float>(lCurrentTangent[1]);
			//	vertices[lVertexCount].tan.z = static_cast<float>(lCurrentTangent[2]);
			//}

			bool lUnmappedUV;
			if (m_pMesh->GetPolygonVertexUV(lPolygonIndex, lVerticeIndex, lUVName, lCurrentUV, lUnmappedUV))
			{
				vertices[lVertexCount].tex.x = static_cast<float>(lCurrentUV[0]);
				vertices[lVertexCount].tex.y = static_cast<float>(1.0f - lCurrentUV[1]);
			}

			m_subMeshes[lMaterialIndex]->vertexListSimple.resize(m_subMeshes[lMaterialIndex]->vertexListSimple.size() + 1);
			m_subMeshes[lMaterialIndex]->vertexListSimple[m_subMeshes[lMaterialIndex]->vertexListSimple.size() - 1].pos = vertices[lVertexCount].pos;
			m_subMeshes[lMaterialIndex]->indexList.resize(m_subMeshes[lMaterialIndex]->indexList.size() + 1);
			m_subMeshes[lMaterialIndex]->indexList[m_subMeshes[lMaterialIndex]->indexList.size() - 1] = indices[lIndexOffset + lVerticeIndex];

			++lVertexCount;
		}
		m_subMeshes[lMaterialIndex]->TriangleCount += 1;
	}

	FbxAMatrix pParentGlobalPosition;
	DWORD dwDeformerCount = m_pMesh->GetDeformerCount(FbxDeformer::eSkin);
	FbxAMatrix lGlobalPosition = GetGlobalPosition(m_pNode, 0, m_pFbxScene->GetPose(1), &pParentGlobalPosition);
	if (dwDeformerCount != 0)
	{
		for (DWORD dwDeformerIndex = 0; dwDeformerIndex < dwDeformerCount; ++dwDeformerIndex)
		{
			FbxSkin* pSkin = (FbxSkin*)m_pMesh->GetDeformer(dwDeformerIndex, FbxDeformer::eSkin);
			DWORD dwClusterCount = pSkin->GetClusterCount();

			for (DWORD dwClusterIndex = 0; dwClusterIndex < dwClusterCount; ++dwClusterIndex)
			{
				FbxCluster* pCluster = pSkin->GetCluster(dwClusterIndex);

				DWORD dwClusterSize = pCluster->GetControlPointIndicesCount();
				if (dwClusterSize == 0)
					continue;

				FbxAMatrix lMatrix;

				// Geometry offset.
				// it is not inherited by the children.
				FbxAMatrix lGeometryOffset = GetGeometry(m_pNode);
				FbxAMatrix lGloballOffPosition = lGlobalPosition * lGeometryOffset;

				FbxTime time;
				time.SetTimeString("1");
				ComputeClusterDeformation(lGloballOffPosition, m_pMesh, pCluster, lMatrix, time, m_pFbxScene->GetPose(1));
				XMMATRIX matBind;
				matBind.r[0].m128_f32[0] = (FLOAT)lMatrix.mData[0][0];
				matBind.r[0].m128_f32[1] = (FLOAT)lMatrix.mData[0][1];
				matBind.r[0].m128_f32[2] = (FLOAT)lMatrix.mData[0][2];
				matBind.r[0].m128_f32[3] = (FLOAT)lMatrix.mData[0][3];
				matBind.r[1].m128_f32[0] = (FLOAT)lMatrix.mData[1][0];
				matBind.r[1].m128_f32[1] = (FLOAT)lMatrix.mData[1][1];
				matBind.r[1].m128_f32[2] = (FLOAT)lMatrix.mData[1][2];
				matBind.r[1].m128_f32[3] = (FLOAT)lMatrix.mData[1][3];
				matBind.r[2].m128_f32[0] = (FLOAT)lMatrix.mData[2][0];
				matBind.r[2].m128_f32[1] = (FLOAT)lMatrix.mData[2][1];
				matBind.r[2].m128_f32[2] = (FLOAT)lMatrix.mData[2][2];
				matBind.r[2].m128_f32[3] = (FLOAT)lMatrix.mData[2][3];
				matBind.r[3].m128_f32[0] = (FLOAT)lMatrix.mData[3][0];
				matBind.r[3].m128_f32[1] = (FLOAT)lMatrix.mData[3][1];
				matBind.r[3].m128_f32[2] = (FLOAT)lMatrix.mData[3][2];
				matBind.r[3].m128_f32[3] = (FLOAT)lMatrix.mData[3][3];

				m_boneMatrix[dwClusterIndex] = matBind;
				INT* pIndices = pCluster->GetControlPointIndices();
				DOUBLE* pWeights = pCluster->GetControlPointWeights();
				if (pWeights && pIndices)
					isSkinMesh = true;
				else
					isSkinMesh = false;

				for (int i = 0; i < nVertices; i++)
				{
					for (size_t j = 0; j < pCluster->GetControlPointIndicesCount(); j++)
					{
						if (vertices[i].vertIndex == pIndices[j])
						{
							if (vertices[i].boneWeight1.x == 0.0f)
							{
								vertices[i].boneIndex1.x = (FLOAT)dwClusterIndex;
								vertices[i].boneWeight1.x = (FLOAT)pWeights[j];
								break;
							}
							else if (vertices[i].boneWeight1.y == 0.0f)
							{
								vertices[i].boneIndex1.y = (FLOAT)dwClusterIndex;
								vertices[i].boneWeight1.y = (FLOAT)pWeights[j];
								break;
							}
							else if (vertices[i].boneWeight1.z == 0.0f)
							{
								vertices[i].boneIndex1.z = (FLOAT)dwClusterIndex;
								vertices[i].boneWeight1.z = (FLOAT)pWeights[j];
								break;
							}
							else if (vertices[i].boneWeight1.w == 0.0f)
							{
								vertices[i].boneIndex1.w = (FLOAT)dwClusterIndex;
								vertices[i].boneWeight1.w = (FLOAT)pWeights[j];
								break;
							}
							else if (vertices[i].boneWeight2.x == 0.0f)
							{
								vertices[i].boneIndex2.x = (FLOAT)dwClusterIndex;
								vertices[i].boneWeight2.x = (FLOAT)pWeights[j];
								break;
							}
							else if (vertices[i].boneWeight2.y == 0.0f)
							{
								vertices[i].boneIndex2.y = (FLOAT)dwClusterIndex;
								vertices[i].boneWeight2.y = (FLOAT)pWeights[j];
								break;
							}
							else if (vertices[i].boneWeight2.z == 0.0f)
							{
								vertices[i].boneIndex2.z = (FLOAT)dwClusterIndex;
								vertices[i].boneWeight2.z = (FLOAT)pWeights[j];
								break;
							}
							else if (vertices[i].boneWeight2.w == 0.0f)
							{
								vertices[i].boneIndex2.w = (FLOAT)dwClusterIndex;
								vertices[i].boneWeight2.w = (FLOAT)pWeights[j];
								break;
							}
						}
					}
				}
			}
		}
	}

	int lMaterialIndex;
	FbxProperty lProperty;
	FbxColor theColor;
	int lNbMat = m_pNode->GetSrcObjectCount<FbxSurfaceMaterial>() <= 0 ? 1 : m_pNode->GetSrcObjectCount<FbxSurfaceMaterial>();
	for (lMaterialIndex = 0; lMaterialIndex < lNbMat; lMaterialIndex++)
	{
		FbxSurfaceMaterial *lMaterial = m_pNode->GetSrcObject<FbxSurfaceMaterial>(lMaterialIndex);
		//go through all the possible textures
		if (lMaterial && m_subMeshes.Size() > lMaterialIndex)
		{
			const FbxDouble3 lDiffuse = GetMaterialProperty(lMaterial, FbxSurfaceMaterial::sDiffuse, FbxSurfaceMaterial::sDiffuseFactor, m_subMeshes[lMaterialIndex]->DiffuseTexture);

			theColor.Set(lDiffuse.mData[0], lDiffuse.mData[1], lDiffuse.mData[2]);
			m_subMeshes[lMaterialIndex]->diffuseColor = XMFLOAT4((FLOAT)lDiffuse.mData[0], (FLOAT)lDiffuse.mData[1], (FLOAT)lDiffuse.mData[2], 1.0f);

			const FbxDouble3 lAmbient = GetMaterialProperty(lMaterial, FbxSurfaceMaterial::sAmbient, FbxSurfaceMaterial::sAmbientFactor, m_subMeshes[lMaterialIndex]->ambientTexture);

			theColor.Set(lAmbient.mData[0], lAmbient.mData[1], lAmbient.mData[2]);
			m_subMeshes[lMaterialIndex]->ambientColor = XMFLOAT4((FLOAT)lAmbient.mData[0], (FLOAT)lAmbient.mData[1], (FLOAT)lAmbient.mData[2], 1.0f);

			const FbxDouble3 lSpecular = GetMaterialProperty(lMaterial, FbxSurfaceMaterial::sSpecular, FbxSurfaceMaterial::sSpecularFactor, m_subMeshes[lMaterialIndex]->SpecularTexture);

			theColor.Set(lSpecular.mData[0], lSpecular.mData[1], lSpecular.mData[2]);
			m_subMeshes[lMaterialIndex]->specularColor = XMFLOAT4((FLOAT)lSpecular.mData[0], (FLOAT)lSpecular.mData[1], (FLOAT)lSpecular.mData[2], 1.0f);

			const FbxDouble3 lNormal = GetMaterialProperty(lMaterial, FbxSurfaceMaterial::sNormalMap, FbxSurfaceMaterial::sBumpFactor, m_subMeshes[lMaterialIndex]->NormalTexture);
			const FbxDouble3 lTransperency = GetMaterialProperty(lMaterial, FbxSurfaceMaterial::sTransparentColor, FbxSurfaceMaterial::sTransparencyFactor, string());
		}
	}

	return true;
}

bool Model::m_LoadShaders()
{
	string blendShapeDeformerCount = to_string(static_cast<int>(m_pInfluenceVBSRV.size()));
	D3D_SHADER_MACRO    pDetailTessellationDefines[] =
	{ "MAX_BONE_MATRICES", "64",
	"VERTEX_COUNT", "1",
	"SKINNED_MESH", isSkinMesh ? "1" : "0",
	"DEPTH_ONLY", "0",
	"BLEND_SHAPE_DEFORMER_COUNT", m_pInfluenceVBSRV.size() > 0 ? blendShapeDeformerCount.c_str() : "1",
	NULL, NULL };

	std::wstring shaderName;

	m_pGBufferFillShader = NULL;
	m_pGBufferFillShader = Engine::resourceManager->LoadShader("base.sdr", pDetailTessellationDefines, NULL);

	m_pGBufferFillShaderWOTess = NULL;
	m_pGBufferFillShaderWOTess = Engine::resourceManager->LoadShader("baseWOTess.sdr", pDetailTessellationDefines, NULL);

	m_pCSMShader = NULL;
	m_pCSMShader = Engine::resourceManager->LoadShader("CascadedShadowMap.sdr", pDetailTessellationDefines, NULL);

	pDetailTessellationDefines[3] =
	{ "DEPTH_ONLY", "0" };

	m_pDX11ShaderCubeMap[0] = NULL;
	m_pDX11ShaderCubeMap[0] = Engine::resourceManager->LoadShader("baseCubeMap.sdr", pDetailTessellationDefines, NULL);

	pDetailTessellationDefines[3] =
	{ "DEPTH_ONLY", "1" };

	m_pDX11ShaderCubeMap[1] = NULL;
	m_pDX11ShaderCubeMap[1] = Engine::resourceManager->LoadShader("baseCubeMap.sdr", pDetailTessellationDefines, NULL);

	m_pDX11ShaderVoxelize = NULL;
	m_pDX11ShaderVoxelize = Engine::resourceManager->LoadShader("baseVoxelize.sdr", pDetailTessellationDefines, NULL);

	//VertexElementDesc vertexElementDescs[] = { POSITION_ELEMENT, 0, R32G32B32_FLOAT_EF, 0,
	//	NORMAL_ELEMENT, 0, R32G32B32_FLOAT_EF, D3D11_APPEND_ALIGNED_ELEMENT,
	//	BINORMAL_ELEMENT, 0, R32G32B32_FLOAT_EF, D3D11_APPEND_ALIGNED_ELEMENT,
	//	TANGENT_ELEMENT, 0, R32G32B32_FLOAT_EF, D3D11_APPEND_ALIGNED_ELEMENT,
	//	TEXCOORDS_ELEMENT, 0, R32G32_FLOAT_EF, D3D11_APPEND_ALIGNED_ELEMENT,
	//	BONE_INDICES_ELEMENT, 0, R32G32B32A32_FLOAT_EF, D3D11_APPEND_ALIGNED_ELEMENT,
	//	BONE_INDICES_ELEMENT, 1, R32G32B32A32_FLOAT_EF, D3D11_APPEND_ALIGNED_ELEMENT,
	//	BONE_WEIGHTS_ELEMENT, 0, R32G32B32A32_FLOAT_EF, D3D11_APPEND_ALIGNED_ELEMENT,
	//	BONE_WEIGHTS_ELEMENT, 1, R32G32B32A32_FLOAT_EF, D3D11_APPEND_ALIGNED_ELEMENT,
	//};

	//baseVertexLayout = Engine::m_pRender->CreateVertexLayout(vertexElementDescs, isSkinMesh ? 9 : 5);
	//if (!baseVertexLayout)
	//	return false;

	{
		DX12_VertexLayout *vertexLayout = new DX12_VertexLayout();
		VertexElementDesc vertexElementDescs[] = { POSITION_ELEMENT, 0, R32G32B32_FLOAT_EF, 0,
			NORMAL_ELEMENT, 0, R32G32B32_FLOAT_EF, D3D11_APPEND_ALIGNED_ELEMENT,
			BINORMAL_ELEMENT, 0, R32G32B32_FLOAT_EF, D3D11_APPEND_ALIGNED_ELEMENT,
			TANGENT_ELEMENT, 0, R32G32B32_FLOAT_EF, D3D11_APPEND_ALIGNED_ELEMENT,
			TEXCOORDS_ELEMENT, 0, R32G32_FLOAT_EF, D3D11_APPEND_ALIGNED_ELEMENT,
			BONE_INDICES_ELEMENT, 0, R32G32B32A32_FLOAT_EF, D3D11_APPEND_ALIGNED_ELEMENT,
			BONE_INDICES_ELEMENT, 1, R32G32B32A32_FLOAT_EF, D3D11_APPEND_ALIGNED_ELEMENT,
			BONE_WEIGHTS_ELEMENT, 0, R32G32B32A32_FLOAT_EF, D3D11_APPEND_ALIGNED_ELEMENT,
			BONE_WEIGHTS_ELEMENT, 1, R32G32B32A32_FLOAT_EF, D3D11_APPEND_ALIGNED_ELEMENT,
		};
		vertexLayout->Create(vertexElementDescs, isSkinMesh ? 9 : 5);

		PIPELINE_STATE_DESC psoDesc;
		psoDesc.shader = m_pGBufferFillShaderWOTess;
		psoDesc.vertexLayout = vertexLayout;
		pPso = Engine::GetRender()->CreatePipelineStateObject(psoDesc);

		psoDesc.shader = m_pDX11ShaderCubeMap[1];
		m_pCubeMapPso = Engine::GetRender()->CreatePipelineStateObject(psoDesc);

		psoDesc.shader = m_pCSMShader;
		psoDesc.rasterizerDesc.AntialiasedLineEnable = TRUE;
		psoDesc.rasterizerDesc.MultisampleEnable = TRUE;
		psoDesc.rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.rasterizerDesc.SlopeScaledDepthBias = 1.0;
		m_pCSMPso = Engine::GetRender()->CreatePipelineStateObject(psoDesc);
	}

	return true;
}

bool Model::m_LoadPhysics()
{
	if (m_pScene->m_pSceneInfo->modelsInfo[m_pNode->GetName()].physicalType == "rigidStatic")
		physicalType = 1;
	else if (m_pScene->m_pSceneInfo->modelsInfo[m_pNode->GetName()].physicalType == "rigidDynamic")
		physicalType = 2;
	else
		physicalType = 1;

	PxShape *convexShape;
	PxTransform transform;

	int mPoseIndex = -1;
	FbxPose * lPose = NULL;
	if (mPoseIndex != -1)
	{
		lPose = m_pFbxScene->GetPose(mPoseIndex);
	}
	FbxAMatrix lDummyGlobalPosition;
	FbxAMatrix lGlobalPosition = GetGlobalPosition(m_pNode, 0, lPose, &lDummyGlobalPosition);
	FbxAMatrix lGeometryOffset = GetGeometry(m_pNode);
	m_globallOffPosition = lGlobalPosition * lGeometryOffset;
	transform.p.x = m_globallOffPosition.GetT().mData[0] + m_pScene->GetPosition().x;
	transform.p.y = m_globallOffPosition.GetT().mData[1] + m_pScene->GetPosition().y;
	transform.p.z = m_globallOffPosition.GetT().mData[2] + m_pScene->GetPosition().z;
	transform.q.x = m_globallOffPosition.GetQ().mData[0];
	transform.q.y = m_globallOffPosition.GetQ().mData[1];
	transform.q.z = m_globallOffPosition.GetQ().mData[2];
	transform.q.w = m_globallOffPosition.GetQ().mData[3];

	switch (physicalType)
	{
	case 1:
	{
		rigidStaticActor = Engine::m_pPhysics->createRigidStatic(transform);
		if (rigidStaticActor)
		{
			PxTriangleMeshDesc meshDesc;
			meshDesc.points.count = nVertices;
			meshDesc.points.stride = sizeof(PxVec3);
			meshDesc.points.data = m_pPxVertices;
			meshDesc.triangles.count = nIndices / 3;
			meshDesc.triangles.stride = 3 * sizeof(unsigned long);
			meshDesc.triangles.data = indices;

			PxDefaultMemoryOutputStream writeBuffer;
			if (!Engine::m_pCooking->cookTriangleMesh(meshDesc, writeBuffer))
				return NULL;

			PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
			PxTriangleMesh *triangleMesh = Engine::m_pPhysics->createTriangleMesh(readBuffer);
			PxTriangleMeshGeometry triGeom;
			triGeom.triangleMesh = triangleMesh;
			PxMeshScale scale(PxVec3(m_globallOffPosition.GetS().mData[0] * m_pScene->m_pSceneInfo->scale.x
				, m_globallOffPosition.GetS().mData[1] * m_pScene->m_pSceneInfo->scale.y
				, m_globallOffPosition.GetS().mData[2] * m_pScene->m_pSceneInfo->scale.z), PxQuat(PxIdentity));
			triGeom.scale = scale;
			convexShape = rigidStaticActor->createShape(triGeom, *Engine::m_pPhysics->createMaterial(0.5, 0.5, 0.5));
			Engine::m_pPxScene->addActor(*rigidStaticActor);
		}
	}
	break;

	case 2:
	{
		rigidDynamicActor = Engine::m_pPhysics->createRigidDynamic(transform);
		if (rigidDynamicActor)
		{
			PxConvexMeshDesc convexDesc;
			convexDesc.points.count = nVertices;
			convexDesc.points.stride = sizeof(PxVec3);
			convexDesc.points.data = m_pPxVertices;
			convexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX | PxConvexFlag::eINFLATE_CONVEX;

			PxDefaultMemoryOutputStream writeBuffer;
			if (!Engine::m_pCooking->cookConvexMesh(convexDesc, writeBuffer))
				return NULL;

			PxDefaultMemoryInputData input(writeBuffer.getData(), writeBuffer.getSize());
			PxConvexMesh *convexBox = Engine::m_pPhysics->createConvexMesh(input);
			PxConvexMeshGeometry convexGeom = PxConvexMeshGeometry(convexBox);
			PxMeshScale scale(PxVec3(m_globallOffPosition.GetS().mData[0] * m_pScene->m_pSceneInfo->scale.x
				, m_globallOffPosition.GetS().mData[1] * m_pScene->m_pSceneInfo->scale.y
				, m_globallOffPosition.GetS().mData[2] * m_pScene->m_pSceneInfo->scale.z), PxQuat(PxIdentity));
			convexGeom.scale = scale;
			convexShape = rigidDynamicActor->createShape(convexGeom, *Engine::m_pPhysics->createMaterial(0.5, 0.5, 0.5));
			Engine::m_pPxScene->addActor(*rigidDynamicActor);
		}
	}
	break;

	default:
		break;
	}

	return true;
}

FbxDouble3 Model::GetMaterialProperty(const FbxSurfaceMaterial * pMaterial,
	const char * pPropertyName,
	const char * pFactorPropertyName,
	string &pTextureName)
{
	FbxDouble3 lResult(0, 0, 0);
	const FbxProperty lProperty = pMaterial->FindProperty(pPropertyName);
	const FbxProperty lFactorProperty = pMaterial->FindProperty(pFactorPropertyName);
	if (lProperty.IsValid() && lFactorProperty.IsValid())
	{
		lResult = lProperty.Get<FbxDouble3>();
		double lFactor = lFactorProperty.Get<FbxDouble>();
		if (lFactor != 1)
		{
			lResult[0] *= lFactor;
			lResult[1] *= lFactor;
			lResult[2] *= lFactor;
		}
	}

	if (lProperty.IsValid())
	{
		int lTextureCount = lProperty.GetSrcObjectCount<FbxTexture>();
		for (int j = 0; j < lTextureCount; ++j)
		{
			//Here we have to check if it's layeredtextures, or just textures:
			FbxLayeredTexture *lLayeredTexture = lProperty.GetSrcObject<FbxLayeredTexture>(j);
			if (lLayeredTexture)
			{
				FbxLayeredTexture *lLayeredTexture = lProperty.GetSrcObject<FbxLayeredTexture>(j);
				int lNbTextures = lLayeredTexture->GetSrcObjectCount<FbxTexture>();
				for (int k = 0; k < lNbTextures; ++k)
				{
					FbxTexture* lTexture = lLayeredTexture->GetSrcObject<FbxTexture>(k);
					if (lTexture)
					{
						//NOTE the blend mode is ALWAYS on the LayeredTexture and NOT the one on the texture.
						//Why is that?  because one texture can be shared on different layered textures and might
						//have different blend modes.

						FbxFileTexture *lFileTexture = FbxCast<FbxFileTexture>(lTexture);
						string lFileTextureName = (string)lFileTexture->GetFileName();
						string::size_type pos = lFileTextureName.find_last_of("\\");
						if (pos != -1) {
							lFileTextureName = lFileTextureName.substr(pos + 1);
						}

						pTextureName = lFileTextureName;

						FbxLayeredTexture::EBlendMode lBlendMode;
						lLayeredTexture->GetTextureBlendMode(k, lBlendMode);
					}

				}
			}
			else
			{
				//no layered texture simply get on the property
				FbxTexture* lTexture = lProperty.GetSrcObject<FbxTexture>(j);
				if (lTexture)
				{
					FbxFileTexture *lFileTexture = FbxCast<FbxFileTexture>(lTexture);
					string lFileTextureName = (string)lFileTexture->GetFileName();
					string::size_type pos = lFileTextureName.find_last_of("\\");
					if (pos != -1) {
						lFileTextureName = lFileTextureName.substr(pos + 1);
					}

					pTextureName = lFileTextureName;
				}
			}
		}
	}

	return lResult;
}

bool Model::Initialize(Scene *scene, FbxScene *pScene, FbxNode* pNode)
{
	HRESULT hr;
	m_pScene = scene;
	m_pFbxScene = pScene;
	m_pNode = pNode;
	lMaterialCount = pNode->GetMaterialCount() <= 0 ? 1 : pNode->GetMaterialCount();
	nVertices = 0;
	nIndices = 0;

	//Fbx Cache
	//Log::Get()->Print("Scene: %s; Model: %s; Загрузка данных о модели...", m_pScene->m_pSceneInfo->fbxName.c_str(), m_pNode->GetName());
	if (!m_LoadFbxCache())
	{
		Log::Get()->Err("Scene: %s; Model: %s; Не удалось загрузить данные о модели", m_pScene->m_pSceneInfo->fbxName.c_str(), m_pNode->GetName());
		return false;
	}
	//Log::Get()->Print("Scene: %s; Model: %s; Загрузка данных о модели завершена.", m_pScene->m_pSceneInfo->fbxName.c_str(), m_pNode->GetName());

	/*for (int i = 0; i < m_subMeshes.Size(); i++)
	{
		wchar_t pathToTexture[256];
		wchar_t densityTextureName[64];
		wchar_t gridDensityTextureName[64];
		wcscpy(densityTextureName, ToWChar((char*)m_subMeshes[i]->DiffuseTexture.c_str()));
		wcscpy(gridDensityTextureName, ToWChar((char*)m_subMeshes[i]->DiffuseTexture.c_str()));
		wcstok(densityTextureName, L".");
		wcstok(gridDensityTextureName, L".");

		if (m_subMeshes[i]->DiffuseTexture != "")
		{
			m_pDiffuseTexture = Engine::resourceManager->LoadTexture(m_subMeshes[i]->DiffuseTexture.c_str());
			if (!m_pDiffuseTexture)
			{
				Log::Get()->Err("Не удалось загрузить diffuse текстуру %s", m_subMeshes[i]->DiffuseTexture.c_str());
			}
		}

		if (m_subMeshes[i]->SpecularTexture != "")
		{
			m_pSpecularTexture = Engine::resourceManager->LoadTexture(m_subMeshes[i]->SpecularTexture.c_str());
			if (!m_pSpecularTexture)
			{
				Log::Get()->Err("Не удалось загрузить specular текстуру %s", m_subMeshes[i]->SpecularTexture.c_str());
			}
		}

		if (m_subMeshes[i]->NormalTexture != "")
		{
			m_pNormalTexture = Engine::resourceManager->LoadTexture(m_subMeshes[i]->NormalTexture.c_str());
			if (!m_pNormalTexture)
			{
				Log::Get()->Err("Не удалось загрузить текстуру нормалей %s", m_subMeshes[i]->NormalTexture.c_str());
			}
			else
			{
				//{
				//	swprintf_s(pathToTexture, sizeof(densityTextureName),
				//		L"%s%s", densityTextureName, L"_density.dds");
				//	wstring ws(pathToTexture);
				//	m_subMeshes[i]->DensityTexture = string(ws.begin(), ws.end());

				//	swprintf_s(pathToTexture, wcslen(Framework::m_pPaths[PATH_TO_TEXTURES].c_str()) * sizeof(wchar_t) + sizeof(densityTextureName),
				//		L"%s%s%s", Framework::m_pPaths[PATH_TO_TEXTURES].c_str(), densityTextureName, L"_density.dds");
				//}

				//m_pDensityTexture = Engine::resourceManager->LoadTexture(densityTextureName);
				//if (!m_pDensityTexture)
				//{
				//	hr = CreateDensityMapTexture(m_pNormalTexture->GetShaderResourceView(), pathToTexture, 30, 2.0f / 255.0f);
				//	if (FAILED(hr))
				//	{
				//		goto stop;
				//	}
				//	else
				//	{
				//		m_pDensityTexture = Framework::resourceManager->LoadTexture(_bstr_t(pathToTexture));
				//		if (!m_pDensityTexture)
				//			goto stop;
				//	}
				//}

				//{
				//	swprintf_s(pathToTexture, sizeof(ToWChar((char*)m_pScene->m_pSceneInfo->fbxName.c_str())) + sizeof(m_pFbxScene->GetName()) + sizeof(m_pNode->GetName()) + sizeof(gridDensityTextureName),
				//		L"%s%i%i%s%s", ToWChar((char*)m_pScene->m_pSceneInfo->fbxName.c_str()), m_pFbxScene->GetUniqueID(), m_pNode->GetUniqueID(), gridDensityTextureName, L"_density_grid.dds");
				//	wstring ws(pathToTexture);
				//	m_subMeshes[i]->GridDensityTexture = string(ws.begin(), ws.end());
				//}

				//m_pSimpleVertexBuffer = Buffer::CreateVertexBuffer(Framework::m_pRender->GetDevice(), sizeof(SimpleVertex)*m_subMeshes[i]->vertexListSimple.size(), false, m_subMeshes[i]->vertexListSimple.data());
				//if (!m_pSimpleVertexBuffer)
				//	break;

				//m_pSimpleIndexBuffer = Buffer::CreateIndexBuffer(Framework::m_pRender->GetDevice(), sizeof(int)*m_subMeshes[i]->indexList.size(), false, m_subMeshes[i]->indexList.data());
				//if (!m_pSimpleIndexBuffer)
				//	break;

				//m_pGridDensityTexture = new DX11_Texture();
				//ID3D11ShaderResourceView *tempGridDensitySRV = NULL;
				//hr = CreateDensity(&tempGridDensitySRV, m_pSimpleVertexBuffer, m_pSimpleIndexBuffer, i, sizeof(SimpleVertex), m_subMeshes[i]->indexList.size(), 4.0f, 50);
				//if (FAILED(hr))
				//{
				//	Log::Get()->Err("Не удалось создать карту плотности %ls", m_subMeshes[i]->GridDensityTexture);
				//	break;
				//}
				//else
				//{
				//	m_pGridDensityTexture->SetShaderResourceView(tempGridDensitySRV);
				//}
			}
		}
	stop:;
	}*/

	//Animation
	m_pAnimStack = pScene->GetSrcObject<FbxAnimStack>(0);
	if (m_pAnimStack)
	{
		m_pAnimLayer = m_pAnimStack->GetMember<FbxAnimLayer>(0);
		m_LoadFbxAnimation(m_pAnimLayer);
	}

	//Physics
	//Log::Get()->Print("Scene: %s; Model: %s; Загрузка физической модели...", m_pScene->m_pSceneInfo->fbxName.c_str(), m_pNode->GetName());
	if (!m_LoadPhysics())
	{
		Log::Get()->Err("Scene: %s; Model: %s; Не удалось создать физическую модель", m_pScene->m_pSceneInfo->fbxName.c_str(), m_pNode->GetName());
		return false;
	}
	//Log::Get()->Print("Scene: %s; Model: %s; Загрузка физической модели завершена.", m_pScene->m_pSceneInfo->fbxName.c_str(), m_pNode->GetName());

	//Shaders
	//Log::Get()->Print("Scene: %s; Model: %s; Загрузка шейдеров...", m_pScene->m_pSceneInfo->fbxName.c_str(), m_pNode->GetName());
	if (!m_LoadShaders())
	{
		Log::Get()->Err("Scene: %s; Model: %s; Не удалось создать шейдеры", m_pScene->m_pSceneInfo->fbxName.c_str(), m_pNode->GetName());
		return false;
	}
	//Log::Get()->Print("Scene: %s; Model: %s; Загрузка шейдеров завершена.", m_pScene->m_pSceneInfo->fbxName.c_str(), m_pNode->GetName());

	//m_pUniformBuffer = Engine::m_pRender->CreateUniformBuffer(sizeof(ModelBufferData));
	//if (!m_pUniformBuffer)
	//	return false;

	m_pUniformBuffer = new DX12_UniformBuffer();
	m_pUniformBuffer->Create(sizeof(ModelBufferData));

	// create vertex buffer
	//m_pVertexBuffer = Framework::m_pRender->CreateVertexBuffer(sizeof(Vertex), nVertices, false);
	//if (!m_pVertexBuffer)
	//{
	//	SAFE_DELETE_ARRAY(vertices);
	//	return false;
	//}
	//m_pVertexBuffer->AddVertices(nVertices, (float*)vertices);
	//m_pVertexBuffer->Update();
	//SAFE_DELETE_ARRAY(vertices);

	//// create index-buffer
	//m_pIndexBuffer = Framework::m_pRender->CreateIndexBuffer(nIndices, false);
	//if (!m_pIndexBuffer)
	//{
	//	SAFE_DELETE_ARRAY(indices);
	//	return false;
	//}
	//m_pIndexBuffer->AddIndices(nIndices, indices);
	//m_pIndexBuffer->Update();
	//SAFE_DELETE_ARRAY(indices);

	// DX 12
	m_pDX12VertexBuffer = new DX12_VertexBuffer();
	if (!m_pDX12VertexBuffer)
		return NULL;

	if (!m_pDX12VertexBuffer->Create(sizeof(Vertex), nVertices, false))
	{
		SAFE_DELETE(m_pDX12VertexBuffer);
		return NULL;
	}

	m_pDX12VertexBuffer->AddVertices(nVertices, (float*)vertices);
	m_pDX12VertexBuffer->Update();

	m_pDX12IndexBuffer = new DX12_IndexBuffer();
	if (!m_pDX12IndexBuffer)
		return NULL;

	if (!m_pDX12IndexBuffer->Create(nIndices, false))
	{
		SAFE_DELETE(m_pDX12IndexBuffer);
		return NULL;
	}

	m_pDX12IndexBuffer->AddIndices(nIndices, indices);
	m_pDX12IndexBuffer->Update();

	//rasterizerState = Framework::m_pRender->CreateRasterizerState(rasterDesc);
	//if (!rasterizerState)
	//	return false;

	//depthStencilDesc.stencilTest = true;
	//depthStencilState = Framework::m_pRender->CreateDepthStencilState(depthStencilDesc);
	//if (!depthStencilState)
	//	return false;

	//blendState = Framework::m_pRender->CreateBlendState(blendDesc);
	//if (!blendState)
	//	return false;

	RtConfigDesc rtcDesc;
	rtcDesc.numColorBuffers = 3;
	rtcDesc.firstColorBufferIndex = 1;
	multiRTC = Engine::GetRender()->CreateRenderTargetConfig(rtcDesc);
	if (!multiRTC)
		return false;

	//_DELETE_ARRAY(materialFaces);
	//_DELETE_ARRAY(materialVertexList);
	//_DELETE_ARRAY(materialVertexCache);
	//_DELETE_ARRAY(vertices);
	//_DELETE_ARRAY(indices);

	return true;
}

void Model::StepPhysX()
{
	switch (physicalType)
	{
	case 1:
	{
		FXMVECTOR quat = XMVectorSet(rigidStaticActor->getGlobalPose().q.x, rigidStaticActor->getGlobalPose().q.y, rigidStaticActor->getGlobalPose().q.z, rigidStaticActor->getGlobalPose().q.w);
		m_objectMatrix = XMMatrixScaling(m_globallOffPosition.GetS().mData[0] * m_pScene->m_pSceneInfo->scale.x,
			m_globallOffPosition.GetS().mData[1] * m_pScene->m_pSceneInfo->scale.y,
			m_globallOffPosition.GetS().mData[2] * m_pScene->m_pSceneInfo->scale.z);
		m_objectMatrix *= XMMatrixRotationQuaternion(quat);
		m_objectMatrix *= XMMatrixTranslation(rigidStaticActor->getGlobalPose().p.x, rigidStaticActor->getGlobalPose().p.y, rigidStaticActor->getGlobalPose().p.z);
	}
	break;

	case 2:
	{
		FXMVECTOR quat = XMVectorSet(rigidDynamicActor->getGlobalPose().q.x, rigidDynamicActor->getGlobalPose().q.y, rigidDynamicActor->getGlobalPose().q.z, rigidDynamicActor->getGlobalPose().q.w);
		m_objectMatrix = XMMatrixScaling(m_globallOffPosition.GetS().mData[0] * m_pScene->m_pSceneInfo->scale.x,
			m_globallOffPosition.GetS().mData[1] * m_pScene->m_pSceneInfo->scale.y,
			m_globallOffPosition.GetS().mData[2] * m_pScene->m_pSceneInfo->scale.z);
		m_objectMatrix *= XMMatrixRotationQuaternion(quat);
		m_objectMatrix *= XMMatrixTranslation(rigidDynamicActor->getGlobalPose().p.x, rigidDynamicActor->getGlobalPose().p.y, rigidDynamicActor->getGlobalPose().p.z);
	}
	break;

	default:
		break;
	}
}

bool Model::animateModel(float keyFrame)
{
	FbxTime time;
	time.SetTimeString(to_string(keyFrame).c_str());
	FbxAnimCurve *lAnimCurve = NULL;
	DWORD dwDeformerCount = m_pMesh->GetDeformerCount(FbxDeformer::eSkin);
	FbxAMatrix pParentGlobalPosition;
	FbxAMatrix lGlobalPosition = GetGlobalPosition(m_pNode, 0, m_pFbxScene->GetPose(1), &pParentGlobalPosition);
	if (dwDeformerCount != 0)
	{
		for (DWORD dwDeformerIndex = 0; dwDeformerIndex < dwDeformerCount; ++dwDeformerIndex)
		{
			FbxSkin* pSkin = (FbxSkin*)m_pMesh->GetDeformer(dwDeformerIndex, FbxDeformer::eSkin);
			DWORD dwClusterCount = pSkin->GetClusterCount();

			for (DWORD dwClusterIndex = 0; dwClusterIndex < dwClusterCount; ++dwClusterIndex)
			{
				FbxCluster* pCluster = pSkin->GetCluster(dwClusterIndex);

				DWORD dwClusterSize = pCluster->GetControlPointIndicesCount();
				if (dwClusterSize == 0)
					continue;

				FbxAMatrix lMatrix;

				// Geometry offset.
				// it is not inherited by the children.
				FbxAMatrix lGeometryOffset = GetGeometry(m_pNode);
				FbxAMatrix m_globallOffPosition = lGlobalPosition * lGeometryOffset;
				ComputeClusterDeformation(m_globallOffPosition, m_pMesh, pCluster, lMatrix, time, m_pFbxScene->GetPose(1));

				XMMATRIX matBind;
				matBind.r[0].m128_f32[0] = lMatrix.mData[0][0];
				matBind.r[0].m128_f32[1] = lMatrix.mData[0][1];
				matBind.r[0].m128_f32[2] = lMatrix.mData[0][2];
				matBind.r[0].m128_f32[3] = lMatrix.mData[0][3];
				matBind.r[1].m128_f32[0] = lMatrix.mData[1][0];
				matBind.r[1].m128_f32[1] = lMatrix.mData[1][1];
				matBind.r[1].m128_f32[2] = lMatrix.mData[1][2];
				matBind.r[1].m128_f32[3] = lMatrix.mData[1][3];
				matBind.r[2].m128_f32[0] = lMatrix.mData[2][0];
				matBind.r[2].m128_f32[1] = lMatrix.mData[2][1];
				matBind.r[2].m128_f32[2] = lMatrix.mData[2][2];
				matBind.r[2].m128_f32[3] = lMatrix.mData[2][3];
				matBind.r[3].m128_f32[0] = lMatrix.mData[3][0];
				matBind.r[3].m128_f32[1] = lMatrix.mData[3][1];
				matBind.r[3].m128_f32[2] = lMatrix.mData[3][2];
				matBind.r[3].m128_f32[3] = lMatrix.mData[3][3];

				m_boneMatrix[dwClusterIndex] = matBind;
			}
		}
	}
	/*
	FbxNodeAttribute* lNodeAttribute = m_pNode->GetNodeAttribute();

	if (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh ||
		lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eNurbs ||
		lNodeAttribute->GetAttributeType() == FbxNodeAttribute::ePatch)
	{
		FbxGeometry* lGeometry = (FbxGeometry*)lNodeAttribute;

		int lBlendShapeDeformerCount = lGeometry->GetDeformerCount(FbxDeformer::eBlendShape);
		for (int lBlendShapeIndex = 0; lBlendShapeIndex < lBlendShapeDeformerCount; ++lBlendShapeIndex)
		{
			FbxBlendShape* lBlendShape = (FbxBlendShape*)lGeometry->GetDeformer(lBlendShapeIndex, FbxDeformer::eBlendShape);

			lBlendShapeChannelCount = lBlendShape->GetBlendShapeChannelCount();

			m_pInfluenceVertex = new std::vector<Influence>[lBlendShapeChannelCount];
			if (!m_pInfluenceVertex)
				return false;

			for (int lChannelIndex = 0; lChannelIndex < lBlendShapeChannelCount; ++lChannelIndex)
			{
				FbxBlendShapeChannel* lChannel = lBlendShape->GetBlendShapeChannel(lChannelIndex);
				const char* lChannelName = lChannel->GetName();

				lAnimCurve = lGeometry->GetShapeChannel(lBlendShapeIndex, lChannelIndex, m_pAnimLayer, true);
				if (lChannel)
				{
					if (!lAnimCurve) continue;

					isMorphMesh = true;
					double lWeight = lAnimCurve->Evaluate(time);

					int lShapeCount = lChannel->GetTargetShapeCount();
					double* lFullWeights = lChannel->GetTargetShapeFullWeights();

					// Find out which scope the lWeight falls in.
					int lStartIndex = -1;
					int lEndIndex = -1;
					for (int lShapeIndex = 0; lShapeIndex < lShapeCount; ++lShapeIndex)
					{
						if (lWeight > 0 && lWeight <= lFullWeights[0])
						{
							lEndIndex = 0;
							break;
						}
						if (lWeight > lFullWeights[lShapeIndex] && lWeight < lFullWeights[lShapeIndex + 1])
						{
							lStartIndex = lShapeIndex;
							lEndIndex = lShapeIndex + 1;
							break;
						}
					}

					FbxShape* lStartShape = NULL;
					FbxShape* lEndShape = NULL;
					if (lStartIndex > -1)
					{
						lStartShape = lChannel->GetTargetShape(lStartIndex);
					}
					if (lEndIndex > -1)
					{
						lEndShape = lChannel->GetTargetShape(lEndIndex);
					}

					//The weight percentage falls between base geometry and the first target shape.
					if (lStartIndex == -1 && lEndShape)
					{
						double lEndWeight = lFullWeights[0];
						// Calculate the real weight.
						lWeight = (lWeight / lEndWeight) * 100;
						// Initialize the lDstVertexArray with vertex of base geometry.
						//memcpy(lDstVertexArray, lSrcVertexArray, lVertexCount * sizeof(FbxVector4));
						for (size_t i = 0; i < nVertices; i++)
						{
							m_pInfluenceVertex[lChannelIndex].resize(m_pInfluenceVertex[lChannelIndex].size() + 1);
							m_pInfluenceVertex[lChannelIndex][i].position.m128_f32[0] = lEndShape->GetControlPointAt(vertices[i].vertIndex).mData[0];
							m_pInfluenceVertex[lChannelIndex][i].position.m128_f32[1] = lEndShape->GetControlPointAt(vertices[i].vertIndex).mData[2];
							m_pInfluenceVertex[lChannelIndex][i].position.m128_f32[2] = -lEndShape->GetControlPointAt(vertices[i].vertIndex).mData[1];
							m_pInfluenceVertex[lChannelIndex][i].position.m128_f32[3] = 1.0f;
							m_pInfluenceVertex[lChannelIndex][i].normal.m128_f32[0] = lEndShape->GetElementNormal(0)->GetDirectArray().GetAt(vertices[i].vertIndex).mData[0];
							m_pInfluenceVertex[lChannelIndex][i].normal.m128_f32[1] = lEndShape->GetElementNormal(0)->GetDirectArray().GetAt(vertices[i].vertIndex).mData[2];
							m_pInfluenceVertex[lChannelIndex][i].normal.m128_f32[2] = -lEndShape->GetElementNormal(0)->GetDirectArray().GetAt(vertices[i].vertIndex).mData[1];
							m_pInfluenceVertex[lChannelIndex][i].normal.m128_f32[3] = 1.0f;
						}
						//for (int j = 0; j < lVertexCount; j++)
						//{
						//// Add the influence of the shape vertex to the mesh vertex.
						//FbxVector4 lInfluence = (lEndShape->GetControlPoints()[j] - lSrcVertexArray[j]) * lWeight * 0.01;
						//lDstVertexArray[j] += lInfluence;
						//}
					}
					//The weight percentage falls between two target shapes.
					else if (lStartShape && lEndShape)
					{
						double lStartWeight = lFullWeights[lStartIndex];
						double lEndWeight = lFullWeights[lEndIndex];
						// Calculate the real weight.
						lWeight = ((lWeight - lStartWeight) / (lEndWeight - lStartWeight)) * 100;
						// Initialize the lDstVertexArray with vertex of the previous target shape geometry.
						//memcpy(lDstVertexArray, lStartShape->GetControlPoints(), lVertexCount * sizeof(FbxVector4));
						//for (int j = 0; j < lVertexCount; j++)
						//{
						//// Add the influence of the shape vertex to the previous shape vertex.
						//FbxVector4 lInfluence = (lEndShape->GetControlPoints()[j] - lStartShape->GetControlPoints()[j]) * lWeight * 0.01;
						//lDstVertexArray[j] += lInfluence;
						//}
					}
				}
			}
		}
	}

	HRESULT hr;
	ID3D11Buffer *pInfluenceVB;
	D3D11_SUBRESOURCE_DATA initData;

	Influence *influenceVertex = new Influence[nVertices];
	for (size_t i = 0; i < nVertices; i++)
	{
		influenceVertex[i].position = m_pInfluenceVertex[0][i].position;
		influenceVertex[i].normal = m_pInfluenceVertex[0][i].normal;
	}
	for (size_t lChannelIndex = 1; lChannelIndex < lBlendShapeChannelCount; lChannelIndex++)
	{
		for (size_t i = 0; i < nVertices; i++)
		{
			influenceVertex[i].position += m_pInfluenceVertex[lChannelIndex][i].position - influenceVertex[i].position;
			influenceVertex[i].normal += m_pInfluenceVertex[lChannelIndex][i].normal - influenceVertex[i].normal;
		}
	}
	initData.pSysMem = influenceVertex;

	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = nVertices * sizeof(Influence);
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bufferDesc.StructureByteStride = sizeof(Influence);
	hr = Framework::m_pRender->GetDevice()->CreateBuffer(&bufferDesc, &initData, &pInfluenceVB);
	if (FAILED(hr))
		OutputDebugString(L"Failed to create vertex buffer.\n");

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVBufferDesc;
	SRVBufferDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SRVBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	SRVBufferDesc.Buffer.ElementOffset = 0;
	SRVBufferDesc.Buffer.ElementWidth = nVertices;
	hr = Framework::m_pRender->GetDevice()->CreateShaderResourceView(pInfluenceVB, &SRVBufferDesc, &m_pInfluenceVBSRV);
	if (FAILED(hr))
		OutputDebugString(L"Failed to create vertex buffer.\n");
	*/
	return true;
}

void Model::Draw(Camera *camera, RENDER_TYPE rendertype)
{
	for (auto submeshNo = 0; submeshNo < m_subMeshes.GetCount(); submeshNo++)
	{
		GpuCmd gpuCmd(DRAW_CM);
		gpuCmd.draw.pipelineStateObject = pPso;
		gpuCmd.draw.renderTargets[0] = Engine::GetRender()->GetRenderTarget(GBUFFERS_RT_ID);
		gpuCmd.draw.renderTargetConfigs[0] = multiRTC;
		gpuCmd.draw.vertexBuffer = m_pDX12VertexBuffer;
		gpuCmd.draw.indexBuffer = m_pDX12IndexBuffer;
		if (TESSELLATION_ENABLED && m_pGridDensityTexture)
			gpuCmd.draw.primitiveType = D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
		else
			gpuCmd.draw.primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		bufferData.mWorld = XMMatrixTranspose(m_objectMatrix);
		bufferData.material = static_cast<float>(m_pNode->GetUniqueID());
		// Calculate plane equations of frustum in world space
		ExtractPlanesFromFrustum(g_pWorldSpaceFrustumPlaneEquation, &(camera->GetViewMatrix() * camera->GetProjection()));
		bufferData.vFrustumPlaneEquation[0] = g_pWorldSpaceFrustumPlaneEquation[0];
		bufferData.vFrustumPlaneEquation[1] = g_pWorldSpaceFrustumPlaneEquation[1];
		bufferData.vFrustumPlaneEquation[2] = g_pWorldSpaceFrustumPlaneEquation[2];
		bufferData.vFrustumPlaneEquation[3] = g_pWorldSpaceFrustumPlaneEquation[3];
		// Not using front clip plane for culling
		//bufferData.vFrustumPlaneEquation[4] = g_pWorldSpaceFrustumPlaneEquation[4]; 
		// Not using far clip plane for culling
		//bufferData.vFrustumPlaneEquation[5] = g_pWorldSpaceFrustumPlaneEquation[5]; 
		bufferData.vTessellationFactor = XMVectorSet(7.0f, 7.0f, 7.0f / 2.0f, 1.0f / 10.0f);
		bufferData.detailTessellationHeightScale = atof(m_pScene->m_pSceneInfo->modelsInfo[m_pNode->GetName()].detailTessellationHeightScale.c_str());
		bufferData.vDiffuseColor = XMVectorSet(m_subMeshes[submeshNo]->diffuseColor.x, m_subMeshes[submeshNo]->diffuseColor.y,
			m_subMeshes[submeshNo]->diffuseColor.z, m_subMeshes[submeshNo]->diffuseColor.w);
		bufferData.specularLevel = 0.3f;
		for (size_t i = 0; i < 64; i++)
		{
			bufferData.mBone[i] = m_boneMatrix[i];
		}

		if (g_time > 1)
		{
			g_time = 0;
		}
		else
		{
			g_time += Engine::GetTimer()->getTimeInterval() / 30;
		}
		if (isMorphMesh)
		{
			bufferData.timeElapsed = g_time;
			//bufferData.timeElapsed = 0.0f;
		}
		else
		{
			bufferData.timeElapsed = 0.0f;
		}
		bufferData.hasDiffuseTexture = m_pDiffuseTexture ? true : false;
		bufferData.hasSpecularTexture = m_pSpecularTexture ? true : false;
		bufferData.hasNormalTexture = m_pNormalTexture ? true : false;

		m_pUniformBuffer->Update(&bufferData);

		gpuCmd.draw.camera = camera;
		gpuCmd.draw.customUBs[0] = m_pUniformBuffer;
		gpuCmd.draw.firstIndex = m_subMeshes[submeshNo]->IndexOffset;
		gpuCmd.draw.numElements = m_subMeshes[submeshNo]->TriangleCount * 3;
		gpuCmd.draw.numInstances = 1;
		gpuCmd.draw.vertexOffset = m_subMeshes[submeshNo]->VertexOffeset;
		if (m_pDiffuseTexture)
			gpuCmd.draw.textures[COLOR_TEX_BP] = m_pDiffuseTexture;
		gpuCmd.draw.samplers[COLOR_TEX_ID] = Engine::GetRender()->GetSampler(TRILINEAR_SAMPLER_ID);

		Engine::m_pRender->AddGpuCmd(gpuCmd);
	}
}

void Model::AddShadowMapSurfaces(unsigned int lightIndex)
{
	for (auto submeshNo = 0; submeshNo < m_subMeshes.GetCount(); submeshNo++)
	{
		GpuCmd gpuCmd(DRAW_CM);
		//gpuCmd.draw.pipelineStateObject = m_pCubeMapPso;
		gpuCmd.draw.pipelineStateObject = m_pCSMPso;
		gpuCmd.draw.vertexBuffer = m_pDX12VertexBuffer;
		gpuCmd.draw.indexBuffer = m_pDX12IndexBuffer;
		gpuCmd.draw.primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		bufferData.mWorld = XMMatrixTranspose(m_objectMatrix);
		ExtractPlanesFromFrustum(g_pWorldSpaceFrustumPlaneEquation, &(Engine::m_pGameWorld->GetLight(lightIndex)->GetLightCamera()->GetViewMatrix() *
			Engine::m_pGameWorld->GetLight(lightIndex)->GetLightCamera()->GetProjection()));
		bufferData.vFrustumPlaneEquation[0] = g_pWorldSpaceFrustumPlaneEquation[0];
		bufferData.vFrustumPlaneEquation[1] = g_pWorldSpaceFrustumPlaneEquation[1];
		bufferData.vFrustumPlaneEquation[2] = g_pWorldSpaceFrustumPlaneEquation[2];
		bufferData.vFrustumPlaneEquation[3] = g_pWorldSpaceFrustumPlaneEquation[3];
		bufferData.vTessellationFactor = XMVectorSet(7.0f, 7.0f, 7.0f / 2.0f, 1.0f / 10.0f);
		bufferData.detailTessellationHeightScale = atof(m_pScene->m_pSceneInfo->modelsInfo[m_pNode->GetName()].detailTessellationHeightScale.c_str());
		bufferData.vDiffuseColor = XMVectorSet(m_subMeshes[submeshNo]->diffuseColor.x, m_subMeshes[submeshNo]->diffuseColor.y,
			m_subMeshes[submeshNo]->diffuseColor.z, m_subMeshes[submeshNo]->diffuseColor.w);
		bufferData.specularLevel = 0.3f;

		for (auto i = 0; i < 64; i++)
			bufferData.mBone[i] = m_boneMatrix[i];

		if (g_time > 1)
			g_time = 0;
		else
			g_time += Engine::GetTimer()->getTimeInterval() / 30;

		if (isMorphMesh)
			bufferData.timeElapsed = g_time;
		else
			bufferData.timeElapsed = 0.0f;

		bufferData.hasDiffuseTexture = m_pDiffuseTexture ? true : false;
		bufferData.hasSpecularTexture = m_pSpecularTexture ? true : false;
		bufferData.hasNormalTexture = m_pNormalTexture ? true : false;

		m_pUniformBuffer->Update(&bufferData);

		gpuCmd.draw.customUBs[0] = m_pUniformBuffer;
		gpuCmd.draw.firstIndex = m_subMeshes[submeshNo]->IndexOffset;
		gpuCmd.draw.numElements = m_subMeshes[submeshNo]->TriangleCount * 3;
		gpuCmd.draw.vertexOffset = m_subMeshes[submeshNo]->VertexOffeset;
		//if (m_pDiffuseTexture)
		//	gpuCmd.draw.textures[COLOR_TEX_BP] = m_pDiffuseTexture;
		//gpuCmd.draw.samplers[COLOR_TEX_ID] = Engine::GetRender()->GetSampler(TRILINEAR_SAMPLER_ID);
		//gpuCmd.draw.samplers[CUSTOM0_TEX_ID] = Engine::GetRender()->GetSampler(SHADOW_MAP_SAMPLER_ID);

		Engine::m_pGameWorld->GetLight(lightIndex)->DrawShadowSurface(gpuCmd.draw);
		Engine::m_pRender->AddGpuCmd(gpuCmd);
	}
}

bool Model::m_UpdateInfluenceVB()
{
	//HRESULT hr;
	//ID3D11Buffer *pInfluenceVB;
	//D3D11_SUBRESOURCE_DATA initData;

	//Influence *influenceVertex = new Influence[nVertices];
	//for (size_t i = 0; i < nVertices; i++)
	//{
	//	influenceVertex[i].position = m_pInfluenceVertex[0][i].position;
	//	influenceVertex[i].normal = m_pInfluenceVertex[0][i].normal;
	//}
	//for (size_t lChannelIndex = 1; lChannelIndex < lBlendShapeChannelCount; lChannelIndex++)
	//{
	//	for (size_t i = 0; i < nVertices; i++)
	//	{
	//		influenceVertex[i].position += m_pInfluenceVertex[lChannelIndex][i].position - influenceVertex[i].position;
	//		influenceVertex[i].normal += m_pInfluenceVertex[lChannelIndex][i].normal - influenceVertex[i].normal;
	//	}
	//}
	//initData.pSysMem = influenceVertex;

	//D3D11_BUFFER_DESC bufferDesc;
	//bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	//bufferDesc.ByteWidth = nVertices * sizeof(Influence);
	//bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	//bufferDesc.CPUAccessFlags = 0;
	//bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	//bufferDesc.StructureByteStride = sizeof(Influence);
	//hr = Framework::m_pRender->GetDevice()->CreateBuffer(&bufferDesc, &initData, &pInfluenceVB);
	//if (FAILED(hr))
	//{
	//	OutputDebugString(L"Failed to create vertex buffer.\n");
	//	return hr;
	//}

	//D3D11_SHADER_RESOURCE_VIEW_DESC SRVBufferDesc;
	//SRVBufferDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	//SRVBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	//SRVBufferDesc.Buffer.ElementOffset = 0;
	//SRVBufferDesc.Buffer.ElementWidth = nVertices;
	//hr = Framework::m_pRender->GetDevice()->CreateShaderResourceView(pInfluenceVB, &SRVBufferDesc, &m_pInfluenceVBSRV);
	//if (FAILED(hr))
	//{
	//	OutputDebugString(L"Failed to create vertex buffer shader resource view.\n");
	//	return hr;
	//}

	return true;
}

void Model::m_RenderBuffers(unsigned int submeshNo, RENDER_TYPE rendertype, GpuCmd &gpuCmd)
{
	//Engine::m_pRender->m_commandList->IASetVertexBuffers(0, 1, &m_pDX12VertexBuffer->m_vertexBufferView);
	//Engine::m_pRender->m_commandList->IASetIndexBuffer(&m_pDX12IndexBuffer->m_indexBufferView);
	gpuCmd.draw.vertexBuffer = m_pDX12VertexBuffer;
	gpuCmd.draw.indexBuffer = m_pDX12IndexBuffer;
	switch (rendertype)
	{
	case D3D11Framework::GBUFFER_FILL_RENDER:
		_MUTEXLOCK(Engine::m_pMutex);
		if (TESSELLATION_ENABLED && m_pGridDensityTexture)
		{
			//Engine::m_pRender->m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
			gpuCmd.draw.primitiveType = D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
		}
		else
		{
			//Engine::m_pRender->m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			gpuCmd.draw.primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;
	case D3D11Framework::CASCADED_SHADOW_RENDER:
	case D3D11Framework::CUBEMAP_DEPTH_RENDER:
	case D3D11Framework::CUBEMAP_COLOR_RENDER:
	case D3D11Framework::VOXELIZE_RENDER:
	default:
		_MUTEXLOCK(Engine::m_pMutex);
		//Engine::m_pRender->m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		gpuCmd.draw.primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;
	}

	return;
	//m_pVertexBuffer->Bind();
	//m_pIndexBuffer->Bind();
	switch (rendertype)
	{
	case D3D11Framework::GBUFFER_FILL_RENDER:
		_MUTEXLOCK(Engine::m_pMutex);
		if (TESSELLATION_ENABLED && m_pGridDensityTexture)
		{
			Engine::m_pRender->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
		}
		else
		{
			Engine::m_pRender->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		}
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;
	case D3D11Framework::CASCADED_SHADOW_RENDER:
	case D3D11Framework::CUBEMAP_DEPTH_RENDER:
	case D3D11Framework::CUBEMAP_COLOR_RENDER:
	case D3D11Framework::VOXELIZE_RENDER:
	default:
		_MUTEXLOCK(Engine::m_pMutex);
		Engine::m_pRender->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;
	}
}

void Model::m_SetShaderParameters(unsigned int submeshNo, Camera *camera, RENDER_TYPE rendertype, GpuCmd &gpuCmd)
{
	bufferData.mWorld = XMMatrixTranspose(m_objectMatrix);
	// Calculate plane equations of frustum in world space
	ExtractPlanesFromFrustum(g_pWorldSpaceFrustumPlaneEquation, &(camera->GetViewMatrix() * camera->GetProjection()));
	bufferData.vFrustumPlaneEquation[0] = g_pWorldSpaceFrustumPlaneEquation[0];
	bufferData.vFrustumPlaneEquation[1] = g_pWorldSpaceFrustumPlaneEquation[1];
	bufferData.vFrustumPlaneEquation[2] = g_pWorldSpaceFrustumPlaneEquation[2];
	bufferData.vFrustumPlaneEquation[3] = g_pWorldSpaceFrustumPlaneEquation[3];
	// Not using front clip plane for culling
	//bufferData.vFrustumPlaneEquation[4] = g_pWorldSpaceFrustumPlaneEquation[4]; 
	// Not using far clip plane for culling
	//bufferData.vFrustumPlaneEquation[5] = g_pWorldSpaceFrustumPlaneEquation[5]; 
	bufferData.vTessellationFactor = XMVectorSet(7.0f, 7.0f, 7.0f / 2.0f, 1.0f / 10.0f);
	bufferData.detailTessellationHeightScale = atof(m_pScene->m_pSceneInfo->modelsInfo[m_pNode->GetName()].detailTessellationHeightScale.c_str());
	bufferData.vDiffuseColor = XMVectorSet(m_subMeshes[submeshNo]->diffuseColor.x, m_subMeshes[submeshNo]->diffuseColor.y,
		m_subMeshes[submeshNo]->diffuseColor.z, m_subMeshes[submeshNo]->diffuseColor.w);
	bufferData.specularLevel = 0.3f;
	for (size_t i = 0; i < 64; i++)
	{
		bufferData.mBone[i] = m_boneMatrix[i];
	}

	if (g_time > 1)
	{
		g_time = 0;
	}
	else
	{
		g_time += Engine::GetTimer()->getTimeInterval() / 30;
	}
	if (isMorphMesh)
	{
		bufferData.timeElapsed = g_time;
		//bufferData.timeElapsed = 0.0f;
	}
	else
	{
		bufferData.timeElapsed = 0.0f;
	}
	bufferData.hasDiffuseTexture = m_pDiffuseTexture ? true : false;
	bufferData.hasSpecularTexture = m_pSpecularTexture ? true : false;
	bufferData.hasNormalTexture = m_pNormalTexture ? true : false;

	m_pUniformBuffer->Update(&bufferData);

	gpuCmd.draw.camera = camera;
	gpuCmd.draw.customUBs[0] = m_pUniformBuffer;

	return;

	//m_pUniformBuffer->Update(&bufferData);

	//m_pUniformBuffer->Bind(CUSTOM0_UB_BP, VERTEX_SHADER);
	//m_pUniformBuffer->Bind(CUSTOM0_UB_BP, HULL_SHADER);
	//m_pUniformBuffer->Bind(CUSTOM0_UB_BP, DOMAIN_SHADER);
	//m_pUniformBuffer->Bind(CUSTOM0_UB_BP, GEOMETRY_SHADER);
	//m_pUniformBuffer->Bind(CUSTOM0_UB_BP, PIXEL_SHADER);
}

void Model::m_RenderShader(unsigned int submeshNo, RENDER_TYPE rendertype, GpuCmd &gpuCmd)
{
	//Engine::m_pRender->m_commandList->DrawIndexedInstanced(m_subMeshes[submeshNo]->TriangleCount * 3, 6, m_subMeshes[submeshNo]->IndexOffset, m_subMeshes[submeshNo]->VertexOffeset, 0);

	gpuCmd.draw.firstIndex = m_subMeshes[submeshNo]->IndexOffset;
	gpuCmd.draw.numElements = m_subMeshes[submeshNo]->TriangleCount * 3;
	gpuCmd.draw.numInstances = 1;
	gpuCmd.draw.vertexOffset = m_subMeshes[submeshNo]->VertexOffeset;
	if (m_pDiffuseTexture)
		gpuCmd.draw.textures[COLOR_TEX_BP] = m_pDiffuseTexture;

	return;
	//baseVertexLayout->Bind();

	//switch (rendertype)
	//{
	//case D3D11Framework::NORMAL_RENDER:
	//	if (TESSELLATION_ENABLED && m_pGridDensityTexture)
	//		m_pGBufferFillShader->Bind();
	//	else
	//		m_pWOTessShader->Bind();
	//	break;
	//case D3D11Framework::CASCADED_SHADOW_RENDER:
	//	m_pDX11ShaderCascaded->Bind();
	//	break;
	//case D3D11Framework::CUBEMAP_DEPTH_RENDER:
	//	m_pDX11ShaderCubeMap[1]->Bind();
	//	break;
	//case D3D11Framework::CUBEMAP_COLOR_RENDER:
	//	m_pDX11ShaderCubeMap[0]->Bind();
	//	break;
	//case D3D11Framework::VOXELIZE_RENDER:
	//	m_pDX11ShaderVoxelize->Bind();
	//	break;
	//default:
	//	break;
	//}

	//if (m_pDiffuseTexture)
	//	m_pDiffuseTexture->Bind(COLOR_TEX_BP, PIXEL_SHADER);

	//if (m_pSpecularTexture)
	//	m_pSpecularTexture->Bind(SPECULAR_TEX_BP, PIXEL_SHADER);

	//if (m_pNormalTexture)
	//	m_pNormalTexture->Bind(NORMAL_TEX_BP, DOMAIN_SHADER);

	//if (m_pNormalTexture)
	//	m_pNormalTexture->Bind(NORMAL_TEX_BP, PIXEL_SHADER);

	//if (TESSELLATION_ENABLED && m_pGridDensityTexture)
	//	m_pGridDensityTexture->Bind(CUSTOM0_TEX_BP, HULL_SHADER);

	// Set shader resources
	ID3D11ShaderResourceView *pSRV[MAX_BLEND_SHAPE_DEFORMER_COUNT];
	for (size_t i = 0; i < m_pInfluenceVBSRV.size(); i++)
		pSRV[i] = m_pInfluenceVBSRV[i];

	_MUTEXLOCK(Engine::m_pMutex);
	Engine::m_pRender->GetDeviceContext()->VSSetShaderResources(3, m_pInfluenceVBSRV.size(), pSRV);
	_MUTEXUNLOCK(Engine::m_pMutex);

	//Engine::m_pRender->GetSampler(TRILINEAR_SAMPLER_ID)->Bind(COLOR_TEX_BP, DOMAIN_SHADER);
	//Engine::m_pRender->GetSampler(TRILINEAR_SAMPLER_ID)->Bind(COLOR_TEX_BP, PIXEL_SHADER);

	switch (rendertype)
	{
	case D3D11Framework::CUBEMAP_DEPTH_RENDER:
	case D3D11Framework::CUBEMAP_COLOR_RENDER:
		_MUTEXLOCK(Engine::m_pMutex);
		Engine::m_pRender->GetDeviceContext()->DrawIndexedInstanced(m_subMeshes[submeshNo]->TriangleCount * 3, 6, m_subMeshes[submeshNo]->IndexOffset, m_subMeshes[submeshNo]->VertexOffeset, 0);
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;
	default:
		_MUTEXLOCK(Engine::m_pMutex);
		Engine::m_pRender->GetDeviceContext()->DrawIndexed(m_subMeshes[submeshNo]->TriangleCount * 3, m_subMeshes[submeshNo]->IndexOffset, m_subMeshes[submeshNo]->VertexOffeset);
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;
	}
}

void Model::ApplyForce(PxVec3 force)
{
	switch (physicalType)
	{
	case 1:
		printf("Can not applies a force to the static actor.\n");
		break;
	case 2:
		rigidDynamicActor->addForce(force);
		break;
	default:
		break;
	}
}

XMFLOAT3 Model::GetPosition()
{
	XMFLOAT3 returnsPosition;
	PxTransform position;
	switch (physicalType)
	{
	case 1:
	{
		position = rigidStaticActor->getGlobalPose();
		returnsPosition.x = position.p.x;
		returnsPosition.y = position.p.y;
		returnsPosition.z = position.p.z;
	}
	break;
	case 2:
	{
		position = rigidDynamicActor->getGlobalPose();
		returnsPosition.x = position.p.x;
		returnsPosition.y = position.p.y;
		returnsPosition.z = position.p.z;
	}
	break;
	default:
		break;
	}

	return returnsPosition;
}

void Model::SetPosition(float x, float y, float z)
{
	PxTransform position;
	switch (physicalType)
	{
	case 1:
		position = rigidStaticActor->getGlobalPose();
		position.p.x = x;
		position.p.y = y;
		position.p.z = z;
		rigidStaticActor->setGlobalPose(position);
		break;
	case 2:
		position = rigidDynamicActor->getGlobalPose();
		position.p.x += x;
		position.p.y += y;
		position.p.z += z;
		rigidDynamicActor->setGlobalPose(position);
		break;
	default:
		break;
	}
}

PxBounds3 Model::GetWorldBounds()
{
	PxBounds3 bounds;
	switch (physicalType)
	{
	case 1:
		bounds = rigidStaticActor->getWorldBounds();
		break;
	case 2:
		bounds = rigidDynamicActor->getWorldBounds();
		break;
	default:
		break;
	}

	return bounds;
}

int Model::GetPhysicalType()
{
	return physicalType;
}

void Model::SetPhysicalType(int pPhysicalType)
{
	physicalType = pPhysicalType;
}