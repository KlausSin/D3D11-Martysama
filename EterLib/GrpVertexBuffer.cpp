#include "StdAfx.h"
#include "../eterBase/Stl.h"
#include "GrpVertexBuffer.h"
#include "ShaderManager.h"

void CGraphicVertexBuffer::Initialize()
{
	m_pBuffer = nullptr;
	m_dwBufferSize = 0;
	m_uStride = 0;
	m_Usage = D3D11_USAGE_DEFAULT;
	m_vtxCount = 0;
	m_LayoutType = INPUT_LAYOUT_PDT;
	m_bMapped = false;
}

CGraphicVertexBuffer::CGraphicVertexBuffer()
{
	Initialize();
}

CGraphicVertexBuffer::~CGraphicVertexBuffer()
{
	Destroy();
}

bool CGraphicVertexBuffer::Create(int vtxCount, UINT stride, D3D11_USAGE usage, const void* pInitData)
{
	if (!ms_pDevice)
		return false;

	if (vtxCount <= 0 || stride == 0)
		return false;

	Destroy();

	m_vtxCount = vtxCount;
	m_uStride = stride;
	m_dwBufferSize = stride * vtxCount;
	m_Usage = usage;

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = m_dwBufferSize;
	desc.Usage = usage;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

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

bool CGraphicVertexBuffer::Create(int vtxCount, EInputLayoutType layoutType, D3D11_USAGE usage, const void* pInitData)
{
	m_LayoutType = layoutType;
	UINT stride = ::GetVertexStride(layoutType);  // Use global function
	return Create(vtxCount, stride, usage, pInitData);
}

bool CGraphicVertexBuffer::CreateDeviceObjects()
{
	return m_pBuffer != nullptr;
}

void CGraphicVertexBuffer::DestroyDeviceObjects()
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

void CGraphicVertexBuffer::Destroy()
{
	DestroyDeviceObjects();
	Initialize();
}

bool CGraphicVertexBuffer::Copy(int bufSize, const void* srcVertices)
{
	if (!m_pBuffer || !srcVertices || bufSize <= 0)
		return false;

	if (!GetActiveContext())
		return false;

	if (m_Usage == D3D11_USAGE_DYNAMIC)
	{
		// For dynamic buffers, use Map/Unmap
		void* pData = nullptr;
		if (!Map(&pData))
			return false;

		memcpy(pData, srcVertices, min(static_cast<DWORD>(bufSize), m_dwBufferSize));
		Unmap();
		return true;
	}
	else if (m_Usage == D3D11_USAGE_DEFAULT)
	{
		// For default buffers, use UpdateSubresource
		GetActiveContext()->UpdateSubresource(m_pBuffer, 0, nullptr, srcVertices, 0, 0);
		return true;
	}

	return false;
}

bool CGraphicVertexBuffer::Map(void** ppData, D3D11_MAP mapType)
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

void CGraphicVertexBuffer::Unmap()
{
	if (m_pBuffer && m_bMapped && GetActiveContext())
	{
		GetActiveContext()->Unmap(m_pBuffer, 0);
		m_bMapped = false;
	}
}

bool CGraphicVertexBuffer::Map(void** ppData, D3D11_MAP mapType) const
{
	return const_cast<CGraphicVertexBuffer*>(this)->Map(ppData, mapType);
}

void CGraphicVertexBuffer::Unmap() const
{
	const_cast<CGraphicVertexBuffer*>(this)->Unmap();
}

void CGraphicVertexBuffer::Bind(UINT slot, UINT offset) const
{
	if (m_pBuffer && GetActiveContext())
	{
		UINT stride = m_uStride;
		GetActiveContext()->IASetVertexBuffers(slot, 1, &m_pBuffer, &stride, &offset);
	}
}

void CGraphicVertexBuffer::SetStream(int stride, int layer) const
{
	if (m_pBuffer && GetActiveContext())
	{
		UINT uStride = (stride > 0) ? static_cast<UINT>(stride) : m_uStride;
		UINT offset = 0;
		GetActiveContext()->IASetVertexBuffers(static_cast<UINT>(layer), 1, &m_pBuffer, &uStride, &offset);
	}
}

// GetVertexStride() is defined inline in GrpVertexBuffer.h
