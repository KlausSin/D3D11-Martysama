#include "StdAfx.h"
#include "GrpDevice.h"
#include "MatrixStack.h"
#include "ShaderManager.h"
#include "StateManager.h"
#include "VTFInstanceManager.h"
#include "VolumetricFog.h"
#include "../eterBase/Stl.h"
#include "../eterBase/Debug.h"

#include <dxgi1_2.h>
#include <intrin.h>

bool GRAPHICS_CAPS_CAN_NOT_DRAW_LINE = false;
bool GRAPHICS_CAPS_CAN_NOT_DRAW_SHADOW = false;
bool GRAPHICS_CAPS_HALF_SIZE_IMAGE = false;
bool GRAPHICS_CAPS_CAN_NOT_TEXTURE_ADDRESS_BORDER = false;
bool GRAPHICS_CAPS_SOFTWARE_TILING = false;

bool g_isBrowserMode = false;
RECT g_rcBrowser;

static CShaderManager gs_kShaderManager;

static CVTFInstanceManager gs_kVTFInstanceManager;
static CVolumetricFog gs_kVolumetricFog;

static CStateManager* gs_pStateManager = nullptr;

bool CPU_HAS_SSE2 = true;


CGraphicDevice::CGraphicDevice()
	: m_uBackBufferCount(1)
	, m_bWindowed(true)
{
	__Initialize();
}

CGraphicDevice::~CGraphicDevice()
{
	Destroy();
}

void CGraphicDevice::__Initialize()
{
	ms_iD3DAdapterInfo = 0;
	ms_iD3DDevInfo = 0;
	ms_iD3DModeInfo = 0;

	ms_pDevice = nullptr;
	ms_pContext = nullptr;
	ms_pSwapChain = nullptr;
	ms_pRenderTargetView = nullptr;
	ms_pDepthStencilView = nullptr;
	ms_pDepthStencilBuffer = nullptr;
	ms_pMatrixStack = nullptr;

#ifdef ENABLE_MSAA
	ms_pMSAARenderTarget = nullptr;
	ms_pMSAARenderTargetView = nullptr;
	ms_pMSAADepthStencilBuffer = nullptr;
	ms_pMSAADepthStencilView = nullptr;
	ms_uMSAASampleCount = 1;
#endif

#ifdef ENABLE_SSAO
	ms_pDepthStencilSRV = nullptr;
	ms_pDepthStencilViewReadOnly = nullptr;
#ifdef ENABLE_MSAA
	ms_pMSAADepthStencilSRV = nullptr;
#endif
	ms_pResolvedDepthTex = nullptr;
	ms_pResolvedDepthRTV = nullptr;
	ms_pResolvedDepthSRV = nullptr;
	ms_pSSAO_RT = nullptr;
	ms_pSSAO_RTV = nullptr;
	ms_pSSAO_SRV = nullptr;
	ms_pSSAOBlur_RT = nullptr;
	ms_pSSAOBlur_RTV = nullptr;
	ms_pSSAOBlur_SRV = nullptr;
	ms_bSSAOEnabled = true;
#endif

#ifdef ENABLE_BLOOM
	ms_pSceneTexture = nullptr;
	ms_pSceneTextureRTV = nullptr;
	ms_pSceneTextureSRV = nullptr;
	ms_pBloomRTA = nullptr;
	ms_pBloomRTA_RTV = nullptr;
	ms_pBloomRTA_SRV = nullptr;
	ms_pBloomRTB = nullptr;
	ms_pBloomRTB_RTV = nullptr;
	ms_pBloomRTB_SRV = nullptr;
	ms_bBloomEnabled = true;
#ifdef ENABLE_GODRAYS
	ms_pGodRaysRT = nullptr;
	ms_pGodRaysRTV = nullptr;
	ms_pGodRaysSRV = nullptr;
#endif
#endif


	ms_dwWavingEndTime = 0;
	ms_dwFlashingEndTime = 0;

	ZeroMemory(&m_SwapChainDesc, sizeof(m_SwapChainDesc));

	__InitializeDefaultIndexBufferList();
	__InitializePDTVertexBufferList();
}

void CGraphicDevice::RegisterWarningString(UINT uiMsg, const char* c_szString)
{
	m_kMap_strWarningMessage[uiMsg] = c_szString;
}

void CGraphicDevice::__WarningMessage(HWND hWnd, UINT uiMsg)
{
	if (m_kMap_strWarningMessage.end() == m_kMap_strWarningMessage.find(uiMsg))
		return;
	MessageBox(hWnd, m_kMap_strWarningMessage[uiMsg].c_str(), "Warning", MB_OK | MB_TOPMOST);
}

void CGraphicDevice::MoveWebBrowserRect(const RECT& c_rcWebPage)
{
	g_rcBrowser = c_rcWebPage;
}

void CGraphicDevice::EnableWebBrowserMode(const RECT& c_rcWebPage)
{
	g_isBrowserMode = true;
	g_rcBrowser = c_rcWebPage;
}

void CGraphicDevice::DisableWebBrowserMode()
{
	g_isBrowserMode = false;
}

bool CGraphicDevice::ResizeBuffers(UINT uWidth, UINT uHeight)
{
	if (!ms_pSwapChain || !ms_pDevice)
		return false;

	if (m_SwapChainDesc.BufferDesc.Width == uWidth &&
		m_SwapChainDesc.BufferDesc.Height == uHeight)
		return true;

	// Release existing back buffer resources
	__ReleaseBackBufferResources();

	// Resize swap chain buffers.
	HRESULT hr = ms_pSwapChain->ResizeBuffers(
		0,
		uWidth,
		uHeight,
		m_SwapChainDesc.BufferDesc.Format,
		m_SwapChainDesc.Flags
	);

	if (FAILED(hr))
	{
		TraceError("CGraphicDevice::ResizeBuffers - ResizeBuffers failed: 0x%08X", hr);
		return false;
	}

	// Update stored dimensions
	m_SwapChainDesc.BufferDesc.Width = uWidth;
	m_SwapChainDesc.BufferDesc.Height = uHeight;
	ms_iWidth = uWidth;
	ms_iHeight = uHeight;

	// Recreate render target view
	if (!__CreateRenderTargetView())
		return false;

	// Recreate depth stencil view
	if (!__CreateDepthStencilView(uWidth, uHeight))
		return false;

#ifdef ENABLE_MSAA
	// Recreate MSAA resources at new size
	__CreateMSAARenderTarget(uWidth, uHeight);
#endif

#ifdef ENABLE_BLOOM
	// Recreate bloom resources at new size
	if (!__CreateBloomResources(uWidth, uHeight))
	{
		Tracef("ResizeBuffers: Bloom resources not available at new size\n");
		ms_bBloomEnabled = false;
	}
#endif

#ifdef ENABLE_SSAO
	if (!__CreateSSAOResources(uWidth, uHeight))
	{
		Tracef("ResizeBuffers: SSAO resources not available at new size\n");
		ms_bSSAOEnabled = false;
	}
#endif


	// Update viewport
	ms_Viewport.Width = (float)uWidth;
	ms_Viewport.Height = (float)uHeight;
	ms_pContext->RSSetViewports(1, &ms_Viewport);

	// Rebind render targets (MSAA if available)
	ID3D11RenderTargetView* pRTV = GetRenderTargetView();
	ID3D11DepthStencilView* pDSV = GetDepthStencilView();
	ms_pContext->OMSetRenderTargets(1, &pRTV, pDSV);

	if (SHADERMANAGER.IsInitialized())
		SHADERMANAGER.SetDefaultState();

	return true;
}

