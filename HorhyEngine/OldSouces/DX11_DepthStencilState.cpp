#include "stdafx.h"
#include "Engine.h"
#include "Render.h"
#include "DX11_DepthStencilState.h"

using namespace D3D11Framework;

void DX11_DepthStencilState::Release()
{
	SAFE_RELEASE(depthStencilState);
}

bool DX11_DepthStencilState::Create(const DepthStencilDesc &desc)
{
	this->desc = desc;
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	depthStencilDesc.DepthEnable = desc.depthTest;
	depthStencilDesc.DepthWriteMask = (D3D11_DEPTH_WRITE_MASK)desc.depthMask;
	depthStencilDesc.DepthFunc = (D3D11_COMPARISON_FUNC)desc.depthFunc;
	depthStencilDesc.StencilEnable = desc.stencilTest;
	depthStencilDesc.StencilReadMask = desc.stencilMask;
	depthStencilDesc.StencilWriteMask = desc.stencilMask;
	depthStencilDesc.FrontFace.StencilFailOp = (D3D11_STENCIL_OP)desc.stencilFailOp;
	depthStencilDesc.FrontFace.StencilDepthFailOp = (D3D11_STENCIL_OP)desc.stencilDepthFailOp;
	depthStencilDesc.FrontFace.StencilPassOp = (D3D11_STENCIL_OP)desc.stencilPassOp;
	depthStencilDesc.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)desc.stencilFunc;
	depthStencilDesc.BackFace.StencilFailOp = (D3D11_STENCIL_OP)desc.stencilFailOp;
	depthStencilDesc.BackFace.StencilDepthFailOp = (D3D11_STENCIL_OP)desc.stencilDepthFailOp;
	depthStencilDesc.BackFace.StencilPassOp = (D3D11_STENCIL_OP)desc.stencilPassOp;
	depthStencilDesc.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)desc.stencilFunc;

	if (Engine::GetRender()->GetDevice()->CreateDepthStencilState(&depthStencilDesc, &depthStencilState) != S_OK)
		return false;

	return true;
}

void DX11_DepthStencilState::Set() const
{
	Engine::GetRender()->GetDeviceContext()->OMSetDepthStencilState(depthStencilState, desc.stencilRef);
}

