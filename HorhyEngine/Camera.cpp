#include "stdafx.h"
#include "Camera.h"
#include "Engine.h"
#include "DX12_UniformBuffer.h"

using namespace D3D11Framework;
using namespace physx;

Camera::Camera()
{
	m_Projection = XMMatrixIdentity();
	m_frameTime = 0.0f;
	m_accumulateTime = 0.0f;
	m_leftTurnSpeed = 0.0f;
	m_rightTurnSpeed = 0.0f;
	m_strafeSpeed = 0.0f;
	m_forwardSpeed = 0.0f;
	m_isPhysFixed = false;
	m_isDirectional = false;

	m_pos = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	m_rot = XMVectorSet(0.0f, 90.0f, 0.0f, 0.0f);
	m_focusPosition = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	m_pUniformBuffer = Engine::GetRender()->CreateUniformBuffer(sizeof(CameraBufferData));

	PxCapsuleControllerDesc cDesc;
	cDesc.material = Engine::m_pPhysics->createMaterial(0.5, 0.5, 0.5);
	cDesc.position = PxExtendedVec3(0, 0, 0);
	cDesc.height = 1.25f;
	cDesc.radius = 0.05f;
	//cDesc.slopeLimit = 0.9f;
	cDesc.nonWalkableMode = PxControllerNonWalkableMode::ePREVENT_CLIMBING_AND_FORCE_SLIDING;
	//cDesc.maxJumpHeight = 1.0f;
	//cDesc.invisibleWallHeight = 1.0f;

	m_pController = PxCreateControllerManager(*Engine::m_pPxScene)->createController(cDesc);
}

XMVECTOR Cross(XMVECTOR vV1, XMVECTOR vV2, XMVECTOR vVector2)
{
	XMVECTOR vNormal;
	XMVECTOR vVector1;
	vVector1.m128_f32[0] = vV1.m128_f32[0] - vV2.m128_f32[0];
	vVector1.m128_f32[1] = vV1.m128_f32[1] - vV2.m128_f32[1];
	vVector1.m128_f32[2] = vV1.m128_f32[2] - vV2.m128_f32[2];

	vNormal.m128_f32[0] = ((vVector1.m128_f32[1] * vVector2.m128_f32[2]) - (vVector1.m128_f32[2] * vVector2.m128_f32[1]));
	vNormal.m128_f32[1] = ((vVector1.m128_f32[2] * vVector2.m128_f32[0]) - (vVector1.m128_f32[0] * vVector2.m128_f32[2]));
	vNormal.m128_f32[2] = ((vVector1.m128_f32[0] * vVector2.m128_f32[1]) - (vVector1.m128_f32[1] * vVector2.m128_f32[0]));

	return vNormal;
}