CGraphicDevice::EDeviceState CGraphicDevice::GetDeviceState()
{
	if (!ms_pDevice)
		return DEVICESTATE_NULL;

	// Check for device removal (driver crash/update)
	HRESULT hr = ms_pDevice->GetDeviceRemovedReason();
	if (FAILED(hr))
		return DEVICESTATE_BROKEN;

	return DEVICESTATE_OK;
}

bool CGraphicDevice::__CreateRenderTargetView()
{
	// Get the back buffer
	ID3D11Texture2D* pBackBuffer = nullptr;
	HRESULT hr = ms_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
	if (FAILED(hr))
	{
		TraceError("Failed to get back buffer: 0x%08X", hr);
		return false;
	}

	// Create render target view
	hr = ms_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &ms_pRenderTargetView);
	pBackBuffer->Release();

	if (FAILED(hr))
	{
		TraceError("Failed to create render target view: 0x%08X", hr);
		return false;
	}

	return true;
}

bool CGraphicDevice::__CreateDepthStencilView(int width, int height)
{
	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC depthStencilDesc;
	ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));
	depthStencilDesc.Width = width;
	depthStencilDesc.Height = height;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
#ifdef ENABLE_SSAO
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
#else
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
#endif
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilDesc.CPUAccessFlags = 0;
	depthStencilDesc.MiscFlags = 0;

	HRESULT hr = ms_pDevice->CreateTexture2D(&depthStencilDesc, nullptr, &ms_pDepthStencilBuffer);
	if (FAILED(hr))
	{
		TraceError("Failed to create depth stencil buffer: 0x%08X", hr);
		return false;
	}

	// Create depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	ZeroMemory(&dsvDesc, sizeof(dsvDesc));
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	hr = ms_pDevice->CreateDepthStencilView(ms_pDepthStencilBuffer, &dsvDesc, &ms_pDepthStencilView);
	if (FAILED(hr))
	{
		TraceError("Failed to create depth stencil view: 0x%08X", hr);
		return false;
	}

#ifdef ENABLE_SSAO
	// Create SRV for reading depth in SSAO shader
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	hr = ms_pDevice->CreateShaderResourceView(ms_pDepthStencilBuffer, &srvDesc, &ms_pDepthStencilSRV);
	if (FAILED(hr))
	{
		TraceError("Failed to create depth stencil SRV: 0x%08X", hr);
		return false;
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvReadOnlyDesc = {};
	dsvReadOnlyDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvReadOnlyDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvReadOnlyDesc.Texture2D.MipSlice = 0;
	dsvReadOnlyDesc.Flags = D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL;
	hr = ms_pDevice->CreateDepthStencilView(ms_pDepthStencilBuffer, &dsvReadOnlyDesc, &ms_pDepthStencilViewReadOnly);
	if (FAILED(hr))
	{
		TraceError("Failed to create read-only depth stencil view: 0x%08X", hr);
		// Non-fatal — SSR will fall back to synth sky if this is missing.
		ms_pDepthStencilViewReadOnly = nullptr;
	}
#endif

	return true;
}

void CGraphicDevice::__ReleaseBackBufferResources()
{
	if (ms_pContext)
		ms_pContext->OMSetRenderTargets(0, nullptr, nullptr);


#ifdef ENABLE_SSAO
	ms_bSSAOEnabled = false;
	__ReleaseSSAOResources();
#endif

#ifdef ENABLE_BLOOM
	__ReleaseBloomResources();
#endif

#ifdef ENABLE_MSAA
	__ReleaseMSAAResources();
#endif

	if (ms_pRenderTargetView)
	{
		ms_pRenderTargetView->Release();
		ms_pRenderTargetView = nullptr;
	}

	if (ms_pDepthStencilView)
	{
		ms_pDepthStencilView->Release();
		ms_pDepthStencilView = nullptr;
	}

#ifdef ENABLE_SSAO
	if (ms_pDepthStencilSRV)
	{
		ms_pDepthStencilSRV->Release();
		ms_pDepthStencilSRV = nullptr;
	}
	if (ms_pDepthStencilViewReadOnly)
	{
		ms_pDepthStencilViewReadOnly->Release();
		ms_pDepthStencilViewReadOnly = nullptr;
	}
#endif

	if (ms_pDepthStencilBuffer)
	{
		ms_pDepthStencilBuffer->Release();
		ms_pDepthStencilBuffer = nullptr;
	}
}

#ifdef ENABLE_MSAA
void CGraphicDevice::__ReleaseMSAAResources()
{
	if (ms_pMSAARenderTargetView)
	{
		ms_pMSAARenderTargetView->Release();
		ms_pMSAARenderTargetView = nullptr;
	}

	if (ms_pMSAARenderTarget)
	{
		ms_pMSAARenderTarget->Release();
		ms_pMSAARenderTarget = nullptr;
	}

	if (ms_pMSAADepthStencilView)
	{
		ms_pMSAADepthStencilView->Release();
		ms_pMSAADepthStencilView = nullptr;
	}

#ifdef ENABLE_SSAO
	if (ms_pMSAADepthStencilSRV)
	{
		ms_pMSAADepthStencilSRV->Release();
		ms_pMSAADepthStencilSRV = nullptr;
	}
#endif

	if (ms_pMSAADepthStencilBuffer)
	{
		ms_pMSAADepthStencilBuffer->Release();
		ms_pMSAADepthStencilBuffer = nullptr;
	}
}

