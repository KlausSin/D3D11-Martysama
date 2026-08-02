/*
 * GrpDetector.cpp
 * DX11: Display mode detection using DXGI
 */

#include "StdAfx.h"
#include "../eterBase/Stl.h"
#include "GrpDetector.h"

// Minimum resolution filter
static const UINT FILTEROUT_LOWRESOLUTION_WIDTH = 800;
static const UINT FILTEROUT_LOWRESOLUTION_HEIGHT = 600;

// Comparison function for sorting display modes
static int CompareDisplayModeOrder(const void* arg1, const void* arg2)
{
	const DXGI_MODE_DESC* p1 = (const DXGI_MODE_DESC*)arg1;
	const DXGI_MODE_DESC* p2 = (const DXGI_MODE_DESC*)arg2;

	if (p1->Format > p2->Format) return -1;
	if (p1->Format < p2->Format) return +1;
	if (p1->Width < p2->Width) return -1;
	if (p1->Width > p2->Width) return +1;
	if (p1->Height < p2->Height) return -1;
	if (p1->Height > p2->Height) return +1;

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////
// D3D_SModeInfo
/////////////////////////////////////////////////////////////////////////////////

VOID D3D_SModeInfo::GetString(std::string* pstEnumList)
{
	UINT uScrDepthBits = 32;  // DX11 primarily uses 32-bit formats
	switch (m_eD3DFmtPixel)
	{
		case DXGI_FORMAT_B5G6R5_UNORM:
		case DXGI_FORMAT_B5G5R5A1_UNORM:
			uScrDepthBits = 16;
			break;
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		default:
			uScrDepthBits = 32;
			break;
	}

	// DX11 always uses hardware vertex processing
	const char* szVP = "HWVP";

	char szText[1024 + 1];
	_snprintf(szText, sizeof(szText), "%dx%dx%d %s\r\n", m_uScrWidth, m_uScrHeight, uScrDepthBits, szVP);
	pstEnumList->append(szText);
}

/////////////////////////////////////////////////////////////////////////////////
// D3D_CAdapterDisplayModeList
/////////////////////////////////////////////////////////////////////////////////

VOID D3D_CAdapterDisplayModeList::Build(IDXGIAdapter* pAdapter, DXGI_FORMAT eDefaultFormat)
{
	m_uModeNum = 0;
	m_uFmtNum = 0;

	if (!pAdapter)
		return;

	// Get the first output (monitor) for this adapter
	IDXGIOutput* pOutput = NULL;
	if (FAILED(pAdapter->EnumOutputs(0, &pOutput)) || !pOutput)
		return;

	// Store default format
	m_aeFormats[m_uFmtNum++] = eDefaultFormat;

	// Get display mode count
	UINT numModes = 0;
	pOutput->GetDisplayModeList(eDefaultFormat, DXGI_ENUM_MODES_INTERLACED, &numModes, NULL);

	if (numModes > 0)
	{
		DXGI_MODE_DESC* pModes = new DXGI_MODE_DESC[numModes];
		pOutput->GetDisplayModeList(eDefaultFormat, DXGI_ENUM_MODES_INTERLACED, &numModes, pModes);

		for (UINT i = 0; i < numModes && m_uModeNum < MODE_MAX; ++i)
		{
			// Filter out low resolution modes
			if (pModes[i].Width < FILTEROUT_LOWRESOLUTION_WIDTH ||
				pModes[i].Height < FILTEROUT_LOWRESOLUTION_HEIGHT)
				continue;

			bool bDuplicate = false;
			for (UINT j = 0; j < m_uModeNum; ++j)
			{
				if (m_akModes[j].Width == pModes[i].Width &&
					m_akModes[j].Height == pModes[i].Height &&
					m_akModes[j].Format == pModes[i].Format)
				{
					// Keep the one with higher refresh rate
					if (pModes[i].RefreshRate.Numerator / (pModes[i].RefreshRate.Denominator ? pModes[i].RefreshRate.Denominator : 1) >
						m_akModes[j].RefreshRate.Numerator / (m_akModes[j].RefreshRate.Denominator ? m_akModes[j].RefreshRate.Denominator : 1))
					{
						m_akModes[j] = pModes[i];
					}
					bDuplicate = true;
					break;
				}
			}

			if (!bDuplicate)
			{
				m_akModes[m_uModeNum++] = pModes[i];

				// Track unique formats
				bool bFormatExists = false;
				for (UINT f = 0; f < m_uFmtNum; ++f)
				{
					if (m_aeFormats[f] == pModes[i].Format)
					{
						bFormatExists = true;
						break;
					}
				}
				if (!bFormatExists && m_uFmtNum < FORMAT_MAX)
				{
					m_aeFormats[m_uFmtNum++] = pModes[i].Format;
				}
			}
		}

		delete[] pModes;
	}

	// Sort modes by format, then resolution
	if (m_uModeNum > 1)
	{
		qsort(m_akModes, m_uModeNum, sizeof(DXGI_MODE_DESC), CompareDisplayModeOrder);
	}

	pOutput->Release();
}

/////////////////////////////////////////////////////////////////////////////////
// D3D_CDeviceInfo
/////////////////////////////////////////////////////////////////////////////////

BOOL D3D_CDeviceInfo::Build(IDXGIAdapter* pAdapter, D3D_CAdapterDisplayModeList& rkModeList)
{
	m_uModeInfoNum = 0;

	if (!pAdapter)
		return FALSE;

	// Create a temporary D3D11 device to check feature level
	ID3D11Device* pTempDevice = NULL;
	ID3D11DeviceContext* pTempContext = NULL;
	D3D_FEATURE_LEVEL featureLevel;
	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };

	HRESULT hr = D3D11CreateDevice(
		pAdapter,
		D3D_DRIVER_TYPE_UNKNOWN,
		NULL,
		0,
		featureLevels,
		1,
		D3D11_SDK_VERSION,
		&pTempDevice,
		&featureLevel,
		&pTempContext
	);

	if (FAILED(hr) || !pTempDevice)
	{
		// Try without specifying feature level
		hr = D3D11CreateDevice(
			pAdapter,
			D3D_DRIVER_TYPE_UNKNOWN,
			NULL,
			0,
			NULL,
			0,
			D3D11_SDK_VERSION,
			&pTempDevice,
			&featureLevel,
			&pTempContext
		);

		if (FAILED(hr))
			return FALSE;
	}

	m_FeatureLevel = featureLevel;
	m_szDevDesc = TEXT("DX11 HAL");

	DXGI_FORMAT depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// Check format support
	UINT formatSupport = 0;
	if (FAILED(pTempDevice->CheckFormatSupport(DXGI_FORMAT_D24_UNORM_S8_UINT, &formatSupport)) ||
		!(formatSupport & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL))
	{
		// Fall back to D16
		depthFormat = DXGI_FORMAT_D16_UNORM;
	}

	// Build mode info list from display modes
	UINT uModeNum = rkModeList.GetDisplayModeNum();
	for (UINT i = 0; i < uModeNum && m_uModeInfoNum < MODEINFO_NUM; ++i)
	{
		const DXGI_MODE_DESC& mode = rkModeList.GetDisplayModer(i);

		D3D_SModeInfo& modeInfo = m_akModeInfo[m_uModeInfoNum++];
		modeInfo.m_uScrWidth = mode.Width;
		modeInfo.m_uScrHeight = mode.Height;
		modeInfo.m_eD3DFmtPixel = mode.Format;
		modeInfo.m_eD3DFmtDepthStencil = depthFormat;
		modeInfo.m_dwD3DBehavior = 0;  // Not used in DX11

		// Determine color depth
		switch (mode.Format)
		{
			case DXGI_FORMAT_B5G6R5_UNORM:
			case DXGI_FORMAT_B5G5R5A1_UNORM:
				modeInfo.m_uScrDepthBit = 16;
				break;
			default:
				modeInfo.m_uScrDepthBit = 32;
				break;
		}
	}

	if (pTempContext) pTempContext->Release();
	if (pTempDevice) pTempDevice->Release();

	return (m_uModeInfoNum > 0) ? TRUE : FALSE;
}

