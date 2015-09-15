#include "stdafx.h"
#include "InputMgr.h"
#include "InputCodes.h"
#include "InputListener.h"
#include "Log.h"

namespace D3D11Framework
{
	void InputMgr::Init()
	{
		m_MouseWheel = m_curx = m_cury = 0;
		Log::Get()->Debug("InputMgr init");
	}

	void InputMgr::Close()
	{
		if (!m_Listener.empty())
			m_Listener.clear();
		Log::Get()->Debug("InputMgr close");
	}

	void InputMgr::SetWinRect(const RECT &winrect)
	{
		m_windowrect.left = winrect.left;
		m_windowrect.right = winrect.right;
		m_windowrect.top = winrect.top;
		m_windowrect.bottom = winrect.bottom;
	}

	void InputMgr::AddListener(InputListener *Listener)
	{
		m_Listener.push_back(Listener);
	}

	void InputMgr::Run(const UINT &msg, WPARAM wParam, LPARAM lParam)
	{
		if (m_Listener.empty())
			return;

		eKeyCodes KeyIndex;
		wchar_t buffer[1];
		BYTE lpKeyState[256];

		m_eventcursor();// событие движения мыши
		switch (msg)
		{
		case WM_KEYDOWN:
			KeyIndex = static_cast<eKeyCodes>(wParam);
			GetKeyboardState(lpKeyState);
			ToUnicode(wParam, HIWORD(lParam) & 0xFF, lpKeyState, buffer, 1, 0);
			m_eventkey(KeyIndex, buffer[0], true);
			break;
		case WM_KEYUP:
			KeyIndex = static_cast<eKeyCodes>(wParam);
			GetKeyboardState(lpKeyState);
			ToUnicode(wParam, HIWORD(lParam) & 0xFF, lpKeyState, buffer, 1, 0);
			m_eventkey(KeyIndex, buffer[0], false);
			break;
		case WM_LBUTTONDOWN:
			m_eventmouse(MOUSE_LEFT, true);
			break;
		case WM_LBUTTONUP:
			m_eventmouse(MOUSE_LEFT, false);
			break;
		case WM_RBUTTONDOWN:
			m_eventmouse(MOUSE_RIGHT, true);
			break;
		case WM_RBUTTONUP:
			m_eventmouse(MOUSE_RIGHT, false);
			break;
		case WM_MBUTTONDOWN:
			m_eventmouse(MOUSE_MIDDLE, true);
			break;
		case WM_MBUTTONUP:
			m_eventmouse(MOUSE_MIDDLE, false);
			break;
		case WM_MOUSEWHEEL:
			m_mousewheel((short)GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA);
			break;
		}
	}

	void InputMgr::m_eventcursor()
	{
		POINT Position;
		GetCursorPos(&Position);

		Position.x -= m_windowrect.left;
		Position.y -= m_windowrect.top;

		if (m_curx == Position.x && m_cury == Position.y)
			return;

		m_curx = Position.x;
		m_cury = Position.y;

		for (auto it = m_Listener.begin(); it != m_Listener.end(); ++it)
		{
			if (!(*it))
				continue;
			else if ((*it)->MouseMove(MouseEvent(m_curx, m_cury)) == true)
				return;
		}
	}

	void InputMgr::m_eventmouse(const eMouseKeyCodes Code, bool press)
	{
		for (auto it = m_Listener.begin(); it != m_Listener.end(); ++it)
		{
			if (!(*it))
				continue;
			// кнопка нажата
			if (press == true)
			{
				if ((*it)->MousePressed(MouseEventClick(Code, m_curx, m_cury)) == true)
					return;
			}
			// кнопка отпущена
			else
			{
				if ((*it)->MouseReleased(MouseEventClick(Code, m_curx, m_cury)) == true)
					return;
			}
		}
	}

	void InputMgr::m_mousewheel(short Value)
	{
		if (m_MouseWheel == Value)
			return;

		m_MouseWheel = Value;

		for (auto it = m_Listener.begin(); it != m_Listener.end(); ++it)
		{
			if (!(*it))
				continue;
			else if ((*it)->MouseWheel(MouseEventWheel(m_MouseWheel, m_curx, m_cury)) == true)
				return;
		}
	}

	void InputMgr::m_eventkey(const eKeyCodes KeyCode, const wchar_t ch, bool press)
	{
		for (auto it = m_Listener.begin(); it != m_Listener.end(); ++it)
		{
			if (!(*it))
				continue;
			// кнопка нажата
			if (press == true)
			{
				if ((*it)->KeyPressed(KeyEvent(ch, KeyCode)) == true)
					return;
			}
			// кнопка отпущена
			else
			{
				if ((*it)->KeyReleased(KeyEvent(ch, KeyCode)) == true)
					return;
			}
		}
	}

	int InputMgr::GetCursorX()
	{
		return m_curx;
	}

	int InputMgr::GetCursorY()
	{
		return m_cury;
	}
}