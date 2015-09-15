#include "stdafx.h"
#include "GameWorld.h"
#include "macros.h"
#include "Scene.h"
#include "Model.h"
#include "SkyBox.h"
#include "Camera.h"
//#include "Water.h"
#include "Timer.h"
#include "DirectionalLight.h"
#include "PointLight.h"
//#include "Light.h"

using namespace D3D11Framework;

bool WaterIsLoad = false;
bool g_isIncrease = true;

GameWorld::GameWorld()
{
	m_pThreads = NULL;
	m_pSkyBox = NULL;
	//m_lightCamera = NULL;
	m_upcam = false;
	m_downcam = false;
	m_leftcam = false;
	m_rightcam = false;
}

GameWorld::~GameWorld()
{
	SAFE_DELETE(m_pSkyBox);
	m_Scene.clear();
}

bool GameWorld::Initialize()
{
	m_Frame = 0;
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);

	m_nCPU = sysinfo.dwNumberOfProcessors;
	m_pThreads = new std::thread[m_nCPU];
	m_ambientColor = XMFLOAT3(0.0f, 0.0f, 0.0f);

	return true;
}

bool GameWorld::LoadWorld()
{
	int threadNo = 0;
	//m_pSkyBox = new SkyBox();
	//if (!m_pSkyBox)
	//	return false;

	//m_pSkyBox->Init();

	DirectionalLight *dirLight = new DirectionalLight;
	dirLight->Init();
	dirLight->GetLightCamera()->SetRotation(135, 110, 0);

	//PointLight *pointLight = new PointLight;
	//pointLight->Init();
	//pointLight->SetPosition(5, 3.5, 0);
	//pointLight->SetColor(0.80, 0.905, 0.80);

	m_Lights.push_back(dirLight);

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file("Data/Levels/Level_1.xml");
	pugi::xml_node nodeCurrent = doc.child("level");
	nodeCurrent = nodeCurrent.child("objects");
	ModelInfo modelInfo;

	m_nScenes = 0;
	for (pugi::xml_node item = nodeCurrent.first_child(); item; item = item.next_sibling())
	{
		const char* name = item.name();
		if (strcmp(name, "object") == 0)
		{
			m_ScenesInfo.resize(m_ScenesInfo.size() + 1);
			m_ScenesInfo[m_nScenes].fileName = (string)item.child_value();
			m_ScenesInfo[m_nScenes].fileName = (string)(strcat((char*)string("Data/Scenes/").c_str(), (char*)m_ScenesInfo[m_nScenes].fileName.c_str()));
			m_ScenesInfo[m_nScenes].fileName += ".xml";
			m_ScenesInfo[m_nScenes].position.x = 1.0f;
			m_ScenesInfo[m_nScenes].position.x = atof(item.attribute("posX").value());
			m_ScenesInfo[m_nScenes].position.y = atof(item.attribute("posY").value());
			m_ScenesInfo[m_nScenes].position.z = atof(item.attribute("posZ").value());
			m_ScenesInfo[m_nScenes].rotation.x = atof(item.attribute("rotX").value());
			m_ScenesInfo[m_nScenes].rotation.y = atof(item.attribute("rotY").value());
			m_ScenesInfo[m_nScenes].rotation.z = atof(item.attribute("rotZ").value());
			m_ScenesInfo[m_nScenes].scale.x = atof(item.attribute("scaleX").value());
			m_ScenesInfo[m_nScenes].scale.y = atof(item.attribute("scaleY").value());
			m_ScenesInfo[m_nScenes].scale.z = atof(item.attribute("scaleZ").value());
			m_nScenes++;
		}
	}

	for (int i = 0; i < m_nScenes; i++)
	{
		result = doc.load_file(m_ScenesInfo[i].fileName.c_str());

		pugi::xml_node nodeScene = doc.child("scene");
		pugi::xml_node nodeSceneName = nodeScene.child("scenename");
		pugi::xml_node nodeProp = nodeScene.child("nodeProp");
		m_ScenesInfo[i].fbxName = (string)nodeSceneName.child_value();

		m_nModels = 0;
		for (pugi::xml_node item = nodeProp.first_child(); item; item = item.next_sibling())
		{
			const char* name = item.name();
			if (strcmp(name, "Model") == 0)
			{
				m_ScenesInfo[i].modelsInfo.insert(pair<std::string, ModelInfo>(item.child_value(),
					ModelInfo{ item.attribute("physType").value(),
					item.attribute("detailTessellationHeightScale").value(),
					item.attribute("addSpecular").value(),
					item.attribute("perPixelDiffuseLighting").value() }));

				m_nModels++;
			}
		}
		m_Scene.resize(m_Scene.size() + 1);
		m_Scene[i] = new Scene;

		if (i >= m_nCPU)
		{
			m_pThreads[threadNo].detach();
			m_pThreads[threadNo] = std::thread(&Scene::Init, std::ref(m_Scene[i]), &m_ScenesInfo[i]);
			if (threadNo > m_nCPU)
				threadNo = 0;
		}
		else
		{
			m_Scene[i]->Init(&m_ScenesInfo[i]);
			//m_pThreads[i] = std::thread(&Scene::Init, m_Scene[i], m_pFramework->GetRender(), &m_ScenesInfo[i]);
		}
	}

	// Create ocean simulating object
	// Ocean object
	//OceanParameter ocean_param;

	//// The size of displacement map. In this sample, it's fixed to 512.
	//ocean_param.dmap_dim = 512;
	//// The side length (world space) of square patch
	//ocean_param.patch_length = 2000.0f;
	//// Adjust this parameter to control the simulation speed
	//ocean_param.time_scale = 0.8f;
	//// A scale to control the amplitude. Not the world space height
	//ocean_param.wave_amplitude = 0.35f;
	//// 2D wind direction. No need to be normalized
	//ocean_param.wind_dir = XMVectorSet(0.8f, 0.6f, 0.0f, 0.0f);
	//// The bigger the wind speed, the larger scale of wave crest.
	//// But the wave scale can be no larger than patch_length
	//ocean_param.wind_speed = 600.0f;
	//// Damp out the components opposite to wind direction.
	//// The smaller the value, the higher wind dependency
	//ocean_param.wind_dependency = 0.07f;
	//// Control the scale of horizontal movement. Higher value creates
	//// pointy crests.
	//ocean_param.choppy_scale = 1.3f;

	/*m_pWater = new Water(ocean_param, m_pFramework->GetRender()->GetDevice(), m_pFramework);
	m_pWater->initRenderResource(ocean_param, m_pFramework->GetRender()->GetDevice());
	WaterIsLoad = true;*/

	return true;
}