bool CGraphicDevice::__CreateMSAARenderTarget(int width, int height)
{
	if (!ms_pDevice)
		return false;

	// Determine sample count with fallback
	UINT sampleCount = MSAA_DEFAULT_SAMPLE_COUNT;
	UINT qualityLevels = 0;

	while (sampleCount > 1)
	{
		HRESULT hr = ms_pDevice->CheckMultisampleQualityLevels(
			DXGI_FORMAT_B8G8R8A8_UNORM, sampleCount, &qualityLevels);

		if (SUCCEEDED(hr) && qualityLevels > 0)
			break;

		sampleCount /= 2;  // Fall back: 4 -> 2 -> 1
	}

	ms_uMSAASampleCount = sampleCount;

	if (sampleCount <= 1)
	{
		Tracef("MSAA: No multisampling support, running at 1x\n");
		return true;  // Not an error, just no MSAA
	}

	Tracef("MSAA: Using %ux multisampling (quality levels: %u)\n", sampleCount, qualityLevels);

	// Create MSAA color texture
	D3D11_TEXTURE2D_DESC colorDesc;
	ZeroMemory(&colorDesc, sizeof(colorDesc));
	colorDesc.Width = width;
	colorDesc.Height = height;
	colorDesc.MipLevels = 1;
	colorDesc.ArraySize = 1;
	colorDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	colorDesc.SampleDesc.Count = sampleCount;
	colorDesc.SampleDesc.Quality = 0;
	colorDesc.Usage = D3D11_USAGE_DEFAULT;
	colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

	HRESULT hr = ms_pDevice->CreateTexture2D(&colorDesc, nullptr, &ms_pMSAARenderTarget);
	if (FAILED(hr))
	{
		TraceError("MSAA: Failed to create color texture: 0x%08X", hr);
		ms_uMSAASampleCount = 1;
		return false;
	}

	// Create MSAA RTV
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	ZeroMemory(&rtvDesc, sizeof(rtvDesc));
	rtvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;

	hr = ms_pDevice->CreateRenderTargetView(ms_pMSAARenderTarget, &rtvDesc, &ms_pMSAARenderTargetView);
	if (ms_pContext && ms_pMSAARenderTargetView) { const float c[4] = {0.0f,0.0f,0.0f,0.0f}; ms_pContext->ClearRenderTargetView(ms_pMSAARenderTargetView, c); }
	if (FAILED(hr))
	{
		TraceError("MSAA: Failed to create RTV: 0x%08X", hr);
		__ReleaseMSAAResources();
		ms_uMSAASampleCount = 1;
		return false;
	}

	// Create MSAA depth stencil texture
	D3D11_TEXTURE2D_DESC depthDesc;
	ZeroMemory(&depthDesc, sizeof(depthDesc));
	depthDesc.Width = width;
	depthDesc.Height = height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
#ifdef ENABLE_SSAO
	depthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
#else
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
#endif
	depthDesc.SampleDesc.Count = sampleCount;
	depthDesc.SampleDesc.Quality = 0;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;

	hr = ms_pDevice->CreateTexture2D(&depthDesc, nullptr, &ms_pMSAADepthStencilBuffer);
	if (FAILED(hr))
	{
		TraceError("MSAA: Failed to create depth texture: 0x%08X", hr);
		__ReleaseMSAAResources();
		ms_uMSAASampleCount = 1;
		return false;
	}

	// Create MSAA DSV
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	ZeroMemory(&dsvDesc, sizeof(dsvDesc));
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;

	hr = ms_pDevice->CreateDepthStencilView(ms_pMSAADepthStencilBuffer, &dsvDesc, &ms_pMSAADepthStencilView);
	if (FAILED(hr))
	{
		TraceError("MSAA: Failed to create DSV: 0x%08X", hr);
		__ReleaseMSAAResources();
		ms_uMSAASampleCount = 1;
		return false;
	}

#ifdef ENABLE_SSAO
	// Create MSAA depth SRV for depth resolve shader
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;

		hr = ms_pDevice->CreateShaderResourceView(ms_pMSAADepthStencilBuffer, &srvDesc, &ms_pMSAADepthStencilSRV);
		if (FAILED(hr))
		{
			TraceError("MSAA: Failed to create depth SRV: 0x%08X", hr);
			// Non-fatal for SSAO, just disable it
		}
	}
#endif

	return true;
}
#endif

#ifdef ENABLE_BLOOM
bool CGraphicDevice::__CreateBloomResources(int width, int height)
{
	if (!ms_pDevice)
		return false;

	__ReleaseBloomResources();

	HRESULT hr;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	hr = ms_pDevice->CreateTexture2D(&texDesc, nullptr, &ms_pSceneTexture);
	if (FAILED(hr))
	{
		TraceError("Bloom: Failed to create scene texture: 0x%08X", hr);
		return false;
	}

	hr = ms_pDevice->CreateRenderTargetView(ms_pSceneTexture, nullptr, &ms_pSceneTextureRTV);
	if (ms_pContext && ms_pSceneTextureRTV) { const float c[4] = {0.0f,0.0f,0.0f,0.0f}; ms_pContext->ClearRenderTargetView(ms_pSceneTextureRTV, c); }
	if (FAILED(hr))
	{
		TraceError("Bloom: Failed to create scene texture RTV: 0x%08X", hr);
		__ReleaseBloomResources();
		return false;
	}

	hr = ms_pDevice->CreateShaderResourceView(ms_pSceneTexture, nullptr, &ms_pSceneTextureSRV);
	if (FAILED(hr))
	{
		TraceError("Bloom: Failed to create scene texture SRV: 0x%08X", hr);
		__ReleaseBloomResources();
		return false;
	}

	// Bloom RT A (1/4 resolution)
	UINT bloomW = max(1u, (UINT)width / 4);
	UINT bloomH = max(1u, (UINT)height / 4);

	texDesc.Width = bloomW;
	texDesc.Height = bloomH;

	hr = ms_pDevice->CreateTexture2D(&texDesc, nullptr, &ms_pBloomRTA);
	if (FAILED(hr))
	{
		TraceError("Bloom: Failed to create bloom RTA texture: 0x%08X", hr);
		__ReleaseBloomResources();
		return false;
	}

	hr = ms_pDevice->CreateRenderTargetView(ms_pBloomRTA, nullptr, &ms_pBloomRTA_RTV);
	if (ms_pContext && ms_pBloomRTA_RTV) { const float c[4] = {0.0f,0.0f,0.0f,0.0f}; ms_pContext->ClearRenderTargetView(ms_pBloomRTA_RTV, c); }
	if (FAILED(hr))
	{
		TraceError("Bloom: Failed to create bloom RTA RTV: 0x%08X", hr);
		__ReleaseBloomResources();
		return false;
	}

	hr = ms_pDevice->CreateShaderResourceView(ms_pBloomRTA, nullptr, &ms_pBloomRTA_SRV);
	if (FAILED(hr))
	{
		TraceError("Bloom: Failed to create bloom RTA SRV: 0x%08X", hr);
		__ReleaseBloomResources();
		return false;
	}

	// Bloom RT B (1/4 resolution)
	hr = ms_pDevice->CreateTexture2D(&texDesc, nullptr, &ms_pBloomRTB);
	if (FAILED(hr))
	{
		TraceError("Bloom: Failed to create bloom RTB texture: 0x%08X", hr);
		__ReleaseBloomResources();
		return false;
	}

	hr = ms_pDevice->CreateRenderTargetView(ms_pBloomRTB, nullptr, &ms_pBloomRTB_RTV);
	if (ms_pContext && ms_pBloomRTB_RTV) { const float c[4] = {0.0f,0.0f,0.0f,0.0f}; ms_pContext->ClearRenderTargetView(ms_pBloomRTB_RTV, c); }
	if (FAILED(hr))
	{
		TraceError("Bloom: Failed to create bloom RTB RTV: 0x%08X", hr);
		__ReleaseBloomResources();
		return false;
	}

	hr = ms_pDevice->CreateShaderResourceView(ms_pBloomRTB, nullptr, &ms_pBloomRTB_SRV);
	if (FAILED(hr))
	{
		TraceError("Bloom: Failed to create bloom RTB SRV: 0x%08X", hr);
		__ReleaseBloomResources();
		return false;
	}

	Tracef("Bloom: Resources created (%dx%d scene, %dx%d bloom)\n", width, height, bloomW, bloomH);

#ifdef ENABLE_GODRAYS
	// God rays RT (same 1/4 resolution as bloom RTs)
	texDesc.Width = bloomW;
	texDesc.Height = bloomH;

	hr = ms_pDevice->CreateTexture2D(&texDesc, nullptr, &ms_pGodRaysRT);
	if (FAILED(hr))
	{
		TraceError("GodRays: Failed to create RT texture: 0x%08X", hr);
		// Non-fatal: god rays will just be disabled
	}
	else
	{
		hr = ms_pDevice->CreateRenderTargetView(ms_pGodRaysRT, nullptr, &ms_pGodRaysRTV);
		if (ms_pContext && ms_pGodRaysRTV) { const float c[4] = {0.0f,0.0f,0.0f,0.0f}; ms_pContext->ClearRenderTargetView(ms_pGodRaysRTV, c); }
		if (FAILED(hr))
		{
			TraceError("GodRays: Failed to create RTV: 0x%08X", hr);
			ms_pGodRaysRT->Release(); ms_pGodRaysRT = nullptr;
		}
		else
		{
			hr = ms_pDevice->CreateShaderResourceView(ms_pGodRaysRT, nullptr, &ms_pGodRaysSRV);
			if (FAILED(hr))
			{
				TraceError("GodRays: Failed to create SRV: 0x%08X", hr);
				ms_pGodRaysRTV->Release(); ms_pGodRaysRTV = nullptr;
				ms_pGodRaysRT->Release(); ms_pGodRaysRT = nullptr;
			}
		}
	}
#endif

	return true;
}

