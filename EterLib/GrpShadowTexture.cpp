#include "StdAfx.h"
#include "GrpShadowTexture.h"
#include "ShaderManager.h"

//////////////////////////////////////////////////////////////////////////
void CGraphicShadowTexture::Destroy()
{
	CGraphicTexture::Destroy();

	if (m_pShadowSRV)
	{
		m_pShadowSRV->Release();
		m_pShadowSRV = nullptr;
	}

	if (m_pShadowRTV)
	{
		m_pShadowRTV->Release();
		m_pShadowRTV = nullptr;
	}

	if (m_pShadowTexture)
	{
		m_pShadowTexture->Release();
		m_pShadowTexture = nullptr;
	}

	if (m_pDepthDSV)
	{
		m_pDepthDSV->Release();
		m_pDepthDSV = nullptr;
	}

	if (m_pDepthTexture)
	{
		m_pDepthTexture->Release();
		m_pDepthTexture = nullptr;
	}

	Initialize();
}

bool CGraphicShadowTexture::Create(int width, int height)
{
	Destroy();

	m_width = width;
	m_height = height;

	// Create shadow render target texture
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	if (FAILED(ms_pDevice->CreateTexture2D(&texDesc, nullptr, &m_pShadowTexture)))
		return false;

	// Create render target view
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = texDesc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;

	if (FAILED(ms_pDevice->CreateRenderTargetView(m_pShadowTexture, &rtvDesc, &m_pShadowRTV)))
	{
		Destroy();
		return false;
	}

	// Create shader resource view
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	if (FAILED(ms_pDevice->CreateShaderResourceView(m_pShadowTexture, &srvDesc, &m_pShadowSRV)))
	{
		Destroy();
		return false;
	}

	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = width;
	depthDesc.Height = height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D16_UNORM;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.SampleDesc.Quality = 0;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthDesc.CPUAccessFlags = 0;
	depthDesc.MiscFlags = 0;

	if (FAILED(ms_pDevice->CreateTexture2D(&depthDesc, nullptr, &m_pDepthTexture)))
	{
		Destroy();
		return false;
	}

	// Create depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = depthDesc.Format;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	if (FAILED(ms_pDevice->CreateDepthStencilView(m_pDepthTexture, &dsvDesc, &m_pDepthDSV)))
	{
		Destroy();
		return false;
	}

	return true;
}

void CGraphicShadowTexture::Set(int stage) const
{
	SHADERMANAGER.SetShaderResource(stage, m_pShadowSRV);
}

const Matrix& CGraphicShadowTexture::GetLightVPMatrixReference() const
{
	return m_d3dLightVPMatrix;
}

ID3D11ShaderResourceView* CGraphicShadowTexture::GetD3DTexture() const
{
	return m_pShadowSRV;
}

