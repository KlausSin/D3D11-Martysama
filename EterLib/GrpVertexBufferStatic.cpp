#include "StdAfx.h"
#include "GrpVertexBufferStatic.h"

bool CStaticVertexBuffer::Create(int vtxCount, EInputLayoutType layoutType)
{
	return CGraphicVertexBuffer::Create(vtxCount, layoutType, D3D11_USAGE_DEFAULT);
}

bool CStaticVertexBuffer::Create(int vtxCount, UINT stride)
{
	return CGraphicVertexBuffer::Create(vtxCount, stride, D3D11_USAGE_DEFAULT);
}

CStaticVertexBuffer::CStaticVertexBuffer()
{
}

CStaticVertexBuffer::~CStaticVertexBuffer()
{
}
