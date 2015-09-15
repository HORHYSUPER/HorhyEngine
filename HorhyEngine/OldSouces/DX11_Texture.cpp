#include "stdafx.h"
#include "DX11_Texture.h"
#include "Engine.h"
#include "Render.h"
#include "DX11_RenderTarget.h"
#include "Util.h"

using namespace D3D11Framework;

void DX11_Texture::Release()
{
	SAFE_RELEASE(texture);
	SAFE_RELEASE(shaderResourceView);
	SAFE_RELEASE(unorderedAccessView);
}

bool DX11_Texture::LoadFromFile(const char *fileName)
{
	strcpy(name, fileName);
	char filePath[MAX_FILEPATH];
	//if (!Demo::fileManager->GetFilePath(fileName, filePath))
	//	return false;

	HRESULT hr = S_OK;
	TexMetadata info;
	unique_ptr<ScratchImage> image(new ScratchImage);
	WCHAR ext[_MAX_EXT];
	_wsplitpath_s(ToWChar((char*)fileName), nullptr, 0, nullptr, 0, nullptr, 0, ext, _MAX_EXT);
	if (_wcsicmp(ext, L".dds") == 0)
	{
		hr = LoadFromDDSFile(ToWChar((char*)fileName), DDS_FLAGS_NONE, &info, *image);
	}
	else if (_wcsicmp(ext, L".tga") == 0)
	{
		hr = LoadFromTGAFile(ToWChar((char*)fileName), &info, *image);
	}
	else
	{
		//hr = LoadFromWICFile(ToWChar((char*)fileName), WIC_FLAGS_NONE, &info, *image);
	}

	if (SUCCEEDED(hr))
	{

		hr = CreateShaderResourceView(Engine::GetRender()->GetDevice(), image.get()->GetImages(), image.get()->GetImageCount(), info, &shaderResourceView);
		if (FAILED(hr))
		{
			Log::Get()->Err("Не удалось создать srv %ls", fileName);
			return false;
		}
	}

	return true;
}

bool DX11_Texture::CreateRenderable(unsigned int width, unsigned int height, unsigned int depth, DXGI_FORMAT format,
	unsigned int sampleCountMSAA, unsigned int numLevels, unsigned int rtFlags)
{
	strcpy(name, "renderTargetTexture");
	const bool useUAV = rtFlags & UAV_RTF;
	const bool texture3D = rtFlags & TEXTURE_3D_RTF;
	this->numLevels = numLevels;
	bool typeless = (format == DXGI_FORMAT_R24_UNORM_X8_TYPELESS) || (rtFlags & SRGB_RTF);
	DXGI_FORMAT textureFormat = GetDX11TexFormat(format, typeless);
	if (textureFormat == DXGI_FORMAT_UNKNOWN)
		return false;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = format;

	if (!texture3D)
	{
		D3D11_TEXTURE2D_DESC textureDesc;
		ZeroMemory(&textureDesc, sizeof(textureDesc));
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.ArraySize = depth;
		textureDesc.MipLevels = numLevels;
		textureDesc.Format = textureFormat;
		textureDesc.SampleDesc.Count = sampleCountMSAA;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		if (format != DXGI_FORMAT_R24_UNORM_X8_TYPELESS)
			textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		else
			textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
		if (useUAV)
			textureDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		textureDesc.CPUAccessFlags = 0;
		textureDesc.MiscFlags = (numLevels == 1) ? 0 : D3D11_RESOURCE_MISC_GENERATE_MIPS;
		if (Engine::GetRender()->GetDevice()->CreateTexture2D(&textureDesc, NULL, (ID3D11Texture2D**)&texture) != S_OK)
			return false;

		if (depth == 1)
		{
			srvDesc.ViewDimension = (sampleCountMSAA > 1) ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = numLevels;
			srvDesc.Texture2D.MostDetailedMip = 0;
		}
		else
		{
			srvDesc.ViewDimension = (sampleCountMSAA > 1) ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.MipLevels = numLevels;
			srvDesc.Texture2DArray.MostDetailedMip = 0;
			srvDesc.Texture2DArray.ArraySize = depth;
			srvDesc.Texture2DArray.FirstArraySlice = 0;
		}
	}
	else
	{
		D3D11_TEXTURE3D_DESC textureDesc;
		ZeroMemory(&textureDesc, sizeof(textureDesc));
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.Depth = depth;
		textureDesc.MipLevels = numLevels;
		textureDesc.Format = textureFormat;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		if (format != DXGI_FORMAT_R24_UNORM_X8_TYPELESS)
			textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		else
			textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
		if (useUAV)
			textureDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		textureDesc.CPUAccessFlags = 0;
		textureDesc.MiscFlags = (numLevels == 1) ? 0 : D3D11_RESOURCE_MISC_GENERATE_MIPS;
		if (Engine::GetRender()->GetDevice()->CreateTexture3D(&textureDesc, NULL, (ID3D11Texture3D**)&texture) != S_OK)
			return false;

		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MipLevels = numLevels;
		srvDesc.Texture3D.MostDetailedMip = 0;
	}

	if (Engine::GetRender()->GetDevice()->CreateShaderResourceView(texture, &srvDesc, &shaderResourceView) != S_OK)
		return false;

	if (useUAV)
	{
		if (Engine::GetRender()->GetDevice()->CreateUnorderedAccessView(texture, NULL, &unorderedAccessView) != S_OK)
			return false;
	}

	return true;
}

