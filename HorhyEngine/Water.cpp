#include "stdafx.h"
#include "Water.h"
#include "Shader.h"
#include "Buffer.h"
#include "macros.h"

using namespace D3D11Framework;

// Disable warning "conditional expression is constant"
#pragma warning(disable:4127)

#define COHERENCY_GRANULARITY 128
#define HALF_SQRT_2	0.7071068f
#define GRAV_ACCEL	981.0f	// The acceleration of gravity, cm/s^2
#define TWO_PI 6.283185307179586476925286766559
#define FFT_DIMENSIONS 3U
#define FFT_PLAN_SIZE_LIMIT (1U << 27)
#define FFT_FORWARD -1
#define FFT_INVERSE 1
#define BLOCK_SIZE_X 16
#define BLOCK_SIZE_Y 16

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)       { if (p) { delete (p);     (p)=NULL; } }
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p);   (p)=NULL; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return DXUTTrace( __FILE__, (DWORD)__LINE__, hr, L#x, true ); } }
#endif

HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	// find the file
	WCHAR str[MAX_PATH];
	//V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

	// open the file
	HANDLE hFile = CreateFile(szFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (INVALID_HANDLE_VALUE == hFile)
		return E_FAIL;

	// Get the file size
	LARGE_INTEGER FileSize;
	GetFileSizeEx(hFile, &FileSize);

	// create enough space for the file data
	BYTE* pFileData = new BYTE[FileSize.LowPart];
	if (!pFileData)
		return E_OUTOFMEMORY;

	// read the data in
	DWORD BytesRead;
	if (!ReadFile(hFile, pFileData, FileSize.LowPart, &BytesRead, NULL))
		return E_FAIL;

	CloseHandle(hFile);

	// Compile the shader
	char pFilePathName[MAX_PATH];
	WideCharToMultiByte(CP_ACP, 0, str, -1, pFilePathName, MAX_PATH, NULL, NULL);
	ID3DBlob* pErrorBlob;
	hr = D3DCompile(pFileData, FileSize.LowPart, pFilePathName, NULL, NULL, szEntryPoint, szShaderModel, D3D10_SHADER_ENABLE_STRICTNESS, 0, ppBlobOut, &pErrorBlob);

	delete[]pFileData;

	if (FAILED(hr))
	{
		OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		SAFE_RELEASE(pErrorBlob);
		return hr;
	}
	SAFE_RELEASE(pErrorBlob);

	return S_OK;
}

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)       { if (p) { delete (p);     (p)=NULL; } }
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p);   (p)=NULL; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

struct Const_Shading
{
	// Water-reflected sky color
	XMVECTOR		SkyColor;
	// The color of bottomless water body
	XMVECTOR		WaterbodyColor;

	// The strength, direction and color of sun streak
	float			Shineness;
	XMVECTOR		SunDir;
	XMVECTOR		SunColor;

	// The parameter is used for fixing an artifact
	XMVECTOR		BendParam;

	// Perlin noise for distant wave crest
	float			PerlinSize;
	XMVECTOR		PerlinAmplitude;
	XMVECTOR		PerlinOctave;
	XMVECTOR		PerlinGradient;

	// Constants for calculating texcoord from position
	float			TexelLength_x2;
	float			UVScale;
	float			UVOffset;
};

#define FRESNEL_TEX_SIZE			256
#define PERLIN_TEX_SIZE				64

#define MESH_INDEX_2D(x, y)	(((y) + vert_rect.bottom) * (m_MeshDim + 1) + (x) + vert_rect.left)

bool isLeaf(const QuadNode& quad_node)
{
	return (quad_node.sub_node[0] == -1 && quad_node.sub_node[1] == -1 && quad_node.sub_node[2] == -1 && quad_node.sub_node[3] == -1);
}

int searchLeaf(const std::vector<QuadNode>& node_list, const XMVECTOR& point)
{
	int index = -1;

	int size = (int)node_list.size();
	QuadNode node = node_list[size - 1];

	while (!isLeaf(node))
	{
		bool found = false;

		for (int i = 0; i < 4; i++)
		{
			index = node.sub_node[i];
			if (index == -1)
				continue;

			QuadNode sub_node = node_list[index];
			if (point.m128_f32[0] >= sub_node.bottom_left.m128_f32[0] && point.m128_f32[0] <= sub_node.bottom_left.m128_f32[0] + sub_node.length &&
				point.m128_f32[1] >= sub_node.bottom_left.m128_f32[1] && point.m128_f32[1] <= sub_node.bottom_left.m128_f32[1] + sub_node.length)
			{
				node = sub_node;
				found = true;
				break;
			}
		}

		if (!found)
			return -1;
	}

	return index;
}

Water::QuadRenderParam& Water::selectMeshPattern(const QuadNode& quad_node)
{
	// Check 4 adjacent quad.
	XMVECTOR point_left = XMVectorSet(quad_node.bottom_left.m128_f32[0], quad_node.bottom_left.m128_f32[1], 0.0f, 0.0f) + XMVectorSet(-m_param.patch_length * 0.5f, quad_node.length * 0.5f, 0.0f, 0.0f);
	int left_adj_index = searchLeaf(m_render_list, point_left);

	XMVECTOR point_right = XMVectorSet(quad_node.bottom_left.m128_f32[0], quad_node.bottom_left.m128_f32[1], 0.0f, 0.0f) + XMVectorSet(quad_node.length + m_param.patch_length * 0.5f, quad_node.length * 0.5f, 0.0f, 0.0f);
	int right_adj_index = searchLeaf(m_render_list, point_right);

	XMVECTOR point_bottom = XMVectorSet(quad_node.bottom_left.m128_f32[0], quad_node.bottom_left.m128_f32[1], 0.0f, 0.0f) + XMVectorSet(quad_node.length * 0.5f, -m_param.patch_length * 0.5f, 0.0f, 0.0f);
	int bottom_adj_index = searchLeaf(m_render_list, point_bottom);

	XMVECTOR point_top = XMVectorSet(quad_node.bottom_left.m128_f32[0], quad_node.bottom_left.m128_f32[1], 0.0f, 0.0f) + XMVectorSet(quad_node.length * 0.5f, quad_node.length + m_param.patch_length * 0.5f, 0.0f, 0.0f);
	int top_adj_index = searchLeaf(m_render_list, point_top);

	int left_type = 0;
	if (left_adj_index != -1 && m_render_list[left_adj_index].length > quad_node.length * 0.999f)
	{
		QuadNode adj_node = m_render_list[left_adj_index];
		float scale = adj_node.length / quad_node.length * (m_MeshDim >> quad_node.lod) / (m_MeshDim >> adj_node.lod);
		if (scale > 3.999f)
			left_type = 2;
		else if (scale > 1.999f)
			left_type = 1;
	}

	int right_type = 0;
	if (right_adj_index != -1 && m_render_list[right_adj_index].length > quad_node.length * 0.999f)
	{
		QuadNode adj_node = m_render_list[right_adj_index];
		float scale = adj_node.length / quad_node.length * (m_MeshDim >> quad_node.lod) / (m_MeshDim >> adj_node.lod);
		if (scale > 3.999f)
			right_type = 2;
		else if (scale > 1.999f)
			right_type = 1;
	}

	int bottom_type = 0;
	if (bottom_adj_index != -1 && m_render_list[bottom_adj_index].length > quad_node.length * 0.999f)
	{
		QuadNode adj_node = m_render_list[bottom_adj_index];
		float scale = adj_node.length / quad_node.length * (m_MeshDim >> quad_node.lod) / (m_MeshDim >> adj_node.lod);
		if (scale > 3.999f)
			bottom_type = 2;
		else if (scale > 1.999f)
			bottom_type = 1;
	}

	int top_type = 0;
	if (top_adj_index != -1 && m_render_list[top_adj_index].length > quad_node.length * 0.999f)
	{
		QuadNode adj_node = m_render_list[top_adj_index];
		float scale = adj_node.length / quad_node.length * (m_MeshDim >> quad_node.lod) / (m_MeshDim >> adj_node.lod);
		if (scale > 3.999f)
			top_type = 2;
		else if (scale > 1.999f)
			top_type = 1;
	}

	// Check lookup table, [L][R][B][T]
	return m_mesh_patterns[quad_node.lod][left_type][right_type][bottom_type][top_type];
}

