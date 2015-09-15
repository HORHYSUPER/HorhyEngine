#pragma once

using namespace DirectX;

namespace D3D11Framework
{
	class Frustum
	{
	public:
		void ConstructFrustum(float screenDepth, CXMMATRIX projectionMatrix, CXMMATRIX viewMatrix);

		bool CheckPoint(float x, float y, float z);
		bool CheckCube(float xCenter, float yCenter, float zCenter, float size);
		bool CheckSphere(float xCenter, float yCenter, float zCenter, float radius);
		bool CheckRectangle(float xCenter, float yCenter, float zCenter, float xSize, float ySize, float zSize, bool checkByRange);

	private:
		XMVECTOR m_planes[6];
	};
}