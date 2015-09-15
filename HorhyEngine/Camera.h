#pragma once

#include "Render.h"
#include "Engine.h"

namespace D3D11Framework
{
	class Engine;
	class DX11_UniformBuffer;
	class DX12_UniformBuffer;

	struct CameraBufferData
	{
		CameraBufferData() :
			nearClipDistance(0.0f),
			farClipDistance(0.0f),
			nearFarClipDistance(0.0f)
		{
		}

		XMMATRIX viewMatrix;
		XMMATRIX projMatrix;
		XMMATRIX viewProjMatrix;
		XMMATRIX invViewProjMatrix;
		XMVECTOR screenResolution;
		XMVECTOR position;
		float nearClipDistance;
		float farClipDistance;
		float nearFarClipDistance;
	};

	class Camera
	{
	public:
		Camera();

		void Render(float time);

		void TurnUp(bool keydown);
		void TurnDown(bool keydown);
		void TurnLeft(bool keydown);
		void TurnRight(bool keydown);

		void SetForwardSpeed(float speed);
		void SetStrafeSpeed(float speed);
		void SetPosition(float x, float y, float z);
		void SetFocusPosition(float x, float y, float z);
		void SetRotation(float x, float y, float z);
		void SetProjection(XMMATRIX projection);
		void SetPhysFixation(bool);
		void SetDirectionalCamera(bool);
		void SetScreenWidth(float screenwidth);
		void SetScreenHeight(float screenheight);
		void SetScreenNearZ(float screennearz);
		void SetScreenFarZ(float screenfarz);

		DX12_UniformBuffer	*GetUniformBuffer();
		XMVECTOR	GetPosition();
		XMVECTOR	GetFocusPosition();
		XMVECTOR	GetRotation();
		XMMATRIX	GetProjection();
		CXMMATRIX	GetViewMatrix();
		XMMATRIX	GetOrthoMatrix();
		float		GetScreenWidth();
		float		GetScreenHeight();
		float		GetScreenNearZ();
		float		GetScreenFarZ();
		float		m_frameTime;
	private:
		XMVECTOR m_focusPosition;
		XMVECTOR camUp;
		XMMATRIX m_ortho;
		XMMATRIX m_viewMatrix;
		XMMATRIX m_Projection;
		XMVECTOR m_pos;
		XMVECTOR m_rot;
		DX12_UniformBuffer		*m_pUniformBuffer;
		CameraBufferData		m_BufferData;
		PxController			*m_pController;
		float m_screenWidth, m_screenHeight, m_screenNearZ, m_screenFarZ;

		bool m_isPhysFixed, m_isDirectional;
		float m_accumulateTime;
		float m_forwardSpeed, m_strafeSpeed, m_UpTurnSpeed, m_DownTurnSpeed, m_leftTurnSpeed, m_rightTurnSpeed;
	};
}