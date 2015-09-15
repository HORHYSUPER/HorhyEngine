#include "stdafx.h"
#include "Render.h"
#include "Camera.h"
#include "macros.h"
#include "Engine.h"
//#include "Util.h"
#include "Buffer.h"
#include "BitmapFont.h"

using namespace D3D11Framework;
using namespace DirectX;
using namespace std;

inline wchar_t* CharToWChar(char *mbString)
{
	int len = 0;
	len = (int)strlen(mbString) + 1;
	wchar_t *ucString = new wchar_t[len];
	mbstowcs(ucString, mbString, len);
	return ucString;
}
BitmapFont::BitmapFont(Render *render)
{
	m_pRender = render;
	//m_pConstantBuffer = nullptr;
	//m_pPixelBuffer = nullptr;
	//m_pShader = nullptr;
	m_WidthTex = 0;
	m_HeightTex = 0;
}

bool BitmapFont::Init(char *fontFilename)
{
	if (!m_parse(fontFilename))
		return false;

	//m_pShader = new Shader();
	//if (!m_pShader)
	//	return false;

	HRESULT hr = S_OK;
	//TexMetadata info;
	//unique_ptr<ScratchImage> image(new ScratchImage);
	//hr = LoadFromTGAFile(m_file.c_str(), &info, *image);
	//if (FAILED(hr))
	//{
	//	Log::Get()->Err("Ќе удалось загрузить текстуру %ls", m_file.c_str());
	//	return false;
	//}

	//hr = CreateShaderResourceView(m_pRender->GetDevice(), image.get()->GetImages(), image.get()->GetImageCount(), info, &m_pTexture);
	//if (FAILED(hr))
	//{
	//	Log::Get()->Err("Ќе удалось создать srv %ls", m_file.c_str());
	//	return false;
	//}

	D3D11_SAMPLER_DESC samplerDesc;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samplerDesc.BorderColor[0] = 0;
	samplerDesc.BorderColor[1] = 0;
	samplerDesc.BorderColor[2] = 0;
	samplerDesc.BorderColor[3] = 0;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	//hr = m_pRender->GetDevice()->CreateSamplerState(&samplerDesc, &m_pSampleState);
	if (FAILED(hr))
	{
		Log::Get()->Err("Ќе удалось создать sample state");
		return false;
	}

	//std::wstring shaderName;
	//shaderName = *m_pRender->GetFramework()->GetSystemPath(PATH_TO_SHADERS) + L"BitmapFont.hlsl";

	//m_pShader->AddInputElementDesc("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT);
	//m_pShader->AddInputElementDesc("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT);

	//if (!m_pShader->CreateShaderVs((WCHAR*)shaderName.c_str(), NULL, "VS"))
	//	return false;

	//if (!m_pShader->CreateShaderPs((WCHAR*)shaderName.c_str(), NULL, "PS"))
	//	return false;
	//
	//m_pConstantBuffer = Buffer::CreateConstantBuffer(m_pRender->GetDevice(), sizeof(ConstantBuffer), false, NULL);
	//if (!m_pConstantBuffer)
	//	return false;

	//m_pPixelBuffer = Buffer::CreateConstantBuffer(m_pRender->GetDevice(), sizeof(PixelBufferType), false, NULL);
	//if (!m_pPixelBuffer)
	//	return false;

	return true;
}

bool BitmapFont::m_parse(char *fontFilename)
{
	ifstream fin;
	fin.open(fontFilename);
	if (fin.fail())
		return false;

	string Line;
	string Read, Key, Value;
	size_t i;
	while (!fin.eof())
	{
		std::stringstream LineStream;
		std::getline(fin, Line);
		LineStream << Line;

		LineStream >> Read;
		if (Read == "common")
		{
			while (!LineStream.eof())
			{
				std::stringstream Converter;
				LineStream >> Read;
				i = Read.find('=');
				Key = Read.substr(0, i);
				Value = Read.substr(i + 1);

				Converter << Value;
				if (Key == "scaleW")
					Converter >> m_WidthTex;
				else if (Key == "scaleH")
					Converter >> m_HeightTex;
			}
		}
		else if (Read == "page")
		{
			while (!LineStream.eof())
			{
				std::stringstream Converter;
				LineStream >> Read;
				i = Read.find('=');
				Key = Read.substr(0, i);
				Value = Read.substr(i + 1);

				std::string str;
				Converter << Value;
				if (Key == "file")
				{
					Converter >> str;
					wchar_t *name = CharToWChar((char*)str.substr(1, Value.length() - 2).c_str());
					m_file = name;
					SAFE_DELETE_ARRAY(name);
				}
			}
		}
		else if (Read == "char")
		{
			unsigned short CharID = 0;
			CharDesc chard;

			while (!LineStream.eof())
			{
				std::stringstream Converter;
				LineStream >> Read;
				i = Read.find('=');
				Key = Read.substr(0, i);
				Value = Read.substr(i + 1);

				Converter << Value;
				if (Key == "id")
					Converter >> CharID;
				else if (Key == "x")
					Converter >> chard.srcX;
				else if (Key == "y")
					Converter >> chard.srcY;
				else if (Key == "width")
					Converter >> chard.srcW;
				else if (Key == "height")
					Converter >> chard.srcH;
				else if (Key == "xoffset")
					Converter >> chard.xOff;
				else if (Key == "yoffset")
					Converter >> chard.yOff;
				else if (Key == "xadvance")
					Converter >> chard.xAdv;
			}
			m_Chars.insert(std::pair<int, CharDesc>(CharID, chard));
		}
	}

	fin.close();

	return true;
}

