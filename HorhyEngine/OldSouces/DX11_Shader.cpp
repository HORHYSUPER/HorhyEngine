#include "stdafx.h"
#include "Engine.h"
#include "Render.h"
#include "DX11_Shader.h"
#include "DX11_StructuredBuffer.h"
#include "DX11_UniformBuffer.h"
#include "DX11_Texture.h"
#include "Util.h"

using namespace D3D11Framework;

const char* DX11_Shader::shaderModels[NUM_SHADER_TYPES] =
{ "vs_5_0", "hs_5_0", "ds_5_0", "gs_5_0", "ps_5_0", "cs_5_0" };
const char* DX11_Shader::uniformBufferRegisterNames[NUM_UNIFORM_BUFFER_BP] =
{ "b0", "b1", "b2", "b3" };
const char* DX11_Shader::textureRegisterNames[NUM_TEXTURE_BP + NUM_STRUCTURED_BUFFER_BP] =
{ "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8", "t9", "t10", "t11" };

//class ShaderInclude : public ID3D10Include
//{
//public:
//	STDMETHOD(Open)(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
//	{
//		char fileName[DEMO_MAX_FILENAME];
//		strcpy(fileName, "Shaders/HLSL/src/");
//		strcat(fileName, pFileName);
//		//if (!DX11_Shader::ReadShaderFile(fileName, (unsigned char**)ppData, pBytes))
//		//	return S_FALSE;
//		return S_OK;
//	}
//
//	STDMETHOD(Close)(LPCVOID pData)
//	{
//		SAFE_DELETE_ARRAY(pData);
//		return S_OK;
//	}
//} includeHandler;

void DX11_Shader::Release()
{
	SAFE_RELEASE(vertexShader);
	SAFE_RELEASE(hullShader);
	SAFE_RELEASE(domainShader);
	SAFE_RELEASE(geometryShader);
	SAFE_RELEASE(fragmentShader);
	SAFE_RELEASE(computeShader);
	for (unsigned int i = 0; i < NUM_SHADER_TYPES; i++)
		SAFE_RELEASE(byteCodes[i]);
}

void DX11_Shader::LoadDefines(std::ifstream &file)
{
	std::string str;
	file >> str;
	while (true)
	{
		file >> str;
		if ((str == "}") || (file.eof()))
			break;
		assert(str.length() <= DEMO_MAX_STRING);
		char elementString[DEMO_MAX_STRING];
		strcpy(elementString, str.c_str());
		defineStrings.push_back(elementString);
	}
}

bool DX11_Shader::ReadShaderFile(const char *fileName, unsigned char **data, unsigned int *dataSize)
{
	if ((!fileName) || (!data) || (!dataSize))
		return false;
	char filePath[MAX_FILEPATH];
	//if (!Demo::fileManager->GetFilePath(fileName, filePath))
	//	return false;
	FILE *file;
	fopen_s(&file, fileName, "rt");
	if (!file)
		return false;
	struct _stat fileStats;
	if (_stat(fileName, &fileStats) != 0)
	{
		fclose(file);
		return false;
	}
	*data = new unsigned char[fileStats.st_size];
	if (!(*data))
	{
		fclose(file);
		return false;
	}
	*dataSize = fread(*data, 1, fileStats.st_size, file);
	(*data)[*dataSize] = 0;
	fclose(file);
	return true;
}

bool DX11_Shader::CreateShaderUnit(ShaderTypes shaderType, void *byteCode, unsigned int shaderSize)
{
	switch (shaderType)
	{
	case VERTEX_SHADER:
		if (!Engine::GetRender()->GetDevice()->CreateVertexShader(byteCode, shaderSize, NULL, &vertexShader) == S_OK)
			return false;
		break;

	case HULL_SHADER:
		if (!Engine::GetRender()->GetDevice()->CreateHullShader(byteCode, shaderSize, NULL, &hullShader) == S_OK)
			return false;
		break;

	case DOMAIN_SHADER:
		if (!Engine::GetRender()->GetDevice()->CreateDomainShader(byteCode, shaderSize, NULL, &domainShader) == S_OK)
			return false;
		break;

	case GEOMETRY_SHADER:
		if (!Engine::GetRender()->GetDevice()->CreateGeometryShader(byteCode, shaderSize, NULL, &geometryShader) == S_OK)
			return false;
		break;

	case PIXEL_SHADER:
		if (!Engine::GetRender()->GetDevice()->CreatePixelShader(byteCode, shaderSize, NULL, &fragmentShader) == S_OK)
			return false;
		break;

	case COMPUTE_SHADER:
		if (!Engine::GetRender()->GetDevice()->CreateComputeShader(byteCode, shaderSize, NULL, &computeShader) == S_OK)
			return false;
		break;
	}

	return true;
}

