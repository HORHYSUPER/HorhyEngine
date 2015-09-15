#pragma once

#include "DX12_Shader.h"
#include "DX12_Texture.h"

namespace D3D11Framework
{
	class ResourceManager
	{
	public:
		ResourceManager()
		{
		}

		~ResourceManager()
		{
			Release();
		}

		void Release();

		// loads ".sdr" shader-file (references the actual shader source files)
		DX12_Shader* LoadShader(const char *fileName, CONST D3D_SHADER_MACRO* pDefines, unsigned int permutationMask = 0);

		DX12_Texture* LoadTexture(const char *fileName);

		// loads ".mtl" material-file
		//Material* LoadMaterial(const char *fileName);

		// loads ".font" font-file 
		//Font* LoadFont(const char *fileName);

		/*Font* GetFont(unsigned int index) const
	  {
		assert(index < fonts.GetSize());
		return fonts[index];
	  }*/

	  // loads ".mesh" mesh-file
	  /*DemoMesh* LoadDemoMesh(const char *fileName);

	DemoMesh* GetDemoMesh(unsigned int index) const
	{
	  assert(index < demoMeshes.GetSize());
	  return demoMeshes[index];
	}

	  unsigned int GetNumFonts() const
	  {
		  return fonts.GetSize();
	  }

	  unsigned int GetNumDemoMeshes() const
	  {
		  return demoMeshes.GetSize();
	  }*/

	private:
		vector<DX12_Shader*> shaders;
		vector<DX12_Texture*> textures;
		//List<Material*> materials;
		//List<Font*> fonts;
		//List<DemoMesh*> demoMeshes;

	};
}