void BitmapFont::BuildVertexArray(VertexFont *vertex, int numvert, const wchar_t *sentence)
{
	int numLetters = (int)wcslen(sentence);
	// следим чтобы число букв не было больше числа вершин
	if (numLetters * 4 > numvert)
		numLetters = numvert / 4;

	float drawX = (float)m_pRender->GetScreenWidth() / 2 * -1;
	float drawY = (float)m_pRender->GetScreenHeight() / 2;

	int index = 0;
	for (int i = 0; i < numLetters; i++)
	{
		float CharX = m_Chars[sentence[i]].srcX;
		float CharY = m_Chars[sentence[i]].srcY;
		float Width = m_Chars[sentence[i]].srcW;
		float Height = m_Chars[sentence[i]].srcH;
		float OffsetX = m_Chars[sentence[i]].xOff;
		float OffsetY = m_Chars[sentence[i]].yOff;

		float left = drawX + OffsetX;
		float right = left + Width;
		float top = drawY - OffsetY;
		float bottom = top - Height;
		float lefttex = CharX / m_WidthTex;
		float righttex = (CharX + Width) / m_WidthTex;
		float toptex = CharY / m_HeightTex;
		float bottomtex = (CharY + Height) / m_HeightTex;

		vertex[index].pos = XMFLOAT3(left, top, 0.0f);
		vertex[index].tex = XMFLOAT2(lefttex, toptex);
		index++;
		vertex[index].pos = XMFLOAT3(right, bottom, 0.0f);
		vertex[index].tex = XMFLOAT2(righttex, bottomtex);
		index++;
		vertex[index].pos = XMFLOAT3(left, bottom, 0.0f);
		vertex[index].tex = XMFLOAT2(lefttex, bottomtex);
		index++;
		vertex[index].pos = XMFLOAT3(right, top, 0.0f);
		vertex[index].tex = XMFLOAT2(righttex, toptex);
		index++;

		drawX += m_Chars[sentence[i]].xAdv;
	}
}

void BitmapFont::Draw(unsigned int index, float r, float g, float b, float x, float y)
{
	m_SetShaderParameters(r, g, b, x, y);

	//_MUTEXLOCK(m_pRender->GetFramework()->GetMutex());
	//m_pRender->GetDeviceContext()->IASetInputLayout(m_pShader->GetInputLayout());
	//m_pRender->GetDeviceContext()->VSSetShader(m_pShader->GetVertexShader(0), NULL, 0);
	//m_pRender->GetDeviceContext()->PSSetShader(m_pShader->GetPixelShader(0), NULL, 0);
	//m_pRender->GetDeviceContext()->PSSetShaderResources(0, 1, &m_pTexture);
	//m_pRender->GetDeviceContext()->PSSetSamplers(0, 1, &m_pSampleState);
	//m_pRender->GetDeviceContext()->DrawIndexed(index, 0, 0);
	//_MUTEXUNLOCK(m_pRender->GetFramework()->GetMutex());
}

void BitmapFont::m_SetShaderParameters(float r, float g, float b, float x, float y)
{
	//XMMATRIX objmatrix = XMMatrixTranslation(x, -y, 0);
	//XMMATRIX wvp = objmatrix * m_pRender->GetCamera()->GetOrthoMatrix();
	//ConstantBuffer cb;
	//cb.WVP = XMMatrixTranspose(wvp);
	//_MUTEXLOCK(m_pRender->GetFramework()->GetMutex());
	//m_pRender->GetDeviceContext()->UpdateSubresource(m_pConstantBuffer, 0, NULL, &cb, 0, 0);
	//m_pRender->GetDeviceContext()->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
	//_MUTEXUNLOCK(m_pRender->GetFramework()->GetMutex());

	//PixelBufferType pb;
	//pb.pixelColor = XMFLOAT4(r, g, b, 1.0f);
	//_MUTEXLOCK(m_pRender->GetFramework()->GetMutex());
	//m_pRender->GetDeviceContext()->UpdateSubresource(m_pPixelBuffer, 0, NULL, &pb, 0, 0);
	//m_pRender->GetDeviceContext()->PSSetConstantBuffers(0, 1, &m_pPixelBuffer);
	//_MUTEXUNLOCK(m_pRender->GetFramework()->GetMutex());
}

void BitmapFont::Close()
{
	//SAFE_RELEASE(m_pConstantBuffer);
	//SAFE_RELEASE(m_pPixelBuffer);
	//SAFE_CLOSE(m_pShader);
}