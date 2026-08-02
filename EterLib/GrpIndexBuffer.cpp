#include "StdAfx.h"
#include "../eterBase/Stl.h"
#include "GrpIndexBuffer.h"
#include "ShaderManager.h"

void CGraphicIndexBuffer::Initialize()
{
	m_pBuffer = nullptr;
	m_dwBufferSize = 0;
	m_Format = DXGI_FORMAT_R16_UINT;
	m_Usage = D3D11_USAGE_DEFAULT;
	m_idxCount = 0;
	m_bMapped = false;
}

CGraphicIndexBuffer::CGraphicIndexBuffer()
{
	Initialize();
}

CGraphicIndexBuffer::~CGraphicIndexBuffer()
{
	Destroy();
}

bool CGraphicIndexBuffer::Create(int idxCount, DXGI_FORMAT format, D3D11_USAGE usage, const void* pInitData)
{
	if (!ms_pDevice)
		return false;

	if (idxCount <= 0)
		return false;

	Destroy();

	m_idxCount = idxCount;
	m_Format = format;
	m_Usage = usage;

	// Calculate buffer size based on format
	UINT indexSize = (format == DXGI_FORMAT_R32_UINT) ? sizeof(UINT) : sizeof(WORD);
	m_dwBufferSize = indexSize * idxCount;

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = m_dwBufferSize;
	desc.Usage = usage;
	desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	// Set CPU access based on usage
	if (usage == D3D11_USAGE_DYNAMIC)
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	else if (usage == D3D11_USAGE_STAGING)
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	else
		desc.CPUAccessFlags = 0;

	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA initData = {};
	D3D11_SUBRESOURCE_DATA* pInit = nullptr;

	if (pInitData)
	{
		initData.pSysMem = pInitData;
		initData.SysMemPitch = 0;
		initData.SysMemSlicePitch = 0;
		pInit = &initData;
	}

	HRESULT hr = ms_pDevice->CreateBuffer(&desc, pInit, &m_pBuffer);
	return SUCCEEDED(hr);
}

bool CGraphicIndexBuffer::Create(int faceCount, TFace* faces)
{
	if (faceCount <= 0 || !faces)
		return false;

	int idxCount = faceCount * 3;

	// Create temporary buffer with face data
	std::vector<WORD> indices(idxCount);
	for (int i = 0; i < faceCount; ++i)
	{
		indices[i * 3 + 0] = faces[i].indices[0];
		indices[i * 3 + 1] = faces[i].indices[1];
		indices[i * 3 + 2] = faces[i].indices[2];
	}

	// Create with initial data
	return Create(idxCount, DXGI_FORMAT_R16_UINT, D3D11_USAGE_DEFAULT, indices.data());
}

bool CGraphicIndexBuffer::CreateDeviceObjects()
{
	return m_pBuffer != nullptr;
}

void CGraphicIndexBuffer::DestroyDeviceObjects()
{
	if (m_bMapped && m_pBuffer)
	{
		Unmap();
	}

	if (m_pBuffer)
	{
		m_pBuffer->Release();
		m_pBuffer = nullptr;
	}
}

void CGraphicIndexBuffer::Destroy()
{
	DestroyDeviceObjects();
	Initialize();
}

bool CGraphicIndexBuffer::Copy(int bufSize, const void* srcIndices)
{
	if (!m_pBuffer || !srcIndices || bufSize <= 0)
		return false;

	if (!GetActiveContext())
		return false;

	if (m_Usage == D3D11_USAGE_DYNAMIC)
	{
		// For dynamic buffers, use Map/Unmap
		void* pData = nullptr;
		if (!Map(&pData))
			return false;

		memcpy(pData, srcIndices, min(static_cast<DWORD>(bufSize), m_dwBufferSize));
		Unmap();
		return true;
	}
	else if (m_Usage == D3D11_USAGE_DEFAULT)
	{
		// For default buffers, use UpdateSubresource
		GetActiveContext()->UpdateSubresource(m_pBuffer, 0, nullptr, srcIndices, 0, 0);
		return true;
	}

	return false;
}

bool CGraphicIndexBuffer::Map(void** ppData, D3D11_MAP mapType)
{
	if (!m_pBuffer || !ppData || !GetActiveContext())
		return false;

	if (m_bMapped)
	{
		// Already mapped
		return false;
	}

	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = GetActiveContext()->Map(m_pBuffer, 0, mapType, 0, &mapped);

	if (FAILED(hr))
		return false;

	*ppData = mapped.pData;
	m_bMapped = true;
	return true;
}

void CGraphicIndexBuffer::Unmap()
{
	if (m_pBuffer && m_bMapped && GetActiveContext())
	{
		GetActiveContext()->Unmap(m_pBuffer, 0);
		m_bMapped = false;
	}
}

bool CGraphicIndexBuffer::Map(void** ppData, D3D11_MAP mapType) const
{
	return const_cast<CGraphicIndexBuffer*>(this)->Map(ppData, mapType);
}

void CGraphicIndexBuffer::Unmap() const
{
	const_cast<CGraphicIndexBuffer*>(this)->Unmap();
}

void CGraphicIndexBuffer::Bind(UINT offset) const
{
	if (m_pBuffer && GetActiveContext())
	{
		GetActiveContext()->IASetIndexBuffer(m_pBuffer, m_Format, offset);
	}
}

void CGraphicIndexBuffer::SetIndexBuffer(int startIndex) const
{
	if (m_pBuffer && GetActiveContext())
	{
		// startIndex is now an offset in bytes for DX11
		UINT indexSize = (m_Format == DXGI_FORMAT_R32_UINT) ? sizeof(UINT) : sizeof(WORD);
		UINT offset = startIndex * indexSize;
		GetActiveContext()->IASetIndexBuffer(m_pBuffer, m_Format, offset);
	}
}