// Generate boundary mesh for a patch. Return the number of generated indices
int Water::generateInnerMesh(RECT vert_rect, DWORD* output)
{
	int i, j;
	int counter = 0;
	int width = vert_rect.right - vert_rect.left;
	int height = vert_rect.top - vert_rect.bottom;

	bool reverse = false;
	for (i = 0; i < height; i++)
	{
		if (reverse == false)
		{
			output[counter++] = MESH_INDEX_2D(0, i);
			output[counter++] = MESH_INDEX_2D(0, i + 1);
			for (j = 0; j < width; j++)
			{
				output[counter++] = MESH_INDEX_2D(j + 1, i);
				output[counter++] = MESH_INDEX_2D(j + 1, i + 1);
			}
		}
		else
		{
			output[counter++] = MESH_INDEX_2D(width, i);
			output[counter++] = MESH_INDEX_2D(width, i + 1);
			for (j = width - 1; j >= 0; j--)
			{
				output[counter++] = MESH_INDEX_2D(j, i);
				output[counter++] = MESH_INDEX_2D(j, i + 1);
			}
		}

		reverse = !reverse;
	}

	return counter;
}
// Generate boundary mesh for a patch. Return the number of generated indices
int Water::generateBoundaryMesh(int left_degree, int right_degree, int bottom_degree, int top_degree,
	RECT vert_rect, DWORD* output)
{
	// Triangle list for bottom boundary
	int i, j;
	int counter = 0;
	int width = vert_rect.right - vert_rect.left;

	if (bottom_degree > 0)
	{
		int b_step = width / bottom_degree;

		for (i = 0; i < width; i += b_step)
		{
			output[counter++] = MESH_INDEX_2D(i, 0);
			output[counter++] = MESH_INDEX_2D(i + b_step / 2, 1);
			output[counter++] = MESH_INDEX_2D(i + b_step, 0);

			for (j = 0; j < b_step / 2; j++)
			{
				if (i == 0 && j == 0 && left_degree > 0)
					continue;

				output[counter++] = MESH_INDEX_2D(i, 0);
				output[counter++] = MESH_INDEX_2D(i + j, 1);
				output[counter++] = MESH_INDEX_2D(i + j + 1, 1);
			}

			for (j = b_step / 2; j < b_step; j++)
			{
				if (i == width - b_step && j == b_step - 1 && right_degree > 0)
					continue;

				output[counter++] = MESH_INDEX_2D(i + b_step, 0);
				output[counter++] = MESH_INDEX_2D(i + j, 1);
				output[counter++] = MESH_INDEX_2D(i + j + 1, 1);
			}
		}
	}

	// Right boundary
	int height = vert_rect.top - vert_rect.bottom;

	if (right_degree > 0)
	{
		int r_step = height / right_degree;

		for (i = 0; i < height; i += r_step)
		{
			output[counter++] = MESH_INDEX_2D(width, i);
			output[counter++] = MESH_INDEX_2D(width - 1, i + r_step / 2);
			output[counter++] = MESH_INDEX_2D(width, i + r_step);

			for (j = 0; j < r_step / 2; j++)
			{
				if (i == 0 && j == 0 && bottom_degree > 0)
					continue;

				output[counter++] = MESH_INDEX_2D(width, i);
				output[counter++] = MESH_INDEX_2D(width - 1, i + j);
				output[counter++] = MESH_INDEX_2D(width - 1, i + j + 1);
			}

			for (j = r_step / 2; j < r_step; j++)
			{
				if (i == height - r_step && j == r_step - 1 && top_degree > 0)
					continue;

				output[counter++] = MESH_INDEX_2D(width, i + r_step);
				output[counter++] = MESH_INDEX_2D(width - 1, i + j);
				output[counter++] = MESH_INDEX_2D(width - 1, i + j + 1);
			}
		}
	}

	// Top boundary
	if (top_degree > 0)
	{
		int t_step = width / top_degree;

		for (i = 0; i < width; i += t_step)
		{
			output[counter++] = MESH_INDEX_2D(i, height);
			output[counter++] = MESH_INDEX_2D(i + t_step / 2, height - 1);
			output[counter++] = MESH_INDEX_2D(i + t_step, height);

			for (j = 0; j < t_step / 2; j++)
			{
				if (i == 0 && j == 0 && left_degree > 0)
					continue;

				output[counter++] = MESH_INDEX_2D(i, height);
				output[counter++] = MESH_INDEX_2D(i + j, height - 1);
				output[counter++] = MESH_INDEX_2D(i + j + 1, height - 1);
			}

			for (j = t_step / 2; j < t_step; j++)
			{
				if (i == width - t_step && j == t_step - 1 && right_degree > 0)
					continue;

				output[counter++] = MESH_INDEX_2D(i + t_step, height);
				output[counter++] = MESH_INDEX_2D(i + j, height - 1);
				output[counter++] = MESH_INDEX_2D(i + j + 1, height - 1);
			}
		}
	}

	// Left boundary
	if (left_degree > 0)
	{
		int l_step = height / left_degree;

		for (i = 0; i < height; i += l_step)
		{
			output[counter++] = MESH_INDEX_2D(0, i);
			output[counter++] = MESH_INDEX_2D(1, i + l_step / 2);
			output[counter++] = MESH_INDEX_2D(0, i + l_step);

			for (j = 0; j < l_step / 2; j++)
			{
				if (i == 0 && j == 0 && bottom_degree > 0)
					continue;

				output[counter++] = MESH_INDEX_2D(0, i);
				output[counter++] = MESH_INDEX_2D(1, i + j);
				output[counter++] = MESH_INDEX_2D(1, i + j + 1);
			}

			for (j = l_step / 2; j < l_step; j++)
			{
				if (i == height - l_step && j == l_step - 1 && top_degree > 0)
					continue;

				output[counter++] = MESH_INDEX_2D(0, i + l_step);
				output[counter++] = MESH_INDEX_2D(1, i + j);
				output[counter++] = MESH_INDEX_2D(1, i + j + 1);
			}
		}
	}

	return counter;
}

void Water::createSurfaceMesh()
{
	// --------------------------------- Vertex Buffer -------------------------------
	int num_verts = (m_MeshDim + 1) * (m_MeshDim + 1);
	ocean_vertex* pV = new ocean_vertex[num_verts];
	assert(pV);

	int i, j;
	for (i = 0; i <= m_MeshDim; i++)
	{
		for (j = 0; j <= m_MeshDim; j++)
		{
			pV[i * (m_MeshDim + 1) + j].index_x = (float)j;
			pV[i * (m_MeshDim + 1) + j].index_y = (float)i;
		}
	}

	D3D11_BUFFER_DESC vb_desc;
	vb_desc.ByteWidth = num_verts * sizeof(ocean_vertex);
	vb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vb_desc.CPUAccessFlags = 0;
	vb_desc.MiscFlags = 0;
	vb_desc.StructureByteStride = sizeof(ocean_vertex);

	D3D11_SUBRESOURCE_DATA init_data;
	init_data.pSysMem = pV;
	init_data.SysMemPitch = 0;
	init_data.SysMemSlicePitch = 0;

	SAFE_RELEASE(m_pMeshVB);
	m_pFramework->GetRender()->GetDevice()->CreateBuffer(&vb_desc, &init_data, &m_pMeshVB);
	assert(m_pMeshVB);

	SAFE_DELETE_ARRAY(pV);


	// --------------------------------- Index Buffer -------------------------------
	// The index numbers for all mesh LODs (up to 256x256)
	const int index_size_lookup[] = { 0, 0, 4284, 18828, 69444, 254412, 956916, 3689820, 14464836 };

	memset(&m_mesh_patterns[0][0][0][0][0], 0, sizeof(m_mesh_patterns));

	m_Lods = 0;
	for (i = m_MeshDim; i > 1; i >>= 1)
		m_Lods++;

	// Generate patch meshes. Each patch contains two parts: the inner mesh which is a regular
	// grids in a triangle strip. The boundary mesh is constructed w.r.t. the edge degrees to
	// meet water-tight requirement.
	DWORD* index_array = new DWORD[index_size_lookup[m_Lods]];
	assert(index_array);

	int offset = 0;
	int level_size = m_MeshDim;

	// Enumerate patterns
	for (int level = 0; level <= m_Lods - 2; level++)
	{
		int left_degree = level_size;

		for (int left_type = 0; left_type < 3; left_type++)
		{
			int right_degree = level_size;

			for (int right_type = 0; right_type < 3; right_type++)
			{
				int bottom_degree = level_size;

				for (int bottom_type = 0; bottom_type < 3; bottom_type++)
				{
					int top_degree = level_size;

					for (int top_type = 0; top_type < 3; top_type++)
					{
						QuadRenderParam* pattern = &m_mesh_patterns[level][left_type][right_type][bottom_type][top_type];

						// Inner mesh (triangle strip)
						RECT inner_rect;
						inner_rect.left = (left_degree == level_size) ? 0 : 1;
						inner_rect.right = (right_degree == level_size) ? level_size : level_size - 1;
						inner_rect.bottom = (bottom_degree == level_size) ? 0 : 1;
						inner_rect.top = (top_degree == level_size) ? level_size : level_size - 1;

						int num_new_indices = generateInnerMesh(inner_rect, index_array + offset);

						pattern->inner_start_index = offset;
						pattern->num_inner_verts = (level_size + 1) * (level_size + 1);
						pattern->num_inner_faces = num_new_indices - 2;
						offset += num_new_indices;

						// Boundary mesh (triangle list)
						int l_degree = (left_degree == level_size) ? 0 : left_degree;
						int r_degree = (right_degree == level_size) ? 0 : right_degree;
						int b_degree = (bottom_degree == level_size) ? 0 : bottom_degree;
						int t_degree = (top_degree == level_size) ? 0 : top_degree;

						RECT outer_rect = { 0, level_size, level_size, 0 };
						num_new_indices = generateBoundaryMesh(l_degree, r_degree, b_degree, t_degree, outer_rect, index_array + offset);

						pattern->boundary_start_index = offset;
						pattern->num_boundary_verts = (level_size + 1) * (level_size + 1);
						pattern->num_boundary_faces = num_new_indices / 3;
						offset += num_new_indices;

						top_degree /= 2;
					}
					bottom_degree /= 2;
				}
				right_degree /= 2;
			}
			left_degree /= 2;
		}
		level_size /= 2;
	}

	assert(offset == index_size_lookup[m_Lods]);

	D3D11_BUFFER_DESC ib_desc;
	ib_desc.ByteWidth = index_size_lookup[m_Lods] * sizeof(DWORD);
	ib_desc.Usage = D3D11_USAGE_IMMUTABLE;
	ib_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ib_desc.CPUAccessFlags = 0;
	ib_desc.MiscFlags = 0;
	ib_desc.StructureByteStride = sizeof(DWORD);

	init_data.pSysMem = index_array;

	SAFE_RELEASE(m_pMeshIB);
	m_pFramework->GetRender()->GetDevice()->CreateBuffer(&ib_desc, &init_data, &m_pMeshIB);
	assert(m_pMeshIB);

	SAFE_DELETE_ARRAY(index_array);
}

void Water::createFresnelMap()
{
	DWORD* buffer = new DWORD[FRESNEL_TEX_SIZE];
	for (int i = 0; i < FRESNEL_TEX_SIZE; i++)
	{
		float cos_a = i / (FLOAT)FRESNEL_TEX_SIZE;
		// Using water's refraction index 1.33
		/*DWORD fresnel = (DWORD)(D3DXFresnelTerm(cos_a, 1.33f) * 255);

		DWORD sky_blend = (DWORD)(powf(1 / (1 + cos_a), m_SkyBlending) * 255);

		buffer[i] = (sky_blend << 8) | fresnel;*/
	}

	D3D11_TEXTURE1D_DESC tex_desc;
	tex_desc.Width = FRESNEL_TEX_SIZE;
	tex_desc.MipLevels = 1;
	tex_desc.ArraySize = 1;
	tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
	tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	tex_desc.CPUAccessFlags = 0;
	tex_desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA init_data;
	init_data.pSysMem = buffer;
	init_data.SysMemPitch = 0;
	init_data.SysMemSlicePitch = 0;

	m_pFramework->GetRender()->GetDevice()->CreateTexture1D(&tex_desc, &init_data, &m_pFresnelMap);
	assert(m_pFresnelMap);

	SAFE_DELETE_ARRAY(buffer);

	// Create shader resource
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
	srv_desc.Texture1D.MipLevels = 1;
	srv_desc.Texture1D.MostDetailedMip = 0;

	m_pFramework->GetRender()->GetDevice()->CreateShaderResourceView(m_pFresnelMap, &srv_desc, &m_pSRV_Fresnel);
	assert(m_pSRV_Fresnel);
}

HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut);

bool Water::initRenderResource(const OceanParameter& ocean_param, ID3D11Device* pd3dDevice)
{
	m_FurthestCover = 8;
	m_UpperGridCoverage = 64.0f;
	m_PerlinSpeed = 0.06f;
	m_PerlinSize = 1.0f;
	m_MeshDim = 128;
	m_Lods = 0;
	m_PerlinSize = 1.0f;
	m_PerlinSpeed = 0.06f;
	m_PerlinAmplitude = XMVectorSet(35, 42, 57, 0.0f);
	m_PerlinGradient = XMVectorSet(1.4f, 1.6f, 2.2f, 0.0f);
	m_PerlinOctave = XMVectorSet(1.12f, 0.59f, 0.23f, 0.0f);
	m_BendParam = XMVectorSet(0.1f, -0.4f, 0.2f, 0.0f);
	m_SunDir = XMVectorSet(0.936016f, -0.343206f, 0.0780013f, 0.0f);
	m_SunColor = XMVectorSet(1.0f, 1.0f, 0.6f, 0.0f);
	m_Shineness = 400.0f;
	m_SkyBlending = 16.0f;

	// D3D buffers
	createSurfaceMesh();
	createFresnelMap();

	D3DX11CreateShaderResourceViewFromFile(pd3dDevice, L"perlin_noise.dds", NULL, NULL, &m_pSRV_Perlin, NULL);
	assert(m_pSRV_Perlin);

	D3DX11CreateShaderResourceViewFromFile(pd3dDevice, L"skymap.dds", NULL, NULL, &m_pSRV_ReflectCube, NULL);
	assert(m_pSRV_ReflectCube);

	m_pSurfaceShader = new Shader(m_pFramework->GetRender());
	if (!m_pSurfaceShader)
		return false;

	D3D_SHADER_MACRO    shaderMacro[] =
	{ "DEPTH_ONLY", "0", NULL, NULL };

	m_pSurfaceShader->AddInputElementDesc("POSITION", 0, DXGI_FORMAT_R32G32_FLOAT);
	
	std::wstring shaderName;
	shaderName = *m_pFramework->GetSystemPath(PATH_TO_SHADERS) + L"Water.hlsl";

	if (!m_pSurfaceShader->CreateShaderVs((WCHAR*)shaderName.c_str(), shaderMacro, "VS"))
		return false;

	if (!m_pSurfaceShader->CreateShaderPs((WCHAR*)shaderName.c_str(), shaderMacro, "PS"))
		return false;

	m_pDepthShader = new Shader(m_pFramework->GetRender());
	if (!m_pDepthShader)
		return false;

	shaderMacro[0] =
	{ "DEPTH_ONLY", "0" };

	m_pDepthShader->AddInputElementDesc("POSITION", 0, DXGI_FORMAT_R32G32_FLOAT);
	if (!m_pDepthShader->CreateShaderVs((WCHAR*)shaderName.c_str(), shaderMacro, "VS_CubeMap_Inst"))
		return false;

	if (!m_pDepthShader->CreateShaderGs((WCHAR*)shaderName.c_str(), shaderMacro, "GS_CubeMap_Inst"))
		return false;

	if (!m_pDepthShader->CreateShaderPs((WCHAR*)shaderName.c_str(), shaderMacro, "PS_CubeMap_Inst"))
		return false;

	shaderMacro[0] =
	{ "DEPTH_ONLY", "1" };

	if (!m_pDepthShader->CreateShaderPs((WCHAR*)shaderName.c_str(), shaderMacro, "PS_CubeMap_Inst"))
		return false;

	shaderName = *m_pFramework->GetSystemPath(PATH_TO_SHADERS) + L"ocean_simulator_cs.hlsl";
	m_pSurfaceShader->CreateShaderCs((WCHAR*)shaderName.c_str(), NULL, "UpdateSpectrumCS");
	shaderName = *m_pFramework->GetSystemPath(PATH_TO_SHADERS) + L"ocean_simulator_vs_ps.hlsl";

	//m_pSurfaceShader->AddInputElementDesc("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT);

	m_pSurfaceShader->CreateShaderVs((WCHAR*)shaderName.c_str(), NULL, "QuadVS");
	m_pSurfaceShader->CreateShaderPs((WCHAR*)shaderName.c_str(), NULL, "UpdateDisplacementPS");
	m_pSurfaceShader->CreateShaderPs((WCHAR*)shaderName.c_str(), NULL, "GenGradientFoldingPS");

	m_pPerCallCB = Buffer::CreateConstantBuffer(m_pFramework->GetRender()->GetDevice(), (sizeof(Const_Per_Call)), true, NULL);
	if (!m_pPerCallCB)
		return false;
	
	// Height map H(0)
	int height_map_size = (m_param.dmap_dim + 4) * (m_param.dmap_dim + 1);
	XMVECTOR* h0_data = new XMVECTOR[height_map_size * sizeof(XMVECTOR)];
	float* omega_data = new float[height_map_size * sizeof(float)];
	initHeightMap(m_param, h0_data, omega_data);

	int hmap_dim = m_param.dmap_dim;
	int input_full_size = (hmap_dim + 4) * (hmap_dim + 1);
	// This value should be (hmap_dim / 2 + 1) * hmap_dim, but we use full sized buffer here for simplicity.
	int input_half_size = hmap_dim * hmap_dim;
	int output_size = hmap_dim * hmap_dim;

	// For filling the buffer with zeroes.
	char* zero_data = new char[3 * output_size * sizeof(XMVECTOR)];
	memset(zero_data, 0, 3 * output_size * sizeof(XMVECTOR));

	// RW buffer allocations
	// H0
	//UINT float2_stride = 2 * sizeof(float);
	CreateBufferAndUAV(h0_data, input_full_size * sizeof(XMVECTOR), sizeof(XMVECTOR), &m_pBuffer_Float2_H0, &m_pUAV_H0, &m_pSRV_H0);

	// Notice: The following 3 buffers should be half sized buffer because of conjugate symmetric input. But
	// we use full sized buffers due to the CS4.0 restriction.

	// Put H(t), Dx(t) and Dy(t) into one buffer because CS4.0 allows only 1 UAV at a time
	CreateBufferAndUAV(zero_data, 3 * input_half_size * sizeof(XMVECTOR), sizeof(XMVECTOR), &m_pBuffer_Float2_Ht, &m_pUAV_Ht, &m_pSRV_Ht);

	// omega
	CreateBufferAndUAV(omega_data, input_full_size * sizeof(float), sizeof(float), &m_pBuffer_Float_Omega, &m_pUAV_Omega, &m_pSRV_Omega);

	// Notice: The following 3 should be real number data. But here we use the complex numbers and C2C FFT
	// due to the CS4.0 restriction.
	// Put Dz, Dx and Dy into one buffer because CS4.0 allows only 1 UAV at a time
	CreateBufferAndUAV(zero_data, 3 * output_size * sizeof(XMVECTOR), sizeof(XMVECTOR), &m_pBuffer_Float_Dxyz, &m_pUAV_Dxyz, &m_pSRV_Dxyz);

	SAFE_DELETE_ARRAY(zero_data);
	SAFE_DELETE_ARRAY(h0_data);
	SAFE_DELETE_ARRAY(omega_data);

	// D3D11 Textures
	createTextureAndViews(hmap_dim, hmap_dim, DXGI_FORMAT_R32G32B32A32_FLOAT, &m_pDisplacementMap, &m_pDisplacementSRV, &m_pDisplacementRTV);
	createTextureAndViews(hmap_dim, hmap_dim, DXGI_FORMAT_R16G16B16A16_FLOAT, &m_pGradientMap, &m_pGradientSRV, &m_pGradientRTV);

	// Samplers
	D3D11_SAMPLER_DESC sam_desc;
	sam_desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	sam_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sam_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sam_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sam_desc.MipLODBias = 0;
	sam_desc.MaxAnisotropy = 1;
	sam_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sam_desc.BorderColor[0] = 1.0f;
	sam_desc.BorderColor[1] = 1.0f;
	sam_desc.BorderColor[2] = 1.0f;
	sam_desc.BorderColor[3] = 1.0f;
	sam_desc.MinLOD = -FLT_MAX;
	sam_desc.MaxLOD = FLT_MAX;
	m_pFramework->GetRender()->GetDevice()->CreateSamplerState(&sam_desc, &m_pPointSamplerState);
	assert(m_pPointSamplerState);

	// Quad vertex buffer
	D3D11_BUFFER_DESC vb_desc;
	vb_desc.ByteWidth = 4 * sizeof(XMVECTOR);
	vb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vb_desc.CPUAccessFlags = 0;
	vb_desc.MiscFlags = 0;

	float quad_verts[] =
	{
		-1, -1, 0, 1,
		-1, 1, 0, 1,
		1, -1, 0, 1,
		1, 1, 0, 1,
	};
	D3D11_SUBRESOURCE_DATA init_data;
	init_data.pSysMem = &quad_verts[0];
	init_data.SysMemPitch = 0;
	init_data.SysMemSlicePitch = 0;

	m_pFramework->GetRender()->GetDevice()->CreateBuffer(&vb_desc, &init_data, &m_pQuadVB);
	assert(m_pQuadVB);

	// Constant buffers
	UINT actual_dim = m_param.dmap_dim;
	UINT input_width = actual_dim + 4;
	// We use full sized data here. The value "output_width" should be actual_dim/2+1 though.
	UINT output_width = actual_dim;
	UINT output_height = actual_dim;
	UINT dtx_offset = actual_dim * actual_dim;
	UINT dty_offset = actual_dim * actual_dim * 2;
	UINT immutable_consts[] = { actual_dim, input_width, output_width, output_height, dtx_offset, dty_offset };
	D3D11_SUBRESOURCE_DATA init_cb0 = { &immutable_consts[0], 0, 0 };

	D3D11_BUFFER_DESC cb_desc;
	cb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.CPUAccessFlags = 0;
	cb_desc.MiscFlags = 0;
	cb_desc.ByteWidth = PAD16(sizeof(immutable_consts));
	m_pFramework->GetRender()->GetDevice()->CreateBuffer(&cb_desc, &init_cb0, &m_pImmutableCB);
	assert(m_pImmutableCB);

	ID3D11Buffer* cbs[1] = { m_pImmutableCB };
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->CSSetConstantBuffers(0, 1, cbs);
	m_pFramework->GetRender()->GetDeviceContext()->PSSetConstantBuffers(0, 1, cbs);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	cb_desc.Usage = D3D11_USAGE_DYNAMIC;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cb_desc.MiscFlags = 0;
	cb_desc.ByteWidth = PAD16(sizeof(float) * 3);
	m_pFramework->GetRender()->GetDevice()->CreateBuffer(&cb_desc, NULL, &m_pPerFrameCB);
	assert(m_pPerFrameCB);

	// FFT
	fft512x512_create_plan(&m_fft_plan, m_pFramework->GetRender()->GetDevice(), 3);

	Const_Shading shading_data;
	// Grid side length * 2
	shading_data.TexelLength_x2 = m_param.patch_length / m_param.dmap_dim * 2;;
	// Color
	shading_data.SkyColor = XMVectorSet(0.38f, 0.45f, 0.56f, 0.0f);
	shading_data.WaterbodyColor = XMVectorSet(0.07f, 0.15f, 0.2f, 0.0f);
	// Texcoord
	shading_data.UVScale = 1.0f / m_param.patch_length;
	shading_data.UVOffset = 0.5f / m_param.dmap_dim;
	// Perlin
	shading_data.PerlinSize = m_PerlinSize;
	shading_data.PerlinAmplitude = m_PerlinAmplitude;
	shading_data.PerlinGradient = m_PerlinGradient;
	shading_data.PerlinOctave = m_PerlinOctave;
	// Multiple reflection workaround
	shading_data.BendParam = m_BendParam;
	// Sun streaks
	shading_data.SunColor = m_SunColor;
	shading_data.SunDir = m_SunDir;
	shading_data.Shineness = m_Shineness;

	m_pShadingCB = Buffer::CreateConstantBuffer(m_pFramework->GetRender()->GetDevice(), sizeof(Const_Shading), false, &shading_data);
	if (!m_pShadingCB)
		return false;

	sam_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sam_desc.MinLOD = 0;
	sam_desc.MaxLOD = FLT_MAX;
	pd3dDevice->CreateSamplerState(&sam_desc, &m_pHeightSampler);
	assert(m_pHeightSampler);

	sam_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	pd3dDevice->CreateSamplerState(&sam_desc, &m_pCubeSampler);
	assert(m_pCubeSampler);

	sam_desc.Filter = D3D11_FILTER_ANISOTROPIC;
	sam_desc.MaxAnisotropy = 8;
	pd3dDevice->CreateSamplerState(&sam_desc, &m_pGradientSampler);
	assert(m_pGradientSampler);

	sam_desc.MaxLOD = FLT_MAX;
	sam_desc.MaxAnisotropy = 4;
	pd3dDevice->CreateSamplerState(&sam_desc, &m_pPerlinSampler);
	assert(m_pPerlinSampler);

	sam_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sam_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sam_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sam_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	pd3dDevice->CreateSamplerState(&sam_desc, &m_pFresnelSampler);
	assert(m_pFresnelSampler);

	// State blocks
	D3D11_RASTERIZER_DESC ras_desc;
	ras_desc.FillMode = D3D11_FILL_SOLID;
	ras_desc.CullMode = D3D11_CULL_NONE;
	ras_desc.FrontCounterClockwise = FALSE;
	ras_desc.DepthBias = 0;
	ras_desc.SlopeScaledDepthBias = 0.0f;
	ras_desc.DepthBiasClamp = 0.0f;
	ras_desc.DepthClipEnable = TRUE;
	ras_desc.ScissorEnable = FALSE;
	ras_desc.MultisampleEnable = TRUE;
	ras_desc.AntialiasedLineEnable = FALSE;

	pd3dDevice->CreateRasterizerState(&ras_desc, &m_pRSState_Solid);
	assert(m_pRSState_Solid);
}