BOOL D3D_CDeviceInfo::Find(UINT uScrWidth, UINT uScrHeight, UINT uScrDepthBits, BOOL isWindowed, UINT* piModeInfo)
{
	for (UINT i = 0; i < m_uModeInfoNum; ++i)
	{
		D3D_SModeInfo& modeInfo = m_akModeInfo[i];
		if (modeInfo.m_uScrWidth == uScrWidth && modeInfo.m_uScrHeight == uScrHeight)
		{
			if (uScrDepthBits == 16)
			{
				switch (modeInfo.m_eD3DFmtPixel)
				{
					case DXGI_FORMAT_B5G6R5_UNORM:
					case DXGI_FORMAT_B5G5R5A1_UNORM:
						*piModeInfo = i;
						return TRUE;
				}
			}
			else // 32-bit
			{
				switch (modeInfo.m_eD3DFmtPixel)
				{
					case DXGI_FORMAT_R8G8B8A8_UNORM:
					case DXGI_FORMAT_B8G8R8A8_UNORM:
					case DXGI_FORMAT_B8G8R8X8_UNORM:
						*piModeInfo = i;
						return TRUE;
				}
			}
		}
	}

	for (UINT i = 0; i < m_uModeInfoNum; ++i)
	{
		D3D_SModeInfo& modeInfo = m_akModeInfo[i];
		if (modeInfo.m_uScrWidth == uScrWidth && modeInfo.m_uScrHeight == uScrHeight)
		{
			*piModeInfo = i;
			return TRUE;
		}
	}

	return FALSE;
}

