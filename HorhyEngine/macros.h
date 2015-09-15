#pragma once

#define SAFE_CLOSE(p)		{ if(p){(p)->Close(); delete (p); (p)=NULL;} }
#define _MUTEXLOCK(p)	{ if(p){(p)->lock();} }
#define _MUTEXUNLOCK(p)	{ if(p){(p)->unlock();} }

#define SAFE_DELETE(x) { if(x) delete (x); (x) = NULL; }
#define SAFE_DELETE_ARRAY(x) { if(x) delete [] (x); (x) = NULL; }

// safe delete LIST of pointers
#define SAFE_DELETE_PLIST(x) { for(unsigned int i=0; i<(x).size(); i++) { SAFE_DELETE((x)[i]) } (x).erase(x.begin(), x.end()); }

#define SAFE_RELEASE(x) { if(x) (x)->Release(); (x) = NULL; }

#define DEMO_MAX_STRING 256 // max length of a string
#define DEMO_MAX_FILENAME 512 // max length of file name
#define MAX_FILEPATH 4096 // max length of file path

#define EPSILON 0.0001f
#define IS_EQUAL(a, b) (((a) >= ((b)-EPSILON)) && ((a) <= ((b)+EPSILON)))

// game configs
#define SCREEN_WIDTH 1024// 1920, 1280
#define SCREEN_HEIGHT 1024// 1080, 720
#define MSAA_COUNT 1 // 1 - Off msaa, also can be 2,4,8
#define VSYNC_ENABLED 0
#define TESSELLATION_ENABLED TRUE
#define VXGI_ENABLED TRUE
#define VOXELIZE_DIMENSION 256
#define VOXELIZE_SIZE 32.0f