void Water::radix008A(CSFFT512x512_Plan* fft_plan,
	ID3D11UnorderedAccessView* pUAV_Dst,
	ID3D11ShaderResourceView* pSRV_Src,
	UINT thread_count,
	UINT istride)
{
	// Setup execution configuration
	UINT grid = thread_count / COHERENCY_GRANULARITY;
	_MUTEXLOCK(m_pFramework->GetMutex());
	ID3D11DeviceContext* pd3dImmediateContext = m_pFramework->GetRender()->GetDeviceContext();
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	// Buffers
	ID3D11ShaderResourceView* cs_srvs[1] = { pSRV_Src };
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dImmediateContext->CSSetShaderResources(0, 1, cs_srvs);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	ID3D11UnorderedAccessView* cs_uavs[1] = { pUAV_Dst };
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, cs_uavs, (UINT*)(&cs_uavs[0]));
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	// Shader
	if (istride > 1)
	{
		_MUTEXLOCK(m_pFramework->GetMutex());
		pd3dImmediateContext->CSSetShader(m_pSurfaceShader->GetComputeShader(1), NULL, 0);
		_MUTEXUNLOCK(m_pFramework->GetMutex());
	}
	else
	{
		_MUTEXLOCK(m_pFramework->GetMutex());
		pd3dImmediateContext->CSSetShader(m_pSurfaceShader->GetComputeShader(2), NULL, 0);
		_MUTEXUNLOCK(m_pFramework->GetMutex());
	}

	// Execute
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dImmediateContext->Dispatch(grid, 1, 1);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	// Unbind resource
	cs_srvs[0] = NULL;
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dImmediateContext->CSSetShaderResources(0, 1, cs_srvs);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	cs_uavs[0] = NULL;
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, cs_uavs, (UINT*)(&cs_uavs[0]));
	_MUTEXUNLOCK(m_pFramework->GetMutex());
}

void Water::fft_512x512_c2c(CSFFT512x512_Plan* fft_plan,
	ID3D11UnorderedAccessView* pUAV_Dst,
	ID3D11ShaderResourceView* pSRV_Dst,
	ID3D11ShaderResourceView* pSRV_Src)
{
	const UINT thread_count = fft_plan->slices * (512 * 512) / 8;
	ID3D11UnorderedAccessView* pUAV_Tmp = fft_plan->pUAV_Tmp;
	ID3D11ShaderResourceView* pSRV_Tmp = fft_plan->pSRV_Tmp;
	_MUTEXLOCK(m_pFramework->GetMutex());
	ID3D11DeviceContext* pd3dImmediateContext = m_pFramework->GetRender()->GetDeviceContext();
	_MUTEXUNLOCK(m_pFramework->GetMutex());
	ID3D11Buffer* cs_cbs[1];

	UINT istride = 512 * 512 / 8;
	cs_cbs[0] = fft_plan->pRadix008A_CB[0];
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, &cs_cbs[0]);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
	radix008A(fft_plan, pUAV_Tmp, pSRV_Src, thread_count, istride);

	istride /= 8;
	cs_cbs[0] = fft_plan->pRadix008A_CB[1];
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, &cs_cbs[0]);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
	radix008A(fft_plan, pUAV_Dst, pSRV_Tmp, thread_count, istride);

	istride /= 8;
	cs_cbs[0] = fft_plan->pRadix008A_CB[2];
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, &cs_cbs[0]);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
	radix008A(fft_plan, pUAV_Tmp, pSRV_Dst, thread_count, istride);

	istride /= 8;
	cs_cbs[0] = fft_plan->pRadix008A_CB[3];
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, &cs_cbs[0]);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
	radix008A(fft_plan, pUAV_Dst, pSRV_Tmp, thread_count, istride);

	istride /= 8;
	cs_cbs[0] = fft_plan->pRadix008A_CB[4];
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, &cs_cbs[0]);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
	radix008A(fft_plan, pUAV_Tmp, pSRV_Dst, thread_count, istride);

	istride /= 8;
	cs_cbs[0] = fft_plan->pRadix008A_CB[5];
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, &cs_cbs[0]);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
	radix008A(fft_plan, pUAV_Dst, pSRV_Tmp, thread_count, istride);
}

void Water::create_cbuffers_512x512(CSFFT512x512_Plan* plan, ID3D11Device* pd3dDevice, UINT slices)
{
	// Create 6 cbuffers for 512x512 transform.

	D3D11_BUFFER_DESC cb_desc;
	cb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.CPUAccessFlags = 0;
	cb_desc.MiscFlags = 0;
	cb_desc.ByteWidth = 32;//sizeof(float) * 5;
	cb_desc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA cb_data;
	cb_data.SysMemPitch = 0;
	cb_data.SysMemSlicePitch = 0;

	struct CB_Structure
	{
		UINT thread_count;
		UINT ostride;
		UINT istride;
		UINT pstride;
		float phase_base;
	};

	// Buffer 0
	const UINT thread_count = slices * (512 * 512) / 8;
	UINT ostride = 512 * 512 / 8;
	UINT istride = ostride;
	double phase_base = -TWO_PI / (512.0 * 512.0);

	CB_Structure cb_data_buf0 = { thread_count, ostride, istride, 512, (float)phase_base };
	cb_data.pSysMem = &cb_data_buf0;

	pd3dDevice->CreateBuffer(&cb_desc, &cb_data, &plan->pRadix008A_CB[0]);
	assert(plan->pRadix008A_CB[0]);

	// Buffer 1
	istride /= 8;
	phase_base *= 8.0;

	CB_Structure cb_data_buf1 = { thread_count, ostride, istride, 512, (float)phase_base };
	cb_data.pSysMem = &cb_data_buf1;

	pd3dDevice->CreateBuffer(&cb_desc, &cb_data, &plan->pRadix008A_CB[1]);
	assert(plan->pRadix008A_CB[1]);

	// Buffer 2
	istride /= 8;
	phase_base *= 8.0;

	CB_Structure cb_data_buf2 = { thread_count, ostride, istride, 512, (float)phase_base };
	cb_data.pSysMem = &cb_data_buf2;

	pd3dDevice->CreateBuffer(&cb_desc, &cb_data, &plan->pRadix008A_CB[2]);
	assert(plan->pRadix008A_CB[2]);

	// Buffer 3
	istride /= 8;
	phase_base *= 8.0;
	ostride /= 512;

	CB_Structure cb_data_buf3 = { thread_count, ostride, istride, 1, (float)phase_base };
	cb_data.pSysMem = &cb_data_buf3;

	pd3dDevice->CreateBuffer(&cb_desc, &cb_data, &plan->pRadix008A_CB[3]);
	assert(plan->pRadix008A_CB[3]);

	// Buffer 4
	istride /= 8;
	phase_base *= 8.0;

	CB_Structure cb_data_buf4 = { thread_count, ostride, istride, 1, (float)phase_base };
	cb_data.pSysMem = &cb_data_buf4;

	pd3dDevice->CreateBuffer(&cb_desc, &cb_data, &plan->pRadix008A_CB[4]);
	assert(plan->pRadix008A_CB[4]);

	// Buffer 5
	istride /= 8;
	phase_base *= 8.0;

	CB_Structure cb_data_buf5 = { thread_count, ostride, istride, 1, (float)phase_base };
	cb_data.pSysMem = &cb_data_buf5;

	pd3dDevice->CreateBuffer(&cb_desc, &cb_data, &plan->pRadix008A_CB[5]);
	assert(plan->pRadix008A_CB[5]);
}

