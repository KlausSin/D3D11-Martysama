#include "StdAfx.h"
#include "../eterBase/Stl.h"
#include "GrpTexture.h"

void CGraphicTexture::DestroyDeviceObjects()
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

void CGraphicTexture::Destroy()
{
	DestroyDeviceObjects();
	Initialize();
}

void CGraphicTexture::Initialize()
{
	m_pTexture = nullptr;
	m_pSRV = nullptr;
	m_width = 0;
	m_height = 0;
	m_Format = DXGI_FORMAT_UNKNOWN;
	m_bEmpty = true;
}

bool CGraphicTexture::IsEmpty() const
{
	return m_bEmpty;
}

void CGraphicTexture::SetTextureStage(int stage) const
{
	BindToPS(static_cast<UINT>(stage));
}

void CGraphicTexture::BindToVS(UINT slot) const
{
	if (ms_pContext && m_pSRV)
	{
		ms_pContext->VSSetShaderResources(slot, 1, &m_pSRV);
	}
}

void CGraphicTexture::BindToPS(UINT slot) const
{
	if (ms_pContext && m_pSRV)
	{
		ms_pContext->PSSetShaderResources(slot, 1, &m_pSRV);
	}
}

void CGraphicTexture::BindToGS(UINT slot) const
{
	if (ms_pContext && m_pSRV)
	{
		ms_pContext->GSSetShaderResources(slot, 1, &m_pSRV);
	}
}

void CGraphicTexture::UnbindFromPS(UINT slot)
{
	if (ms_pContext)
	{
		ID3D11ShaderResourceView* pNullSRV = nullptr;
		ms_pContext->PSSetShaderResources(slot, 1, &pNullSRV);
	}
}

ID3D11Texture2D* CGraphicTexture::GetTexture() const
{
	return m_pTexture;
}

ID3D11ShaderResourceView* CGraphicTexture::GetSRV() const
{
	return m_pSRV;
}

int CGraphicTexture::GetWidth() const
{
	return m_width;
}

int CGraphicTexture::GetHeight() const
{
	return m_height;
}

DXGI_FORMAT CGraphicTexture::GetFormat() const
{
	return m_Format;
}

bool CGraphicTexture::CreateShaderResourceView()
{
	if (!m_pTexture || !ms_pDevice)
		return false;

	// Get texture description to create matching SRV
	D3D11_TEXTURE2D_DESC texDesc;
	m_pTexture->GetDesc(&texDesc);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = texDesc.MipLevels;

	// Release existing SRV if any
	if (m_pSRV)
	{
		m_pSRV->Release();
		m_pSRV = nullptr;
	}

	HRESULT hr = ms_pDevice->CreateShaderResourceView(m_pTexture, &srvDesc, &m_pSRV);
	return SUCCEEDED(hr);
}

CGraphicTexture::CGraphicTexture()
{
	Initialize();
}

CGraphicTexture::~CGraphicTexture()
{
	Destroy();
}