void CGraphicDevice::__ReleaseBloomResources()
{
#ifdef ENABLE_GODRAYS
	if (ms_pGodRaysSRV) { ms_pGodRaysSRV->Release(); ms_pGodRaysSRV = nullptr; }
	if (ms_pGodRaysRTV) { ms_pGodRaysRTV->Release(); ms_pGodRaysRTV = nullptr; }
	if (ms_pGodRaysRT) { ms_pGodRaysRT->Release(); ms_pGodRaysRT = nullptr; }
#endif
	if (ms_pSceneTextureSRV) { ms_pSceneTextureSRV->Release(); ms_pSceneTextureSRV = nullptr; }
	if (ms_pSceneTextureRTV) { ms_pSceneTextureRTV->Release(); ms_pSceneTextureRTV = nullptr; }
	if (ms_pSceneTexture) { ms_pSceneTexture->Release(); ms_pSceneTexture = nullptr; }
	if (ms_pBloomRTA_SRV) { ms_pBloomRTA_SRV->Release(); ms_pBloomRTA_SRV = nullptr; }
	if (ms_pBloomRTA_RTV) { ms_pBloomRTA_RTV->Release(); ms_pBloomRTA_RTV = nullptr; }
	if (ms_pBloomRTA) { ms_pBloomRTA->Release(); ms_pBloomRTA = nullptr; }
	if (ms_pBloomRTB_SRV) { ms_pBloomRTB_SRV->Release(); ms_pBloomRTB_SRV = nullptr; }
	if (ms_pBloomRTB_RTV) { ms_pBloomRTB_RTV->Release(); ms_pBloomRTB_RTV = nullptr; }
	if (ms_pBloomRTB) { ms_pBloomRTB->Release(); ms_pBloomRTB = nullptr; }
}
#endif

#ifdef ENABLE_SSAO
bool CGraphicDevice::__CreateSSAOResources(int width, int height)
{
	if (!ms_pDevice)
		return false;

	__ReleaseSSAOResources();

	HRESULT hr;
	UINT halfW = max(1u, (UINT)width / 2);
	UINT halfH = max(1u, (UINT)height / 2);

	{
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R32_FLOAT;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		hr = ms_pDevice->CreateTexture2D(&desc, nullptr, &ms_pResolvedDepthTex);
		if (FAILED(hr))
		{
			TraceError("SSAO: Failed to create resolved depth texture: 0x%08X", hr);
			return false;
		}

		hr = ms_pDevice->CreateRenderTargetView(ms_pResolvedDepthTex, nullptr, &ms_pResolvedDepthRTV);
		if (ms_pContext && ms_pResolvedDepthRTV) { const float c[4] = {0.0f,0.0f,0.0f,0.0f}; ms_pContext->ClearRenderTargetView(ms_pResolvedDepthRTV, c); }
		if (FAILED(hr)) { __ReleaseSSAOResources(); return false; }

		hr = ms_pDevice->CreateShaderResourceView(ms_pResolvedDepthTex, nullptr, &ms_pResolvedDepthSRV);
		if (FAILED(hr)) { __ReleaseSSAOResources(); return false; }
	}

	// SSAO RT (half resolution, R8_UNORM)
	{
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = halfW;
		desc.Height = halfH;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		hr = ms_pDevice->CreateTexture2D(&desc, nullptr, &ms_pSSAO_RT);
		if (FAILED(hr)) { __ReleaseSSAOResources(); return false; }

		hr = ms_pDevice->CreateRenderTargetView(ms_pSSAO_RT, nullptr, &ms_pSSAO_RTV);
		if (ms_pContext && ms_pSSAO_RTV) { const float c[4] = {0.0f,0.0f,0.0f,0.0f}; ms_pContext->ClearRenderTargetView(ms_pSSAO_RTV, c); }
		if (FAILED(hr)) { __ReleaseSSAOResources(); return false; }

		hr = ms_pDevice->CreateShaderResourceView(ms_pSSAO_RT, nullptr, &ms_pSSAO_SRV);
		if (FAILED(hr)) { __ReleaseSSAOResources(); return false; }
	}

	// SSAO Blur RT (half resolution, R8_UNORM)
	{
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = halfW;
		desc.Height = halfH;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		hr = ms_pDevice->CreateTexture2D(&desc, nullptr, &ms_pSSAOBlur_RT);
		if (FAILED(hr)) { __ReleaseSSAOResources(); return false; }

		hr = ms_pDevice->CreateRenderTargetView(ms_pSSAOBlur_RT, nullptr, &ms_pSSAOBlur_RTV);
		if (ms_pContext && ms_pSSAOBlur_RTV) { const float c[4] = {0.0f,0.0f,0.0f,0.0f}; ms_pContext->ClearRenderTargetView(ms_pSSAOBlur_RTV, c); }
		if (FAILED(hr)) { __ReleaseSSAOResources(); return false; }

		hr = ms_pDevice->CreateShaderResourceView(ms_pSSAOBlur_RT, nullptr, &ms_pSSAOBlur_SRV);
		if (FAILED(hr)) { __ReleaseSSAOResources(); return false; }
	}

	Tracef("SSAO: Resources created (%dx%d half-res, %dx%d resolved depth)\n", halfW, halfH, width, height);
	return true;
}

void CGraphicDevice::__ReleaseSSAOResources()
{
	if (ms_pSSAOBlur_SRV) { ms_pSSAOBlur_SRV->Release(); ms_pSSAOBlur_SRV = nullptr; }
	if (ms_pSSAOBlur_RTV) { ms_pSSAOBlur_RTV->Release(); ms_pSSAOBlur_RTV = nullptr; }
	if (ms_pSSAOBlur_RT) { ms_pSSAOBlur_RT->Release(); ms_pSSAOBlur_RT = nullptr; }
	if (ms_pSSAO_SRV) { ms_pSSAO_SRV->Release(); ms_pSSAO_SRV = nullptr; }
	if (ms_pSSAO_RTV) { ms_pSSAO_RTV->Release(); ms_pSSAO_RTV = nullptr; }
	if (ms_pSSAO_RT) { ms_pSSAO_RT->Release(); ms_pSSAO_RT = nullptr; }
	if (ms_pResolvedDepthSRV) { ms_pResolvedDepthSRV->Release(); ms_pResolvedDepthSRV = nullptr; }
	if (ms_pResolvedDepthRTV) { ms_pResolvedDepthRTV->Release(); ms_pResolvedDepthRTV = nullptr; }
	if (ms_pResolvedDepthTex) { ms_pResolvedDepthTex->Release(); ms_pResolvedDepthTex = nullptr; }
}
#endif


