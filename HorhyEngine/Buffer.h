#pragma once

namespace D3D11Framework
{
	class Buffer
	{
	public:
		static ID3D11Buffer* CreateVertexBuffer(ID3D11Device *device, int size, bool dynamic, const void *Mem);
		static ID3D11Buffer* CreateIndexBuffer(ID3D11Device *device, int size, bool dynamic, const void *Mem);
		static ID3D11Buffer* CreateConstantBuffer(ID3D11Device *device, int size, bool dynamic, const void *Mem);
	};
}