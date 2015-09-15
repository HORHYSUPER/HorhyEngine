#pragma once

#include "Window.h"
#include "InputMgr.h"
#include "Log.h"
#include "Timer.h"

using namespace std;
using namespace DirectX;
using namespace physx;

namespace D3D11Framework
{
	class Render;
	class Sound;
	class GameWorld;
	class Camera;
	class ResourceManager;
	class Window;

	struct FrameworkDesc
	{
		DescWindow wnd;
		Render *render;
		std::string pathToData;
	};

	
	typedef enum RENDER_TYPE
	{
		GBUFFER_FILL_RENDER,
		CASCADED_SHADOW_RENDER,
		CUBEMAP_DEPTH_RENDER,
		CUBEMAP_COLOR_RENDER,
		VOXELIZE_RENDER
	} RENDER_TYPE;

	typedef enum POST_PROCESSING
	{
		SHADOW_RENDER,
		HORIZONTAL_BLUR,
		VERTICAL_BLUR,
		FINAL_RENDER
	} POST_PROCESSING;

	typedef enum SYSTEM_PATHS
	{
		PATH_TO_SHADERS,
		PATH_TO_MODELS,
		PATH_TO_SCENES,
		PATH_TO_SOUNDS,
		PATH_TO_TEXTURES,
		PATH_TO_LEVELS
	} SYSTEM_PATHS;
	
	struct Vertex
	{
		Vertex() :
			pos(XMFLOAT3(0.0f, 0.0f, 0.0f)), tex(XMFLOAT2(0.0f, 0.0f)),
			norm(XMFLOAT3(0.0f, 0.0f, 0.0f)), binorm(XMFLOAT3(0.0f, 0.0f, 0.0f)), tan(XMFLOAT3(0.0f, 0.0f, 0.0f)),
			boneIndex1(XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f)), boneIndex2(XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f)),
			boneWeight1(XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f)), boneWeight2(XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f)), vertIndex(0) 
		{}

		XMFLOAT3 pos;
		XMFLOAT3 norm;
		XMFLOAT3 binorm;
		XMFLOAT3 tan;
		XMFLOAT2 tex;
		XMFLOAT4 boneIndex1;
		XMFLOAT4 boneIndex2;
		XMFLOAT4 boneWeight1;
		XMFLOAT4 boneWeight2;
		int vertIndex;
	};

	struct SimpleVertex
	{
		XMFLOAT3 pos;
	};

	struct VERTEX
	{
		int id;
		int index;
		float weight;
		float x, y, z;
		float nx, ny, nz;
		float u, v;
	};

	class Engine
	{
		friend class Render;
		friend class Scene;
		friend class Model;
		friend class Water;

	public:
		Engine();
		~Engine();

		bool		Initialize(const FrameworkDesc &desc);
		void		Run();
		void		RunPhysX();
		void		AddInputListener(InputListener *listener);
		static Render *GetRender()
		{
			return m_pRender;
		}
		//GameWorld	*GetGameWorld();
		//PxPhysics	*GetPxPhysics();
		//PxScene		*GetPxScene();
		static Timer		*GetTimer();
		static std::wstring *GetSystemPath(SYSTEM_PATHS pathTo);
		std::recursive_mutex *GetMutex();
		static GameWorld				*m_pGameWorld;
		static std::recursive_mutex		*m_pMutex;
		static std::wstring				*m_pPaths;
		static PxPhysics				*m_pPhysics;
		static PxCooking				*m_pCooking;
		static PxScene					*m_pPxScene;
		static Timer					*m_pTimer;
		static ResourceManager			*resourceManager;
		static InputMgr					*m_pInputMgr;

	protected:
		void					InitPhysX();
		bool					UpdateWorld();
		void					ConnectPVD();
		bool					m_frame();

		static Render						*m_pRender;
		std::thread							m_physXThread, m_loadSceneThread;
		Timer								*m_pPXTimer;               //HR timer
		PxFoundation						*m_pFoundation;			//Instance of singleton foundation SDK class
		PxDefaultErrorCallback				m_defaultErrorCallback;		//Instance of default implementation of the error callback
		PxDefaultAllocator					m_defaultAllocatorCallback;	//Instance of default implementation of the allocator interface required by the SDK
		PxReal								m_timeStep;		//Time-step value for PhysX simulation
		Engine::FrameworkDesc						m_desc;
		Window								*m_pWindow;
		Sound								*m_pSound;
		Log									m_log;
		bool								m_isInit;
		bool								m_isActive;
		float								mAccumulator;
	};
}