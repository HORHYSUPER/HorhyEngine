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

inline bool HasStencil(DXGI_FORMAT texFormat)
{
	return((texFormat == DXGI_FORMAT_D24_UNORM_S8_UINT) ||
		(texFormat == DXGI_FORMAT_D32_FLOAT_S8X24_UINT));
}

inline bool IsDepthFormat(DXGI_FORMAT texFormat)
{
	return((texFormat == DXGI_FORMAT_D16_UNORM) || (texFormat == DXGI_FORMAT_D24_UNORM_S8_UINT) ||
		(texFormat == DXGI_FORMAT_D32_FLOAT) ||	(texFormat == DXGI_FORMAT_D32_FLOAT_S8X24_UINT));
}

inline DXGI_FORMAT GetDepthResourceFormat(DXGI_FORMAT depthformat)
{
	DXGI_FORMAT resformat;
	switch (depthformat)
	{
	case DXGI_FORMAT::DXGI_FORMAT_D16_UNORM:
		resformat = DXGI_FORMAT::DXGI_FORMAT_R16_TYPELESS;
		break;
	case DXGI_FORMAT::DXGI_FORMAT_D24_UNORM_S8_UINT:
		resformat = DXGI_FORMAT::DXGI_FORMAT_R24G8_TYPELESS;
		break;
	case DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT:
		resformat = DXGI_FORMAT::DXGI_FORMAT_R32_TYPELESS;
		break;
	case DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		resformat = DXGI_FORMAT::DXGI_FORMAT_R32G8X24_TYPELESS;
		break;
	default:
		resformat = depthformat;
		break;
	}

	return resformat;
}

inline DXGI_FORMAT GetDepthSRVFormat(DXGI_FORMAT depthformat)
{
	DXGI_FORMAT srvformat;
	switch (depthformat)
	{
	case DXGI_FORMAT::DXGI_FORMAT_D16_UNORM:
		srvformat = DXGI_FORMAT::DXGI_FORMAT_R16_FLOAT;
		break;
	case DXGI_FORMAT::DXGI_FORMAT_D24_UNORM_S8_UINT:
		srvformat = DXGI_FORMAT::DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		break;
	case DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT:
		srvformat = DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;
		break;
	case DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		srvformat = DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
		break;
	default:
		srvformat = depthformat;
		break;
	}
	return srvformat;
}