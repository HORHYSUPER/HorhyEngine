#include "stdafx.h"
#include "macros.h"
#include "Log.h"
#include "Render.h"
#include "GameWorld.h"
#include "ResourceManager.h"
#include "Sound.h"
#include "Engine.h"

using namespace physx;
using namespace D3D11Framework;

Render					*Engine::m_pRender = NULL;
std::recursive_mutex	*Engine::m_pMutex = NULL;
GameWorld				*Engine::m_pGameWorld = NULL;
std::wstring			*Engine::m_pPaths = NULL;
PxPhysics				*Engine::m_pPhysics = NULL;
PxCooking				*Engine::m_pCooking = NULL;
PxScene					*Engine::m_pPxScene = NULL;
ResourceManager			*Engine::resourceManager = NULL;
InputMgr				*Engine::m_pInputMgr = NULL;
Timer					*Engine::m_pTimer;

Engine::Engine()
{
	m_pPhysics = NULL;
	m_pCooking = NULL;
	m_pFoundation = NULL;
	m_pPxScene = NULL;
	m_pGameWorld = NULL;
	m_pMutex = NULL;
	m_pSound = NULL;
	mAccumulator = 0.0f;
	m_timeStep = 1.0f / 60.0f;
}

Engine::~Engine()
{
	m_isInit = false;
	m_isActive = false;
	SAFE_DELETE(m_pRender);
	SAFE_CLOSE(m_pWindow);
	SAFE_CLOSE(m_pInputMgr);
}

void Engine::AddInputListener(InputListener *listener)
{
	if (m_pInputMgr)
		m_pInputMgr->AddListener(listener);
}

bool Engine::Initialize(const FrameworkDesc &desc)
{
	resourceManager = new ResourceManager;
	if (!resourceManager)
		return false;

	m_pTimer = new Timer;
	m_pPXTimer = new Timer;
	m_timeStep = 1.0f / 60.0f;
	m_pRender = desc.render;
	m_pWindow = new Window();
	m_pInputMgr = new InputMgr();
	m_pMutex = new std::recursive_mutex;
	m_pPaths = new std::wstring[6];
	m_pPaths[PATH_TO_SHADERS] = L"Data/Shaders/";
	m_pPaths[PATH_TO_MODELS] = L"Data/Models/";
	m_pPaths[PATH_TO_SCENES] = L"Data/Scenes/";
	m_pPaths[PATH_TO_SOUNDS] = L"Data/Sounds/";
	m_pPaths[PATH_TO_TEXTURES] = L"Data/Textures/";
	m_pPaths[PATH_TO_LEVELS] = L"Data/Levels/";

	m_pInputMgr->Init();

	if (!m_pWindow->Create(desc.wnd))
	{
		Log::Get()->Err("Ќе удалось создать окно");
		return false;
	}
	m_pWindow->SetInputMgr(m_pInputMgr);

	InitPhysX();
#ifdef _DEBUG
	ConnectPVD();
#endif

	if (!m_pRender->Create(m_pWindow->GetHWND()))
	{
		Log::Get()->Err("Ќе удалось создать рендер");
		return false;
	}

	m_pGameWorld = new GameWorld();
	m_pGameWorld->Initialize();
	m_pGameWorld->LoadWorld(); //Without multithread
	//m_loadSceneThread = std::thread(&GameWorld::LoadWorld, std::ref(m_pGameWorld));
	//m_loadSceneThread.detach();

	// Create the sound object.
	m_pSound = new Sound();
	if (!m_pSound)
	{
		return false;
	}

	// Initialize the sound object.
	//HRESULT hr = m_pSound->Initialize(m_pWindow->GetHWND(), L"sound05.wav");
	//if (FAILED(hr))
	//{
	//	MessageBox(m_pWindow->GetHWND(), L"Could not initialize Direct Sound.", L"Error", MB_OK);
	//	return false;
	//}

	m_pPXTimer->startTimer();
	m_pTimer->startTimer();
	m_isInit = true;

	return true;
}

