#include "stdafx.h"
#include "Engine.h"
#include "Render.h"
#include "DX11_BlendState.h"

using namespace D3D11Framework;

void DX11_BlendState::Release()
{
	SAFE_RELEASE(blendState);
}

bool DX11_BlendState::Create(const BlendDesc &desc)
{
	this->desc = desc;
	D3D11_BLEND_DESC blendStateDesc;
	ZeroMemory(&blendStateDesc, sizeof(D3D11_BLEND_DESC));
	blendStateDesc.AlphaToCoverageEnable = FALSE;
	blendStateDesc.IndependentBlendEnable = FALSE;
	blendStateDesc.RenderTarget[0].BlendEnable = desc.blend;
	blendStateDesc.RenderTarget[0].SrcBlend = (D3D11_BLEND)desc.srcColorBlend;
	blendStateDesc.RenderTarget[0].DestBlend = (D3D11_BLEND)desc.dstColorBlend;
	blendStateDesc.RenderTarget[0].BlendOp = (D3D11_BLEND_OP)desc.blendColorOp;
	blendStateDesc.RenderTarget[0].SrcBlendAlpha = (D3D11_BLEND)desc.srcAlphaBlend;
	blendStateDesc.RenderTarget[0].DestBlendAlpha = (D3D11_BLEND)desc.dstAlphaBlend;
	blendStateDesc.RenderTarget[0].BlendOpAlpha = (D3D11_BLEND_OP)desc.blendAlphaOp;
	blendStateDesc.RenderTarget[0].RenderTargetWriteMask = desc.colorMask;

	if(Engine::GetRender()->GetDevice()->CreateBlendState(&blendStateDesc, &blendState) != S_OK)
		return false;
	
  return true;
}

void DX11_BlendState::Set() const
{
	Engine::GetRender()->GetDeviceContext()->OMSetBlendState(blendState, desc.constBlendColor, 0xFFFFFFFF);
}


