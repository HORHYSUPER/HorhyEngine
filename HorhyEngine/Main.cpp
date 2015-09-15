#include "stdafx.h"
#include "MyRender.h"
#include "GameWorld.h"
#include "Camera.h"
#include "Scene.h"
#include "Engine.h"
#include "ILight.h"

#include "D3D11_Framework.h"

using namespace D3D11Framework;

Engine framework;
MyRender *render = new MyRender();

#define MOUSE_SPEED 0.25f
int camSpeed = 5;
bool camControl = true;

class MyInput : public InputListener
{
public:
	float lastX, lastY, forwardSpeed, strafeSpeed;
	bool m_camFixed = false, m_forwardSpeedForced, m_strafeSpeedForced;

	bool KeyPressed(const KeyEvent &arg)
	{
		switch (arg.code)
		{
		case KEY_UP:
			if (camControl)
			{
				render->m_upcam = true;
			}
			else
			{
				framework.m_pGameWorld->m_upcam = true;
			}
			break;
		case KEY_DOWN:
			if (camControl)
			{
				render->m_downcam = true;
			}
			else
			{
				framework.m_pGameWorld->m_downcam = true;
			}
			break;
		case KEY_LEFT:
			if (camControl)
			{
				render->m_leftcam = true;
			}
			else
			{
				framework.m_pGameWorld->m_leftcam = true;
			}
			break;
		case KEY_RIGHT:
			if (camControl)
			{
				render->m_rightcam = true;
			}
			else
			{
				framework.m_pGameWorld->m_rightcam = true;
			}
			break;
		case KEY_SHIFT:
			m_forwardSpeedForced = true;
			m_strafeSpeedForced = true;
			break;
		case KEY_W:
			if (m_forwardSpeedForced == true)
			{
				forwardSpeed = camSpeed * 10;
			}
			else
			{
				forwardSpeed = camSpeed;
			}
			render->GetCamera()->SetForwardSpeed(forwardSpeed);
			break;
		case KEY_S:
			if (m_forwardSpeedForced == true)
			{
				forwardSpeed = -camSpeed * 10;
			}
			else
			{
				forwardSpeed = -camSpeed;
			}
			render->GetCamera()->SetForwardSpeed(forwardSpeed);
			break;

		case KEY_A:
			if (m_strafeSpeedForced == true)
			{
				strafeSpeed = -camSpeed * 10;
			}
			else
			{
				strafeSpeed = -camSpeed;
			}
			render->GetCamera()->SetStrafeSpeed(strafeSpeed);
			break;

		case KEY_D:
			if (m_strafeSpeedForced == true)
			{
				strafeSpeed = camSpeed * 10;
			}
			else
			{
				strafeSpeed = camSpeed;
			}
			render->GetCamera()->SetStrafeSpeed(strafeSpeed);
			break;

		default:
			break;
		}

		float force = 100;
		if (arg.wc == 'i') {
			framework.m_pGameWorld->GetSceneByIndex(0)->ApplyForce(-1, PxVec3(force, 0, 0));
		}
		if (arg.wc == 'k') {
			framework.m_pGameWorld->GetSceneByIndex(0)->ApplyForce(-1, PxVec3(-force, 0, 0));
		}
		if (arg.wc == 'j') {
			framework.m_pGameWorld->GetSceneByIndex(0)->ApplyForce(-1, PxVec3(0, 0, force));
		}
		if (arg.wc == 'l') {
			framework.m_pGameWorld->GetSceneByIndex(0)->ApplyForce(-1, PxVec3(0, 0, -force));
		}
		if (arg.wc == 'u') {
			framework.m_pGameWorld->GetSceneByIndex(0)->ApplyForce(-1, PxVec3(0, force, 0));
		}
		if (arg.wc == 'o') {
			framework.m_pGameWorld->GetSceneByIndex(0)->ApplyForce(-1, PxVec3(0, -force, 0));
		}

		return false;
	}

	bool KeyReleased(const KeyEvent &arg)
	{
		switch (arg.code)
		{
		case KEY_F1:
			render->GetCamera()->SetPhysFixation(true);
			break;
		case KEY_F2:
			render->GetCamera()->SetPhysFixation(false);
			break;
		case KEY_F3:
			render->GetCamera()->SetDirectionalCamera(true);
			break;
		case KEY_F5:
			camControl = camControl == true ? false : true;
			break;
		case KEY_F9:
			render->vxgi = render->vxgi == true ? false : true;
			break;
		case KEY_F11:
			render->m_isWireFrameRender = render->m_isWireFrameRender == true ? false : true;
			break;
		case KEY_UP:
			if (camControl)
			{
				render->m_upcam = false;
			}
			else
			{
				framework.m_pGameWorld->m_upcam = false;
			}
			break;
		case KEY_DOWN:
			if (camControl)
			{
				render->m_downcam = false;
			}
			else
			{
				framework.m_pGameWorld->m_downcam = false;
			}
			break;
		case KEY_LEFT:
			if (camControl)
			{
				render->m_leftcam = false;
			}
			else
			{
				framework.m_pGameWorld->m_leftcam = false;
			}
			break;
		case KEY_RIGHT:
			if (camControl)
			{
				render->m_rightcam = false;
			}
			else
			{
				framework.m_pGameWorld->m_rightcam = false;
			}
			break;
		case KEY_SHIFT:
			m_forwardSpeedForced = false;
			m_strafeSpeedForced = false;
			break;
		case KEY_W:
			forwardSpeed = 0;
			render->GetCamera()->SetForwardSpeed(forwardSpeed);
			break;
		case KEY_S:
			forwardSpeed = 0;
			render->GetCamera()->SetForwardSpeed(forwardSpeed);
			break;
		case KEY_A:
			strafeSpeed = 0;
			render->GetCamera()->SetStrafeSpeed(strafeSpeed);
			break;
		case KEY_D:
			strafeSpeed = 0;
			render->GetCamera()->SetStrafeSpeed(strafeSpeed);
			break;
		default:
			break;
		}

		return true;
	}

