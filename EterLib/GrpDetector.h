#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <string>


struct D3D_SModeInfo
{
	UINT m_uScrWidth;
	UINT m_uScrHeight;
	UINT m_uScrDepthBit;
 	UINT m_dwD3DBehavior;
	DXGI_FORMAT m_eD3DFmtPixel;
	DXGI_FORMAT m_eD3DFmtDepthStencil;

	VOID GetString(std::string* pstEnumList);
};

class D3D_CAdapterDisplayModeList
{
	public:
		D3D_CAdapterDisplayModeList() : m_uModeNum(0) {}
		~D3D_CAdapterDisplayModeList() {}
		VOID Build(IDXGIAdapter* pAdapter, DXGI_FORMAT eDefaultFormat);

		UINT GetDisplayModeNum() { return m_uModeNum; }
		UINT GetPixelFormatNum() { return m_uFmtNum; }

		const DXGI_MODE_DESC& GetDisplayModer(UINT iMode) { return m_akModes[iMode]; }
		const DXGI_FORMAT& GetPixelFormatr(UINT iFmt) { return m_aeFormats[iFmt]; }

	protected:
		enum
		{
			MODE_MAX = 100,
			FORMAT_MAX = 20,
		};

	protected:
		DXGI_MODE_DESC m_akModes[MODE_MAX];
		DXGI_FORMAT m_aeFormats[FORMAT_MAX];

		UINT m_uModeNum;
		UINT m_uFmtNum;
};

class D3D_CDeviceInfo
{
	public:
		D3D_CDeviceInfo() : m_uModeInfoNum(0) {}
		~D3D_CDeviceInfo() {}
		BOOL Build(IDXGIAdapter* pAdapter, D3D_CAdapterDisplayModeList& rkModeList);
		BOOL Find(UINT uScrWidth, UINT uScrHeight, UINT uScrDepthBits, BOOL isWindowed, UINT* piModeInfo);

		UINT GetD3DModeInfoNum() { return m_uModeInfoNum; }

		VOID GetString(std::string* pstEnumList);

		D3D_SModeInfo& GetD3DModeInfor(UINT iModeInfo) { return m_akModeInfo[iModeInfo]; }
		D3D_SModeInfo* GetD3DModeInfop(UINT iModeInfo) { return &m_akModeInfo[iModeInfo]; }

	protected:
		enum
		{
			MODEINFO_NUM = 150,
		};

	protected:
		const TCHAR* m_szDevDesc;
		D3D_FEATURE_LEVEL m_FeatureLevel;

		UINT m_uModeInfoNum;
		D3D_SModeInfo m_akModeInfo[MODEINFO_NUM];
};

class D3D_CAdapterInfo
{
	public:
		D3D_CAdapterInfo() : m_uDevInfoNum(0) {}
		~D3D_CAdapterInfo() {}
		BOOL Find(UINT uScrWidth, UINT uScrHeight, UINT uScrDepthBits, BOOL isWindowed, UINT* piModeInfo, UINT* piDevInfo);

		BOOL Build(IDXGIAdapter* pAdapter);
		VOID GetString(std::string* pstEnumList);

		DXGI_ADAPTER_DESC& GetIdentifier() { return m_kAdapterDesc; }

		DXGI_MODE_DESC& GetDesktopD3DDisplayModer() { return m_kDesktopMode; }
		DXGI_MODE_DESC* GetDesktopD3DDisplayModep() { return &m_kDesktopMode; }

		D3D_CDeviceInfo* GetD3DDeviceInfop(UINT iDevInfo) { return &m_akDevInfo[iDevInfo]; }
		D3D_SModeInfo* GetD3DModeInfop(UINT iDevInfo, UINT iModeInfo);

	protected:
		enum
		{
			DEVINFO_NUM = 5,
		};

	protected:
		DXGI_ADAPTER_DESC m_kAdapterDesc;
		DXGI_MODE_DESC m_kDesktopMode;

		UINT m_uDevInfoNum;
		D3D_CDeviceInfo m_akDevInfo[DEVINFO_NUM];
};

class D3D_CDisplayModeAutoDetector
{
	public:
		D3D_CDisplayModeAutoDetector();
		~D3D_CDisplayModeAutoDetector();

		BOOL Find(UINT uScrWidth, UINT uScrHeight, UINT uScrDepthBits, BOOL isWindowed, UINT* piModeInfo, UINT* piDevInfo, UINT* piAdapterInfo);
		BOOL Build(IDXGIFactory* pFactory);

		D3D_CAdapterInfo* GetD3DAdapterInfop(UINT iAdapterInfo) { return &m_akAdapterInfo[iAdapterInfo]; }
		D3D_SModeInfo* GetD3DModeInfop(UINT iAdapterInfo, UINT iDevInfo, UINT iModeInfo);

		VOID GetString(std::string* pstEnumList);

	protected:
		enum
		{
			ADAPTERINFO_NUM = 10,
		};

	protected:
		D3D_CAdapterInfo m_akAdapterInfo[ADAPTERINFO_NUM];
		UINT m_uAdapterInfoCount;
};