void Camera::Render(float time)
{
	float radiansX;
	float radiansY;

	if (m_isDirectional)
	{
		radiansX = m_pos.m128_f32[0] - m_focusPosition.m128_f32[0];
		radiansY = m_pos.m128_f32[1] - m_focusPosition.m128_f32[1];
	}
	else
	{
		radiansX = m_rot.m128_f32[0] * 0.0174532925f;
		radiansY = m_rot.m128_f32[1] * 0.0174532925f;
	}

	if (m_isPhysFixed)
	{
		if (m_forwardSpeed != 0 || m_strafeSpeed != 0)
		{
			float force = 1.0f;
			m_pController->move(PxVec3(
				((force*m_forwardSpeed*m_frameTime) * sinf(radiansY)*sinf(radiansX)) + (force*m_strafeSpeed*m_frameTime) * cosf(radiansX),
				(force*m_forwardSpeed*m_frameTime) * cosf(radiansY),
				((force*m_forwardSpeed*m_frameTime) * sinf(radiansY)*cosf(radiansX)) - (force*m_strafeSpeed*m_frameTime) * sinf(radiansX)
				), 0.0f, time, 0);
		}
		m_pController->move(PxVec3(0, -9.1f * m_frameTime, 0), 0.0f, time, 0);
		m_pos.m128_f32[0] = m_pController->getPosition().x;
		m_pos.m128_f32[1] = m_pController->getPosition().y + 0.65;
		m_pos.m128_f32[2] = m_pController->getPosition().z;
	}
	else
	{
		m_pos.m128_f32[0] += (m_forwardSpeed*m_frameTime) * sinf(radiansY)*sinf(radiansX);
		m_pos.m128_f32[1] += (m_forwardSpeed*m_frameTime) * cosf(radiansY);
		m_pos.m128_f32[2] += (m_forwardSpeed*m_frameTime) * sinf(radiansY)*cosf(radiansX);

		m_pos.m128_f32[0] += (m_strafeSpeed*m_frameTime) * cosf(radiansX);
		m_pos.m128_f32[2] -= (m_strafeSpeed*m_frameTime) * sinf(radiansX);
	}

	if (!m_isDirectional)
	{
		m_focusPosition = XMVectorSet(
			sinf(radiansY)*sinf(radiansX) + m_pos.m128_f32[0],
			cosf(radiansY) + m_pos.m128_f32[1],
			sinf(radiansY)*cosf(radiansX) + m_pos.m128_f32[2],
			0.0f);
	}

	camUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	m_viewMatrix = XMMatrixLookAtLH(m_pos, m_focusPosition, camUp);
	m_ortho = XMMatrixOrthographicLH(m_screenWidth, m_screenHeight, 0.0f, 100000.0f);

	m_frameTime = time;

	m_BufferData.viewMatrix = XMMatrixTranspose(m_viewMatrix);
	m_BufferData.projMatrix = XMMatrixTranspose(m_Projection);
	m_BufferData.viewProjMatrix = XMMatrixTranspose(m_viewMatrix * m_Projection);
	m_BufferData.invViewProjMatrix = XMMatrixTranspose(XMMatrixInverse(&XMMatrixDeterminant(m_viewMatrix * m_Projection), m_viewMatrix * m_Projection));
	m_BufferData.screenResolution = XMVectorSet(m_screenWidth, m_screenHeight, 1.0f / m_screenWidth, 1.0f / m_screenHeight);
	m_BufferData.position = m_pos;
	m_BufferData.nearClipDistance = m_screenNearZ;
	m_BufferData.farClipDistance = m_screenFarZ;
	m_BufferData.nearFarClipDistance = m_BufferData.farClipDistance - m_BufferData.nearClipDistance;
	m_pUniformBuffer->Update(&m_BufferData);
}

void Camera::TurnUp(bool keydown)
{
	if (keydown)
	{
		m_UpTurnSpeed += m_frameTime * 10.0f;

		if (m_UpTurnSpeed > (m_frameTime * 150.0f))
			m_UpTurnSpeed = m_frameTime * 150.0f;
	}
	else
	{
		m_UpTurnSpeed -= m_frameTime* 5.0f;

		if (m_UpTurnSpeed < 0.0f)
			m_UpTurnSpeed = 0.0f;
	}

	m_rot.m128_f32[1] -= m_UpTurnSpeed;
	if (m_rot.m128_f32[1] < 0.0f)
		m_rot.m128_f32[1] = 0.01f;
}


void Camera::TurnDown(bool keydown)
{
	if (keydown)
	{
		m_DownTurnSpeed += m_frameTime * 10.0f;

		if (m_DownTurnSpeed > (m_frameTime * 150.0f))
			m_DownTurnSpeed = m_frameTime * 150.0f;
	}
	else
	{
		m_DownTurnSpeed -= m_frameTime* 5.0f;

		if (m_DownTurnSpeed < 0.0f)
			m_DownTurnSpeed = 0.0f;
	}

	m_rot.m128_f32[1] += m_DownTurnSpeed;
	if (m_rot.m128_f32[1] > 180.0f)
		m_rot.m128_f32[1] = 179.99f;
}

