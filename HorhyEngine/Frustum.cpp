#include "stdafx.h"
#include "Frustum.h"

using namespace D3D11Framework;

void Frustum::ConstructFrustum(float screenDepth, CXMMATRIX projectionMatrix, CXMMATRIX viewMatrix)
{
	XMMATRIX projMatrix = projectionMatrix;
	// Вычисление минимальной дистации по Z в фрустуме.
	float zMinimum = -projMatrix.r[3].m128_f32[2] / projMatrix.r[2].m128_f32[2];
	float r = screenDepth / (screenDepth - zMinimum);
	projMatrix.r[2].m128_f32[2] = r;
	projMatrix.r[3].m128_f32[2] = -r * zMinimum;

	// Создание матрицы фрустума из видовой и обновленой проекционной матриц.
	XMMATRIX matrix = XMMatrixMultiply(viewMatrix, projMatrix);

	float a, b, c, d;

	// Вычисление близкой (near) плоскости.
	a = matrix.r[0].m128_f32[3] + matrix.r[0].m128_f32[2];
	b = matrix.r[1].m128_f32[3] + matrix.r[1].m128_f32[2];
	c = matrix.r[2].m128_f32[3] + matrix.r[2].m128_f32[2];
	d = matrix.r[3].m128_f32[3] + matrix.r[3].m128_f32[2];
	m_planes[0] = XMVectorSet(a, b, c, d);
	m_planes[0] = XMPlaneNormalize(m_planes[0]);

	// Вычисление дальней (far) плоскости.
	a = matrix.r[0].m128_f32[3] - matrix.r[0].m128_f32[2];
	b = matrix.r[1].m128_f32[3] - matrix.r[1].m128_f32[2];
	c = matrix.r[2].m128_f32[3] - matrix.r[2].m128_f32[2];
	d = matrix.r[3].m128_f32[3] - matrix.r[3].m128_f32[2];
	m_planes[1] = XMVectorSet(a, b, c, d);
	m_planes[1] = XMPlaneNormalize(m_planes[1]);

	// Вычисление левой (left) плоскости.
	a = matrix.r[0].m128_f32[3] + matrix.r[0].m128_f32[0];
	b = matrix.r[1].m128_f32[3] + matrix.r[1].m128_f32[0];
	c = matrix.r[2].m128_f32[3] + matrix.r[2].m128_f32[0];
	d = matrix.r[3].m128_f32[3] + matrix.r[3].m128_f32[0];
	m_planes[2] = XMVectorSet(a, b, c, d);
	m_planes[2] = XMPlaneNormalize(m_planes[2]);

	// Вычисление правой (right) плоскости.
	a = matrix.r[0].m128_f32[3] - matrix.r[0].m128_f32[0];
	b = matrix.r[1].m128_f32[3] - matrix.r[1].m128_f32[0];
	c = matrix.r[2].m128_f32[3] - matrix.r[2].m128_f32[0];
	d = matrix.r[3].m128_f32[3] - matrix.r[3].m128_f32[0];
	m_planes[3] = XMVectorSet(a, b, c, d);
	m_planes[3] = XMPlaneNormalize(m_planes[3]);

	// Вычисление верхней (top) плоскости.
	a = matrix.r[0].m128_f32[3] - matrix.r[0].m128_f32[1];
	b = matrix.r[1].m128_f32[3] - matrix.r[1].m128_f32[1];
	c = matrix.r[2].m128_f32[3] - matrix.r[2].m128_f32[1];
	d = matrix.r[3].m128_f32[3] - matrix.r[3].m128_f32[1];
	m_planes[4] = XMVectorSet(a, b, c, d);
	m_planes[4] = XMPlaneNormalize(m_planes[4]);

	// Вычисление нижней (bottom) плоскости.
	a = matrix.r[0].m128_f32[3] + matrix.r[0].m128_f32[1];
	b = matrix.r[1].m128_f32[3] + matrix.r[1].m128_f32[1];
	c = matrix.r[2].m128_f32[3] + matrix.r[2].m128_f32[1];
	d = matrix.r[3].m128_f32[3] + matrix.r[3].m128_f32[1];
	m_planes[5] = XMVectorSet(a, b, c, d);
	m_planes[5] = XMPlaneNormalize(m_planes[5]);
}