bool DX11_Shader::InitShaderUnit(ShaderTypes shaderType, const char *fileName)
{
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	unsigned char *shaderSource = NULL;
	unsigned int shaderSize = 0;
	ID3D10Blob *errorMsgs = NULL;
	ID3D10Blob *shaderText = NULL;

	// read shader file
	if (!ReadShaderFile(fileName, &shaderSource, &shaderSize))
		return false;

	// preprocess shader
	//if (D3DPreprocess(shaderSource, shaderSize, NULL, &shaderMacros[0], &includeHandler, &shaderText, &errorMsgs) != S_OK)
	//{
	//	if (errorMsgs)
	//	{
	//		char errorTitle[512];
	//		wsprintf((LPWSTR)errorTitle, L"Shader Preprocess Error in %s", fileName);
	//		MessageBox(NULL, (LPWSTR)errorMsgs->GetBufferPointer(), (LPWSTR)errorTitle, MB_OK | MB_ICONEXCLAMATION);
	//		SAFE_RELEASE(errorMsgs);
	//	}
	//	SAFE_DELETE_ARRAY(shaderSource);
	//	return false;
	//}
	//SAFE_RELEASE(errorMsgs);
	//SAFE_DELETE_ARRAY(shaderSource);

	//// parse shader
	//ParseShaderString(shaderType, (const char*)shaderText->GetBufferPointer());

	//// compile shader
	//if (D3DCompile(shaderText->GetBufferPointer(), shaderText->GetBufferSize(), NULL, NULL, NULL, "main",
	//	shaderModels[shaderType], dwShaderFlags, 0, &byteCodes[shaderType], &errorMsgs) != S_OK)
	//{
	//	if (errorMsgs)
	//	{
	//		OutputDebugStringA((char*)errorMsgs->GetBufferPointer());
	//		char errorTitle[512];
	//		wsprintf((LPWSTR)errorTitle, L"Shader Compile Error in %s", fileName);
	//		MessageBox(NULL, (LPWSTR)errorMsgs->GetBufferPointer(), (LPWSTR)errorTitle, MB_OK | MB_ICONEXCLAMATION);
	//		SAFE_RELEASE(errorMsgs);
	//	}
	//	SAFE_RELEASE(shaderText);
	//	return false;
	//}

	ID3DBlob *pErrorBlob = nullptr;
	HRESULT hr = D3DCompileFromFile(ToWChar((char*)fileName), &shaderMacros[0], D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", shaderModels[shaderType], dwShaderFlags, 0, &byteCodes[shaderType], &pErrorBlob);
	if (FAILED(hr) && pErrorBlob)
		OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());

	SAFE_RELEASE(shaderText);
	SAFE_RELEASE(errorMsgs);

	if (!CreateShaderUnit(shaderType, byteCodes[shaderType]->GetBufferPointer(), byteCodes[shaderType]->GetBufferSize()))
	{
		SAFE_RELEASE(byteCodes[shaderType]);
		return false;
	}

	return true;
}

bool DX11_Shader::LoadShaderUnit(ShaderTypes shaderType, std::ifstream &file)
{
	std::string str, token;
	file >> token >> str >> token;
	if (str != "NULL")
	{
		std::string filename = "Data/Shaders/";
		filename.append(str);
		if (!InitShaderUnit(shaderType, filename.c_str()))
		{
			file.close();
			Release();
			return false;
		}
	}
	return true;
}

void DX11_Shader::ParseShaderString(ShaderTypes shaderType, const char *shaderString)
{
	if (!shaderString)
		return;
	std::istringstream is(shaderString);
	std::string str, token, param;
	do
	{
		is >> str;
		if ((str == "Texture1D") || (str == "Texture2D") || (str == "Texture2DMS") ||
			(str == "Texture3D") || (str == "TextureCube") || (str == "Texture2DArray") ||
			(str == "StructuredBuffer"))
		{
			is >> token;
			if (token == "<")
				is >> token >> token >> token;
			is >> token >> token >> token >> param;
			std::getline(is, token);
			for (unsigned int i = 0; i < (NUM_TEXTURE_BP + NUM_STRUCTURED_BUFFER_BP); i++)
			{
				if (param == textureRegisterNames[i])
					textureMasks[i] |= 1 << shaderType;
			}
		}
		else if (str == "cbuffer")
		{
			is >> token >> token >> token >> token >> param;
			std::getline(is, token);
			for (unsigned int i = 0; i < NUM_UNIFORM_BUFFER_BP; i++)
			{
				if (param == uniformBufferRegisterNames[i])
					uniformBufferMasks[i] |= 1 << shaderType;
			}
		}
		else if ((str == "VS_OUTPUT") || (str == "void") || (str == "FS_Output"))
		{
			is >> str;
			std::getline(is, token);
		}
	} while (str != "main");
}

