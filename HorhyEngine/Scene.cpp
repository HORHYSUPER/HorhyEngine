#include "stdafx.h"
#include "Engine.h"
#include "Scene.h"
#include "macros.h"
#include "GameWorld.h"

using namespace D3D11Framework;
using namespace std;

Scene::Scene() : isLoaded(false)
{
}

Scene::Scene(const Scene &copy)
{
	operator=(copy);
}

Scene::~Scene()
{
	isLoaded = false;
	//_DELETE_ARRAY(m_model);
	//_DELETE_ARRAY(m_modelInfo);
}

Scene &Scene::operator=(const Scene &other)
{
	if (this != &other)
	{
		this->isLoaded = other.isLoaded;
		this->m_indexCount = other.m_indexCount;
		this->m_objectMatrix = other.m_objectMatrix;
		this->m_pAnimLayer = other.m_pAnimLayer;
		this->m_pAnimStack = other.m_pAnimStack;
		this->m_pFbxScene = other.m_pFbxScene;
		this->m_model = other.m_model;
		for (size_t i = 0; i < m_model.size(); i++)
			this->m_model[i].m_pScene = this;
		this->m_position = other.m_position;
		this->m_pSceneInfo = other.m_pSceneInfo;
		this->m_rotation = other.m_rotation;
		this->m_scale = other.m_scale;
	}
	return *this;
}

bool Scene::ParseAttribute(FbxNodeAttribute* pAttribute)
{
	if (!pAttribute)
		return false;

	if (pAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
	{
		m_model.resize(m_model.size() + 1);
		m_model[m_model.size() - 1].Initialize(this, m_pFbxScene, pAttribute->GetNode());
	}

	return true;
}

bool Scene::ParseNode(FbxNode* pNode)
{
	for (int i = 0; i < pNode->GetNodeAttributeCount(); i++)
		ParseAttribute(pNode->GetNodeAttributeByIndex(i));

	const int lChildCount = pNode->GetChildCount();
	for (int lChildIndex = 0; lChildIndex < lChildCount; ++lChildIndex)
	{
		ParseNode(pNode->GetChild(lChildIndex));
	}

	return true;
}

bool Scene::Init(SceneInfo *scenesInfo)
{
	m_pSceneInfo = scenesInfo;
	m_position = scenesInfo->position;
	m_rotation = scenesInfo->rotation;

	_MUTEXLOCK(Engine::m_pMutex);
	FbxManager* lSdkManager = FbxManager::Create();
	FbxIOSettings *ios = FbxIOSettings::Create(lSdkManager, IOSROOT);
	lSdkManager->SetIOSettings(ios);

	FbxImporter* lImporter = FbxImporter::Create(lSdkManager, "");
	if (!lImporter->Initialize(m_pSceneInfo->fbxName.c_str(), -1, lSdkManager->GetIOSettings())) {
		printf("Call to FbxImporter::Initialize() failed.\n");
		printf("Error returned: %s\n\n", lImporter->GetStatus().GetErrorString());
		exit(-1);
	}

	m_pFbxScene = FbxScene::Create(lSdkManager, "myScene");
	lImporter->Import(m_pFbxScene);

	//lImporter->Destroy();

	_MUTEXUNLOCK(Engine::m_pMutex);

	FbxNode* lRootNode = m_pFbxScene->GetRootNode();
	ParseNode(lRootNode);
	/*if (lRootNode) {
		for (int i = 0; i < lRootNode->GetChildCount(); i++)
			ParseNode(lRootNode->GetChild(i));
	}*/

	_MUTEXLOCK(Engine::m_pMutex);
 	//lSdkManager->Destroy();
	_MUTEXUNLOCK(Engine::m_pMutex);

	isLoaded = true;

	return true;
}

void Scene::StepPhysX()
{
	for (int i = 0; i < m_model.size(); i++)
	{
		m_model[i].StepPhysX();
	}
}

void Scene::animateScene(float keyFrame)
{
	for (int i = 0; i < m_model.size(); i++)
	{
		m_model[i].animateModel(keyFrame);
	}
}

void Scene::Draw(Camera *camera, RENDER_TYPE rendertype)
{
	for (int i = 0; i < m_model.size(); i++)
	{
		m_model[i].Draw(camera, rendertype);
	}
}

Model *Scene::GetModelByIndex(int index)
{
	return &m_model[index];
}

int Scene::GetModelsCount()
{
	return m_model.size();
}

void Scene::ApplyForce(int modelIndex, PxVec3 force)
{
	if (modelIndex==-1)
	{
		for (int i = 0; i < m_model.size(); i++)
		{
			m_model[i].ApplyForce(force);
		}
	}
	else
	{
		m_model[modelIndex].ApplyForce(force);
	}
}

void Scene::SetPosition(XMFLOAT3 position)
{
	m_position = position;
	for (int i = 0; i < m_model.size(); i++)
	{
		m_model[i].SetPosition(m_position.x, m_position.y, m_position.z);
	}
}

XMFLOAT3 Scene::GetPosition()
{
	return m_position;
}

void Scene::SetRotation(XMFLOAT3 rotation)
{
	m_rotation = rotation;
}

XMFLOAT3 Scene::GetRotation()
{
	return m_rotation;
}

void Scene::SetRotationAngle(float rotationAngle)
{
	m_rotationAngle = rotationAngle;
}

float Scene::GetRotationAngle()
{
	return m_rotationAngle;
}

void Scene::SetScale(XMFLOAT3 scale)
{
	m_scale = scale;
}

XMFLOAT3 Scene::GetScale()
{
	return m_scale;
}