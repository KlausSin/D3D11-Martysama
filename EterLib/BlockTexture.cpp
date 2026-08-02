#include "StdAfx.h"
#include "BlockTexture.h"
#include "GrpBase.h"
#include "GrpDib.h"
#include "../eterbase/Stl.h"
#include "../eterlib/ShaderManager.h"

void CBlockTexture::SetClipRect(const RECT & c_rRect)
{
	m_bClipEnable = TRUE;
	m_clipRect = c_rRect;
}

void CBlockTexture::Render(int ix, int iy)
{
	int isx = ix + m_rect.left;
	int isy = iy + m_rect.top;
	int iex = ix + m_rect.left + m_dwWidth;
	int iey = iy + m_rect.top + m_dwHeight;

	float su = 0.0f;
	float sv = 0.0f;
	float eu = 1.0f;
	float ev = 1.0f;

	if (m_bClipEnable)
	{
		if (isx > m_clipRect.right)
			return;
		if (iex < m_clipRect.left)
			return;

		if (isy > m_clipRect.bottom)
			return;
		if (iey < m_clipRect.top)
			return;

		if (m_clipRect.left > isx)
		{
			int idx = m_clipRect.left - isx;
			isx += idx;
			su += float(idx) / float(m_dwWidth);
		}
		if (iex > m_clipRect.right)
		{
			int idx = iex - m_clipRect.right;
			iex -= idx;
			eu -= float(idx) / float(m_dwWidth);
		}

		if (m_clipRect.top > isy)
		{
			int idy = m_clipRect.top - isy;
			isy += idy;
			sv += float(idy) / float(m_dwHeight);
		}
		if (iey > m_clipRect.bottom)
		{
			int idy = iey - m_clipRect.bottom;
			iey -= idy;
			ev -= float(idy) / float(m_dwHeight);
		}
	}

	TPDTVertex vertices[4];
	vertices[0].position.x	= (float)isx;
	vertices[0].position.y	= (float)isy;
	vertices[0].position.z	= 0.0f;
	vertices[0].texCoord	= TTextureCoordinate(su, sv);
	vertices[0].diffuse		= 0xffffffff;

	vertices[1].position.x	= (float)iex;
	vertices[1].position.y	= (float)isy;
	vertices[1].position.z	= 0.0f;
	vertices[1].texCoord	= TTextureCoordinate(eu, sv);
	vertices[1].diffuse		= 0xffffffff;

	vertices[2].position.x	= (float)isx;
	vertices[2].position.y	= (float)iey;
	vertices[2].position.z	= 0.0f;
	vertices[2].texCoord	= TTextureCoordinate(su, ev);
	vertices[2].diffuse		= 0xffffffff;

	vertices[3].position.x	= (float)iex;
	vertices[3].position.y	= (float)iey;
	vertices[3].position.z	= 0.0f;
	vertices[3].texCoord	= TTextureCoordinate(eu, ev);
	vertices[3].diffuse		= 0xffffffff;

	if (CGraphicBase::SetPDTStream(vertices, 4))
	{
		CGraphicBase::SetDefaultIndexBuffer(CGraphicBase::DEFAULT_IB_FILL_RECT);

		if (SHADERMANAGER.IsInitialized())
		{
			SHADERMANAGER.BeginUI();
			SHADERMANAGER.CommitChanges();
		}

		SHADERMANAGER.SetShaderResource(0, m_pSRV);
		SHADERMANAGER.SetShaderResource(1, NULL);
		SHADERMANAGER.DrawIndexed(TOPOLOGY_TRIANGLELIST, 0, 4, 0, 2);
	}
}

void CBlockTexture::InvalidateRect(const RECT & c_rsrcRect)
{
	RECT dstRect = m_rect;
	if (c_rsrcRect.right < dstRect.left ||
		c_rsrcRect.left > dstRect.right ||
		c_rsrcRect.bottom < dstRect.top ||
		c_rsrcRect.top > dstRect.bottom)
	{
		Tracef("InvalidateRect() - Strange rect");
		return;
	}

	// DIBBAR_LONGSIZE_BUGFIX
	const RECT clipRect = {
		max(c_rsrcRect.left - dstRect.left, 0),
		max(c_rsrcRect.top - dstRect.top, 0),
		min(c_rsrcRect.right - dstRect.left, dstRect.right - dstRect.left),
		min(c_rsrcRect.bottom - dstRect.top, dstRect.bottom - dstRect.top),
	};
	// END_OF_DIBBAR_LONGSIZE_BUGFIX

	DWORD * pdwSrc;
	pdwSrc = (DWORD *)m_pDIB->GetPointer();
	pdwSrc += dstRect.left + dstRect.top*m_pDIB->GetWidth();

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	if (FAILED(ms_pContext->Map(m_pTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
	{
		Tracef("InvalidateRect() - Failed to Map texture");
		return;
	}

	int iclipWidth = clipRect.right - clipRect.left;
	int iclipHeight = clipRect.bottom - clipRect.top;
	DWORD * pdwDst = (DWORD *)mappedResource.pData;
	DWORD dwDstWidth = mappedResource.RowPitch >> 2;
	DWORD dwSrcWidth = m_pDIB->GetWidth();

	// Offset to the clip region
	pdwDst += clipRect.left + clipRect.top * dwDstWidth;
	pdwSrc += clipRect.left + clipRect.top * dwSrcWidth;

	for (int y = 0; y < iclipHeight; ++y)
	{
		for (int x = 0; x < iclipWidth; ++x)
		{
			if (pdwSrc[x])
				pdwDst[x] = pdwSrc[x] | 0xff000000;
			else
				pdwDst[x] = 0;
		}
		pdwDst += dwDstWidth;
		pdwSrc += dwSrcWidth;
	}

	ms_pContext->Unmap(m_pTexture, 0);
}

bool CBlockTexture::Create(CGraphicDib * pDIB, const RECT & c_rRect, DWORD dwWidth, DWORD dwHeight)
{
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = dwWidth;
	texDesc.Height = dwHeight;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DYNAMIC;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	texDesc.MiscFlags = 0;

	if (FAILED(ms_pDevice->CreateTexture2D(&texDesc, nullptr, &m_pTexture)))
	{
		Tracef("Failed to create block texture %u, %u\n", dwWidth, dwHeight);
		return false;
	}

	// Create shader resource view
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	if (FAILED(ms_pDevice->CreateShaderResourceView(m_pTexture, &srvDesc, &m_pSRV)))
	{
		m_pTexture->Release();
		m_pTexture = nullptr;
		Tracef("Failed to create SRV for block texture %u, %u\n", dwWidth, dwHeight);
		return false;
	}

	m_pDIB = pDIB;
	m_rect = c_rRect;
	m_dwWidth = dwWidth;
	m_dwHeight = dwHeight;
	m_bClipEnable = FALSE;

	return true;
}

CBlockTexture::CBlockTexture()
{
	m_pDIB = NULL;
	m_pTexture = nullptr;
	m_pSRV = nullptr;
}

CBlockTexture::~CBlockTexture()
{
	if (m_pSRV)
	{
		m_pSRV->Release();
		m_pSRV = nullptr;
	}
	if (m_pTexture)
	{
		m_pTexture->Release();
		m_pTexture = nullptr;
	}
}
