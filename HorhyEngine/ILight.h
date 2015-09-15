#pragma once

#include "Render.h"
#include "Engine.h"

namespace D3D11Framework
{
	class CascadedShadowsManager;
	class DX12_UniformBuffer;
	class DrawCmd;

	class ILight
	{
	public:
		ILight();
		ILight(const ILight&);
		virtual ~ILight() = 0;

		virtual bool		Init() = 0;
		virtual void		DrawShadowSurface(DrawCmd &) = 0;
		virtual void		DrawScene(RENDER_TYPE) = 0;
		int					GetLightType();
		Camera				*GetLightCamera();
		DX12_UniformBuffer	*GetUniformBuffer();
		XMVECTOR			GetPosition();
		XMVECTOR			GetRotation();
		void				SetPosition(float x, float y, float z);
		void				SetRotation(float x, float y, float z);
		void				SetColor(float r, float g, float b);

	protected:
		Camera				*m_pLightCamera;
		DX12_UniformBuffer	*m_pUniformBuffer;
		XMFLOAT4			m_color;
		int			lightType;
		int			m_width, m_height;
	};
}