bool CGraphicDevice::__IsInDriverBlackList(D3D_CAdapterInfo& rkD3DAdapterInfo)
{
	// Keep the blacklist check for compatibility
	DXGI_ADAPTER_DESC adapterDesc;

	IDXGIDevice* pDXGIDevice = nullptr;
	IDXGIAdapter* pDXGIAdapter = nullptr;

	if (SUCCEEDED(ms_pDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDXGIDevice)))
	{
		if (SUCCEEDED(pDXGIDevice->GetAdapter(&pDXGIAdapter)))
		{
			pDXGIAdapter->GetDesc(&adapterDesc);
			pDXGIAdapter->Release();
		}
		pDXGIDevice->Release();
	}

	// Check against blacklist file
	FILE* fp = fopen("grpblk.txt", "r");
	if (fp)
	{
		char szLine[256];
		while (fgets(szLine, sizeof(szLine) - 1, fp))
		{
			// Simple vendor ID check
			DWORD dwVendorId = 0;
			if (sscanf(szLine, "%x", &dwVendorId) == 1)
			{
				if (adapterDesc.VendorId == dwVendorId)
				{
					fclose(fp);
					return true;
				}
			}
		}
		fclose(fp);
	}

	return false;
}

int CGraphicDevice::Create(HWND hWnd, int iHres, int iVres, bool Windowed, int /*iBit*/, int iRefreshRate)
{
	int iRet = CREATE_OK;

	Destroy();

	ms_iWidth = iHres;
	ms_iHeight = iVres;
	ms_hWnd = hWnd;
	ms_hDC = GetDC(hWnd);
	m_bWindowed = Windowed;

	{
		RECT clientRect;
		GetClientRect(hWnd, &clientRect);
		HBRUSH hBlackBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
		FillRect(ms_hDC, &clientRect, hBlackBrush);
	}

	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_11_0
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	UINT createDeviceFlags = 0;

	const UINT uFlipBufferCount = (m_uBackBufferCount < 2) ? 2 : (UINT)m_uBackBufferCount;

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.BufferCount = uFlipBufferCount;
	swapChainDesc.BufferDesc.Width = iHres;
	swapChainDesc.BufferDesc.Height = iVres;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = iRefreshRate > 0 ? iRefreshRate : 60;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = hWnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.Windowed = Windowed ? TRUE : FALSE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = 0;

	// Check for force WARP mode via config file
	bool bForceWARP = false;
	FILE* fpConfig = fopen("d3d11_warp.txt", "r");
	if (fpConfig)
	{
		bForceWARP = true;
		fclose(fpConfig);
	}

	D3D_DRIVER_TYPE driverType = bForceWARP ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE;
	const char* driverName = bForceWARP ? "WARP" : "Hardware";

	// ── Step 1: create the DEVICE only (the swap chain is created separately, below) ────────────
	HRESULT hr = D3D11CreateDevice(
		nullptr, driverType, nullptr, createDeviceFlags,
		featureLevels, numFeatureLevels, D3D11_SDK_VERSION,
		&ms_pDevice, &ms_FeatureLevel, &ms_pContext);

	if (FAILED(hr))
	{
		TraceError("CGraphicDevice::Create - %s device failed (0x%08X), trying fallback...", driverName, hr);

		// Try the opposite driver type as fallback
		D3D_DRIVER_TYPE fallbackType = bForceWARP ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_WARP;
		const char* fallbackName = bForceWARP ? "Hardware" : "WARP";

		hr = D3D11CreateDevice(
			nullptr, fallbackType, nullptr, createDeviceFlags,
			featureLevels, numFeatureLevels, D3D11_SDK_VERSION,
			&ms_pDevice, &ms_FeatureLevel, &ms_pContext);

		if (FAILED(hr))
		{
			TraceError("CGraphicDevice::Create - %s also failed: 0x%08X", fallbackName, hr);
			return CREATE_DEVICE;
		}
	}

	if (ms_FeatureLevel < D3D_FEATURE_LEVEL_11_0)
	{
		TraceError("CGraphicDevice::Create - Feature level 11.0 required (Shader Model 5.0)");
		Destroy();
		return CREATE_DEVICE;
	}

	// ── Step 2: DXGI factory from the device (IDXGIFactory2 = DXGI 1.2 → flip model available) ──
	IDXGIFactory2* pFactory2 = nullptr;
	{
		IDXGIDevice* pDXGIDevice = nullptr;
		IDXGIAdapter* pDXGIAdapter = nullptr;
		if (SUCCEEDED(ms_pDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDXGIDevice)))
		{
			if (SUCCEEDED(pDXGIDevice->GetAdapter(&pDXGIAdapter)))
			{
				pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&ms_pDXGIFactory);
				pDXGIAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&pFactory2);
				pDXGIAdapter->Release();
			}
			pDXGIDevice->Release();
		}
	}

	// ── Step 3: swap chain — FLIP model first, blt only as a fallback ───────────────────────────
	if (pFactory2)
	{
		DXGI_SWAP_CHAIN_DESC1 sd1 = {};
		sd1.Width              = iHres;
		sd1.Height             = iVres;
		sd1.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;   // flip-model compatible
		sd1.Stereo             = FALSE;
		sd1.SampleDesc.Count   = 1;      // no MSAA on a flip swap chain — engine resolves into it
		sd1.SampleDesc.Quality = 0;
		sd1.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd1.BufferCount        = uFlipBufferCount;             // >= 2 required by flip
		sd1.Scaling            = DXGI_SCALING_STRETCH;         // matches the old blt resize behaviour
		sd1.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sd1.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;
		sd1.Flags              = 0;

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsd = {};
		fsd.RefreshRate.Numerator   = iRefreshRate > 0 ? iRefreshRate : 60;
		fsd.RefreshRate.Denominator = 1;
		fsd.ScanlineOrdering        = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		fsd.Scaling                 = DXGI_MODE_SCALING_UNSPECIFIED;
		fsd.Windowed                = Windowed ? TRUE : FALSE;

		IDXGISwapChain1* pSwap1 = nullptr;
		HRESULT hrSwap = pFactory2->CreateSwapChainForHwnd(ms_pDevice, hWnd, &sd1, &fsd, nullptr, &pSwap1);
		if (SUCCEEDED(hrSwap) && pSwap1)
		{
			hrSwap = pSwap1->QueryInterface(__uuidof(IDXGISwapChain), (void**)&ms_pSwapChain);
			pSwap1->Release();
		}

		if (FAILED(hrSwap) || !ms_pSwapChain)
		{
			TraceError("CGraphicDevice::Create - flip-model swap chain failed (0x%08X), falling back to blt", hrSwap);
			ms_pSwapChain = nullptr;
			swapChainDesc.SwapEffect  = DXGI_SWAP_EFFECT_DISCARD;
			swapChainDesc.BufferCount = m_uBackBufferCount ? m_uBackBufferCount : 1;
		}
		pFactory2->Release();
	}
	else
	{
		// No DXGI 1.2 (pre-Win8 without the Platform Update) — keep the legacy blt path alive.
		swapChainDesc.SwapEffect  = DXGI_SWAP_EFFECT_DISCARD;
		swapChainDesc.BufferCount = m_uBackBufferCount ? m_uBackBufferCount : 1;
	}

	if (!ms_pSwapChain)
	{
		if (!ms_pDXGIFactory)
		{
			TraceError("CGraphicDevice::Create - no DXGI factory available for swap chain creation");
			Destroy();
			return CREATE_DEVICE;
		}
		hr = ms_pDXGIFactory->CreateSwapChain(ms_pDevice, &swapChainDesc, &ms_pSwapChain);
		if (FAILED(hr) || !ms_pSwapChain)
		{
			TraceError("CGraphicDevice::Create - CreateSwapChain failed: 0x%08X", hr);
			Destroy();
			return CREATE_DEVICE;
		}
	}

	if (ms_pDXGIFactory)
		ms_pDXGIFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

	m_SwapChainDesc = swapChainDesc;

	// Create render target view
	if (!__CreateRenderTargetView())
	{
		Destroy();
		return CREATE_RENDERTARGET;
	}

	// Check device status after RTV
	hr = ms_pDevice->GetDeviceRemovedReason();
	if (hr != S_OK)
	{
		TraceError("CGraphicDevice::Create - Device hung after RTV creation: 0x%08X", hr);
		Destroy();
		return CREATE_DEVICE;
	}

	// Create depth stencil view
	if (!__CreateDepthStencilView(iHres, iVres))
	{
		Destroy();
		return CREATE_DEPTHSTENCIL;
	}

	// Check device status after DSV
	hr = ms_pDevice->GetDeviceRemovedReason();
	if (hr != S_OK)
	{
		TraceError("CGraphicDevice::Create - Device hung after DSV creation: 0x%08X", hr);
		Destroy();
		return CREATE_DEVICE;
	}

