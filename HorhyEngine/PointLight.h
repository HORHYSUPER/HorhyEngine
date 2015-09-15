#pragma once

#include "ILight.h"
#include "Engine.h"

namespace D3D11Framework
{
	class DX12_RenderTargetConfig;

	class PointLight :
		public ILight
	{
		struct BufferData
		{
			BufferData() :
				radius(0.0f),
				multiplier(0.0f)
			{
			}

			XMFLOAT3 position;
			float radius;
			XMFLOAT4 color;
			XMMATRIX worldMatrix;
			float multiplier;
		};

	public:
		PointLight() : ILight() {}
		~PointLight();

		bool Init();
		void DrawShadowSurface(DrawCmd &);
		void DrawScene(RENDER_TYPE);
		DX12_RenderTarget *GetShadowMapRT()
		{
			return m_pShadowMapRT;
		}

	private:
		PipelineStateObject					*m_pPointLightPso;
		DX12_UniformBuffer					*m_pCubicViewUB;
		DX12_RenderTarget					*m_pShadowMapRT;
		DX12_RenderTargetConfig				*m_pSceneRTC;
		BufferData							bufferData;
	};
}