bool Frustum::CheckPoint(float x, float y, float z)
{
	for (int i = 0; i<6; i++)
	{
		float ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet(x, y, z, 1.0f)));
		if (ret < 0.0f)
			return false;
	}

	return true;
}


bool Frustum::CheckCube(float xCenter, float yCenter, float zCenter, float size)
{
	for (int i = 0; i<6; i++)
	{
		float ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - size), (yCenter - size), (zCenter - size), 1.0f)));
		if (ret >= 0.0f)
			continue;

		ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + size), (yCenter - size), (zCenter - size), 1.0f)));
		if (ret >= 0.0f)
			continue;

		ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - size), (yCenter + size), (zCenter - size), 1.0f)));
		if (ret >= 0.0f)
			continue;

		ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + size), (yCenter + size), (zCenter - size), 1.0f)));
		if (ret >= 0.0f)
			continue;

		ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - size), (yCenter - size), (zCenter + size), 1.0f)));
		if (ret >= 0.0f)
			continue;

		ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + size), (yCenter - size), (zCenter + size), 1.0f)));
		if (ret >= 0.0f)
			continue;

		ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - size), (yCenter + size), (zCenter + size), 1.0f)));
		if (ret >= 0.0f)
			continue;

		ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + size), (yCenter + size), (zCenter + size), 1.0f)));
		if (ret >= 0.0f)
			continue;

		return false;
	}

	return true;
}

bool Frustum::CheckSphere(float xCenter, float yCenter, float zCenter, float radius)
{
	for (int i = 0; i<6; i++)
	{
		float ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet(xCenter, yCenter, zCenter, 1.0f)));
		if (ret < -radius)
			return false;
	}

	return true;
}

bool Frustum::CheckRectangle(float xCenter, float yCenter, float zCenter, float xSize, float ySize, float zSize, bool checkByRange = false)
{
	if (checkByRange)
	{
		for (int i = 1; i == 1; i++)
		{
			float ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - xSize), (yCenter - ySize), (zCenter - zSize), 1.0f)));
			if (ret >= 0.0f)
				continue;

			ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + xSize), (yCenter - ySize), (zCenter - zSize), 1.0f)));
			if (ret >= 0.0f)
				continue;

			ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - xSize), (yCenter + ySize), (zCenter - zSize), 1.0f)));
			if (ret >= 0.0f)
				continue;

			ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - xSize), (yCenter - ySize), (zCenter + zSize), 1.0f)));
			if (ret >= 0.0f)
				continue;

			ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + xSize), (yCenter + ySize), (zCenter - zSize), 1.0f)));
			if (ret >= 0.0f)
				continue;

			ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + xSize), (yCenter - ySize), (zCenter + zSize), 1.0f)));
			if (ret >= 0.0f)
				continue;

			ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - xSize), (yCenter + ySize), (zCenter + zSize), 1.0f)));
			if (ret >= 0.0f)
				continue;

			ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + xSize), (yCenter + ySize), (zCenter + zSize), 1.0f)));
			if (ret >= 0.0f)
				continue;

			return false;
		}
	}
	else
	{
		for (int i = 0; i<6; i++)
		{
			float ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - xSize), (yCenter - ySize), (zCenter - zSize), 1.0f)));
			if (ret >= 0.0f)
				continue;

			ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + xSize), (yCenter - ySize), (zCenter - zSize), 1.0f)));
			if (ret >= 0.0f)
				continue;

			ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - xSize), (yCenter + ySize), (zCenter - zSize), 1.0f)));
			if (ret >= 0.0f)
				continue;

			ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - xSize), (yCenter - ySize), (zCenter + zSize), 1.0f)));
			if (ret >= 0.0f)
				continue;

			ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + xSize), (yCenter + ySize), (zCenter - zSize), 1.0f)));
			if (ret >= 0.0f)
				continue;

			ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + xSize), (yCenter - ySize), (zCenter + zSize), 1.0f)));
			if (ret >= 0.0f)
				continue;

			ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter - xSize), (yCenter + ySize), (zCenter + zSize), 1.0f)));
			if (ret >= 0.0f)
				continue;

			ret = XMVectorGetX(XMPlaneDotCoord(m_planes[i], XMVectorSet((xCenter + xSize), (yCenter + ySize), (zCenter + zSize), 1.0f)));
			if (ret >= 0.0f)
				continue;

			return false;
		}
	}

	return true;
}