#ifdef ENABLE_MSAA
	// Create MSAA offscreen render target
	if (!__CreateMSAARenderTarget(iHres, iVres))
	{
		Tracef("CGraphicDevice::Create - MSAA not available, continuing without AA\n");
	}
#endif

	// Bind render targets (MSAA if available)
	{
		ID3D11RenderTargetView* pRTV = GetRenderTargetView();
		ID3D11DepthStencilView* pDSV = GetDepthStencilView();
		ms_pContext->OMSetRenderTargets(1, &pRTV, pDSV);
	}

	// Setup viewport
	ms_Viewport.TopLeftX = 0.0f;
	ms_Viewport.TopLeftY = 0.0f;
	ms_Viewport.Width = (float)iHres;
	ms_Viewport.Height = (float)iVres;
	ms_Viewport.MinDepth = 0.0f;
	ms_Viewport.MaxDepth = 1.0f;
	ms_pContext->RSSetViewports(1, &ms_Viewport);

	{
		float blackColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		ID3D11RenderTargetView* pRTV = GetRenderTargetView();
		ID3D11DepthStencilView* pDSV = GetDepthStencilView();
		ms_pContext->ClearRenderTargetView(pRTV, blackColor);
		ms_pContext->ClearDepthStencilView(pDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
#ifdef ENABLE_MSAA
		ResolveMSAA();
#endif
		ms_pSwapChain->Present(0, 0);  // Present the black frame
	}

	// Position window for fullscreen
	if (!Windowed)
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, iHres, iVres, SWP_SHOWWINDOW);

	// Check device status before shader initialization
	hr = ms_pDevice->GetDeviceRemovedReason();
	if (hr != S_OK)
	{
		TraceError("CGraphicDevice::Create - Device hung before shader init: 0x%08X", hr);
		Destroy();
		return CREATE_DEVICE;
	}

	// Initialize shader system for DX11 rendering
	if (!SHADERMANAGER.Initialize(ms_pDevice, ms_pContext))
	{
		TraceError("CGraphicDevice::Create - Shader system failed to initialize");
		Destroy();
		return CREATE_DEVICE;
	}

#ifdef ENABLE_BLOOM
	if (!__CreateBloomResources(iHres, iVres))
	{
		Tracef("CGraphicDevice::Create - Bloom resources not available\n");
		ms_bBloomEnabled = false;
	}
#endif

#ifdef ENABLE_SSAO
	if (!__CreateSSAOResources(iHres, iVres))
	{
		Tracef("CGraphicDevice::Create - SSAO resources not available\n");
		ms_bSSAOEnabled = false;
	}
#endif


	if (!VTFMANAGER.Initialize(ms_pDevice, ms_pContext))
	{
		// VTF is optional - single-draw fallback still works
		Tracef("CGraphicDevice::Create - VTF Instance Manager not available (non-batched rendering)\n");
	}

	{
		float blackColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		ID3D11RenderTargetView* pRTV = GetRenderTargetView();
		ID3D11DepthStencilView* pDSV = GetDepthStencilView();
		if (pRTV)
			ms_pContext->ClearRenderTargetView(pRTV, blackColor);
		if (pDSV)
			ms_pContext->ClearDepthStencilView(pDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		if (ms_pSwapChain)
		{
#ifdef ENABLE_MSAA
			ResolveMSAA();
#endif
			ms_pSwapChain->Present(0, 0);
		}
	}

	if (gs_pStateManager)
	{
		delete gs_pStateManager;
		gs_pStateManager = nullptr;
	}
	gs_pStateManager = new CStateManager(ms_pDevice, ms_pContext);

	// Create matrix stack for compatibility
	ms_pMatrixStack = new CMatrixStack();
	ms_pMatrixStack->LoadIdentity();

	// Initialize identity matrices
	MatrixIdentity(&ms_matIdentity);
	MatrixIdentity(&ms_matView);
	MatrixIdentity(&ms_matProj);
	MatrixIdentity(&ms_matInverseView);
	MatrixIdentity(&ms_matInverseViewYAxis);
	MatrixIdentity(&ms_matScreen0);
	MatrixIdentity(&ms_matScreen1);
	MatrixIdentity(&ms_matScreen2);

	ms_matScreen0._11 = 1;
	ms_matScreen0._22 = -1;

	ms_matScreen1._41 = 1;
	ms_matScreen1._42 = 1;

	ms_matScreen2._11 = (float)iHres / 2;
	ms_matScreen2._22 = (float)iVres / 2;

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	Clear(clearColor);

	if (ms_pSwapChain)
	{
		Present(false);    // First present (handles MSAA resolve)
		Clear(clearColor); // Clear again
		Present(false);    // Second present (for double buffering)
	}

	// Flush GPU and check status after clear
	ms_pContext->Flush();
	hr = ms_pDevice->GetDeviceRemovedReason();
	if (hr != S_OK)
	{
		TraceError("CGraphicDevice::Create - Device hung after clear: 0x%08X", hr);
		Destroy();
		return CREATE_DEVICE;
	}

	// Create default buffers
	if (!__CreateDefaultIndexBufferList())
		return CREATE_DEVICE;

	if (!__CreatePDTVertexBufferList())
		return CREATE_DEVICE;

	// Check texture memory
	// DX11 doesn't expose this directly, use DXGI
	IDXGIDevice* pDXGIDeviceMem = nullptr;
	IDXGIAdapter* pAdapterMem = nullptr;
	if (SUCCEEDED(ms_pDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDXGIDeviceMem)))
	{
		if (SUCCEEDED(pDXGIDeviceMem->GetAdapter(&pAdapterMem)))
		{
			DXGI_ADAPTER_DESC adapterDesc;
			pAdapterMem->GetDesc(&adapterDesc);

			SIZE_T dwTexMemSize = adapterDesc.DedicatedVideoMemory;

			if (dwTexMemSize < 64 * 1024 * 1024)
				ms_isLowTextureMemory = true;
			else
				ms_isLowTextureMemory = false;

			if (dwTexMemSize > 100 * 1024 * 1024)
				ms_isHighTextureMemory = true;
			else
				ms_isHighTextureMemory = false;

			pAdapterMem->Release();
		}
		pDXGIDeviceMem->Release();
	}

	// DX11 always supports texture address border
	GRAPHICS_CAPS_CAN_NOT_TEXTURE_ADDRESS_BORDER = false;

	// DXT support check
	UINT formatSupport = 0;
	ms_bSupportDXT = true;

	if (FAILED(ms_pDevice->CheckFormatSupport(DXGI_FORMAT_BC1_UNORM, &formatSupport)) ||
		!(formatSupport & D3D11_FORMAT_SUPPORT_TEXTURE2D))
	{
		ms_bSupportDXT = false;
	}

	// Flush the GPU to ensure all pending operations complete
	ms_pContext->Flush();

	// Verify device is still valid after initialization
	HRESULT hrCheck = ms_pDevice->GetDeviceRemovedReason();
	if (hrCheck != S_OK)
	{
		TraceError("CGraphicDevice::Create - Device became invalid during initialization: 0x%08X", hrCheck);
		Destroy();
		return CREATE_DEVICE;
	}

	return iRet;
}

