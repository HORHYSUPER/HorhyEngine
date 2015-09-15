#include "stdafx.h"
#include "Buffer.h"
#include "DX12_Sampler.h"
#include "Camera.h"
#include "ResourceManager.h"
#include "macros.h"
#include "SkyBox.h"

using namespace D3D11Framework;
using namespace std;

struct cbPerObject
{
	XMMATRIX	mWorld;
	XMMATRIX	mProjection;
	XMMATRIX	mWorldVievProjection;
}cbPerObj;

SkyBox::SkyBox()
{
	isLoaded = false;
	//m_pShader = NULL;
}

SkyBox::~SkyBox()
{
}

bool SkyBox::Init()
{
	HRESULT hr;

	CreateSphere(10, 10);

	//m_pDiffuseTexture = new DX11_Texture;
	//m_pDiffuseTexture->LoadFromFile("Data/Textures/skymap.dds");

	//m_pShader = new Shader();
	//if (!m_pShader)
		return false;

	D3D_SHADER_MACRO    shaderMacro[] =
	{ "DEPTH_ONLY", "0",	NULL, NULL };

	std::wstring shaderName = Engine::GetRender()->GetFramework()->GetSystemPath(PATH_TO_SHADERS)->c_str();
	shaderName.append(L"Sky.hlsl");

	//m_pShader->AddInputElementDesc("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT);
	//m_pShader->AddInputElementDesc("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT);

	//if (!m_pShader->CreateShaderVs((WCHAR*)shaderName.c_str(), shaderMacro, "VS"))
	//	return false;

	//if (!m_pShader->CreateShaderPs((WCHAR*)shaderName.c_str(), shaderMacro, "PS"))
	//	return false;

	//m_pUniformBuffer = Engine::GetRender()->CreateUniformBuffer(sizeof(XMMATRIX));
	//if (!m_pUniformBuffer)
	//	return false;

	// Describe the Sample State
	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	//Create the Sample State
	hr = Engine::GetRender()->GetDevice()->CreateSamplerState(&sampDesc, &m_pCubesTexSamplerState);

	D3D11_RASTERIZER_DESC cmdesc;

	ZeroMemory(&cmdesc, sizeof(D3D11_RASTERIZER_DESC));
	cmdesc.FillMode = D3D11_FILL_SOLID;
	cmdesc.FrontCounterClockwise = true;
	cmdesc.CullMode = D3D11_CULL_NONE;
	hr = Engine::GetRender()->GetDevice()->CreateRasterizerState(&cmdesc, &m_pRSCullNone);

	D3D11_DEPTH_STENCIL_DESC dssDesc;
	ZeroMemory(&dssDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	dssDesc.DepthEnable = true;
	dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dssDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

	Engine::GetRender()->GetDevice()->CreateDepthStencilState(&dssDesc, &m_pDSLessEqual);

	isLoaded = true;

	return true;
}

void SkyBox::UpdateSkyBox(Camera *camera)
{
	//Reset sphereWorld
	sphereWorld = XMMatrixIdentity();

	//Define sphereWorld's world space matrix
	Scale = XMMatrixScaling(5.0f, 5.0f, 5.0f);
	//Make sure the sphere is always centered around camera
	Translation = XMMatrixTranslation(camera->GetPosition().m128_f32[0], camera->GetPosition().m128_f32[1], camera->GetPosition().m128_f32[2]);

	//Set sphereWorld's world space using the transformations
	sphereWorld = Scale * Translation;

	//m_pUniformBuffer->Update(&sphereWorld);
}

void SkyBox::DrawSkyBox(Camera *camera, RENDER_TYPE rendertype)
{
	UINT stride = sizeof(SkyBoxVertex);
	UINT offset = 0;

	_MUTEXLOCK(Engine::GetRender()->GetFramework()->GetMutex());
	Engine::GetRender()->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Engine::GetRender()->GetDeviceContext()->IASetVertexBuffers(0, 1, &m_pSphereVertexBuffer, &stride, &offset);
	Engine::GetRender()->GetDeviceContext()->IASetIndexBuffer(m_pSphereIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	switch (rendertype)
	{
	case D3D11Framework::GBUFFER_FILL_RENDER:
		//Engine::GetRender()->GetDeviceContext()->IASetInputLayout(m_pShader->GetInputLayout());
		//Engine::GetRender()->GetDeviceContext()->VSSetShader(m_pShader->GetVertexShader(0), 0, 0);
		Engine::GetRender()->GetDeviceContext()->HSSetShader(NULL, NULL, 0);
		Engine::GetRender()->GetDeviceContext()->DSSetShader(NULL, NULL, 0);
		Engine::GetRender()->GetDeviceContext()->GSSetShader(NULL, NULL, 0);
		//Engine::GetRender()->GetDeviceContext()->PSSetShader(m_pShader->GetPixelShader(0), 0, 0);

		break;
	case D3D11Framework::CUBEMAP_DEPTH_RENDER:
	case D3D11Framework::CUBEMAP_COLOR_RENDER:
		//Engine::GetRender()->GetDeviceContext()->IASetInputLayout(m_pDepthShader->GetInputLayout());
		//Engine::GetRender()->GetDeviceContext()->VSSetShader(m_pDepthShader->GetVertexShader(0), NULL, 0);
		//Engine::GetRender()->GetDeviceContext()->HSSetShader(NULL, NULL, 0);
		//Engine::GetRender()->GetDeviceContext()->DSSetShader(NULL, NULL, 0);
		//Engine::GetRender()->GetDeviceContext()->GSSetShader(m_pDepthShader->GetGeometryShader(0), NULL, 0);
		//Engine::GetRender()->GetDeviceContext()->PSSetShader(m_pDepthShader->GetPixelShader(0), NULL, 0);

		break;
	default:
		break;
	}
	_MUTEXUNLOCK(Engine::GetRender()->GetFramework()->GetMutex());

	switch (rendertype)
	{
	case D3D11Framework::GBUFFER_FILL_RENDER:
		WVP = sphereWorld * camera->GetViewMatrix() * camera->GetProjection();
		cbPerObj.mWorld = XMMatrixTranspose(sphereWorld);
		cbPerObj.mProjection = XMMatrixTranspose(camera->GetProjection());
		cbPerObj.mWorldVievProjection = XMMatrixTranspose(WVP);

		break;
	case D3D11Framework::CUBEMAP_DEPTH_RENDER:
	{
		WVP = sphereWorld * camera->GetViewMatrix() * camera->GetProjection();
		cbPerObj.mWorld = XMMatrixTranspose(sphereWorld);
		cbPerObj.mProjection = XMMatrixTranspose(camera->GetProjection());
		cbPerObj.mWorldVievProjection = XMMatrixTranspose(WVP);
	}
	break;
	case D3D11Framework::CUBEMAP_COLOR_RENDER:
	{
		WVP = sphereWorld * camera->GetViewMatrix() * camera->GetProjection();
		cbPerObj.mWorld = XMMatrixTranspose(sphereWorld);
		cbPerObj.mProjection = XMMatrixTranspose(camera->GetProjection());
		cbPerObj.mWorldVievProjection = XMMatrixTranspose(WVP);
	}
	break;
	default:
		break;
	}

	//m_pUniformBuffer->Bind(CUSTOM0_UB_BP, VERTEX_SHADER);
	//m_pDiffuseTexture->Bind(COLOR_TEX_BP, PIXEL_SHADER);

	_MUTEXLOCK(Engine::GetRender()->GetFramework()->GetMutex());
	Engine::GetRender()->GetDeviceContext()->PSSetSamplers(0, 1, &m_pCubesTexSamplerState);
	Engine::GetRender()->GetDeviceContext()->OMSetDepthStencilState(m_pDSLessEqual, 0);
	Engine::GetRender()->GetDeviceContext()->RSSetState(m_pRSCullNone);
	_MUTEXUNLOCK(Engine::GetRender()->GetFramework()->GetMutex());

	switch (rendertype)
	{
	case D3D11Framework::GBUFFER_FILL_RENDER:
		_MUTEXLOCK(Engine::GetRender()->GetFramework()->GetMutex());
		Engine::GetRender()->GetDeviceContext()->DrawIndexed(NumSphereFaces * 3, 0, 0);
		_MUTEXUNLOCK(Engine::GetRender()->GetFramework()->GetMutex());

		break;
	case D3D11Framework::CUBEMAP_DEPTH_RENDER:
		_MUTEXLOCK(Engine::GetRender()->GetFramework()->GetMutex());
		Engine::GetRender()->GetDeviceContext()->DrawIndexedInstanced(NumSphereFaces * 3, 6, 0, 0, 0);
		_MUTEXUNLOCK(Engine::GetRender()->GetFramework()->GetMutex());

		break;
	case D3D11Framework::CUBEMAP_COLOR_RENDER:
		_MUTEXLOCK(Engine::GetRender()->GetFramework()->GetMutex());
		Engine::GetRender()->GetDeviceContext()->DrawIndexedInstanced(NumSphereFaces * 3, 6, 0, 0, 0);
		_MUTEXUNLOCK(Engine::GetRender()->GetFramework()->GetMutex());
		break;
	default:
		break;
	}
}

bool SkyBox::CreateSphere(int LatLines, int LongLines)
{
	HRESULT hr;
	NumSphereVertices = ((LatLines - 2) * LongLines) + 2;
	NumSphereFaces = ((LatLines - 3)*(LongLines)* 2) + (LongLines * 2);

	float sphereYaw = 0.0f;
	float spherePitch = 0.0f;

	SkyBoxVertex *vertices = new SkyBoxVertex[NumSphereVertices];

	XMVECTOR currVertPos = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

	vertices[0].pos.x = 0.0f;
	vertices[0].pos.y = 0.0f;
	vertices[0].pos.z = 1.0f;

	for (DWORD i = 0; i < LatLines - 2; ++i)
	{
		spherePitch = (i + 1) * (3.14 / (LatLines - 1));
		Rotationx = XMMatrixRotationX(spherePitch);
		for (DWORD j = 0; j < LongLines; ++j)
		{
			sphereYaw = j * (6.28 / (LongLines));
			Rotationy = XMMatrixRotationZ(sphereYaw);
			currVertPos = XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), (Rotationx * Rotationy));
			currVertPos = XMVector3Normalize(currVertPos);
			vertices[i*LongLines + j + 1].pos.x = XMVectorGetX(currVertPos);
			vertices[i*LongLines + j + 1].pos.y = XMVectorGetY(currVertPos);
			vertices[i*LongLines + j + 1].pos.z = XMVectorGetZ(currVertPos);
		}
	}

	vertices[NumSphereVertices - 1].pos.x = 0.0f;
	vertices[NumSphereVertices - 1].pos.y = 0.0f;
	vertices[NumSphereVertices - 1].pos.z = -1.0f;

	D3D11_BUFFER_DESC vertexBufferDesc;
	ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(SkyBoxVertex)* NumSphereVertices;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vertexBufferData;

	ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
	vertexBufferData.pSysMem = vertices;
	hr = Engine::GetRender()->GetDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &m_pSphereVertexBuffer);


	DWORD *indices = new DWORD[NumSphereFaces * 3];

	int k = 0;
	for (DWORD l = 0; l < LongLines - 1; ++l)
	{
		indices[k] = 0;
		indices[k + 1] = l + 1;
		indices[k + 2] = l + 2;
		k += 3;
	}

	indices[k] = 0;
	indices[k + 1] = LongLines;
	indices[k + 2] = 1;
	k += 3;

	for (DWORD i = 0; i < LatLines - 3; ++i)
	{
		for (DWORD j = 0; j < LongLines - 1; ++j)
		{
			indices[k] = i*LongLines + j + 1;
			indices[k + 1] = i*LongLines + j + 2;
			indices[k + 2] = (i + 1)*LongLines + j + 1;

			indices[k + 3] = (i + 1)*LongLines + j + 1;
			indices[k + 4] = i*LongLines + j + 2;
			indices[k + 5] = (i + 1)*LongLines + j + 2;

			k += 6;
		}

		indices[k] = (i*LongLines) + LongLines;
		indices[k + 1] = (i*LongLines) + 1;
		indices[k + 2] = ((i + 1)*LongLines) + LongLines;

		indices[k + 3] = ((i + 1)*LongLines) + LongLines;
		indices[k + 4] = (i*LongLines) + 1;
		indices[k + 5] = ((i + 1)*LongLines) + 1;

		k += 6;
	}

	for (DWORD l = 0; l < LongLines - 1; ++l)
	{
		indices[k] = NumSphereVertices - 1;
		indices[k + 1] = (NumSphereVertices - 1) - (l + 1);
		indices[k + 2] = (NumSphereVertices - 1) - (l + 2);
		k += 3;
	}

	indices[k] = NumSphereVertices - 1;
	indices[k + 1] = (NumSphereVertices - 1) - LongLines;
	indices[k + 2] = NumSphereVertices - 2;

	D3D11_BUFFER_DESC indexBufferDesc;
	ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));

	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(DWORD) * NumSphereFaces * 3;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA iinitData;

	iinitData.pSysMem = indices;
	Engine::GetRender()->GetDevice()->CreateBuffer(&indexBufferDesc, &iinitData, &m_pSphereIndexBuffer);

	return true;
}