void Water::fft512x512_create_plan(CSFFT512x512_Plan* plan, ID3D11Device* pd3dDevice, UINT slices)
{
	plan->slices = slices;
	
	std::wstring shaderName;
	shaderName = *m_pFramework->GetSystemPath(PATH_TO_SHADERS) + L"fft_512x512_c2c.hlsl";
	m_pSurfaceShader->CreateShaderCs((WCHAR*)shaderName.c_str(), NULL, "Radix008A_CS");
	m_pSurfaceShader->CreateShaderCs((WCHAR*)shaderName.c_str(), NULL, "Radix008A_CS2");

	// Constants
	// Create 6 cbuffers for 512x512 transform
	create_cbuffers_512x512(plan, pd3dDevice, slices);

	// Temp buffer
	D3D11_BUFFER_DESC buf_desc;
	buf_desc.ByteWidth = sizeof(XMVECTOR) * (512 * slices) * 512;
	buf_desc.Usage = D3D11_USAGE_DEFAULT;
	buf_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	buf_desc.CPUAccessFlags = 0;
	buf_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	buf_desc.StructureByteStride = sizeof(XMVECTOR);

	pd3dDevice->CreateBuffer(&buf_desc, NULL, &plan->pBuffer_Tmp);
	assert(plan->pBuffer_Tmp);

	// Temp undordered access view
	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
	uav_desc.Format = DXGI_FORMAT_UNKNOWN;
	uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uav_desc.Buffer.FirstElement = 0;
	uav_desc.Buffer.NumElements = (512 * slices) * 512;
	uav_desc.Buffer.Flags = 0;

	pd3dDevice->CreateUnorderedAccessView(plan->pBuffer_Tmp, &uav_desc, &plan->pUAV_Tmp);
	assert(plan->pUAV_Tmp);

	// Temp shader resource view
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.Format = DXGI_FORMAT_UNKNOWN;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srv_desc.Buffer.FirstElement = 0;
	srv_desc.Buffer.NumElements = (512 * slices) * 512;

	pd3dDevice->CreateShaderResourceView(plan->pBuffer_Tmp, &srv_desc, &plan->pSRV_Tmp);
	assert(plan->pSRV_Tmp);
}

void Water::fft512x512_destroy_plan(CSFFT512x512_Plan* plan)
{
	SAFE_RELEASE(plan->pSRV_Tmp);
	SAFE_RELEASE(plan->pUAV_Tmp);
	SAFE_RELEASE(plan->pBuffer_Tmp);
	for (int i = 0; i < 6; i++)
		SAFE_RELEASE(plan->pRadix008A_CB[i]);
}


// Generating gaussian random number with mean 0 and standard deviation 1.
float Gauss()
{
	float u1 = rand() / (float)RAND_MAX;
	float u2 = rand() / (float)RAND_MAX;
	if (u1 < 1e-6f)
		u1 = 1e-6f;
	return sqrtf(-2 * logf(u1)) * cosf(/*2 * D3DX_PI **/ u2);
}

// Phillips Spectrum
// K: normalized wave vector, W: wind direction, v: wind velocity, a: amplitude constant
float Phillips(XMVECTOR K, XMVECTOR W, float v, float a, float dir_depend)
{
	// largest possible wave from constant wind of velocity v
	float l = v * v / GRAV_ACCEL;
	// damp out waves with very small length w << l
	float w = l / 1000;

	float Ksqr = K.m128_f32[0] * K.m128_f32[0] + K.m128_f32[1] * K.m128_f32[1];
	float Kcos = K.m128_f32[0] * W.m128_f32[0] + K.m128_f32[1] * W.m128_f32[1];
	float phillips = a * expf(-1 / (l * l * Ksqr)) / (Ksqr * Ksqr * Ksqr) * (Kcos * Kcos);

	// filter out waves moving opposite to wind
	if (Kcos < 0)
		phillips *= dir_depend;

	// damp out waves with very small length w << l
	return phillips * expf(-Ksqr * w * w);
}

void Water::CreateBufferAndUAV(void* data, UINT byte_width, UINT byte_stride,
	ID3D11Buffer** ppBuffer, ID3D11UnorderedAccessView** ppUAV, ID3D11ShaderResourceView** ppSRV)
{
	// Create buffer
	D3D11_BUFFER_DESC buf_desc;
	buf_desc.ByteWidth = byte_width;
	buf_desc.Usage = D3D11_USAGE_DEFAULT;
	buf_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	buf_desc.CPUAccessFlags = 0;
	buf_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	buf_desc.StructureByteStride = byte_stride;

	D3D11_SUBRESOURCE_DATA init_data = { data, 0, 0 };

	m_pFramework->GetRender()->GetDevice()->CreateBuffer(&buf_desc, data != NULL ? &init_data : NULL, ppBuffer);
	assert(*ppBuffer);

	// Create undordered access view
	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
	uav_desc.Format = DXGI_FORMAT_UNKNOWN;
	uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uav_desc.Buffer.FirstElement = 0;
	uav_desc.Buffer.NumElements = byte_width / byte_stride;
	uav_desc.Buffer.Flags = 0;

	m_pFramework->GetRender()->GetDevice()->CreateUnorderedAccessView(*ppBuffer, &uav_desc, ppUAV);
	assert(*ppUAV);

	// Create shader resource view
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.Format = DXGI_FORMAT_UNKNOWN;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srv_desc.Buffer.FirstElement = 0;
	srv_desc.Buffer.NumElements = byte_width / byte_stride;

	m_pFramework->GetRender()->GetDevice()->CreateShaderResourceView(*ppBuffer, &srv_desc, ppSRV);
	assert(*ppSRV);
}

void Water::createTextureAndViews(UINT width, UINT height, DXGI_FORMAT format,
	ID3D11Texture2D** ppTex, ID3D11ShaderResourceView** ppSRV, ID3D11RenderTargetView** ppRTV)
{
	// Create 2D texture
	D3D11_TEXTURE2D_DESC tex_desc;
	tex_desc.Width = width;
	tex_desc.Height = height;
	tex_desc.MipLevels = 0;
	tex_desc.ArraySize = 1;
	tex_desc.Format = format;
	tex_desc.SampleDesc.Count = 1;
	tex_desc.SampleDesc.Quality = 0;
	tex_desc.Usage = D3D11_USAGE_DEFAULT;
	tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	tex_desc.CPUAccessFlags = 0;
	tex_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

	m_pFramework->GetRender()->GetDevice()->CreateTexture2D(&tex_desc, NULL, ppTex);
	assert(*ppTex);

	// Create shader resource view
	(*ppTex)->GetDesc(&tex_desc);
	if (ppSRV)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		srv_desc.Format = format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = tex_desc.MipLevels;
		srv_desc.Texture2D.MostDetailedMip = 0;

		m_pFramework->GetRender()->GetDevice()->CreateShaderResourceView(*ppTex, &srv_desc, ppSRV);
		assert(*ppSRV);
	}

	// Create render target view
	if (ppRTV)
	{
		D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
		rtv_desc.Format = format;
		rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtv_desc.Texture2D.MipSlice = 0;

		m_pFramework->GetRender()->GetDevice()->CreateRenderTargetView(*ppTex, &rtv_desc, ppRTV);
		assert(*ppRTV);
	}
}

Water::Water(OceanParameter& params, ID3D11Device* pd3dDevice, Engine *framework)
{
	m_pFramework = framework;
	m_param = params;
	m_pMeshVB = NULL;
	m_pMeshIB = NULL;
	m_pSRV_Perlin = NULL;
	m_pSRV_ReflectCube = NULL;
	m_pHeightSampler = NULL;
	m_pGradientSampler = NULL;
	m_pFresnelSampler = NULL;
	m_pPerlinSampler = NULL;
	m_pCubeSampler = NULL;
	m_pRSState_Solid = NULL;
	m_pFresnelMap = NULL;
	m_pSRV_Fresnel = NULL;
}

Water::~Water()
{
	fft512x512_destroy_plan(&m_fft_plan);

	SAFE_RELEASE(m_pBuffer_Float2_H0);
	SAFE_RELEASE(m_pBuffer_Float_Omega);
	SAFE_RELEASE(m_pBuffer_Float2_Ht);
	SAFE_RELEASE(m_pBuffer_Float_Dxyz);

	SAFE_RELEASE(m_pPointSamplerState);

	SAFE_RELEASE(m_pQuadVB);

	SAFE_RELEASE(m_pUAV_H0);
	SAFE_RELEASE(m_pUAV_Omega);
	SAFE_RELEASE(m_pUAV_Ht);
	SAFE_RELEASE(m_pUAV_Dxyz);

	SAFE_RELEASE(m_pSRV_H0);
	SAFE_RELEASE(m_pSRV_Omega);
	SAFE_RELEASE(m_pSRV_Ht);
	SAFE_RELEASE(m_pSRV_Dxyz);

	SAFE_RELEASE(m_pDisplacementMap);
	SAFE_RELEASE(m_pDisplacementSRV);
	SAFE_RELEASE(m_pDisplacementRTV);

	SAFE_RELEASE(m_pGradientMap);
	SAFE_RELEASE(m_pGradientSRV);
	SAFE_RELEASE(m_pGradientRTV);

	SAFE_RELEASE(m_pUpdateSpectrumCS);
	//SAFE_RELEASE(m_pQuadVS);
	SAFE_RELEASE(m_pUpdateDisplacementPS);
	SAFE_RELEASE(m_pGenGradientFoldingPS);

	//SAFE_RELEASE(m_pQuadLayout);

	SAFE_RELEASE(m_pImmutableCB);
	SAFE_RELEASE(m_pPerFrameCB);
}


