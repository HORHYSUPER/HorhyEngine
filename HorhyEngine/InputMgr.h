#pragma once

#include "InputCodes.h"

namespace D3D11Framework
{
	class InputListener;

	class InputMgr
	{
	public:
		void Init();
		void Close();

		void Run(const UINT &msg, WPARAM wParam, LPARAM lParam);

		void AddListener(InputListener *Listener);

		// зона окна
		void SetWinRect(const RECT &winrect);

		int	GetCursorX();
		int	GetCursorY();

	private:
		// событие движения мыши
		void m_eventcursor();
		// событие кнопки мыши
		void m_eventmouse(const eMouseKeyCodes KeyCode, bool press);
		// событие вращения колесика
		void m_mousewheel(short Value);
		// обработка события клавиши
		void m_eventkey(const eKeyCodes KeyCode, const wchar_t ch, bool press);

		std::list<InputListener*> m_Listener;

		RECT m_windowrect;
		int m_curx;
		int m_cury;
		int m_MouseWheel;
	};
}