	bool MousePressed(const MouseEventClick &arg)
	{
		if (arg.btn == KEY_RBUTTON)
		{
			m_camFixed = true;
		}
		return false;
	}

	bool MouseReleased(const MouseEventClick &arg)
	{
		if (arg.btn == KEY_RBUTTON)
		{
			m_camFixed = false;
		}
		return true;
	}

	bool MouseMove(const MouseEvent &arg)
	{
		if (m_camFixed)
		{
			render->GetCamera()->SetRotation(
				render->GetCamera()->GetRotation().m128_f32[0] + (arg.x - lastX) * MOUSE_SPEED,
				render->GetCamera()->GetRotation().m128_f32[1] + (arg.y - lastY) * MOUSE_SPEED,
				render->GetCamera()->GetRotation().m128_f32[2]);
		}
		lastX = arg.x;
		lastY = arg.y;
		//cout << render->GetCamera()->GetPosition().m128_f32[0] << ";  " << render->GetCamera()->GetPosition().m128_f32[1] << ";  " << render->GetCamera()->GetPosition().m128_f32[2] << endl;

		return false;
	}
};

void commands()
{
	string command;
	do
	{
		cin >> command;

		if (strncmp(command.c_str(), "camPos", 6) == 0)
		{
			string camPos = command.substr(strchr(command.c_str(), '=') - command.c_str() + 1, command.length());
			string tmpCamPos;
			float x, y, z;
			tmpCamPos = camPos.substr(0, strchr(camPos.c_str(), ';') - camPos.c_str()).c_str();
			x = atof(tmpCamPos.c_str());
			tmpCamPos = camPos.substr(tmpCamPos.length() + 1, camPos.length()).c_str();
			y = atof(tmpCamPos.substr(0, strchr(tmpCamPos.c_str(), ';') - tmpCamPos.c_str()).c_str());
			tmpCamPos = tmpCamPos.substr(strchr(tmpCamPos.c_str(), ';') - tmpCamPos.c_str() + 1, tmpCamPos.length()).c_str();
			z = atof(tmpCamPos.c_str());

			render->GetCamera()->SetPosition(x, y, z);
		}
		else if (strncmp(command.c_str(), "lightPos", 8) == 0)
		{
			string lightPos = command.substr(strchr(command.c_str(), '=') - command.c_str() + 1, command.length());
			string tmplightPos;
			float x, y, z;
			tmplightPos = lightPos.substr(0, strchr(lightPos.c_str(), ';') - lightPos.c_str()).c_str();
			x = atof(tmplightPos.c_str());
			tmplightPos = lightPos.substr(tmplightPos.length() + 1, lightPos.length()).c_str();
			y = atof(tmplightPos.substr(0, strchr(tmplightPos.c_str(), ';') - tmplightPos.c_str()).c_str());
			tmplightPos = tmplightPos.substr(strchr(tmplightPos.c_str(), ';') - tmplightPos.c_str() + 1, tmplightPos.length()).c_str();
			z = atof(tmplightPos.c_str());

			framework.m_pGameWorld->GetLight(0)->SetPosition(x, y, z);
		}
		else if (strncmp(command.c_str(), "coneSlope", 9) == 0)
		{
			string coneSlope = command.substr(strchr(command.c_str(), '=') - command.c_str() + 1, command.length());
			framework.GetRender()->coneSlope = atof(coneSlope.c_str());
		}
		else if (strncmp(command.c_str(), "lightDir", 8) == 0)
		{
			string lightDir = command.substr(strchr(command.c_str(), '=') - command.c_str() + 1, command.length());
			string tmplightDir;
			float x, y, z;
			tmplightDir = lightDir.substr(0, strchr(lightDir.c_str(), ';') - lightDir.c_str()).c_str();
			x = atof(tmplightDir.c_str());
			tmplightDir = lightDir.substr(tmplightDir.length() + 1, lightDir.length()).c_str();
			y = atof(tmplightDir.substr(0, strchr(tmplightDir.c_str(), ';') - tmplightDir.c_str()).c_str());
			tmplightDir = tmplightDir.substr(strchr(tmplightDir.c_str(), ';') - tmplightDir.c_str() + 1, tmplightDir.length()).c_str();
			z = atof(tmplightDir.c_str());

			framework.m_pGameWorld->GetLight(0)->SetRotation(x, y, z);
		}
		else if (strncmp(command.c_str(), "coneSlope", 9) == 0)
		{
			string coneSlope = command.substr(strchr(command.c_str(), '=') - command.c_str() + 1, command.length());
			framework.GetRender()->coneSlope = atof(coneSlope.c_str());
		}
	} while (true);

	return;
};

int main()
{
	MyInput *input = new MyInput();

	std::thread commandParser;
	commandParser = std::thread(commands);
	commandParser.detach();

	FrameworkDesc desc;
	desc.render = render;

	framework.Initialize(desc);

	framework.AddInputListener(input);

	framework.Run();

	return 0;
}