// Initialize the vector field.
// wlen_x: width of wave tile, in meters
// wlen_y: length of wave tile, in meters
void Water::initHeightMap(OceanParameter& params, XMVECTOR* out_h0, float* out_omega)
{
	int i, j;
	XMVECTOR K, Kn;

	XMVECTOR wind_dir;
	XMVECTOR tmpvec = XMVector2Normalize(params.wind_dir);
	wind_dir.m128_f32[0] = tmpvec.m128_f32[0];
	wind_dir.m128_f32[1] = tmpvec.m128_f32[1];
	float a = params.wave_amplitude * 1e-7f;	// It is too small. We must scale it for editing.
	float v = params.wind_speed;
	float dir_depend = params.wind_dependency;

	int height_map_dim = params.dmap_dim;
	float patch_length = params.patch_length;

	// initialize random generator.
	srand(0);

	for (i = 0; i <= height_map_dim; i++)
	{
		// K is wave-vector, range [-|DX/W, |DX/W], [-|DY/H, |DY/H]
		K.m128_f32[1] = (-height_map_dim / 2.0f + i) * (2 /** D3DX_PI*/ / patch_length);

		for (j = 0; j <= height_map_dim; j++)
		{
			K.m128_f32[0] = (-height_map_dim / 2.0f + j) * (/*2 * D3DX_PI /*/ patch_length);

			float phil = (K.m128_f32[0] == 0 && K.m128_f32[1] == 0) ? 0 : sqrtf(Phillips(K, wind_dir, v, a, dir_depend));

			out_h0[i * (height_map_dim + 4) + j].m128_f32[0] = float(phil * Gauss() * HALF_SQRT_2);
			out_h0[i * (height_map_dim + 4) + j].m128_f32[1] = float(phil * Gauss() * HALF_SQRT_2);

			// The angular frequency is following the dispersion relation:
			//            out_omega^2 = g*k
			// The equation of Gerstner wave:
			//            x = x0 - K/k * A * sin(dot(K, x0) - sqrt(g * k) * t), x is a 2D vector.
			//            z = A * cos(dot(K, x0) - sqrt(g * k) * t)
			// Gerstner wave shows that a point on a simple sinusoid wave is doing a uniform circular
			// motion with the center (x0, y0, z0), radius A, and the circular plane is parallel to
			// vector K.
			out_omega[i * (height_map_dim + 4) + j] = sqrtf(GRAV_ACCEL * sqrtf(K.m128_f32[0] * K.m128_f32[0] + K.m128_f32[1] * K.m128_f32[1]));
		}
	}
}