PxFilterFlags VehicleFilterShader(PxFilterObjectAttributes attributes0, PxFilterData filterData0,
	PxFilterObjectAttributes attributes1, PxFilterData filterData1,
	PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize)
{
	PX_UNUSED(attributes0);
	PX_UNUSED(attributes1);
	PX_UNUSED(constantBlock);
	PX_UNUSED(constantBlockSize);

	pairFlags = PxPairFlag::eCONTACT_DEFAULT;
	pairFlags |= PxPairFlag::eCONTACT_EVENT_POSE;


	pairFlags |= physx::PxPairFlag::eRESOLVE_CONTACTS;

	pairFlags |= physx::PxPairFlag::eCONTACT_DEFAULT;
	pairFlags |= physx::PxPairFlag::eTRIGGER_DEFAULT;

	if ((0 == (filterData0.word0 & filterData1.word1)) && (0 == (filterData1.word0 & filterData0.word1)))
		return PxFilterFlag::eSUPPRESS;

	return PxFilterFlags();

}

void Engine::InitPhysX()
{
	//Creating foundation for PhysX
	m_pFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_defaultAllocatorCallback, m_defaultErrorCallback);
	if (!m_pFoundation)
		printf("PxCreateFoundation failed!");

	//Creating instance of PhysX SDK
	m_pPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_pFoundation, PxTolerancesScale());

	if (m_pPhysics == NULL)
	{
		cerr << "Error creating PhysX3 device, Exiting..." << endl;
		exit(1);
	}

	//Creating scene
	PxSceneDesc sceneDesc(m_pPhysics->getTolerancesScale());	//Descriptor class for scenes 
	sceneDesc.gravity = PxVec3(0.0f, -9.8f, 0.0f);		//Setting gravity
	sceneDesc.flags = PxSceneFlag::eENABLE_ACTIVETRANSFORMS;
	sceneDesc.flags |= PxSceneFlag::eENABLE_CCD;

	if (!sceneDesc.cpuDispatcher)
	{
		PxDefaultCpuDispatcher* mCpuDispatcher = PxDefaultCpuDispatcherCreate(1);
		sceneDesc.cpuDispatcher = mCpuDispatcher;
	}
	if (!sceneDesc.filterShader)
	{
		sceneDesc.filterShader = PxDefaultSimulationFilterShader; //VehicleFilterShader; //PxDefaultSimulationFilterShader;
	}

	m_pPxScene = m_pPhysics->createScene(sceneDesc);				//Creates a scene 

	PxTolerancesScale toleranceScale;
	toleranceScale.mass = 1000;
	toleranceScale.speed = sceneDesc.gravity.y;
	bool value = toleranceScale.isValid(); // make sure this value is always true

	m_pCooking = PxCreateCooking(PX_PHYSICS_VERSION, *m_pFoundation, PxCookingParams(toleranceScale));
	if (!m_pCooking)
		printf("PxCreateCooking failed!");

	//---------Creating actors-----------]
	/*
	//Creating PhysX material
	PxMaterial* material = m_pPhysics->createMaterial(0.5, 0.5, 0.5); //Creating a PhysX material

	//1-Creating static plane
	PxTransform planePos = PxTransform(PxVec3(0.0f), PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f)));	//Position and orientation(transform) for plane actor
	PxRigidStatic* plane = m_pPhysics->createRigidStatic(planePos);								//Creating rigid static actor
	PxShape *planeShape = plane->createShape(PxPlaneGeometry(), *material);										//Defining geometry for plane actor
	m_pPxScene->addActor(*plane);																		//Adding plane actor to PhysX scene

	//2-Creating dynamic cube
	PxTransform		boxPos(PxVec3(10.0f, 10.0f, 0.0f));												//Position and orientation(transform) for box actor
	PxBoxGeometry	boxGeometry(PxVec3(5, 5, 20));											//Defining geometry for box actor
	PxRigidDynamic *gBox = PxCreateDynamic(*m_pPhysics, boxPos, boxGeometry, *material, 1.0f);		//Creating rigid static actor
	m_pPxScene->addActor(*gBox);	//Adding box actor to PhysX scene

	//Creating dynamic cube
	PxTransform		boxPos1(PxVec3(20.0f, 20.0f, 10.0f));												//Position and orientation(transform) for box actor
	PxBoxGeometry	boxGeometry1(PxVec3(4, 4, 25));											//Defining geometry for box actor
	PxRigidDynamic *gBox1 = PxCreateDynamic(*m_pPhysics, boxPos1, boxGeometry1, *material, 1.0f);		//Creating rigid static actor
	m_pPxScene->addActor(*gBox1);	//Adding box actor to PhysX scene

	PxFixedJointCreate(*m_pPhysics, gBox, boxPos, gBox1, boxPos1);
	PxRevoluteJointCreate(*m_pPhysics, gBox, boxPos, gBox1, boxPos1);


	PxClothParticle vertices[] = {
		PxClothParticle(PxVec3(0.0f, 0.0f, 0.0f), 0.0f),
		PxClothParticle(PxVec3(0.0f, 10.0f, 0.0f), 1.0f),
		PxClothParticle(PxVec3(10.0f, 0.0f, 0.0f), 1.0f),
		PxClothParticle(PxVec3(10.0f, 10.0f, 0.0f), 1.0f)
	};

	PxU32 primitives[] = { 0, 1, 3, 2 };

	PxClothMeshDesc meshDesc;
	meshDesc.points.data = vertices;
	meshDesc.points.count = 4;
	meshDesc.points.stride = sizeof(PxClothParticle);

	meshDesc.invMasses.data = &vertices->invWeight;
	meshDesc.invMasses.count = 4;
	meshDesc.invMasses.stride = sizeof(PxClothParticle);

	meshDesc.quads.data = primitives;
	meshDesc.quads.count = 1;
	meshDesc.quads.stride = sizeof(PxU32) * 4;

	PxClothFabric *fabric = PxClothFabricCreate(*m_pPhysics, meshDesc, PxVec3(0, -1, 0));
	PxTransform pose = PxTransform(PxVec3(0, 10, 0));
	PxCloth *cloth = m_pPhysics->createCloth(pose, *fabric, vertices, PxClothFlags());
	m_pPxScene->addActor(*cloth);
	//cloth->setSolverFrequency(240.0f);
	*/
}

