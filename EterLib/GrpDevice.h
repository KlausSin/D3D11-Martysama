#pragma once

#include "GrpBase.h"
#include "GrpDetector.h"
#include "../eterBase/Singleton.h"

class CGraphicDevice : public CGraphicBase, public CSingleton<CGraphicDevice>
{
public:
	enum EDeviceState
	{
		DEVICESTATE_OK,           // Device is operational
		DEVICESTATE_BROKEN,       // Device creation failed
		DEVICESTATE_NULL          // Device not created
	};

	enum ECreateReturnValues
	{
		CREATE_OK				= (1 << 0),
		CREATE_NO_DIRECTX		= (1 << 1),
		CREATE_GET_DEVICE_CAPS	= (1 << 2),
		CREATE_GET_DEVICE_CAPS2 = (1 << 3),
		CREATE_DEVICE			= (1 << 4),
		CREATE_REFRESHRATE		= (1 << 5),
		CREATE_ENUM				= (1 << 6),
		CREATE_DETECT			= (1 << 7),
		CREATE_BAD_DRIVER		= (1 << 9),
		CREATE_FORMAT			= (1 << 10),
		CREATE_SWAPCHAIN		= (1 << 11),
		CREATE_RENDERTARGET		= (1 << 12),
		CREATE_DEPTHSTENCIL		= (1 << 13),
	};

	CGraphicDevice();
	virtual ~CGraphicDevice();

	void			InitBackBufferCount(UINT uBackBufferCount);

	void			Destroy();
	int				Create(HWND hWnd, int hres, int vres, bool Windowed = true, int bit = 32, int RefreshRate = 0);

	EDeviceState	GetDeviceState();

	void			EnableWebBrowserMode(const RECT& c_rcWebPage);
	void			DisableWebBrowserMode();
	void			MoveWebBrowserRect(const RECT& c_rcWebPage);

	bool			ResizeBuffers(UINT uWidth, UINT uHeight);
	bool			ResizeBackBuffer(UINT uWidth, UINT uHeight) { return ResizeBuffers(uWidth, uHeight); }  // Legacy alias
	bool			Reset() { return true; }
	void			RegisterWarningString(UINT uiMsg, const char * c_szString);

	// DX11 specific accessors
	ID3D11Device*			GetD3D11Device() const { return ms_pDevice; }
	ID3D11DeviceContext*	GetD3D11Context() const { return ms_pContext; }
	IDXGISwapChain*			GetSwapChain() const { return ms_pSwapChain; }
	ID3D11RenderTargetView*	GetRenderTargetView() const { return CGraphicBase::GetRenderTargetView(); }
	ID3D11DepthStencilView*	GetDepthStencilView() const { return CGraphicBase::GetDepthStencilView(); }
	IDXGIFactory*			GetDXGIFactory() const { return ms_pDXGIFactory; }

	// Present the back buffer
	void			Present(bool bVSync = true);

	// Clear render target and depth buffer
	void			Clear(const float color[4], float depth = 1.0f, UINT8 stencil = 0);

protected:
	void __Initialize();
	bool __IsInDriverBlackList(D3D_CAdapterInfo& rkD3DAdapterInfo);
	void __WarningMessage(HWND hWnd, UINT uiMsg);

	void __InitializeDefaultIndexBufferList();
	void __DestroyDefaultIndexBufferList();
	bool __CreateDefaultIndexBufferList();
	bool __CreateDefaultIndexBuffer(UINT eDefIB, UINT uIdxCount, const WORD* c_awIndices);

	void __InitializePDTVertexBufferList();
	void __DestroyPDTVertexBufferList();
	bool __CreatePDTVertexBufferList();

	// DX11 specific helpers
	bool __CreateRenderTargetView();
	bool __CreateDepthStencilView(int width, int height);
	void __ReleaseBackBufferResources();

#ifdef ENABLE_MSAA
	bool __CreateMSAARenderTarget(int width, int height);
	void __ReleaseMSAAResources();
#endif

#ifdef ENABLE_BLOOM
	bool __CreateBloomResources(int width, int height);
	void __ReleaseBloomResources();
#endif

#ifdef ENABLE_SSAO
	bool __CreateSSAOResources(int width, int height);
	void __ReleaseSSAOResources();
#endif


protected:
	DWORD						m_uBackBufferCount;
	std::map<UINT, std::string>	m_kMap_strWarningMessage;

	// Swap chain description for resize
	DXGI_SWAP_CHAIN_DESC		m_SwapChainDesc;
	bool						m_bWindowed;
};