void CGraphicShadowTexture::Begin()
{
	MatrixMultiply(&m_d3dLightVPMatrix, &ms_matView, &ms_matProj);

	// Save current render targets
	ms_pContext->OMGetRenderTargets(1, &m_pOldRTV, &m_pOldDSV);

	// Save old viewport
	UINT numViewports = 1;
	ms_pContext->RSGetViewports(&numViewports, &m_d3dOldViewport);

	// Set shadow render target
	ms_pContext->OMSetRenderTargets(1, &m_pShadowRTV, m_pDepthDSV);

	// Set new viewport
	D3D11_VIEWPORT viewport;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = (float)m_width;
	viewport.Height = (float)m_height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	ms_pContext->RSSetViewports(1, &viewport);

	// Clear the shadow render target
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	ms_pContext->ClearRenderTargetView(m_pShadowRTV, clearColor);
	ms_pContext->ClearDepthStencilView(m_pDepthDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

	SHADERMANAGER.SavePipelineState(PSTATE_CULLMODE, CULL_NONE);
	SHADERMANAGER.SavePipelineState(PSTATE_DEPTHFUNC, COMPARISON_LESSEQUAL);
	SHADERMANAGER.SavePipelineState(PSTATE_BLENDENABLE, true);

	// Save alpha test state manually and set new value
	m_bSavedAlphaTestEnabled = SHADERMANAGER.GetAlphaTestEnabled();
	SHADERMANAGER.SetAlphaTestEnabled(true);

	// Save texture factor manually and set new value
	m_dwSavedParticleColor = SHADERMANAGER.GetParticleColor();
	SHADERMANAGER.SetParticleColor(0xbb000000);

	SHADERMANAGER.SetDefaultTexture(0);

	SHADERMANAGER.SaveSamplerState(0, SAMPLER_MINFILTER, FILTER_ANISOTROPIC);
	SHADERMANAGER.SaveSamplerState(0, SAMPLER_MAGFILTER, FILTER_ANISOTROPIC);
	SHADERMANAGER.SaveSamplerState(0, SAMPLER_MIPFILTER, FILTER_ANISOTROPIC);
	SHADERMANAGER.SaveSamplerState(0, SAMPLER_ADDRESSU, ADDRESS_CLAMP);
	SHADERMANAGER.SaveSamplerState(0, SAMPLER_ADDRESSV, ADDRESS_CLAMP);

	SHADERMANAGER.SetShaderResource(1, (ID3D11ShaderResourceView*)NULL);

	SHADERMANAGER.SaveSamplerState(1, SAMPLER_MINFILTER, FILTER_ANISOTROPIC);
	SHADERMANAGER.SaveSamplerState(1, SAMPLER_MAGFILTER, FILTER_ANISOTROPIC);
	SHADERMANAGER.SaveSamplerState(1, SAMPLER_MIPFILTER, FILTER_ANISOTROPIC);
	SHADERMANAGER.SaveSamplerState(1, SAMPLER_ADDRESSU, ADDRESS_CLAMP);
	SHADERMANAGER.SaveSamplerState(1, SAMPLER_ADDRESSV, ADDRESS_CLAMP);
}

void CGraphicShadowTexture::End()
{
	// Restore render targets (handles null gracefully)
	ms_pContext->OMSetRenderTargets(1, &m_pOldRTV, m_pOldDSV);
	ms_pContext->RSSetViewports(1, &m_d3dOldViewport);

	if (m_pOldRTV)
	{
		m_pOldRTV->Release();
		m_pOldRTV = nullptr;
	}
	if (m_pOldDSV)
	{
		m_pOldDSV->Release();
		m_pOldDSV = nullptr;
	}

	SHADERMANAGER.RestorePipelineState(PSTATE_CULLMODE);
	SHADERMANAGER.RestorePipelineState(PSTATE_DEPTHFUNC);
	SHADERMANAGER.RestorePipelineState(PSTATE_BLENDENABLE);

	// Restore alpha test state
	SHADERMANAGER.SetAlphaTestEnabled(m_bSavedAlphaTestEnabled);

	// Restore texture factor
	SHADERMANAGER.SetParticleColor(m_dwSavedParticleColor);

	SHADERMANAGER.RestoreSamplerState(0, SAMPLER_MINFILTER);
	SHADERMANAGER.RestoreSamplerState(0, SAMPLER_MAGFILTER);
	SHADERMANAGER.RestoreSamplerState(0, SAMPLER_MIPFILTER);
	SHADERMANAGER.RestoreSamplerState(0, SAMPLER_ADDRESSU);
	SHADERMANAGER.RestoreSamplerState(0, SAMPLER_ADDRESSV);

	SHADERMANAGER.RestoreSamplerState(1, SAMPLER_MINFILTER);
	SHADERMANAGER.RestoreSamplerState(1, SAMPLER_MAGFILTER);
	SHADERMANAGER.RestoreSamplerState(1, SAMPLER_MIPFILTER);
	SHADERMANAGER.RestoreSamplerState(1, SAMPLER_ADDRESSU);
	SHADERMANAGER.RestoreSamplerState(1, SAMPLER_ADDRESSV);
}

void CGraphicShadowTexture::Initialize()
{
	CGraphicTexture::Initialize();

	m_pShadowTexture = nullptr;
	m_pShadowRTV = nullptr;
	m_pShadowSRV = nullptr;
	m_pDepthTexture = nullptr;
	m_pDepthDSV = nullptr;
	m_pOldRTV = nullptr;
	m_pOldDSV = nullptr;
}

CGraphicShadowTexture::CGraphicShadowTexture()
{
	Initialize();
}

CGraphicShadowTexture::~CGraphicShadowTexture()
{
	Destroy();
}
