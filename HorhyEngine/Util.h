#pragma once

inline wchar_t* ToWChar(char *mbString)
{ 
	int len = 0; 
	len = (int)strlen(mbString) + 1; 
	wchar_t *ucString = new wchar_t[len]; 
	mbstowcs(ucString, mbString, len); 
	return ucString; 
}

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw std::runtime_error("HRESULT is failed value.");
	}
}

inline DXGI_FORMAT StringToDXGIFormat(std::string format)
{
	if (format.compare("float4") == 0)
		return DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT;
	else if(format.compare("float3") == 0)
		return DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT;
	else if (format.compare("float2") == 0)
		return DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT;
	else if (format.compare("float") == 0)
		return DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;

	return DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
}