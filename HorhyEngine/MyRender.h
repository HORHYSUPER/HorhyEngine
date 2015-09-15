#pragma once

#include "D3D11_Framework.h"

using namespace D3D11Framework;

class MyInput;

class MyRender : public Render
{
public:
	MyRender();
	bool Init(HWND hwnd);
	//bool Draw();
	void Close();

private:
	friend MyInput;
	Scene *m_scene;
	XMMATRIX m_view;
	ID3D11BlendState* Transparency;
	unsigned short nScenes;
};