void CGraphicDevice::Present(bool bVSync)
{
	if (ms_pSwapChain)
	{
#ifdef ENABLE_MSAA
		ResolveMSAA();  // Always resolve to swap chain (safety fallback)
#ifdef ENABLE_BLOOM
		if (ms_bBloomEnabled && ms_pSceneTextureSRV)
		{
			// Separate resolve to scene texture for bloom input
			ResolveMSAAToSceneTexture();

#ifdef ENABLE_SSAO
			ID3D11ShaderResourceView* pSSAO_SRV = nullptr;
			if (ms_bSSAOEnabled && ms_pSSAO_RTV && SHADERMANAGER.GetFrameCount() > 15)
			{
				UINT fullW = (UINT)ms_Viewport.Width;
				UINT fullH = (UINT)ms_Viewport.Height;
				UINT halfW = max(1u, fullW / 2);
				UINT halfH = max(1u, fullH / 2);

				if (ms_uMSAASampleCount > 1 && ms_pMSAADepthStencilSRV && ms_pResolvedDepthRTV)
				{
					SHADERMANAGER.RenderDepthResolve(ms_pMSAADepthStencilSRV, ms_pResolvedDepthRTV, fullW, fullH);
				}

				ID3D11ShaderResourceView* pDepthSRV = GetDepthSRV();
				if (pDepthSRV)
				{
					SHADERMANAGER.RenderSSAOPass(pDepthSRV, ms_pSSAO_RTV, halfW, halfH, fullW, fullH);
					SHADERMANAGER.RenderSSAOBlur(ms_pSSAO_SRV, pDepthSRV, ms_pSSAOBlur_RTV, halfW, halfH);
					pSSAO_SRV = ms_pSSAOBlur_SRV;
				}
			}
#endif

#ifdef ENABLE_GODRAYS
			if (SHADERMANAGER.IsGodRaysEnabled() && ms_pGodRaysRTV)
			{
				SHADERMANAGER.RenderGodRaysPass(
					ms_pSceneTextureSRV, ms_pGodRaysRTV,
					max(1u, (UINT)ms_Viewport.Width / 4),
					max(1u, (UINT)ms_Viewport.Height / 4));
			}
			ID3D11ShaderResourceView* pGodRaysSRV = ms_pGodRaysSRV;
#else
			ID3D11ShaderResourceView* pGodRaysSRV = nullptr;
#endif

#ifndef ENABLE_SSAO
			ID3D11ShaderResourceView* pSSAO_SRV = nullptr;
#endif

			ID3D11RenderTargetView* pSwapRTV = GetSwapChainRenderTargetView();
			SHADERMANAGER.RenderBloom(
				ms_pSceneTextureSRV,
				ms_pBloomRTA_RTV, ms_pBloomRTA_SRV,
				ms_pBloomRTB_RTV, ms_pBloomRTB_SRV,
				max(1u, (UINT)ms_Viewport.Width / 4), max(1u, (UINT)ms_Viewport.Height / 4),
				pGodRaysSRV,
				pSSAO_SRV,
				pSwapRTV, (UINT)ms_Viewport.Width, (UINT)ms_Viewport.Height);
		}
#endif
#endif
		ms_pSwapChain->Present(bVSync ? 1 : 0, 0);
	}

	SHADERMANAGER.ResetDynamicBuffers();
}

