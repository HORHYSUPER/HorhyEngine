#pragma once

#include "macros.h"

#define CURRENT_SHADER_VERSION 4 // current version of binary pre-compiled shaders

using namespace std;
using namespace Microsoft::WRL;
using namespace DirectX;

namespace D3D11Framework
{
	class DX12_UniformBuffer;
	class DX11_StructuredBuffer;
	class DX12_Texture;
	class DX12_Sampler;
	enum ShaderTypes;

	struct PS_OUTPUT
	{
		string format;
		string name;
		string output;
	};

	// DX11_Shader
	//
	// Loaded from a simple text-file (".sdr"), that references the actual shader source files.
	// To avoid long loading times, per default pre-compiled shaders are used.

	class DX12_Shader
	{
	public:
		friend class Render;
		friend class PipelineStateObject;
		friend class ShaderInclude;

		DX12_Shader()
		{
			name[0] = 0;
			memset(byteCodes, 0, sizeof(ID3DBlob*) * NUM_SHADER_TYPES);
			memset(uniformBufferMasks, 0, sizeof(unsigned int) * (NUM_UNIFORM_BUFFER_BP + NUM_CUSTOM_UNIFORM_BUFFER_BP));
			memset(textureMasks, 0, sizeof(unsigned int) * (NUM_TEXTURE_BP + NUM_STRUCTURED_BUFFER_BP));
		}

		~DX12_Shader()
		{
			Release();
		}

		void Release();

		bool Load(const char *fileName, CONST D3D_SHADER_MACRO* pDefines);

		static bool ReadShaderFile(const char *fileName, unsigned char **data, unsigned int *dataSize);

		vector<D3D10_SHADER_MACRO> *GetMacros()
		{
			return &shaderMacros;
		}

		const char* GetName() const
		{
			return name;
		}

		bool operator== (const DX12_Shader &otherShader) const
		{
			if (strcmp(this->name, otherShader.name) == 0)
			{
				bool shaderCompare = false;
				int defineIter = 0;
				do
				{
					for (int j = 0; j < this->shaderMacros.size() - 1; j++)
					{
						shaderCompare = false;
						if ((strcmp(otherShader.shaderMacros[defineIter].Name, this->shaderMacros[j].Name) == 0) &&
							(strcmp(otherShader.shaderMacros[defineIter].Definition, this->shaderMacros[j].Definition) == 0))
						{
							shaderCompare = true;
							break;
						}
					}
					if (!shaderCompare)
						break;
					defineIter++;
				} while (otherShader.shaderMacros[defineIter].Name);
				if (shaderCompare)
					return true;
			}

			return false;
		}

	private:
		void LoadDefines(std::ifstream &file);

		bool InitShaderUnit(ShaderTypes shaderType, const char *fileName);

		bool LoadShaderUnit(ShaderTypes shaderType, std::ifstream &file);

		bool CreateShaderMacros(CONST D3D_SHADER_MACRO* pDefines);

		void ParseShaderString(ShaderTypes shaderType, const char *shaderString);

		bool LoadShaderBin(const char *filePath);

		bool SaveShaderBin(const char *filePath);

		vector<string> defineStrings;
		vector<PS_OUTPUT> renderTargets;
		char name[DEMO_MAX_FILENAME];

		vector<D3D10_SHADER_MACRO> shaderMacros;
		ID3DBlob *byteCodes[NUM_SHADER_TYPES];

		unsigned int uniformBufferMasks[NUM_UNIFORM_BUFFER_BP + NUM_CUSTOM_UNIFORM_BUFFER_BP];
		unsigned int textureMasks[NUM_TEXTURE_BP + NUM_STRUCTURED_BUFFER_BP];
		unsigned int samplerMasks[NUM_TEXTURE_BP];

		static const char *shaderModels[NUM_SHADER_TYPES];
		static const char *uniformBufferRegisterNames[NUM_UNIFORM_BUFFER_BP];
		static const char *textureRegisterNames[NUM_TEXTURE_BP + NUM_STRUCTURED_BUFFER_BP];
		static const char *samplerRegisterNames[NUM_TEXTURE_BP];
		friend class ShaderInclude;

	};
}