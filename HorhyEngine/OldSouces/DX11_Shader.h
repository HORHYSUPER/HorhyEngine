#pragma once

#include "macros.h"
#include "Render.h"

#define CURRENT_SHADER_VERSION 4 // current version of binary pre-compiled shaders

namespace D3D11Framework
{
	class DX11_UniformBuffer;
	class DX11_StructuredBuffer;
	class DX11_Texture;
	class DX12_Sampler;

	// DX11_Shader
	//
	// Loaded from a simple text-file (".sdr"), that references the actual shader source files.
	// To avoid long loading times, per default pre-compiled shaders are used.
	class DX11_Shader
	{
	public:
		friend class ShaderInclude;

		DX11_Shader() :
			vertexShader(NULL),
			hullShader(NULL),
			domainShader(NULL),
			geometryShader(NULL),
			fragmentShader(NULL),
			computeShader(NULL)
		{
			name[0] = 0;
			memset(byteCodes, 0, sizeof(ID3DBlob*)*NUM_SHADER_TYPES);
			memset(uniformBufferMasks, 0, sizeof(unsigned int)*NUM_UNIFORM_BUFFER_BP);
			memset(textureMasks, 0, sizeof(unsigned int)*(NUM_TEXTURE_BP + NUM_STRUCTURED_BUFFER_BP));
		}

		~DX11_Shader()
		{
			Release();
		}

		void Release();

		bool Load(const char *fileName, CONST D3D_SHADER_MACRO* pDefines);

		void Bind() const;

		void SetUniformBuffer(UniformBufferBP bindingPoint, const DX11_UniformBuffer *uniformBuffer) const;

		void SetStructuredBuffer(StructuredBufferBP bindingPoint, const DX11_StructuredBuffer *structuredBuffer) const;

		void SetTexture(TextureBP bindingPoint, const DX11_Texture *texture, const DX12_Sampler *sampler) const;

		vector<D3D10_SHADER_MACRO> *GetMacros()
		{
			return &shaderMacros;
		}

		const char* GetName() const
		{
			return name;
		}

	private:
		void LoadDefines(std::ifstream &file);

		static bool ReadShaderFile(const char *fileName, unsigned char **data, unsigned int *dataSize);

		bool CreateShaderUnit(ShaderTypes shaderType, void *byteCode, unsigned int shaderSize);

		bool InitShaderUnit(ShaderTypes shaderType, const char *fileName);

		bool LoadShaderUnit(ShaderTypes shaderType, std::ifstream &file);

		bool CreateShaderMacros(CONST D3D_SHADER_MACRO* pDefines);

		void ParseShaderString(ShaderTypes shaderType, const char *shaderString);

		bool LoadShaderBin(const char *filePath);

		bool SaveShaderBin(const char *filePath);

		vector<string> defineStrings;
		char name[DEMO_MAX_FILENAME];

		ID3D11VertexShader *vertexShader;
		ID3D11HullShader *hullShader;
		ID3D11DomainShader *domainShader;
		ID3D11GeometryShader *geometryShader;
		ID3D11PixelShader *fragmentShader;
		ID3D11ComputeShader *computeShader;
		vector<D3D10_SHADER_MACRO> shaderMacros;
		ID3DBlob *byteCodes[NUM_SHADER_TYPES];

		unsigned int uniformBufferMasks[NUM_UNIFORM_BUFFER_BP];
		unsigned int textureMasks[NUM_TEXTURE_BP + NUM_STRUCTURED_BUFFER_BP];

		static const char *shaderModels[NUM_SHADER_TYPES];
		static const char *uniformBufferRegisterNames[NUM_UNIFORM_BUFFER_BP];
		static const char *textureRegisterNames[NUM_TEXTURE_BP + NUM_STRUCTURED_BUFFER_BP];

	};
}