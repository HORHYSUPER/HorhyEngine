#pragma once

//#include "stdafx.h"
//#include "Render.h"

#define NUM_UNIFORM_BUFFER_BP        4  // number of uniform-buffer binding points, used in this demo
#define NUM_CUSTOM_UNIFORM_BUFFER_BP 2  // number of custom uniform-buffer binding points, used in this demo
#define NUM_TEXTURE_BP             	 10 // number of texture binding points, used in this demo
#define NUM_STRUCTURED_BUFFER_BP   	 2  // number of structured-buffer binding points, used in this demo
#define NUM_SHADER_TYPES           	 6  // number of shader types, used in this demo

namespace D3D11Framework
{
	class PipelineStateObject;
	class DX12_VertexBuffer;
	class DX12_IndexBuffer;
	class DX12_UniformBuffer;
	class DX12_RenderTargetConfig;
	class DX12_RenderTarget;
	class DX12_Texture;
	class DX12_Sampler;
	class Camera;
	class ILight;

	enum gpuCmdOrders
	{
		BASE_CO = 0, // fill GBuffers
		GRID_FILL_CO, // fill voxel-grid
		SHADOW_CO, // generate shadow maps
		ILLUM_CO, // direct illumination
		GRID_ILLUM_CO, // illuminate voxel-grid
		GLOBAL_ILLUM_CO, // generate global illumination
		SKY_CO, // render sky
		POST_PROCESS_CO, // perform post-processing
		GUI_CO, // render GUI
	};

	enum gpuCmdModes
	{
		DRAW_CM = 0, // draw-call
		COMPUTE_CM, // dispatch of compute-shader
		GENERATE_MIPS_CM // mip-map generation
	};

	struct ShaderCmd
	{
		PipelineStateObject *pipelineStateObject;
		DX12_RenderTarget *renderTargets[FrameCount];
		DX12_RenderTargetConfig *renderTargetConfigs[FrameCount];
		Camera *camera;
		ILight *light;
		DX12_Texture *textures[NUM_TEXTURE_BP];
		DX12_Sampler *samplers[NUM_TEXTURE_BP];
		DX12_UniformBuffer *customUBs[NUM_CUSTOM_UNIFORM_BUFFER_BP];
		//DX11_StructuredBuffer *customSBs[NUM_STRUCTURED_BUFFER_BP];
	};

	struct DrawCmd : public ShaderCmd
	{
		DX12_VertexBuffer *vertexBuffer;
		DX12_IndexBuffer *indexBuffer;
		//DX11_RasterizerState *rasterizerState;
		//DX11_DepthStencilState *depthStencilState;
		//DX11_BlendState *blendState;
		D3D_PRIMITIVE_TOPOLOGY primitiveType;
		unsigned int firstIndex;
		unsigned int numElements;
		unsigned int numInstances;
		unsigned int vertexOffset;
	};

	struct ComputeCmd : public ShaderCmd
	{
		unsigned int numThreadGroupsX;
		unsigned int numThreadGroupsY;
		unsigned int numThreadGroupsZ;
	};

	struct GenerateMipsCmd
	{
		DX12_Texture *textures[NUM_TEXTURE_BP];
	};

	class GpuCmd
	{
	public:
		friend class Render;

		GpuCmd() {};

		explicit GpuCmd(gpuCmdModes mode)
		{
			Reset(mode);
		}

		void Reset(gpuCmdModes mode)
		{
			memset(this, 0, sizeof(GpuCmd));
			this->mode = mode;
			if (mode == DRAW_CM)
			{
				draw.primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
				draw.numInstances = 1;
			}
		}

		gpuCmdModes GetMode() const
		{
			return mode;
		}

		unsigned int GetID() const
		{
			return ID;
		}

		gpuCmdOrders order;
		union
		{
			DrawCmd draw;
			ComputeCmd compute;
			GenerateMipsCmd generateMips;
		};

	private:
		gpuCmdModes mode;
		unsigned int ID;

	};
}