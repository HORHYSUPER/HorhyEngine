#pragma once

namespace D3D11Framework
{
	// события мыши
	struct MouseEvent
	{
		MouseEvent(int nx, int ny) :  x(nx), y(ny) {}

		// координаты мыши
		int x;				
		int y;
	};

	// событие нажатия кнопки мыши
	struct MouseEventClick : public MouseEvent
	{
		MouseEventClick(eMouseKeyCodes b, int nx, int ny) : MouseEvent(nx,ny), btn(b) {}

		const eMouseKeyCodes btn;
	};

	// событие прокрутки мыши
	struct MouseEventWheel : public MouseEvent
	{
		MouseEventWheel(int nwheel, int nx, int ny) : MouseEvent(nx,ny), wheel(nwheel) {}

		int wheel;
	};

	// событие клавиши
	struct KeyEvent
	{
		KeyEvent(wchar_t c, eKeyCodes kc) : wc(c), code(kc) {}

		const wchar_t wc;
		const eKeyCodes code;
	};

	class InputListener
	{
	public:
		// если методы возращают true - событие больше никем не обрабатывается

		// кнопка нажата
		virtual bool MousePressed(const MouseEventClick &arg) { return false; }
		// кнопка отпущена
		virtual bool MouseReleased(const MouseEventClick &arg) { return false; }
		// вращение колесика
		virtual bool MouseWheel(const MouseEventWheel &arg) { return false; }
		// движение мыши
		virtual bool MouseMove(const MouseEvent &arg) {return false; }

		// кнопка нажата
		virtual bool KeyPressed(const KeyEvent &arg) { return false; }
		// кнопка отпущена
		virtual bool KeyReleased(const KeyEvent &arg) { return false; }
	};}