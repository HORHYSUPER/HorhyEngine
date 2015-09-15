#include "stdafx.h"
#include "Engine.h"
#include "Render.h"
#include "DX11_RasterizerState.h"

using namespace D3D11Framework;

void DX11_RasterizerState::Release()
{
	SAFE_RELEASE(rasterizerState);
}

bool DX11_RasterizerState::Create(const RasterizerDesc &desc)
{
	this->desc = desc;
	D3D11_RASTERIZER_DESC rasterDesc;
	ZeroMemory(&rasterDesc, sizeof(D3D11_RASTERIZER_DESC));
	rasterDesc.FillMode = (D3D11_FILL_MODE)desc.fillMode;
	rasterDesc.CullMode = (D3D11_CULL_MODE)desc.cullMode;
	rasterDesc.FrontCounterClockwise = desc.frontCounterClockwise;
	rasterDesc.DepthBias = desc.depthBias;
	rasterDesc.DepthBiasClamp = desc.depthBiasClamp;
	rasterDesc.SlopeScaledDepthBias = desc.slopeScaledDepthBias;
	rasterDesc.ScissorEnable = desc.scissorTest;
	rasterDesc.MultisampleEnable = desc.multisampleEnable;
	rasterDesc.AntialiasedLineEnable = desc.antialiasedLineEnable;

	if (Engine::GetRender()->GetDevice()->CreateRasterizerState(&rasterDesc, &rasterizerState) != S_OK)
		return false;

	return true;
}

void DX11_RasterizerState::Set() const
{
	Engine::GetRender()->GetDeviceContext()->RSSetState(rasterizerState);
}