VOID D3D_CDeviceInfo::GetString(std::string* pstEnumList)
{
	char szText[1024 + 1];
	_snprintf(szText, sizeof(szText), "%s (Feature Level %d.%d)\r\n========================================\r\n",
		"DX11 HAL",
		(m_FeatureLevel >> 12) & 0xF,
		(m_FeatureLevel >> 8) & 0xF);
	pstEnumList->append(szText);

	for (UINT i = 0; i < m_uModeInfoNum; ++i)
	{
		_snprintf(szText, sizeof(szText), "%d. ", i);
		pstEnumList->append(szText);

		m_akModeInfo[i].GetString(pstEnumList);
	}

	pstEnumList->append("\r\n");
}

/////////////////////////////////////////////////////////////////////////////////
// D3D_CAdapterInfo
/////////////////////////////////////////////////////////////////////////////////

BOOL D3D_CAdapterInfo::Build(IDXGIAdapter* pAdapter)
{
	m_uDevInfoNum = 0;

	if (!pAdapter)
		return FALSE;

	// Get adapter description
	if (FAILED(pAdapter->GetDesc(&m_kAdapterDesc)))
		return FALSE;

	// Get desktop display mode from first output
	IDXGIOutput* pOutput = NULL;
	if (SUCCEEDED(pAdapter->EnumOutputs(0, &pOutput)) && pOutput)
	{
		DXGI_OUTPUT_DESC outputDesc;
		pOutput->GetDesc(&outputDesc);

		// Get current display mode
		m_kDesktopMode.Width = outputDesc.DesktopCoordinates.right - outputDesc.DesktopCoordinates.left;
		m_kDesktopMode.Height = outputDesc.DesktopCoordinates.bottom - outputDesc.DesktopCoordinates.top;
		m_kDesktopMode.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		m_kDesktopMode.RefreshRate.Numerator = 60;
		m_kDesktopMode.RefreshRate.Denominator = 1;
		m_kDesktopMode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
		m_kDesktopMode.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		pOutput->Release();
	}

	// Build display mode list
	D3D_CAdapterDisplayModeList modeList;
	modeList.Build(pAdapter, DXGI_FORMAT_R8G8B8A8_UNORM);

	// Build device info (DX11 only has one device type - HAL)
	D3D_CDeviceInfo& devInfo = m_akDevInfo[m_uDevInfoNum];
	if (devInfo.Build(pAdapter, modeList))
	{
		++m_uDevInfoNum;
	}

	return (m_uDevInfoNum > 0) ? TRUE : FALSE;
}

BOOL D3D_CAdapterInfo::Find(UINT uScrWidth, UINT uScrHeight, UINT uScrDepthBits, BOOL isWindowed, UINT* piModeInfo, UINT* piDevInfo)
{
	for (UINT i = 0; i < m_uDevInfoNum; ++i)
	{
		D3D_CDeviceInfo& devInfo = m_akDevInfo[i];
		if (devInfo.Find(uScrWidth, uScrHeight, uScrDepthBits, isWindowed, piModeInfo))
		{
			*piDevInfo = i;
			return TRUE;
		}
	}
	return FALSE;
}

