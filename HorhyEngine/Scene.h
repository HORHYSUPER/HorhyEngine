#pragma once

#include "Render.h"
#include "Model.h"
#include "GameWorld.h"

namespace D3D11Framework
{
	class Engine;

	class Scene
	{
		friend class Model;

	public:
		Scene();
		Scene(const Scene &copy);
		~Scene();

		Scene &operator = (const Scene &other);

		bool Init(SceneInfo *scenesInfo);
		void animateScene(float keyFrame);
		void StepPhysX();
		void Draw(Camera *camera, RENDER_TYPE rendertype);

		int GetModelsCount();
		Model *GetModelByIndex(int index);
		void ApplyForce(int modelIndex, PxVec3 force);
		void SetPosition(XMFLOAT3 position);
		XMFLOAT3 GetPosition();
		void SetRotation(XMFLOAT3 rotation);
		XMFLOAT3 GetRotation();
		void SetRotationAngle(float rotationAngle);
		float GetRotationAngle();
		void SetScale(XMFLOAT3 scale);
		XMFLOAT3 GetScale();
		bool isLoaded;

		void* operator new(size_t i)
		{
			return _aligned_malloc(i, 16);
		}

		void operator delete(void* p)
		{
			_aligned_free(p);
		}

	private:
		bool ParseAttribute(FbxNodeAttribute* pAttribute);
		bool ParseNode(FbxNode* pNode);

		FbxScene				*m_pFbxScene;
		std::vector<Model>		m_model;
		FbxAnimStack			*m_pAnimStack;
		FbxAnimLayer			*m_pAnimLayer;
		SceneInfo				*m_pSceneInfo;
		XMMATRIX				m_objectMatrix;
		XMFLOAT3				m_position;
		XMFLOAT3				m_rotation;
		XMFLOAT3				m_scale;
		float					m_rotationAngle;
		unsigned short			m_indexCount;
	};
}