void DX11_Texture::Bind(unsigned short bindingPoint, unsigned short shaderType) const
{
	switch (shaderType)
	{
	case VERTEX_SHADER:
		_MUTEXLOCK(Engine::m_pMutex);
		Engine::GetRender()->GetDeviceContext()->VSSetShaderResources(bindingPoint, 1, &shaderResourceView);
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;

	case HULL_SHADER:
		_MUTEXLOCK(Engine::m_pMutex);
		Engine::GetRender()->GetDeviceContext()->HSSetShaderResources(bindingPoint, 1, &shaderResourceView);
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;

	case DOMAIN_SHADER:
		_MUTEXLOCK(Engine::m_pMutex);
		Engine::GetRender()->GetDeviceContext()->DSSetShaderResources(bindingPoint, 1, &shaderResourceView);
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;

	case GEOMETRY_SHADER:
		_MUTEXLOCK(Engine::m_pMutex);
		Engine::GetRender()->GetDeviceContext()->GSSetShaderResources(bindingPoint, 1, &shaderResourceView);
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;

	case PIXEL_SHADER:
		_MUTEXLOCK(Engine::m_pMutex);
		Engine::GetRender()->GetDeviceContext()->PSSetShaderResources(bindingPoint, 1, &shaderResourceView);
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;

	case COMPUTE_SHADER:
		_MUTEXLOCK(Engine::m_pMutex);
		Engine::GetRender()->GetDeviceContext()->CSSetShaderResources(bindingPoint, 1, &shaderResourceView);
		_MUTEXUNLOCK(Engine::m_pMutex);
		break;
	}
}

bool DX11_Texture::IsSrgbFormat(DXGI_FORMAT texFormat)
{
	return((texFormat == DXGI_FORMAT_B8G8R8X8_UNORM_SRGB) || (texFormat == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) || (texFormat == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) ||
		(texFormat == DXGI_FORMAT_BC1_UNORM_SRGB) || (texFormat == DXGI_FORMAT_BC2_UNORM_SRGB) || (texFormat == DXGI_FORMAT_BC3_UNORM_SRGB));
}

DXGI_FORMAT DX11_Texture::ConvertToSrgbFormat(DXGI_FORMAT texFormat)
{
	if (IsSrgbFormat(texFormat))
	{
		return texFormat;
	}
	else if ((texFormat == DXGI_FORMAT_B8G8R8X8_UNORM) || (texFormat == DXGI_FORMAT_B8G8R8A8_UNORM) || (texFormat == DXGI_FORMAT_R8G8B8A8_UNORM) ||
		(texFormat == DXGI_FORMAT_BC1_UNORM) || (texFormat == DXGI_FORMAT_BC2_UNORM) || (texFormat == DXGI_FORMAT_BC3_UNORM))
	{
		return ((DXGI_FORMAT)(texFormat + 1));
	}
	else
	{
		return DXGI_FORMAT_UNKNOWN;
	}
}

DXGI_FORMAT DX11_Texture::ConvertFromSrgbFormat(DXGI_FORMAT texFormat)
{
	if ((texFormat == DXGI_FORMAT_B8G8R8X8_UNORM) || (texFormat == DXGI_FORMAT_B8G8R8A8_UNORM) || (texFormat == DXGI_FORMAT_R8G8B8A8_UNORM) ||
		(texFormat == DXGI_FORMAT_BC1_UNORM) || (texFormat == DXGI_FORMAT_BC2_UNORM) || (texFormat == DXGI_FORMAT_BC3_UNORM))
	{
		return texFormat;
	}
	else if (IsSrgbFormat(texFormat))
	{
		return ((DXGI_FORMAT)(texFormat - 1));
	}
	else
	{
		return DXGI_FORMAT_UNKNOWN;
	}
}

DXGI_FORMAT DX11_Texture::GetDX11TexFormat(DXGI_FORMAT texFormat, bool typeless)
{
	if (!typeless)
		return (DXGI_FORMAT)texFormat;

	switch (texFormat)
	{
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		return DXGI_FORMAT_R24G8_TYPELESS;

	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		return DXGI_FORMAT_B8G8R8X8_TYPELESS;

	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		return DXGI_FORMAT_B8G8R8A8_TYPELESS;

	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		return DXGI_FORMAT_R8G8B8A8_TYPELESS;

	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
		return DXGI_FORMAT_BC1_TYPELESS;

	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
		return DXGI_FORMAT_BC2_TYPELESS;

	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
		return DXGI_FORMAT_BC3_TYPELESS;

	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}