void Water::updateDisplacementMap(float time)
{
	// ---------------------------- H(0) -> H(t), D(x, t), D(y, t) --------------------------------
	// Compute shader
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->CSSetShader(m_pSurfaceShader->GetComputeShader(0), NULL, 0);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	// Buffers
	ID3D11ShaderResourceView* cs0_srvs[2] = { m_pSRV_H0, m_pSRV_Omega };
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->CSSetShaderResources(0, 2, cs0_srvs);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	ID3D11UnorderedAccessView* cs0_uavs[1] = { m_pUAV_Ht };
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->CSSetUnorderedAccessViews(0, 1, cs0_uavs, (UINT*)(&cs0_uavs[0]));
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	// Consts
	D3D11_MAPPED_SUBRESOURCE mapped_res;
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->Map(m_pPerFrameCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
	assert(mapped_res.pData);
	float* per_frame_data = (float*)mapped_res.pData;
	// g_Time
	per_frame_data[0] = time * m_param.time_scale;
	// g_ChoppyScale
	per_frame_data[1] = m_param.choppy_scale;
	// g_GridLen
	per_frame_data[2] = m_param.dmap_dim / m_param.patch_length;
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->Unmap(m_pPerFrameCB, 0);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	ID3D11Buffer* cs_cbs[2] = { m_pImmutableCB, m_pPerFrameCB };
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->CSSetConstantBuffers(0, 2, cs_cbs);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	// Run the CS
	UINT group_count_x = (m_param.dmap_dim + BLOCK_SIZE_X - 1) / BLOCK_SIZE_X;
	UINT group_count_y = (m_param.dmap_dim + BLOCK_SIZE_Y - 1) / BLOCK_SIZE_Y;
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->Dispatch(group_count_x, group_count_y, 1);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	// Unbind resources for CS
	cs0_uavs[0] = NULL;
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->CSSetUnorderedAccessViews(0, 1, cs0_uavs, (UINT*)(&cs0_uavs[0]));
	_MUTEXUNLOCK(m_pFramework->GetMutex());
	cs0_srvs[0] = NULL;
	cs0_srvs[1] = NULL;
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->CSSetShaderResources(0, 2, cs0_srvs);
	_MUTEXUNLOCK(m_pFramework->GetMutex());


	// ------------------------------------ Perform FFT -------------------------------------------
	fft_512x512_c2c(&m_fft_plan, m_pUAV_Dxyz, m_pSRV_Dxyz, m_pSRV_Ht);

	// --------------------------------- Wrap Dx, Dy and Dz ---------------------------------------
	// Push RT
	ID3D11RenderTargetView *old_target[BUFFER_COUNT];
	ID3D11DepthStencilView *old_depth;
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->OMGetRenderTargets(BUFFER_COUNT, old_target, &old_depth);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
	D3D11_VIEWPORT old_viewport;
	UINT num_viewport = 1;
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->RSGetViewports(&num_viewport, &old_viewport);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	D3D11_VIEWPORT new_vp = { 0, 0, (float)m_param.dmap_dim, (float)m_param.dmap_dim, 0.0f, 1.0f };
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->RSSetViewports(1, &new_vp);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	// Set RT
	ID3D11RenderTargetView* rt_views[1] = { m_pDisplacementRTV };
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->OMSetRenderTargets(1, rt_views, NULL);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	// VS & PS
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->VSSetShader(m_pSurfaceShader->GetVertexShader(1), NULL, 0);
	m_pFramework->GetRender()->GetDeviceContext()->PSSetShader(m_pSurfaceShader->GetPixelShader(1), NULL, 0);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	// Constants
	ID3D11Buffer* ps_cbs[2] = { m_pImmutableCB, m_pPerFrameCB };
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->PSSetConstantBuffers(0, 2, ps_cbs);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	// Buffer resources
	ID3D11ShaderResourceView* ps_srvs[1] = { m_pSRV_Dxyz };
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->PSSetShaderResources(0, 1, ps_srvs);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	// IA setup
	ID3D11Buffer* vbs[1] = { m_pQuadVB };
	UINT strides[1] = { sizeof(XMVECTOR) };
	UINT offsets[1] = { 0 };
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->IASetVertexBuffers(0, 1, &vbs[0], &strides[0], &offsets[0]);

	m_pFramework->GetRender()->GetDeviceContext()->IASetInputLayout(m_pSurfaceShader->GetInputLayout());
	m_pFramework->GetRender()->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// Perform draw call
	m_pFramework->GetRender()->GetDeviceContext()->Draw(4, 0);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	// Unbind
	ps_srvs[0] = NULL;
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->PSSetShaderResources(0, 1, ps_srvs);
	_MUTEXUNLOCK(m_pFramework->GetMutex());


	// ----------------------------------- Generate Normal ----------------------------------------
	// Set RT
	rt_views[0] = m_pGradientRTV;
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->OMSetRenderTargets(1, rt_views, NULL);

	// VS & PS
	m_pFramework->GetRender()->GetDeviceContext()->VSSetShader(m_pSurfaceShader->GetVertexShader(1), NULL, 0);
	m_pFramework->GetRender()->GetDeviceContext()->PSSetShader(m_pSurfaceShader->GetPixelShader(2), NULL, 0);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	// Texture resource and sampler
	ps_srvs[0] = m_pDisplacementSRV;
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->PSSetShaderResources(0, 1, ps_srvs);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	ID3D11SamplerState* samplers[1] = { m_pPointSamplerState };
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->PSSetSamplers(0, 1, &samplers[0]);

	// Perform draw call
	m_pFramework->GetRender()->GetDeviceContext()->Draw(4, 0);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	// Unbind
	ps_srvs[0] = NULL;
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->PSSetShaderResources(0, 1, ps_srvs);

	// Pop RT
	m_pFramework->GetRender()->GetDeviceContext()->RSSetViewports(1, &old_viewport);
	m_pFramework->GetRender()->GetDeviceContext()->OMSetRenderTargets(BUFFER_COUNT, old_target, old_depth);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
	SAFE_RELEASE(old_target[0]);
	SAFE_RELEASE(old_target[1]);
	SAFE_RELEASE(old_target[2]);
	SAFE_RELEASE(old_depth);

	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->GenerateMips(m_pGradientSRV);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
}

bool Water::checkNodeVisibility(const QuadNode& quad_node, Camera *camera)
{
	// Left plane
	float fov_x = atan(1.0f / camera->GetProjection()(0, 0));
	XMVECTOR plane_left = XMVectorSet(cos(fov_x), 0, sin(fov_x), 0);
	// Right plane
	XMVECTOR plane_right = XMVectorSet(-cos(fov_x), 0, sin(fov_x), 0);

	// Bottom plane
	float fov_y = atan(1.0f / camera->GetProjection()(1, 1));
	XMVECTOR plane_bottom = XMVectorSet(0, cos(fov_y), sin(fov_y), 0);
	// Top plane
	XMVECTOR plane_top = XMVectorSet(0, -cos(fov_y), sin(fov_y), 0);

	// Test quad corners against view frustum in view space
	XMVECTOR corner_verts[4];
	corner_verts[0] = XMVectorSet(quad_node.bottom_left.m128_f32[0], quad_node.bottom_left.m128_f32[1], 0, 1);
	corner_verts[1] = corner_verts[0] + XMVectorSet(quad_node.length, 0, 0, 0);
	corner_verts[2] = corner_verts[0] + XMVectorSet(quad_node.length, quad_node.length, 0, 0);
	corner_verts[3] = corner_verts[0] + XMVectorSet(0, quad_node.length, 0, 0);

	XMMATRIX matView = XMMATRIX(1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1) * camera->GetViewMatrix();
	corner_verts[0] = XMVector4Transform(corner_verts[0], matView);
	corner_verts[1] = XMVector4Transform(corner_verts[1], matView);
	corner_verts[2] = XMVector4Transform(corner_verts[2], matView);
	corner_verts[3] = XMVector4Transform(corner_verts[3], matView);

	// Test against eye plane
	if (corner_verts[0].m128_f32[2] < 0 && corner_verts[1].m128_f32[2] < 0 && corner_verts[2].m128_f32[2] < 0 && corner_verts[3].m128_f32[2] < 0)
		return false;

	// Test against left plane
	float dist_0 = XMVector4Dot(corner_verts[0], plane_left).m128_f32[0];
	float dist_1 = XMVector4Dot(corner_verts[1], plane_left).m128_f32[0];
	float dist_2 = XMVector4Dot(corner_verts[2], plane_left).m128_f32[0];
	float dist_3 = XMVector4Dot(corner_verts[3], plane_left).m128_f32[0];
	if (dist_0 < 0 && dist_1 < 0 && dist_2 < 0 && dist_3 < 0)
		return false;

	// Test against right plane
	dist_0 = XMVector4Dot(corner_verts[0], plane_right).m128_f32[0];
	dist_1 = XMVector4Dot(corner_verts[1], plane_right).m128_f32[0];
	dist_2 = XMVector4Dot(corner_verts[2], plane_right).m128_f32[0];
	dist_3 = XMVector4Dot(corner_verts[3], plane_right).m128_f32[0];
	if (dist_0 < 0 && dist_1 < 0 && dist_2 < 0 && dist_3 < 0)
		return false;

	// Test against bottom plane
	dist_0 = XMVector4Dot(corner_verts[0], plane_bottom).m128_f32[0];
	dist_1 = XMVector4Dot(corner_verts[1], plane_bottom).m128_f32[0];
	dist_2 = XMVector4Dot(corner_verts[2], plane_bottom).m128_f32[0];
	dist_3 = XMVector4Dot(corner_verts[3], plane_bottom).m128_f32[0];
	if (dist_0 < 0 && dist_1 < 0 && dist_2 < 0 && dist_3 < 0)
		return false;

	// Test against top plane
	dist_0 = XMVector4Dot(corner_verts[0], plane_top).m128_f32[0];
	dist_1 = XMVector4Dot(corner_verts[1], plane_top).m128_f32[0];
	dist_2 = XMVector4Dot(corner_verts[2], plane_top).m128_f32[0];
	dist_3 = XMVector4Dot(corner_verts[3], plane_top).m128_f32[0];
	if (dist_0 < 0 && dist_1 < 0 && dist_2 < 0 && dist_3 < 0)
		return false;

	return true;
}

float Water::estimateGridCoverage(const QuadNode& quad_node, float screen_area, Camera *camera)
{
	// Estimate projected area

	// Test 16 points on the quad and find out the biggest one.
	const static float sample_pos[16][2] =
	{
		{ 0, 0 },
		{ 0, 1 },
		{ 1, 0 },
		{ 1, 1 },
		{ 0.5f, 0.333f },
		{ 0.25f, 0.667f },
		{ 0.75f, 0.111f },
		{ 0.125f, 0.444f },
		{ 0.625f, 0.778f },
		{ 0.375f, 0.222f },
		{ 0.875f, 0.556f },
		{ 0.0625f, 0.889f },
		{ 0.5625f, 0.037f },
		{ 0.3125f, 0.37f },
		{ 0.8125f, 0.704f },
		{ 0.1875f, 0.148f },
	};

	float grid_len_world = quad_node.length / m_MeshDim;

	float max_area_proj = 0;
	for (int i = 0; i < 16; i++)
	{
		XMVECTOR test_point = XMVectorSet(quad_node.bottom_left.m128_f32[0] + quad_node.length * sample_pos[i][0], quad_node.bottom_left.m128_f32[1] + quad_node.length * sample_pos[i][1], 0, 0.0f);
		XMVECTOR eye_vec = test_point - XMVectorSet(camera->GetPosition().m128_f32[0], camera->GetPosition().m128_f32[2], camera->GetPosition().m128_f32[1], 0.0f);
		float dist = XMVector3Length(eye_vec).m128_f32[0];

		float area_world = grid_len_world * grid_len_world;// * abs(eye_point.z) / sqrt(nearest_sqr_dist);
		float area_proj = area_world * camera->GetProjection()(0, 0) * camera->GetProjection()(1, 1) / (dist * dist);

		if (max_area_proj < area_proj)
			max_area_proj = area_proj;
	}

	float pixel_coverage = max_area_proj * screen_area * 0.25f;

	return pixel_coverage;
}

// Return value: if successful pushed into the list, return the position. If failed, return -1.
int Water::buildNodeList(QuadNode& quad_node, Camera *camera, RENDER_TYPE rendertype)
{
	// Check against view frustum
	switch (rendertype)
	{
	case D3D11Framework::GBUFFER_FILL_RENDER:
		if (!checkNodeVisibility(quad_node, camera))
			return -1;
	break;
	case D3D11Framework::CUBEMAP_DEPTH_RENDER:
		if (!checkNodeVisibility(quad_node, m_pFramework->GetRender()->GetCamera()))
			return -1;
		break;
	case D3D11Framework::CUBEMAP_COLOR_RENDER:
		break;
		if (!checkNodeVisibility(quad_node, m_pFramework->GetRender()->GetCamera()))
			return -1;
	default:
		break;
	}

	// Estimate the min grid coverage
	UINT num_vps = 1;
	D3D11_VIEWPORT vp;
	_MUTEXLOCK(m_pFramework->GetMutex());
	m_pFramework->GetRender()->GetDeviceContext()->RSGetViewports(&num_vps, &vp);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	float min_coverage;
	switch (rendertype)
	{
	case D3D11Framework::GBUFFER_FILL_RENDER:
		min_coverage = estimateGridCoverage(quad_node, (float)vp.Width * vp.Height, camera);

		break;
	case D3D11Framework::CUBEMAP_DEPTH_RENDER:
		min_coverage = estimateGridCoverage(quad_node, (float)vp.Width * vp.Height, m_pFramework->GetRender()->GetCamera());

		break;
	case D3D11Framework::CUBEMAP_COLOR_RENDER:
		min_coverage = estimateGridCoverage(quad_node, (float)vp.Width * vp.Height, camera);

		break;
	default:
		min_coverage = estimateGridCoverage(quad_node, (float)vp.Width * vp.Height, camera);

		break;
	}

	// Recursively attatch sub-nodes.
	bool visible = true;
	if (min_coverage > m_UpperGridCoverage && quad_node.length > m_param.patch_length)
	{
		// Recursive rendering for sub-quads.
		QuadNode sub_node_0 = { quad_node.bottom_left, quad_node.length / 2, 0, { -1, -1, -1, -1 } };
		quad_node.sub_node[0] = buildNodeList(sub_node_0, camera, rendertype);

		QuadNode sub_node_1 = { quad_node.bottom_left + XMVectorSet(quad_node.length / 2, 0, 0.0f, 0.0f), quad_node.length / 2, 0, { -1, -1, -1, -1 } };
		quad_node.sub_node[1] = buildNodeList(sub_node_1, camera, rendertype);

		QuadNode sub_node_2 = { quad_node.bottom_left + XMVectorSet(quad_node.length / 2, quad_node.length / 2, 0.0f, 0.0f), quad_node.length / 2, 0, { -1, -1, -1, -1 } };
		quad_node.sub_node[2] = buildNodeList(sub_node_2, camera, rendertype);

		QuadNode sub_node_3 = { quad_node.bottom_left + XMVectorSet(0, quad_node.length / 2, 0.0f, 0.0f), quad_node.length / 2, 0, { -1, -1, -1, -1 } };
		quad_node.sub_node[3] = buildNodeList(sub_node_3, camera, rendertype);

		visible = !isLeaf(quad_node);
	}

	if (visible)
	{
		// Estimate mesh LOD
		int lod = 0;
		for (lod = 0; lod < m_Lods - 1; lod++)
		{
			if (min_coverage > m_UpperGridCoverage)
				break;
			min_coverage *= 4;
		}

		// We don't use 1x1 and 2x2 patch. So the highest level is g_Lods - 2.
		quad_node.lod = min(lod, m_Lods - 2);
	}
	else
		return -1;

	/*if (XMVector3Length(camera->GetPosition() - quad_node.bottom_left).m128_f32[0] > 30000)
		return -1;*/

		// Insert into the list
	int position = (int)m_render_list.size();
	m_render_list.push_back(quad_node);

	return position;
}

void Water::RenderWater(Camera *camera, RENDER_TYPE rendertype, ID3D11ShaderResourceView* displacemnet_map, ID3D11ShaderResourceView* gradient_map, float time, ID3D11DeviceContext* pd3dContext)
{
	// Build rendering list
	m_render_list.clear();
	float ocean_extent = m_param.patch_length * (1 << m_FurthestCover);
	QuadNode root_node = { XMVectorSet(-ocean_extent * 0.5f, -ocean_extent * 0.5f, 0.0f, 0.0f), ocean_extent, 0, { -1, -1, -1, -1 } };
	buildNodeList(root_node, camera, rendertype);

	XMMATRIX rotate = XMMATRIX(1, 0, 0, 0,
		0, 0, 1, 0,
		0, 1, 0, 0,
		0, 0, 0, 1);

	switch (rendertype)
	{
	case D3D11Framework::GBUFFER_FILL_RENDER:
	{
		_MUTEXLOCK(m_pFramework->GetMutex());
		pd3dContext->IASetInputLayout(m_pSurfaceShader->GetInputLayout());
		pd3dContext->VSSetShader(m_pSurfaceShader->GetVertexShader(0), NULL, 0);
		pd3dContext->HSSetShader(NULL, NULL, 0);
		pd3dContext->DSSetShader(NULL, NULL, 0);
		pd3dContext->GSSetShader(NULL, NULL, 0);
		pd3dContext->PSSetShader(m_pSurfaceShader->GetPixelShader(0), NULL, 0);
		_MUTEXUNLOCK(m_pFramework->GetMutex());
	}
	break;
	case D3D11Framework::CUBEMAP_DEPTH_RENDER:
	{
		_MUTEXLOCK(m_pFramework->GetMutex());
		pd3dContext->IASetInputLayout(m_pDepthShader->GetInputLayout());
		pd3dContext->VSSetShader(m_pDepthShader->GetVertexShader(0), NULL, 0);
		pd3dContext->HSSetShader(NULL, NULL, 0);
		pd3dContext->DSSetShader(NULL, NULL, 0);
		pd3dContext->GSSetShader(m_pDepthShader->GetGeometryShader(0), NULL, 0);
		pd3dContext->PSSetShader(m_pDepthShader->GetPixelShader(1), NULL, 0);
		_MUTEXUNLOCK(m_pFramework->GetMutex());
	}
	break;
	case D3D11Framework::CUBEMAP_COLOR_RENDER:
	{
		_MUTEXLOCK(m_pFramework->GetMutex());
		pd3dContext->IASetInputLayout(m_pDepthShader->GetInputLayout());
		pd3dContext->VSSetShader(m_pDepthShader->GetVertexShader(0), NULL, 0);
		pd3dContext->HSSetShader(NULL, NULL, 0);
		pd3dContext->DSSetShader(NULL, NULL, 0);
		pd3dContext->GSSetShader(m_pDepthShader->GetGeometryShader(0), NULL, 0);
		pd3dContext->PSSetShader(m_pDepthShader->GetPixelShader(0), NULL, 0);
		_MUTEXUNLOCK(m_pFramework->GetMutex());
	}
	break;
	default:
		break;
	}

	// Textures
	ID3D11ShaderResourceView* vs_srvs[2] = { displacemnet_map, m_pSRV_Perlin };
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dContext->VSSetShaderResources(0, 2, &vs_srvs[0]);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	ID3D11ShaderResourceView* ps_srvs[4] = { m_pSRV_Perlin, gradient_map, m_pSRV_Fresnel, m_pSRV_ReflectCube };
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dContext->PSSetShaderResources(1, 4, &ps_srvs[0]);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	// Samplers
	ID3D11SamplerState* vs_samplers[2] = { m_pHeightSampler, m_pPerlinSampler };
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dContext->VSSetSamplers(0, 2, &vs_samplers[0]);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	ID3D11SamplerState* ps_samplers[4] = { m_pPerlinSampler, m_pGradientSampler, m_pFresnelSampler, m_pCubeSampler };
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dContext->PSSetSamplers(1, 4, &ps_samplers[0]);

	// IA setup
	pd3dContext->IASetIndexBuffer(m_pMeshIB, DXGI_FORMAT_R32_UINT, 0);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	ID3D11Buffer* vbs[1] = { m_pMeshVB };
	UINT strides[1] = { sizeof(ocean_vertex) };
	UINT offsets[1] = { 0 };
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dContext->IASetVertexBuffers(0, 1, &vbs[0], &strides[0], &offsets[0]);

	// State blocks
	pd3dContext->RSSetState(m_pRSState_Solid);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	// Constants
	ID3D11Buffer* cbs[2] = { m_pShadingCB, NULL };
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dContext->VSSetConstantBuffers(2, 1, cbs);
	pd3dContext->PSSetConstantBuffers(2, 1, cbs);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	// We assume the center of the ocean surface at (0, 0, 0).
	for (int i = 0; i < (int)m_render_list.size(); i++)
	{
		QuadNode& node = m_render_list[i];

		if (!isLeaf(node))
			continue;

		// Check adjacent patches and select mesh pattern
		QuadRenderParam& render_param = selectMeshPattern(node);

		// Find the right LOD to render
		int level_size = m_MeshDim;
		for (int lod = 0; lod < node.lod; lod++)
			level_size >>= 1;

		// Matrices and constants
		Const_Per_Call call_consts;

		// Expand of the local coordinate to world space patch size
		XMMATRIX matScale;
		matScale = XMMatrixScaling(node.length / level_size, node.length / level_size, 0);
		call_consts.MatLocal = XMMatrixTranspose(matScale);

		// WVP matrix
		XMMATRIX matWorld;
		XMMATRIX matWVP;
		switch (rendertype)
		{
		case D3D11Framework::GBUFFER_FILL_RENDER:
		{
			matWorld = XMMatrixTranslation(node.bottom_left.m128_f32[0], node.bottom_left.m128_f32[1], 0);
			matWorld *= rotate;
			matWVP = matWorld * camera->GetViewMatrix() * camera->GetProjection();
			// Eye point
			XMMATRIX matView = rotate * camera->GetViewMatrix();
			XMMATRIX matInvWV = XMMatrixTranslation(node.bottom_left.m128_f32[0], node.bottom_left.m128_f32[1], 0) * matView;
			matInvWV = XMMatrixInverse(&XMMatrixDeterminant(matInvWV), matInvWV);
			XMVECTOR vLocalEye = XMVectorSet(0, 0, 0, 0);
			vLocalEye = XMVector3TransformCoord(vLocalEye, matInvWV);
			call_consts.LocalEye[0] = vLocalEye;
		}
			break;
		case D3D11Framework::CUBEMAP_DEPTH_RENDER:
		case D3D11Framework::CUBEMAP_COLOR_RENDER:
			break;
		default:
			break;
		}

		call_consts.mWorld = XMMatrixTranspose(matWorld);
		call_consts.mView = XMMatrixTranspose(camera->GetViewMatrix());
		call_consts.mProjection = XMMatrixTranspose(camera->GetProjection());
		call_consts.mWorldViewProjection = XMMatrixTranspose(matWVP);

		// Texcoord for perlin noise
		XMVECTOR uv_base = node.bottom_left / m_param.patch_length * m_PerlinSize;
		call_consts.UVBase = uv_base;

		// Constant g_PerlinSpeed need to be adjusted mannually
		XMVECTOR perlin_move = -m_param.wind_dir * time * m_PerlinSpeed;
		call_consts.PerlinMovement = perlin_move;

		// Update constant buffer
		D3D11_MAPPED_SUBRESOURCE mapped_res;
		assert(mapped_res.pData);
		*(Const_Per_Call*)mapped_res.pData = call_consts;
		_MUTEXLOCK(m_pFramework->GetMutex());
		pd3dContext->Unmap(m_pPerCallCB, 0);
		_MUTEXUNLOCK(m_pFramework->GetMutex());

		_MUTEXLOCK(m_pFramework->GetMutex());
		pd3dContext->VSSetConstantBuffers(4, 1, &m_pPerCallCB);
		pd3dContext->PSSetConstantBuffers(4, 1, &m_pPerCallCB);
		_MUTEXUNLOCK(m_pFramework->GetMutex());

		switch (rendertype)
		{
		case D3D11Framework::GBUFFER_FILL_RENDER:
		{
			// Perform draw call
			if (render_param.num_inner_faces > 0)
			{
				// Inner mesh of the patch
				_MUTEXLOCK(m_pFramework->GetMutex());
				pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				pd3dContext->DrawIndexed(render_param.num_inner_faces + 2, render_param.inner_start_index, 0);
				_MUTEXUNLOCK(m_pFramework->GetMutex());
			}

			if (render_param.num_boundary_faces > 0)
			{
				// Boundary mesh of the patch
				_MUTEXLOCK(m_pFramework->GetMutex());
				pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				pd3dContext->DrawIndexed(render_param.num_boundary_faces * 3, render_param.boundary_start_index, 0);
				_MUTEXUNLOCK(m_pFramework->GetMutex());
			}
		}
		break;
		case D3D11Framework::CUBEMAP_DEPTH_RENDER:
		{
			// Perform draw call
			if (render_param.num_inner_faces > 0)
			{
				// Inner mesh of the patch
				_MUTEXLOCK(m_pFramework->GetMutex());
				pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				pd3dContext->DrawIndexedInstanced(render_param.num_inner_faces + 2, 6, render_param.inner_start_index, 0, 0);
				_MUTEXUNLOCK(m_pFramework->GetMutex());
			}

			if (render_param.num_boundary_faces > 0)
			{
				// Boundary mesh of the patch
				_MUTEXLOCK(m_pFramework->GetMutex());
				pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				pd3dContext->DrawIndexedInstanced(render_param.num_boundary_faces * 3, 6, render_param.boundary_start_index, 0, 0);
				_MUTEXUNLOCK(m_pFramework->GetMutex());
			}
		}
			break;
		case D3D11Framework::CUBEMAP_COLOR_RENDER:
		{
			// Perform draw call
			if (render_param.num_inner_faces > 0)
			{
				// Inner mesh of the patch
				_MUTEXLOCK(m_pFramework->GetMutex());
				pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				pd3dContext->DrawIndexedInstanced(render_param.num_inner_faces + 2, 6, render_param.inner_start_index, 0, 0);
				_MUTEXUNLOCK(m_pFramework->GetMutex());
			}

			if (render_param.num_boundary_faces > 0)
			{
				// Boundary mesh of the patch
				_MUTEXLOCK(m_pFramework->GetMutex());
				pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				pd3dContext->DrawIndexedInstanced(render_param.num_boundary_faces * 3, 6, render_param.boundary_start_index, 0, 0);
				_MUTEXUNLOCK(m_pFramework->GetMutex());
			}
		}
			break;
		default:
			break;
		}
	}

	// Unbind
	vs_srvs[0] = NULL;
	vs_srvs[1] = NULL;
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dContext->VSSetShaderResources(0, 2, &vs_srvs[0]);
	_MUTEXUNLOCK(m_pFramework->GetMutex());

	ps_srvs[0] = NULL;
	ps_srvs[1] = NULL;
	ps_srvs[2] = NULL;
	ps_srvs[3] = NULL;
	_MUTEXLOCK(m_pFramework->GetMutex());
	pd3dContext->PSSetShaderResources(1, 4, &ps_srvs[0]);
	_MUTEXUNLOCK(m_pFramework->GetMutex());
}

ID3D11ShaderResourceView* Water::getD3D11DisplacementMap()
{
	return m_pDisplacementSRV;
}

ID3D11ShaderResourceView* Water::getD3D11GradientMap()
{
	return m_pGradientSRV;
}


const OceanParameter& Water::getParameters()
{
	return m_param;
}