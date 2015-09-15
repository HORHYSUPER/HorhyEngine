#pragma once

#include "Engine.h"
#include "Render.h"

namespace D3D11Framework
{
	class DX12_Sampler;

	struct SkyBoxVertex
	{
		SkyBoxVertex(){}
		SkyBoxVertex(float x, float y, float z,
			float u, float v,
			float nx, float ny, float nz)
			: pos(x, y, z), texCoord(u, v){}

		XMFLOAT3 pos;
		XMFLOAT2 texCoord;
	};

	class SkyBox
	{
	public:
		SkyBox();
		~SkyBox();

		bool Init();
		void UpdateSkyBox(Camera *camera);
		void DrawSkyBox(Camera *camera, RENDER_TYPE rendertype);
		bool CreateSphere(int LatLines, int LongLines);
		bool isLoaded;
		//DX11_Texture				*m_pDiffuseTexture;

	protected:
		ID3D11Buffer				*m_pSphereVertexBuffer;
		ID3D11Buffer				*m_pSphereIndexBuffer;
		//DX11_UniformBuffer			*m_pUniformBuffer;
		//Shader						*m_pShader;
		ID3D11SamplerState			*m_pCubesTexSamplerState;
		ID3D11RasterizerState		*m_pRSCullNone;
		ID3D11DepthStencilState		*m_pDSLessEqual;

		XMMATRIX sphereWorld;
		XMMATRIX Rotation;
		XMMATRIX Scale;
		XMMATRIX Translation;
		XMMATRIX Rotationx;
		XMMATRIX Rotationy;
		XMMATRIX Rotationz;
		XMMATRIX WVP;
		int NumSphereVertices;
		int NumSphereFaces;
	};
}