void CGraphicDevice::Clear(const float color[4], float depth, UINT8 stencil)
{
	if (ms_pContext)
	{
		ID3D11RenderTargetView* pRTV = GetRenderTargetView();
		ID3D11DepthStencilView* pDSV = GetDepthStencilView();

		if (pRTV)
			ms_pContext->ClearRenderTargetView(pRTV, color);

		if (pDSV)
			ms_pContext->ClearDepthStencilView(pDSV,
				D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
	}
}

#ifdef ENABLE_FIX_MOBS_LAG
void CGraphicDevice::__InitializePDTVertexBufferList()
{
	m_tinyPdtVertexBuffer = nullptr;
	m_smallPdtVertexBuffer = nullptr;
	m_mediumPdtVertexBuffer = nullptr;
	m_largePdtVertexBuffer = nullptr;
	m_xlargePdtVertexBuffer = nullptr;
}

void CGraphicDevice::__DestroyPDTVertexBufferList()
{
	if (m_tinyPdtVertexBuffer) {
		m_tinyPdtVertexBuffer->Release();
		m_tinyPdtVertexBuffer = nullptr;
	}

	if (m_smallPdtVertexBuffer) {
		m_smallPdtVertexBuffer->Release();
		m_smallPdtVertexBuffer = nullptr;
	}

	if (m_mediumPdtVertexBuffer) {
		m_mediumPdtVertexBuffer->Release();
		m_mediumPdtVertexBuffer = nullptr;
	}

	if (m_largePdtVertexBuffer) {
		m_largePdtVertexBuffer->Release();
		m_largePdtVertexBuffer = nullptr;
	}

	if (m_xlargePdtVertexBuffer) {
		m_xlargePdtVertexBuffer->Release();
		m_xlargePdtVertexBuffer = nullptr;
	}
}

bool CGraphicDevice::__CreatePDTVertexBufferList()
{
	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	// Tiny buffer
	bufferDesc.ByteWidth = sizeof(TPDTVertex) * TINY_PDT_VERTEX_BUFFER_SIZE;
	if (FAILED(ms_pDevice->CreateBuffer(&bufferDesc, nullptr, &m_tinyPdtVertexBuffer)))
		return false;

	// Small buffer
	bufferDesc.ByteWidth = sizeof(TPDTVertex) * SMALL_PDT_VERTEX_BUFFER_SIZE;
	if (FAILED(ms_pDevice->CreateBuffer(&bufferDesc, nullptr, &m_smallPdtVertexBuffer)))
		return false;

	// Medium buffer
	bufferDesc.ByteWidth = sizeof(TPDTVertex) * MEDIUM_PDT_VERTEX_BUFFER_SIZE;
	if (FAILED(ms_pDevice->CreateBuffer(&bufferDesc, nullptr, &m_mediumPdtVertexBuffer)))
		return false;

	// Large buffer
	bufferDesc.ByteWidth = sizeof(TPDTVertex) * LARGE_PDT_VERTEX_BUFFER_SIZE;
	if (FAILED(ms_pDevice->CreateBuffer(&bufferDesc, nullptr, &m_largePdtVertexBuffer)))
		return false;

	// XLarge buffer
	bufferDesc.ByteWidth = sizeof(TPDTVertex) * XLARGE_PDT_VERTEX_BUFFER_SIZE;
	if (FAILED(ms_pDevice->CreateBuffer(&bufferDesc, nullptr, &m_xlargePdtVertexBuffer)))
		return false;

	return true;
}
#else
void CGraphicDevice::__InitializePDTVertexBufferList()
{
	for (UINT i = 0; i < PDT_VERTEXBUFFER_NUM; ++i)
		ms_alpd3dPDTVB[i] = nullptr;
}

void CGraphicDevice::__DestroyPDTVertexBufferList()
{
	for (UINT i = 0; i < PDT_VERTEXBUFFER_NUM; ++i)
	{
		if (ms_alpd3dPDTVB[i])
		{
			ms_alpd3dPDTVB[i]->Release();
			ms_alpd3dPDTVB[i] = nullptr;
		}
	}
}

bool CGraphicDevice::__CreatePDTVertexBufferList()
{
	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.ByteWidth = sizeof(TPDTVertex) * PDT_VERTEX_NUM;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	for (UINT i = 0; i < PDT_VERTEXBUFFER_NUM; ++i)
	{
		if (FAILED(ms_pDevice->CreateBuffer(&bufferDesc, nullptr, &ms_alpd3dPDTVB[i])))
			return false;
	}
	return true;
}
#endif

void CGraphicDevice::__InitializeDefaultIndexBufferList()
{
	for (UINT i = 0; i < DEFAULT_IB_NUM; ++i)
		ms_alpd3dDefIB[i] = nullptr;
}

void CGraphicDevice::__DestroyDefaultIndexBufferList()
{
	for (UINT i = 0; i < DEFAULT_IB_NUM; ++i)
	{
		if (ms_alpd3dDefIB[i])
		{
			ms_alpd3dDefIB[i]->Release();
			ms_alpd3dDefIB[i] = nullptr;
		}
	}
}

bool CGraphicDevice::__CreateDefaultIndexBuffer(UINT eDefIB, UINT uIdxCount, const WORD* c_awIndices)
{
	assert(ms_alpd3dDefIB[eDefIB] == nullptr);

	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.ByteWidth = sizeof(WORD) * uIdxCount;
	bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA initData;
	ZeroMemory(&initData, sizeof(initData));
	initData.pSysMem = c_awIndices;

	if (FAILED(ms_pDevice->CreateBuffer(&bufferDesc, &initData, &ms_alpd3dDefIB[eDefIB])))
		return false;

	return true;
}

bool CGraphicDevice::__CreateDefaultIndexBufferList()
{
	static const WORD c_awLineIndices[2] = { 0, 1, };
	static const WORD c_awLineTriIndices[6] = { 0, 1, 0, 2, 1, 2, };
	static const WORD c_awLineRectIndices[8] = { 0, 1, 0, 2, 1, 3, 2, 3, };
	static const WORD c_awLineCubeIndices[24] = {
		0, 1, 0, 2, 1, 3, 2, 3,
		0, 4, 1, 5, 2, 6, 3, 7,
		4, 5, 4, 6, 5, 7, 6, 7,
	};
	static const WORD c_awFillTriIndices[3] = { 0, 1, 2, };
	static const WORD c_awFillRectIndices[6] = { 0, 2, 1, 2, 3, 1, };
	static const WORD c_awFillCubeIndices[36] = {
		0, 1, 2, 1, 3, 2,
		2, 0, 6, 0, 4, 6,
		0, 1, 4, 1, 5, 4,
		1, 3, 5, 3, 7, 5,
		3, 2, 7, 2, 6, 7,
		4, 5, 6, 5, 7, 6,
	};

	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_LINE, 2, c_awLineIndices))
		return false;
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_LINE_TRI, 6, c_awLineTriIndices))
		return false;
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_LINE_RECT, 8, c_awLineRectIndices))
		return false;
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_LINE_CUBE, 24, c_awLineCubeIndices))
		return false;
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_FILL_TRI, 3, c_awFillTriIndices))
		return false;
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_FILL_RECT, 6, c_awFillRectIndices))
		return false;
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_FILL_CUBE, 36, c_awFillCubeIndices))
		return false;

	static const UINT MAX_BATCH_QUADS = 512;
	WORD awFillRectMultiIndices[MAX_BATCH_QUADS * 6];
	for (UINT q = 0; q < MAX_BATCH_QUADS; ++q)
	{
		WORD base = (WORD)(q * 4);
		awFillRectMultiIndices[q * 6 + 0] = base + 0;
		awFillRectMultiIndices[q * 6 + 1] = base + 2;
		awFillRectMultiIndices[q * 6 + 2] = base + 1;
		awFillRectMultiIndices[q * 6 + 3] = base + 2;
		awFillRectMultiIndices[q * 6 + 4] = base + 3;
		awFillRectMultiIndices[q * 6 + 5] = base + 1;
	}
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_FILL_RECT_MULTI, MAX_BATCH_QUADS * 6, awFillRectMultiIndices))
		return false;

	return true;
}

void CGraphicDevice::InitBackBufferCount(UINT uBackBufferCount)
{
	m_uBackBufferCount = uBackBufferCount;
}

void CGraphicDevice::Destroy()
{
#ifdef ENABLE_SSAO
	ms_bSSAOEnabled = false;
#endif

	if (ms_pContext && ms_pSwapChain)
	{
		DXGI_SWAP_CHAIN_DESC scdShut = {};
		ms_pSwapChain->GetDesc(&scdShut);
		const UINT uShutBuffers = (scdShut.BufferCount > 0 && scdShut.BufferCount <= 8) ? scdShut.BufferCount : 3;
		const float blackColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

		for (UINT b = 0; b < uShutBuffers; ++b)
		{
			ID3D11Texture2D* pBB = nullptr;
			if (FAILED(ms_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBB)) || !pBB)
				break;

			ID3D11RenderTargetView* pShutRTV = nullptr;
			if (SUCCEEDED(ms_pDevice->CreateRenderTargetView(pBB, nullptr, &pShutRTV)) && pShutRTV)
			{
				ms_pContext->ClearRenderTargetView(pShutRTV, blackColor);
				ms_pSwapChain->Present(0, 0);
				pShutRTV->Release();
			}
			pBB->Release();
		}
	}


	if (ms_hWnd)
		ShowWindow(ms_hWnd, SW_HIDE);

	if (gs_pStateManager)
	{
		delete gs_pStateManager;
		gs_pStateManager = nullptr;
	}

	// Shutdown shader system
	CShaderManager* pShaderMgr = CShaderManager::InstancePtr();
	if (pShaderMgr && pShaderMgr->IsInitialized())
	{
		pShaderMgr->Shutdown();
	}

	__DestroyPDTVertexBufferList();
	__DestroyDefaultIndexBufferList();

	if (ms_hDC)
	{
		ReleaseDC(ms_hWnd, ms_hDC);
		ms_hDC = NULL;
	}


	if (ms_pMatrixStack)
	{
		delete ms_pMatrixStack;
		ms_pMatrixStack = nullptr;
	}

	__ReleaseBackBufferResources();

	if (ms_pSwapChain)
	{
		ms_pSwapChain->Release();
		ms_pSwapChain = nullptr;
	}

	if (ms_pDXGIFactory)
	{
		ms_pDXGIFactory->Release();
		ms_pDXGIFactory = nullptr;
	}

	if (ms_pContext)
	{
		ms_pContext->ClearState();
		ms_pContext->Flush();
		ms_pContext->Release();
		ms_pContext = nullptr;
	}

	if (ms_pDevice)
	{
		ms_pDevice->Release();
		ms_pDevice = nullptr;
	}

	__Initialize();
}
