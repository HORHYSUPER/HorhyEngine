#include "stdafx.h"
#include "Engine.h"
#include "Render.h"
#include "DX11_VertexLayout.h"

using namespace D3D11Framework;

static const char *elementFormatNames[] = { "float", "float2", "float3", "float4" };
static const DXGI_FORMAT elementFormats[] = { DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT,
											  DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT };
static const unsigned int elementFormatSizes[] = { 4, 8, 12, 16 };

static const char *elementNames[] = { "position", "texCoords", "normal", "binormal", "tangent", "color", "bone_indices", "bone_weights" };
static const char *semanticNames[] = { "POSITION", "TEXCOORD", "NORMAL", "BINORMAL", "TANGENT", "COLOR", "BONE_INDICES", "BONE_WEIGHTS" };

void DX11_VertexLayout::Release()
{
	SAFE_RELEASE(inputLayout);
	SAFE_DELETE_ARRAY(vertexElementDescs);
}

bool DX11_VertexLayout::Create(const VertexElementDesc *vertexElementDescs, unsigned int numVertexElementDescs)
{
	if ((!vertexElementDescs) || (numVertexElementDescs < 1))
		return false;

	this->numVertexElementDescs = numVertexElementDescs;
	this->vertexElementDescs = new VertexElementDesc[numVertexElementDescs];
	if (!this->vertexElementDescs)
		return false;
	memcpy(this->vertexElementDescs, vertexElementDescs, sizeof(VertexElementDesc)*numVertexElementDescs);

	// create tmp shader for input layout
	std::string str;
	str = "struct VS_Input\n{\n  ";
	for (unsigned int i = 0; i < numVertexElementDescs; i++)
	{
		VertexElementDesc desc = vertexElementDescs[i];
		str += elementFormatNames[desc.format];
		str += " ";
		str += elementNames[desc.vertexElement];
		str += to_string(desc.semanticIndex);
		str += ": ";
		str += semanticNames[desc.vertexElement];
		str += to_string(desc.semanticIndex);
		str += ";\n  ";
	}
	str += "};\n";
	str += "struct VS_Output\n{\n  float4 pos: SV_POSITION;\n};\n";
	str += "VS_Output main(VS_Input input)\n{\n  VS_Output output = (VS_Output)0;\n  return output;\n};";
	
	ofstream tmp;
	tmp.open("tmp");
	tmp << str;
	tmp.close();

	// compile tmp shader
	ID3DBlob* vsBlob = NULL;
	ID3DBlob* errorBlob = NULL;
	if (D3DCompileFromFile(L"tmp", NULL, NULL, "main", "vs_5_0", NULL, NULL, &vsBlob, &errorBlob) != S_OK)
	{
		if (errorBlob)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			MessageBox(NULL, (LPCWSTR)errorBlob->GetBufferPointer(), L"Vertex Shader Error", MB_OK | MB_ICONEXCLAMATION);
			SAFE_RELEASE(errorBlob);
		}
		return false;
	}
	SAFE_RELEASE(errorBlob);
	remove("tmp");

	// create input layout
	D3D11_INPUT_ELEMENT_DESC *layout = new D3D11_INPUT_ELEMENT_DESC[numVertexElementDescs];
	for (unsigned int i = 0; i < numVertexElementDescs; i++)
	{
		VertexElementDesc desc = vertexElementDescs[i];
		layout[i].SemanticName = semanticNames[desc.vertexElement];
		layout[i].SemanticIndex = desc.semanticIndex;
		layout[i].Format = elementFormats[desc.format];
		layout[i].InputSlot = 0;
		layout[i].AlignedByteOffset = desc.offset;
		layout[i].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		layout[i].InstanceDataStepRate = 0;
	}
	if (Engine::GetRender()->GetDevice()->CreateInputLayout(layout, numVertexElementDescs, vsBlob->GetBufferPointer(),
		vsBlob->GetBufferSize(), &inputLayout) != S_OK)
	{
		SAFE_RELEASE(vsBlob);
		SAFE_DELETE_ARRAY(layout);
		return false;
	}
	SAFE_RELEASE(vsBlob);
	SAFE_DELETE_ARRAY(layout);

	return true;
}

void DX11_VertexLayout::Bind() const
{
	_MUTEXLOCK(Engine::m_pMutex);
	Engine::GetRender()->GetDeviceContext()->IASetInputLayout(inputLayout);
	_MUTEXUNLOCK(Engine::m_pMutex);
}

unsigned int DX11_VertexLayout::CalcVertexSize() const
{
	unsigned int vertexSize = 0;
	for (unsigned int i = 0; i < numVertexElementDescs; i++)
		vertexSize += elementFormatSizes[vertexElementDescs[i].format];
	return vertexSize;
}

bool DX11_VertexLayout::IsEqual(const VertexElementDesc *vertexElementDescs, unsigned int numVertexElementDescs) const
{
	if ((!vertexElementDescs) || (this->numVertexElementDescs != numVertexElementDescs))
		return false;
	for (unsigned int i = 0; i < numVertexElementDescs; i++)
	{
		if (this->vertexElementDescs[i].vertexElement != vertexElementDescs[i].vertexElement)
			return false;
		if (this->vertexElementDescs[i].format != vertexElementDescs[i].format)
			return false;
		if (this->vertexElementDescs[i].offset != vertexElementDescs[i].offset)
			return false;
	}
	return true;
}
