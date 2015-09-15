#pragma once

#include "Engine.h"
//#include "GpuCmd.h"

namespace D3D11Framework
{
	struct ModelInfo{
		std::string physicalType;
		std::string detailTessellationHeightScale;
		std::string addSpecular;
		std::string perPixelDiffuseLighting;
	};

	struct SceneInfo{
		std::string fileName;
		std::string fbxName;
		XMFLOAT3 position;
		XMFLOAT3 rotation;
		XMFLOAT3 scale;
		std::map<std::string, ModelInfo> modelsInfo;
	};

	class Scene;
	class Model;
	class SkyBox;
	//class Water;
	class Camera;
	class ILight;
	//class DirectionalLight;

	class GameWorld
	{

	public:
		GameWorld();
		~GameWorld();

		bool Initialize();
		bool LoadWorld();
		bool RenderWorld(RENDER_TYPE rendertype, Camera *camera);
		void UpdateWorld(float timeElapsed);
		bool StepPhysX();
		int				GetScenesCount();
		Scene			*GetSceneByIndex(int index);
		XMFLOAT3		*GetAmbientColor();
		SkyBox			*m_pSkyBox;

		unsigned int GetLightsCount()
		{
			return m_Lights.size();
		}

		ILight *GetLight(unsigned int lightIndex)
		{
			assert(lightIndex < m_Lights.size());
			return m_Lights[lightIndex];
		}

		bool m_upcam;
		bool m_downcam;
		bool m_leftcam;
		bool m_rightcam;

	private:
		PxController			*mController;
		std::vector<Scene*>		m_Scene;
		std::vector<SceneInfo>	m_ScenesInfo;
		std::vector<ILight*>	m_Lights;
		//std::vector<Water>	m_Water;
		//Camera					*m_lightCamera;
		//Water					*m_pWater;
		std::thread				*m_pThreads;
		int						m_nCPU;
		int						m_nScenes;
		int						m_nModels;
		float					m_Frame;
		XMFLOAT3				m_ambientColor;
	};
}