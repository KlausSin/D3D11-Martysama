#include "StdAfx.h"
#include "GrpVertexBufferDynamic.h"

bool CDynamicVertexBuffer::Create(int vtxCount, EInputLayoutType layoutType)
{
	// Reuse existing buffer if compatible
	if (m_pBuffer)
	{
		if (m_cachedLayoutType == layoutType && m_cachedVtxCount >= vtxCount)
			return true;
	}

	m_cachedVtxCount = vtxCount;
	m_cachedLayoutType = layoutType;
	m_cachedStride = ::GetVertexStride(layoutType);  // Use global function

	return CGraphicVertexBuffer::Create(vtxCount, layoutType, D3D11_USAGE_DYNAMIC);
}

bool CDynamicVertexBuffer::Create(int vtxCount, UINT stride)
{
	// Reuse existing buffer if compatible
	if (m_pBuffer)
	{
		if (m_cachedStride == stride && m_cachedVtxCount >= vtxCount)
			return true;
	}

	m_cachedVtxCount = vtxCount;
	m_cachedStride = stride;

	return CGraphicVertexBuffer::Create(vtxCount, stride, D3D11_USAGE_DYNAMIC);
}

CDynamicVertexBuffer::CDynamicVertexBuffer()
{
	m_cachedVtxCount = 0;
	m_cachedLayoutType = INPUT_LAYOUT_PDT;
	m_cachedStride = 0;
}

CDynamicVertexBuffer::~CDynamicVertexBuffer()
{
}