void Camera::TurnLeft(bool keydown)
{
	if (keydown)
	{
		m_leftTurnSpeed += m_frameTime * 10.0f;

		if (m_leftTurnSpeed > (m_frameTime * 150.0f))
			m_leftTurnSpeed = m_frameTime * 150.0f;
	}
	else
	{
		m_leftTurnSpeed -= m_frameTime* 5.0f;

		if (m_leftTurnSpeed < 0.0f)
			m_leftTurnSpeed = 0.0f;
	}

	m_rot.m128_f32[0] -= m_leftTurnSpeed;
	if (m_rot.m128_f32[0] < 0.0f)
		m_rot.m128_f32[0] += 360.0f;
}


void Camera::TurnRight(bool keydown)
{
	if (keydown)
	{
		m_rightTurnSpeed += m_frameTime * 10.0f;

		if (m_rightTurnSpeed > (m_frameTime * 150.0f))
			m_rightTurnSpeed = m_frameTime * 150.0f;
	}
	else
	{
		m_rightTurnSpeed -= m_frameTime* 5.0f;

		if (m_rightTurnSpeed < 0.0f)
			m_rightTurnSpeed = 0.0f;
	}

	m_rot.m128_f32[0] += m_rightTurnSpeed;
	if (m_rot.m128_f32[0] > 360.0f)
		m_rot.m128_f32[0] -= 360.0f;
}

XMVECTOR Camera::GetPosition()
{
	return m_pos;
}

XMVECTOR Camera::GetFocusPosition()
{
	return m_focusPosition;
}

XMVECTOR Camera::GetRotation()
{
	return m_rot;
}

XMMATRIX Camera::GetProjection()
{
	return m_Projection;
}

void Camera::SetForwardSpeed(float speed)
{
	m_forwardSpeed = speed;
}

void Camera::SetStrafeSpeed(float speed)
{
	m_strafeSpeed = speed;
}

void Camera::SetPosition(float x, float y, float z)
{
	m_pos.m128_f32[0] = x;
	m_pos.m128_f32[1] = y;
	m_pos.m128_f32[2] = z;
}

void Camera::SetFocusPosition(float x, float y, float z)
{
	m_focusPosition.m128_f32[0] = x;
	m_focusPosition.m128_f32[1] = y;
	m_focusPosition.m128_f32[2] = z;
}

void Camera::SetRotation(float x, float y, float z)
{
	m_rot.m128_f32[0] = x;

	if (y > 180)
		m_rot.m128_f32[1] = 179.01f;
	else if (y < 0)
		m_rot.m128_f32[1] = 0.01f;
	else
		m_rot.m128_f32[1] = y;

	m_rot.m128_f32[2] = z;
}

void Camera::SetProjection(XMMATRIX projection)
{
	m_Projection = projection;
}

XMMATRIX Camera::GetOrthoMatrix()
{
	return m_ortho;
}

float Camera::GetScreenWidth()
{
	return m_screenWidth;
}

float Camera::GetScreenHeight()
{
	return m_screenHeight;
}

float Camera::GetScreenNearZ()
{
	return m_screenNearZ;
}

float Camera::GetScreenFarZ()
{
	return m_screenFarZ;
}

CXMMATRIX Camera::GetViewMatrix()
{
	return m_viewMatrix;
}

void Camera::SetPhysFixation(bool fix)
{
	if (fix)
		m_pController->setPosition(PxExtendedVec3(m_pos.m128_f32[0], m_pos.m128_f32[1], m_pos.m128_f32[2]));

	m_isPhysFixed = fix;
}

void Camera::SetDirectionalCamera(bool isdirectional)
{
	m_isDirectional = isdirectional;
}

void Camera::SetScreenWidth(float screenwidth)
{
	m_screenWidth = screenwidth;
}

void Camera::SetScreenHeight(float screenheight)
{
	m_screenHeight = screenheight;
}

void Camera::SetScreenNearZ(float screennearz)
{
	m_screenNearZ = screennearz;
}

void Camera::SetScreenFarZ(float screenfarz)
{
	m_screenFarZ = screenfarz;
}

DX12_UniformBuffer *Camera::GetUniformBuffer()
{
	return m_pUniformBuffer;
}