VOID D3D_CAdapterInfo::GetString(std::string* pstEnumList)
{
	char szText[1024 + 1];

	// Convert wide string adapter description to narrow
	char szDesc[128];
	WideCharToMultiByte(CP_ACP, 0, m_kAdapterDesc.Description, -1, szDesc, sizeof(szDesc), NULL, NULL);

	_snprintf(szText, sizeof(szText), "Adapter: %s\r\n", szDesc);
	pstEnumList->append(szText);

	_snprintf(szText, sizeof(szText), "Video Memory: %u MB\r\n",
		(UINT)(m_kAdapterDesc.DedicatedVideoMemory / (1024 * 1024)));
	pstEnumList->append(szText);

	for (UINT i = 0; i < m_uDevInfoNum; ++i)
	{
		_snprintf(szText, sizeof(szText), "\r\nDevice %d\r\n", i);
		pstEnumList->append(szText);

		m_akDevInfo[i].GetString(pstEnumList);
	}
}

D3D_SModeInfo* D3D_CAdapterInfo::GetD3DModeInfop(UINT iDevInfo, UINT iModeInfo)
{
	if (iDevInfo >= m_uDevInfoNum)
		return NULL;

	return m_akDevInfo[iDevInfo].GetD3DModeInfop(iModeInfo);
}

/////////////////////////////////////////////////////////////////////////////////
// D3D_CDisplayModeAutoDetector
/////////////////////////////////////////////////////////////////////////////////

D3D_CDisplayModeAutoDetector::D3D_CDisplayModeAutoDetector()
{
	m_uAdapterInfoCount = 0;
}

D3D_CDisplayModeAutoDetector::~D3D_CDisplayModeAutoDetector()
{
}

BOOL D3D_CDisplayModeAutoDetector::Build(IDXGIFactory* pFactory)
{
	m_uAdapterInfoCount = 0;

	if (!pFactory)
		return FALSE;

	// Enumerate adapters
	IDXGIAdapter* pAdapter = NULL;
	for (UINT i = 0; pFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND && m_uAdapterInfoCount < ADAPTERINFO_NUM; ++i)
	{
		D3D_CAdapterInfo& adapterInfo = m_akAdapterInfo[m_uAdapterInfoCount];
		if (adapterInfo.Build(pAdapter))
		{
			++m_uAdapterInfoCount;
		}
		pAdapter->Release();
	}

	return (m_uAdapterInfoCount > 0) ? TRUE : FALSE;
}

BOOL D3D_CDisplayModeAutoDetector::Find(UINT uScrWidth, UINT uScrHeight, UINT uScrDepthBits, BOOL isWindowed, UINT* piModeInfo, UINT* piDevInfo, UINT* piAdapterInfo)
{
	for (UINT i = 0; i < m_uAdapterInfoCount; ++i)
	{
		D3D_CAdapterInfo& adapterInfo = m_akAdapterInfo[i];
		if (adapterInfo.Find(uScrWidth, uScrHeight, uScrDepthBits, isWindowed, piModeInfo, piDevInfo))
		{
			*piAdapterInfo = i;
			return TRUE;
		}
	}
	return FALSE;
}

D3D_SModeInfo* D3D_CDisplayModeAutoDetector::GetD3DModeInfop(UINT iAdapterInfo, UINT iDevInfo, UINT iModeInfo)
{
	if (iAdapterInfo >= m_uAdapterInfoCount)
		return NULL;

	return m_akAdapterInfo[iAdapterInfo].GetD3DModeInfop(iDevInfo, iModeInfo);
}

VOID D3D_CDisplayModeAutoDetector::GetString(std::string* pstEnumList)
{
	char szText[1024 + 1];
	pstEnumList->append("=== DX11 Display Mode Enumeration ===\r\n\r\n");

	for (UINT i = 0; i < m_uAdapterInfoCount; ++i)
	{
		_snprintf(szText, sizeof(szText), "=== Adapter %d ===\r\n", i);
		pstEnumList->append(szText);

		m_akAdapterInfo[i].GetString(pstEnumList);
		pstEnumList->append("\r\n");
	}
}
