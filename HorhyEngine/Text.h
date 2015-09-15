#pragma once

#include "BitmapFont.h"

namespace D3D11Framework
{
	class Engine;

	class Text
	{
	public:
		Text(Render *render, BitmapFont *font);

		// text - текст который нужно вывести
		// static - если true, то данный текст нельзя изменить
		// size - максимальное количество букв которые можно вывести. 0 означает что это число равно размеру text
		bool Init(const std::wstring &text, bool statictext=true, int charsize=0);
		void Draw(float r, float g, float b, float x, float y);
		void Close();

		bool SetText(const std::wstring &text);

	private:
		bool m_InitBuffers(const std::wstring &text);
		void m_renderBuffers();
		bool m_updatebuffer(const std::wstring &text);

		Render			*m_pRender;

		BitmapFont		*m_font;
		ID3D11Buffer	*m_vertexBuffer; 
		ID3D11Buffer	*m_indexBuffer;	
		int m_numindex;
		int m_numdrawindex;
		int m_numvertex;

		bool m_static;
		int m_size;
	};
}