void Engine::ConnectPVD()					//Function for the visualization of PhysX simulation (Optional and 'Debug' mode only) 
{
	// check if PvdConnection manager is available on this platform
	if (m_pPhysics->getPvdConnectionManager() == NULL)
		return;

	// setup connection parameters
	const char*     pvd_host_ip = "127.0.0.1";  // IP of the PC which is running PVD
	int             port = 5425;         // TCP port to connect to, where PVD is listening
	unsigned int    timeout = 100;          // timeout in milliseconds to wait for PVD to respond,
	// consoles and remote PCs need a higher timeout.
	PxVisualDebuggerConnectionFlags connectionFlags = PxVisualDebuggerExt::getAllConnectionFlags();

	// and now try to connect
	debugger::comm::PvdConnection* theConnection = PxVisualDebuggerExt::createConnection(m_pPhysics->getPvdConnectionManager(),
		pvd_host_ip, port, timeout, connectionFlags);
}

bool Engine::UpdateWorld()					//Stepping PhysX
{
	m_pPXTimer->updateTimer();
	float elapsedTime = static_cast<float>(m_pPXTimer->getTimeInterval());
	mAccumulator += elapsedTime;
	if (mAccumulator < 1.0f)
		return false;

	m_pGameWorld->StepPhysX();

	mAccumulator -= m_timeStep;

	m_pPxScene->simulate(m_timeStep);
	m_pPxScene->fetchResults(true);

	return true;
}

void Engine::RunPhysX()
{
	while (m_isInit)
		UpdateWorld();
}

void Engine::Run()
{
	Sleep(5000);
	if (m_isInit)
	{
		m_physXThread = std::thread(&Engine::RunPhysX, this);
		m_physXThread.detach();
		while (/*m_isActive &&*/ m_frame());
	}
}

bool Engine::m_frame()
{
	// обрабатываем событи€ окна
	m_pWindow->RunEvent();
	// если окно неактивно - завершаем кадр
	if (!m_pWindow->IsActive())
		return true;

	// если окно было закрыто, завершаем работу движка
	if (m_pWindow->IsExit())
		return false;

	// если окно изменило размер
	if (m_pWindow->IsResize())
	{
		m_pRender->m_Resize();
	}

	m_pTimer->updateTimer();

	m_pRender->BeginFrame();
	if (!m_pRender->Draw())
		return false;
	m_pRender->EndFrame();

	return true;
}

//Render *Framework::GetRender()
//{
//	return m_pRender;
//}

//PxPhysics *Framework::GetPxPhysics()
//{
//	return m_pPhysics;
//}

//GameWorld *Framework::GetGameWorld()
//{
//	return m_pGameWorld;
//}

//PxScene *Framework::GetPxScene()
//{
//	return m_pPxScene;
//}

Timer *Engine::GetTimer()
{
	return m_pTimer;
}

std::wstring *Engine::GetSystemPath(SYSTEM_PATHS pathTo)
{
	return &m_pPaths[pathTo];
}

std::recursive_mutex *Engine::GetMutex()
{
	return m_pMutex;
}