bool DX11_Shader::CreateShaderMacros(CONST D3D_SHADER_MACRO* pDefines)
{
	for (unsigned int i = 0; i < defineStrings.size(); i++)
	{
		D3D10_SHADER_MACRO shaderMacro;
		shaderMacro.Name = defineStrings[i].c_str();
		if (!shaderMacro.Name)
			return false;

		shaderMacro.Definition = NULL;
		int defineIter = 0;
		do
		{
			if (strcmp(pDefines[defineIter].Name, shaderMacro.Name) == 0)
			{
				shaderMacro.Definition = pDefines[defineIter].Definition;
			}
			defineIter++;

		} while (pDefines[defineIter].Name);
		shaderMacros.push_back(shaderMacro);
	}
	D3D10_SHADER_MACRO shaderMacro;
	shaderMacro.Name = NULL;
	shaderMacro.Definition = NULL;
	shaderMacros.push_back(shaderMacro);
	return true;
}

bool DX11_Shader::LoadShaderBin(const char *filePath)
{
	FILE *file;
	fopen_s(&file, filePath, "rb");
	if (!file)
		return false;

	char idString[10];
	memset(idString, 0, 10);
	fread(idString, sizeof(char), 9, file);
	if (strcmp(idString, "DEMO_HLSL") != 0)
		return false;

	unsigned int version;
	fread(&version, sizeof(unsigned int), 1, file);
	if (version != CURRENT_SHADER_VERSION)
		return false;

	fread(uniformBufferMasks, sizeof(unsigned int), NUM_UNIFORM_BUFFER_BP, file);
	fread(textureMasks, sizeof(unsigned int), NUM_TEXTURE_BP + NUM_STRUCTURED_BUFFER_BP, file);

	unsigned int numShaders;
	fread(&numShaders, sizeof(unsigned int), 1, file);
	for (unsigned int i = 0; i < numShaders; i++)
	{
		unsigned int shaderType;
		fread(&shaderType, sizeof(unsigned int), 1, file);
		int shaderSize;
		fread(&shaderSize, sizeof(unsigned int), 1, file);
		unsigned char *byteCode = new unsigned char[shaderSize];
		if (!byteCode)
		{
			fclose(file);
			return false;
		}
		fread(byteCode, shaderSize, 1, file);
		if (!CreateShaderUnit((ShaderTypes)shaderType, byteCode, shaderSize))
		{
			SAFE_DELETE_ARRAY(byteCode);
			fclose(file);
			return false;
		}
		SAFE_DELETE_ARRAY(byteCode);
	}

	fclose(file);
	return true;
}

bool DX11_Shader::SaveShaderBin(const char *filePath)
{
	FILE *file;
	fopen_s(&file, filePath, "wb");
	if (!file)
		return false;

	fwrite("DEMO_HLSL", sizeof(char), 9, file);
	unsigned int version = CURRENT_SHADER_VERSION;
	fwrite(&version, sizeof(unsigned int), 1, file);
	fwrite(uniformBufferMasks, sizeof(unsigned int), NUM_UNIFORM_BUFFER_BP, file);
	fwrite(textureMasks, sizeof(unsigned int), NUM_TEXTURE_BP + NUM_STRUCTURED_BUFFER_BP, file);

	unsigned int numShaders = 0;
	for (unsigned int i = 0; i < NUM_SHADER_TYPES; i++)
	{
		if (byteCodes[i])
			numShaders++;
	}

	fwrite(&numShaders, sizeof(unsigned int), 1, file);
	for (unsigned int i = 0; i < NUM_SHADER_TYPES; i++)
	{
		if (byteCodes[i])
		{
			fwrite(&i, sizeof(unsigned int), 1, file);
			unsigned int bufferSize = byteCodes[i]->GetBufferSize();
			fwrite(&bufferSize, sizeof(unsigned int), 1, file);
			fwrite(byteCodes[i]->GetBufferPointer(), bufferSize, 1, file);
		}
	}

	fclose(file);
	return true;
}

