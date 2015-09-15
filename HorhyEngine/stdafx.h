#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_WCSTOK

#include <clocale>
#include <ctime>
#include <sstream>
#include <string>
#include <iostream>
#include <io.h>
#include <fstream>
#include <list>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <stdio.h>
#include <assert.h>
#include <type_traits>
#include <sys/stat.h>
#include <comdef.h>
#include <wrl.h>

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"
#include <fbxsdk.h>
#include <PxPhysicsAPI.h>
#include "pugixml.hpp"

static const UINT FrameCount = 3;
#define NUM_UNIFORM_BUFFER_BP        4  // number of uniform-buffer binding points, used in this demo
#define NUM_CUSTOM_UNIFORM_BUFFER_BP 2  // number of custom uniform-buffer binding points, used in this demo
#define NUM_TEXTURE_BP             	 10 // number of texture binding points, used in this demo
#define NUM_STRUCTURED_BUFFER_BP   	 2  // number of structured-buffer binding points, used in this demo
#define NUM_SHADER_TYPES           	 6  // number of shader types, used in this demo

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "dxguid.lib")

#ifdef _DEBUG
#ifdef _WIN64
#pragma comment(lib, "PhysX3DEBUG_x64.lib")				//Always be needed
#pragma comment(lib, "PhysX3CommonDEBUG_x64.lib")		//Always be needed
#pragma comment(lib, "PhysX3CookingDEBUG_x64.lib")
#pragma comment(lib, "PhysX3CharacterKinematicDEBUG_x64.lib")
//#pragma comment(lib, "pugixmld_x64.lib")
#else
#pragma comment(lib, "PhysX3DEBUG_x86.lib")				//Always be needed
#pragma comment(lib, "PhysX3CommonDEBUG_x86.lib")		//Always be needed
#pragma comment(lib, "PhysX3CookingDEBUG_x86.lib")
#pragma comment(lib, "pugixmld_x86.lib")
#endif
#pragma comment(lib, "libfbxsdk-md.lib")
#pragma comment(lib, "PhysX3ExtensionsDEBUG.lib")		//PhysX extended library 
#pragma comment(lib, "PhysXVisualDebuggerSDKDEBUG.lib") 
#else
#ifdef _WIN64
#pragma comment(lib, "PhysX3_x64.lib")				//Always be needed
#pragma comment(lib, "PhysX3Common_x64.lib")		//Always be needed
#pragma comment(lib, "PhysX3Cooking_x64.lib")
#pragma comment(lib, "PhysX3CharacterKinematic_x64.lib")
#pragma comment(lib, "pugixml_x64.lib")
#else
#pragma comment(lib, "PhysX3_x86.lib")				//Always be needed
#pragma comment(lib, "PhysX3Common_x86.lib")		//Always be needed
#pragma comment(lib, "PhysX3Cooking_x86.lib")
#pragma comment(lib, "pugixml_x86.lib")
#endif
#pragma comment(lib, "d3dx11.lib")
#pragma comment(lib, "libfbxsdk-md.lib")
#pragma comment(lib, "PhysX3Extensions.lib")
#pragma comment(lib, "PhysXVisualDebuggerSDK.lib")
#endif