#pragma once

#include "macros.h"

namespace D3D11Framework
{
	class DX11_UniformBuffer
	{
	public:
		DX11_UniformBuffer() :
			size(0),
			uniformBuffer(NULL)
		{
		}

		~DX11_UniformBuffer()
		{
			Release();
		}

		void Release();

		bool Create(unsigned int bufferSize);

		// Please note: uniforms must be aligned according to the HLSL rules, in order to be able
		// to upload data in 1 block.
		bool Update(const void *uniformBufferData);

		void Bind(UniformBufferBP bindingPoint, ShaderTypes shaderType = VERTEX_SHADER) const;

	private:
		unsigned int size; // size of uniform data
		ID3D11Buffer *uniformBuffer;

	};
}