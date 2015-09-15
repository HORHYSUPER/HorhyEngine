#include "stdafx.h"
#include "ResourceManager.h"

using namespace D3D11Framework;

void ResourceManager::Release()
{
	SAFE_DELETE_PLIST(shaders);
	SAFE_DELETE_PLIST(textures);
	//SAFE_DELETE_PLIST(materials);
	//SAFE_DELETE_PLIST(fonts); 
	//SAFE_DELETE_PLIST(demoMeshes);
}

DX12_Shader* ResourceManager::LoadShader(const char *fileName, CONST D3D_SHADER_MACRO* pDefines, unsigned int permutationMask)
{
	for (unsigned int i = 0; i < shaders.size(); i++)
	{
		if (strcmp(shaders[i]->GetName(), fileName) == 0)
		{
			bool shaderCompare = false;
			int defineIter = 0;
			do
			{
				for (int j = 0; j < shaders[i]->GetMacros()[0].size() - 1; j++)
				{
					shaderCompare = false;
					if ((strcmp(pDefines[defineIter].Name, shaders[i]->GetMacros()[0][j].Name) == 0) &&
						(strcmp(pDefines[defineIter].Definition, shaders[i]->GetMacros()[0][j].Definition) == 0))
					{
						shaderCompare = true;
						break;
					}
				}
				if (!shaderCompare)
					break;
				defineIter++;
			} while (pDefines[defineIter].Name);
			if (shaderCompare)
				return shaders[i];
		}
	}
	DX12_Shader *shader = new DX12_Shader;
	if (!shader)
		return NULL;
	if (!shader->Load(fileName, pDefines))
	{
		SAFE_DELETE(shader);
		return NULL;
	}
	shaders.push_back(shader);
	return shader;
}

DX12_Texture* ResourceManager::LoadTexture(const char *fileName)
{
	for (unsigned int i = 0; i < textures.size(); i++)
	{
		if (strcmp(textures[i]->GetName(), fileName) == 0)
			return textures[i];
	}
	DX12_Texture *texture = new DX12_Texture;
	if (!texture)
		return NULL;
	if (!texture->LoadFromFile(fileName))
	{
		SAFE_DELETE(texture);
		return NULL;
	}
	textures.push_back(texture);
	return texture;
}

/*Material* ResourceManager::LoadMaterial(const char *fileName)
{
	for(unsigned int i=0; i<materials.GetSize(); i++)
	{
		if(strcmp(materials[i]->GetName(), fileName) == 0)
			return materials[i];
	}
	Material *material = new Material;
	if(!material)
		return NULL;
	if(!material->Load(fileName))
	{
		SAFE_DELETE(material);
		return NULL;
	}
	materials.AddElement(&material);
	return material;
}

Font* ResourceManager::LoadFont(const char *fileName)
{
	for(unsigned int i=0; i<fonts.GetSize(); i++)
	{
	if(strcmp(fonts[i]->GetName(), fileName) == 0)
			return fonts[i];
	}
	Font *font = new Font;
	if(!font)
		return NULL;
	if(!font->Load(fileName))
	{
		SAFE_DELETE(font);
		return NULL;
	}
	fonts.AddElement(&font);
	return font;
}

DemoMesh* ResourceManager::LoadDemoMesh(const char *fileName)
{
	DemoMesh *demoMesh = new DemoMesh;
	if(!demoMesh)
		return NULL;
	if(!demoMesh->Load(fileName))
	{
		SAFE_DELETE(demoMesh);
		return NULL;
	}
	demoMeshes.AddElement(&demoMesh);
	return demoMesh;
}*/