float g_Test = 0.0f;

void GameWorld::UpdateWorld(float timeElapsed)
{
	if (m_Frame >= 300)
		m_Frame = 0;
	else
		m_Frame += timeElapsed * 30;

	for (unsigned short i = 0; i < m_Lights.size(); i++)
	{
		m_Lights[i]->GetLightCamera()->TurnUp(m_upcam);
		m_Lights[i]->GetLightCamera()->TurnDown(m_downcam);
		m_Lights[i]->GetLightCamera()->TurnLeft(m_leftcam);
		m_Lights[i]->GetLightCamera()->TurnRight(m_rightcam);
		m_Lights[i]->GetLightCamera()->Render(timeElapsed);
	}

	//if (WaterIsLoad)
	//	m_pWater->updateDisplacementMap(g_Test);

	for (int i = 0; i < m_Scene.size(); i++)
	{
		if (&m_Scene && m_Scene[i]->isLoaded)
		{
			m_Scene[i]->animateScene(m_Frame);
		}
	}
	if (g_isIncrease)
	{
		//m_ambientColor = XMFLOAT3(m_ambientColor.x + 0.0005f, m_ambientColor.z + 0.0005f, m_ambientColor.z + 0.0005f);
	}
	else
	{
		//m_ambientColor = XMFLOAT3(m_ambientColor.x - 0.0005f, m_ambientColor.z - 0.0005f, m_ambientColor.z - 0.0005f);
	}
	if (m_ambientColor.x >= 1.0f)
	{
		g_isIncrease = false;
	}
	if (m_ambientColor.x <= 0.0f)
	{
		g_isIncrease = true;
	}
}

bool GameWorld::RenderWorld(RENDER_TYPE rendertype, Camera *camera)
{
	g_Test += Engine::GetTimer()->getTimeInterval();
	//if (WaterIsLoad)
	//{
	//	m_pWater->RenderWater(camera, rendertype, m_pWater->getD3D11DisplacementMap(), m_pWater->getD3D11GradientMap(), g_Test, Framework::GetRender()->GetDeviceContext());
	//}

	for (int i = 0; i < Engine::GetRender()->m_objectListOnRender.size(); i++)
	{
		Engine::GetRender()->m_objectListOnRender[i]->Draw(camera, rendertype);
	}

	//for (int i = 0; i < m_Scene.size(); i++)
	//{
	//	if (&m_Scene && m_Scene[i]->isLoaded)
	//	{
	//		m_Scene[i]->Draw(camera, rendertype);
	//	}
	//}

	if (m_pSkyBox->isLoaded && rendertype == GBUFFER_FILL_RENDER)
	{
		//m_pSkyBox->UpdateSkyBox(camera);
		//m_pSkyBox->DrawSkyBox(camera, rendertype);
	}

	return true;
}

bool GameWorld::StepPhysX()
{
	for (int i = 0; i < m_Scene.size(); i++)
	{
		if (&m_Scene && m_Scene[i]->isLoaded)
		{
			m_Scene[i]->StepPhysX();
		}
	}

	return true;
}

XMFLOAT3 *GameWorld::GetAmbientColor()
{
	return &m_ambientColor;
}

int GameWorld::GetScenesCount()
{
	return m_Scene.size();
}

Scene *GameWorld::GetSceneByIndex(int index)
{
	return m_Scene[index];
}