bool DX11_Shader::Load(const char *fileName, CONST D3D_SHADER_MACRO* pDefines)
{
	strcpy(name, fileName);

#ifdef USE_SHADER_BIN
	const char *shaderBinPath = NULL;
	char binName[DEMO_MAX_STRING];
	char binPath[DEMO_MAX_FILEPATH];
	if (Demo::fileManager->GetFileName(fileName, binName))
	{
		sprintf(binPath, "../data/Shaders/HLSL/bin/%s_%i.bin", binName, permutationMask);
		shaderBinPath = binPath;
		if (LoadShaderBin(shaderBinPath))
			return true;
	}
#endif

	char filePath[MAX_FILEPATH];
	//if (!Demo::fileManager->GetFilePath(fileName, filePath))
	//	return false;
	std::ifstream file(fileName, std::ios::in);
	if (!file.is_open())
		return false;

	std::string str, token;
	file >> str;
	while (!file.eof())
	{
		if (str == "Defines")
		{
			LoadDefines(file);
			if (!CreateShaderMacros(pDefines))
			{
				file.close();
				return false;
			}
		}
		else if (str == "VertexShader")
		{
			if (!LoadShaderUnit(VERTEX_SHADER, file))
			{
				file.close();
				return false;
			}
		}
		else if (str == "HullShader")
		{
			if (!LoadShaderUnit(HULL_SHADER, file))
			{
				file.close();
				return false;
			}
		}
		else if (str == "DomainShader")
		{
			if (!LoadShaderUnit(DOMAIN_SHADER, file))
			{
				file.close();
				return false;
			}
		}
		else if (str == "GeometryShader")
		{
			if (!LoadShaderUnit(GEOMETRY_SHADER, file))
			{
				file.close();
				return false;
			}
		}
		else if (str == "PixelShader")
		{
			if (!LoadShaderUnit(PIXEL_SHADER, file))
			{
				file.close();
				return false;
			}
		}
		else if (str == "ComputeShader")
		{
			if (!LoadShaderUnit(COMPUTE_SHADER, file))
			{
				file.close();
				return false;
			}
		}
		file >> str;
	}
	file.close();

	//shaderMacros.erase(shaderMacros.begin(), shaderMacros.end());

#ifdef USE_SHADER_BIN
	if (shaderBinPath)
		SaveShaderBin(shaderBinPath);
#endif

	for (unsigned int i = 0; i < NUM_SHADER_TYPES; i++)
		SAFE_RELEASE(byteCodes[i]);

	return true;
}

void DX11_Shader::Bind() const
{
	_MUTEXLOCK(Engine::m_pMutex);
	Engine::GetRender()->GetDeviceContext()->VSSetShader(vertexShader, NULL, 0);
	Engine::GetRender()->GetDeviceContext()->HSSetShader(hullShader, NULL, 0);
	Engine::GetRender()->GetDeviceContext()->DSSetShader(domainShader, NULL, 0);
	Engine::GetRender()->GetDeviceContext()->GSSetShader(geometryShader, NULL, 0);
	Engine::GetRender()->GetDeviceContext()->PSSetShader(fragmentShader, NULL, 0);
	Engine::GetRender()->GetDeviceContext()->CSSetShader(computeShader, NULL, 0);
	_MUTEXUNLOCK(Engine::m_pMutex);
}

void DX11_Shader::SetUniformBuffer(UniformBufferBP bindingPoint, const DX11_UniformBuffer *uniformBuffer) const
{
	if (!uniformBuffer)
		return;
	for (unsigned int i = 0; i < NUM_SHADER_TYPES; i++)
	{
		if (uniformBufferMasks[bindingPoint] & (1 << i))
			uniformBuffer->Bind(bindingPoint, (ShaderTypes)i);
	}
}

void DX11_Shader::SetStructuredBuffer(StructuredBufferBP bindingPoint, const DX11_StructuredBuffer *structuredBuffer) const
{
	if (!structuredBuffer)
		return;
	for (unsigned int i = 0; i < NUM_SHADER_TYPES; i++)
	{
		if (textureMasks[bindingPoint] & (1 << i))
			structuredBuffer->Bind(bindingPoint, (ShaderTypes)i);
	}
}

void DX11_Shader::SetTexture(TextureBP bindingPoint, const DX11_Texture *texture, const DX11_Sampler *sampler) const
{
	if (!texture)
		return;
	for (unsigned int i = 0; i < NUM_SHADER_TYPES; i++)
	{
		if (textureMasks[bindingPoint] & (1 << i))
		{
			texture->Bind(bindingPoint, (ShaderTypes)i);
			if (sampler)
				sampler->Bind(bindingPoint, (ShaderTypes)i);
		}
	}
}





