#include "StdAfx.h"
#include "../eterBase/MappedFile.h"
#include "../eterPack/EterPackManager.h"
#include "../eterImageLib/TGAImage.h"
#include "GrpImageTexture.h"
#include "JpegFile.h"
#include <vector>
#include <wincodec.h>
#include <stb_image.h>

#pragma comment(lib, "windowscodecs.lib")

// Helper to check if file extension matches
static bool HasExtension(const char* fileName, const char* ext)
{
	if (!fileName || !ext) return false;
	size_t fileLen = strlen(fileName);
	size_t extLen = strlen(ext);
	if (fileLen < extLen) return false;
	return _stricmp(fileName + fileLen - extLen, ext) == 0;
}

static bool LoadJPEGFromMemory(const BYTE* data, UINT dataSize, std::vector<BYTE>& pixels, UINT& width, UINT& height)
{
	int w, h, channels;
	unsigned char* loaded = stbi_load_from_memory(data, dataSize, &w, &h, &channels, 0);

	if (!loaded)
		return false;

	width = (UINT)w;
	height = (UINT)h;

	// Validate dimensions
	if (width == 0 || height == 0 || width > 16384 || height > 16384) {
		stbi_image_free(loaded);
		return false;
	}

	// Allocate BGRA pixel buffer
	pixels.resize(width * height * 4);

	// Convert to BGRA
	for (UINT y = 0; y < height; ++y) {
		BYTE* pDest = pixels.data() + y * width * 4;

		if (channels == 1) {
			// Grayscale
			for (UINT x = 0; x < width; ++x) {
				BYTE gray = loaded[y * width + x];
				pDest[x * 4 + 0] = gray;  // B
				pDest[x * 4 + 1] = gray;  // G
				pDest[x * 4 + 2] = gray;  // R
				pDest[x * 4 + 3] = 255;   // A
			}
		}
		else if (channels == 3) {
			// RGB -> BGRA
			for (UINT x = 0; x < width; ++x) {
				const BYTE* pSrc = loaded + (y * width + x) * 3;
				pDest[x * 4 + 0] = pSrc[2];  // B
				pDest[x * 4 + 1] = pSrc[1];  // G
				pDest[x * 4 + 2] = pSrc[0];  // R
				pDest[x * 4 + 3] = 255;      // A
			}
		}
		else if (channels == 4) {
			// RGBA -> BGRA
			for (UINT x = 0; x < width; ++x) {
				const BYTE* pSrc = loaded + (y * width + x) * 4;
				pDest[x * 4 + 0] = pSrc[2];  // B
				pDest[x * 4 + 1] = pSrc[1];  // G
				pDest[x * 4 + 2] = pSrc[0];  // R
				pDest[x * 4 + 3] = pSrc[3];  // A
			}
		}
	}

	stbi_image_free(loaded);
	return true;
}

// Helper function to get bytes per pixel for a format
static UINT GetBytesPerPixel(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_FLOAT:	return 16;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:	return 8;
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:		return 4;
	case DXGI_FORMAT_B5G6R5_UNORM:
	case DXGI_FORMAT_B5G5R5A1_UNORM:
	case DXGI_FORMAT_B4G4R4A4_UNORM:
	case DXGI_FORMAT_R8G8_UNORM:			return 2;
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_A8_UNORM:				return 1;
	default:								return 4;
	}
}

bool CGraphicImageTexture::Lock(int* pRetPitch, void** ppRetPixels, int level)
{
	if (!m_pTexture || !ms_pContext)
		return false;

	D3D11_TEXTURE2D_DESC texDesc;
	m_pTexture->GetDesc(&texDesc);
	if (texDesc.Format == DXGI_FORMAT_BC1_UNORM ||
		texDesc.Format == DXGI_FORMAT_BC2_UNORM ||
		texDesc.Format == DXGI_FORMAT_BC3_UNORM)
		return false;

	// Create a staging texture if not already created
	if (!m_pStagingTexture)
	{
		D3D11_TEXTURE2D_DESC desc;
		m_pTexture->GetDesc(&desc);

		D3D11_TEXTURE2D_DESC stagingDesc = desc;
		stagingDesc.Usage = D3D11_USAGE_STAGING;
		stagingDesc.BindFlags = 0;
		stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		stagingDesc.MiscFlags = 0;

		if (FAILED(ms_pDevice->CreateTexture2D(&stagingDesc, nullptr, &m_pStagingTexture)))
			return false;

		// Copy current texture to staging
		ms_pContext->CopyResource(m_pStagingTexture, m_pTexture);
	}

	D3D11_MAPPED_SUBRESOURCE mapped;
	if (FAILED(ms_pContext->Map(m_pStagingTexture, level, D3D11_MAP_READ_WRITE, 0, &mapped)))
		return false;

	*pRetPitch = mapped.RowPitch;
	*ppRetPixels = mapped.pData;
	return true;
}

void CGraphicImageTexture::Unlock(int level)
{
	if (!m_pStagingTexture || !ms_pContext)
		return;

	ms_pContext->Unmap(m_pStagingTexture, level);

	// Copy staging back to GPU texture
	ms_pContext->CopyResource(m_pTexture, m_pStagingTexture);
}

void CGraphicImageTexture::Initialize()
{
	CGraphicTexture::Initialize();

	m_stFileName = "";
	m_dxgiFmt = DXGI_FORMAT_UNKNOWN;
	m_dwFilter = 0;
	m_pStagingTexture = nullptr;
}

void CGraphicImageTexture::Destroy()
{
	if (m_pStagingTexture)
	{
		m_pStagingTexture->Release();
		m_pStagingTexture = nullptr;
	}

	CGraphicTexture::Destroy();
	Initialize();
}

bool CGraphicImageTexture::CreateDeviceObjects()
{
	assert(ms_pDevice != NULL);
	assert(m_pTexture == NULL);

	if (m_stFileName.empty())
	{
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = m_width;
		desc.Height = m_height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = m_dxgiFmt;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		UINT pixelSize = 4;  // Assuming 4 bytes per pixel (BGRA/RGBA)
		UINT rowPitch = m_width * pixelSize;
		UINT totalSize = rowPitch * m_height;
		std::vector<BYTE> initData(totalSize, 0);  // All zeros = transparent black

		D3D11_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pSysMem = initData.data();
		subresourceData.SysMemPitch = rowPitch;
		subresourceData.SysMemSlicePitch = 0;

		if (FAILED(ms_pDevice->CreateTexture2D(&desc, &subresourceData, &m_pTexture)))
			return false;

		if (!CreateShaderResourceView())
			return false;
	}
	else
	{
		CMappedFile	mappedFile;
		LPCVOID		c_pvMap;

		if (!CEterPackManager::Instance().Get(mappedFile, m_stFileName.c_str(), &c_pvMap))
			return false;

		if (!CreateFromMemoryFile(mappedFile.Size(), c_pvMap, m_dxgiFmt, m_stFileName.c_str(), m_dwFilter))
		{
			TraceError("CGraphicImageTexture::CreateDeviceObjects: CreateFromMemoryFile: texture not found(%s)", m_stFileName.c_str());
			return false;
		}
		return true;
	}

	m_bEmpty = false;
	return true;
}

bool CGraphicImageTexture::Create(UINT width, UINT height, DXGI_FORMAT dxgiFmt, DWORD dwFilter)
{
	assert(ms_pDevice != NULL);
	Destroy();

	m_width = width;
	m_height = height;
	m_dxgiFmt = dxgiFmt;
	m_dwFilter = dwFilter;

	return CreateDeviceObjects();
}

void CGraphicImageTexture::CreateFromTexturePointer(const CGraphicTexture* c_pSrcTexture)
{
	if (m_pTexture)
	{
		m_pTexture->Release();
		m_pTexture = nullptr;
	}
	if (m_pSRV)
	{
		m_pSRV->Release();
		m_pSRV = nullptr;
	}

	m_width = c_pSrcTexture->GetWidth();
	m_height = c_pSrcTexture->GetHeight();
	m_pTexture = c_pSrcTexture->GetTexture();
	m_pSRV = c_pSrcTexture->GetSRV();

	if (m_pTexture)
		m_pTexture->AddRef();
	if (m_pSRV)
		m_pSRV->AddRef();

	m_bEmpty = false;
}

static void ApplyColorKey(DWORD* pixelData, UINT width, UINT height)
{
	UINT pixelCount = width * height;
	for (UINT i = 0; i < pixelCount; ++i)
	{
		DWORD pixel = pixelData[i];
		BYTE b = pixel & 0xFF;
		BYTE g = (pixel >> 8) & 0xFF;
		BYTE r = (pixel >> 16) & 0xFF;
		BYTE a = (pixel >> 24) & 0xFF;

		// Skip fully transparent pixels - no processing needed
		if (a == 0)
			continue;

		bool isMagentaKey = (r >= 250) && (g <= 5) && (b >= 250);

		bool isGreenKey = false;
		if (g >= 120)  // Must have reasonable green component
		{
			// Calculate how much green dominates over red and blue
			int greenOverRed = (int)g - (int)r;
			int greenOverBlue = (int)g - (int)b;


			if (r <= 80 && b <= 80)
			{
				// Low R and B - likely a green background pixel
				// Green just needs to be noticeably dominant
				if (greenOverRed >= 40 && greenOverBlue >= 40)
				{
					isGreenKey = true;
				}
			}
			else if (r <= 120 && b <= 120)
			{
				// Medium R/B - could be edge interpolation with content
				// Require stronger green dominance
				if (greenOverRed >= 80 && greenOverBlue >= 80 && g >= 180)
				{
					isGreenKey = true;
				}
			}
		}

		if (isMagentaKey || isGreenKey)
		{
			// Make color key pixels fully transparent
			pixelData[i] = 0x00000000;  // Transparent black
		}
	}
}

bool CGraphicImageTexture::CreateDDSTexture(CDXTCImage& image, const BYTE* /*c_pbBuf*/)
{
	int mipmapCount = image.m_dwMipMapCount == 0 ? 1 : image.m_dwMipMapCount;

	DXGI_FORMAT format;
	bool isBlockCompressed = false;

	// DXT textures upload raw block data as BC1/BC2/BC3 — GPU decompresses natively.
	// No color key for DXT (legacy behaviour — DXT alpha is used directly).

	if (image.m_CompFormat == PF_DXT1)
	{
		format = DXGI_FORMAT_BC1_UNORM;
		isBlockCompressed = true;
	}
	else if (image.m_CompFormat == PF_DXT3)
	{
		format = DXGI_FORMAT_BC2_UNORM;
		isBlockCompressed = true;
	}
	else if (image.m_CompFormat == PF_DXT5)
	{
		format = DXGI_FORMAT_BC3_UNORM;
		isBlockCompressed = true;
	}
	else if (image.m_CompFormat == PF_ARGB)
	{
		// Uncompressed ARGB format
		format = DXGI_FORMAT_B8G8R8A8_UNORM;
	}
	else
	{
		TraceError("CreateDDSTexture: Unsupported format %d", image.m_CompFormat);
		return false;
	}

	UINT uTexBias = 0;
	if (IsLowTextureMemory())
		uTexBias = 1;

	UINT uMinMipMapIndex = 0;
	UINT texWidth = image.m_nWidth;
	UINT texHeight = image.m_nHeight;
	UINT mipLevels = mipmapCount;

	if (uTexBias > 0)
	{
		if (mipmapCount > (int)uTexBias)
		{
			uMinMipMapIndex = uTexBias;
			texWidth >>= uTexBias;
			texHeight >>= uTexBias;
			mipLevels -= uTexBias;
		}
	}

	// Create texture
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = texWidth;
	desc.Height = texHeight;
	desc.MipLevels = mipLevels;
	desc.ArraySize = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	// Prepare subresource data
	std::vector<D3D11_SUBRESOURCE_DATA> initData(mipLevels);
	std::vector<std::vector<BYTE>> mipData(mipLevels);

	for (UINT i = 0; i < mipLevels; ++i)
	{
		UINT mipWidth = max(1u, texWidth >> i);
		UINT mipHeight = max(1u, texHeight >> i);

		if (isBlockCompressed)
		{
			// Upload raw DXT block data directly — GPU decompresses BC1/BC2/BC3 natively.
			// BC1 (DXT1) block = 8 bytes, BC2 (DXT3) / BC3 (DXT5) block = 16 bytes.
			UINT blockSize = (format == DXGI_FORMAT_BC1_UNORM) ? 8 : 16;
			UINT blocksWide = max(1u, (mipWidth + 3) / 4);

			initData[i].pSysMem = image.m_pbCompBufferByLevels[i + uMinMipMapIndex];
			initData[i].SysMemPitch = blocksWide * blockSize;
			initData[i].SysMemSlicePitch = 0;
		}
		else
		{
			UINT srcBytesPerPixel = image.m_xddPixelFormat.dwRGBBitCount / 8;
			if (srcBytesPerPixel == 0) srcBytesPerPixel = 4; // Default to 32-bit

			UINT srcRowPitch = mipWidth * srcBytesPerPixel;
			UINT srcTotalSize = srcRowPitch * mipHeight;

			// Always output 32-bit BGRA regardless of source format
			UINT pixelCount = mipWidth * mipHeight;
			mipData[i].resize(pixelCount * 4);

			if (srcBytesPerPixel == 4)
			{
				// 32-bit source - copy directly
				image.Copy(i + uMinMipMapIndex, mipData[i].data(), pixelCount * 4);
			}
			else if (srcBytesPerPixel == 2)
			{
				std::vector<BYTE> srcData(srcTotalSize);
				image.Copy(i + uMinMipMapIndex, srcData.data(), srcTotalSize);

				WORD* pSrc = (WORD*)srcData.data();
				DWORD* pDst = (DWORD*)mipData[i].data();

				// Get color masks from the pixel format
				DWORD rMask = image.m_xddPixelFormat.dwRBitMask;
				DWORD gMask = image.m_xddPixelFormat.dwGBitMask;
				DWORD bMask = image.m_xddPixelFormat.dwBBitMask;
				DWORD aMask = image.m_xddPixelFormat.dwRGBAlphaBitMask;

				// Calculate shifts and scales for each channel
				int rShift = 0, gShift = 0, bShift = 0, aShift = 0;
				int rBits = 0, gBits = 0, bBits = 0, aBits = 0;

				for (int s = 0; s < 16; s++) {
					if (rMask & (1 << s)) { if (rBits == 0) rShift = s; rBits++; }
					if (gMask & (1 << s)) { if (gBits == 0) gShift = s; gBits++; }
					if (bMask & (1 << s)) { if (bBits == 0) bShift = s; bBits++; }
					if (aMask & (1 << s)) { if (aBits == 0) aShift = s; aBits++; }
				}

				for (UINT p = 0; p < pixelCount; p++)
				{
					WORD src = pSrc[p];

					// Extract channels
					BYTE r = rBits ? (BYTE)(((src & rMask) >> rShift) * 255 / ((1 << rBits) - 1)) : 0;
					BYTE g = gBits ? (BYTE)(((src & gMask) >> gShift) * 255 / ((1 << gBits) - 1)) : 0;
					BYTE b = bBits ? (BYTE)(((src & bMask) >> bShift) * 255 / ((1 << bBits) - 1)) : 0;
					BYTE a = aBits ? (BYTE)(((src & aMask) >> aShift) * 255 / ((1 << aBits) - 1)) : 255;

					// Output as BGRA (0xAARRGGBB in little-endian DWORD)
					pDst[p] = ((DWORD)a << 24) | ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b;
				}
			}
			else
			{
				// Other bit depths - just copy and hope for the best
				image.Copy(i + uMinMipMapIndex, mipData[i].data(), min(srcTotalSize, pixelCount * 4));
			}

			ApplyColorKey((DWORD*)mipData[i].data(), mipWidth, mipHeight);

			initData[i].pSysMem = mipData[i].data();
			initData[i].SysMemPitch = mipWidth * 4;  // Always 32-bit output
			initData[i].SysMemSlicePitch = 0;
		}
	}

	if (FAILED(ms_pDevice->CreateTexture2D(&desc, initData.data(), &m_pTexture)))
	{
		TraceError("CreateDDSTexture: Cannot create texture");
		return false;
	}

	if (!CreateShaderResourceView())
	{
		m_pTexture->Release();
		m_pTexture = nullptr;
		return false;
	}

	m_width = texWidth;
	m_height = texHeight;
	m_Format = format;
	m_bEmpty = false;

	return true;
}

bool CGraphicImageTexture::CreateFromMemoryFile(UINT bufSize, const void* c_pvBuf, DXGI_FORMAT dxgiFmt, const char* fileName, DWORD dwFilter)
{
	// Validate device is available
	if (!ms_pDevice)
	{
		TraceError("CreateFromMemoryFile: D3D11 device not initialized for %s", fileName);
		return false;
	}

	HRESULT hrDeviceRemoved = ms_pDevice->GetDeviceRemovedReason();
	if (hrDeviceRemoved != S_OK)
	{
		// Decode the error reason for better diagnostics
		const char* reasonStr = "Unknown";
		switch (hrDeviceRemoved)
		{
		case DXGI_ERROR_DEVICE_HUNG: reasonStr = "DEVICE_HUNG - GPU took too long"; break;
		case DXGI_ERROR_DEVICE_REMOVED: reasonStr = "DEVICE_REMOVED - GPU disconnected"; break;
		case DXGI_ERROR_DEVICE_RESET: reasonStr = "DEVICE_RESET - Driver triggered reset"; break;
		case DXGI_ERROR_DRIVER_INTERNAL_ERROR: reasonStr = "DRIVER_INTERNAL_ERROR"; break;
		case DXGI_ERROR_INVALID_CALL: reasonStr = "INVALID_CALL"; break;
		}
		TraceError("CreateFromMemoryFile: Device was removed (reason: 0x%08X = %s) for %s", hrDeviceRemoved, reasonStr, fileName);
		return false;
	}

	if (!ms_pContext)
	{
		TraceError("CreateFromMemoryFile: D3D11 context not available for %s", fileName);
		return false;
	}

	assert(m_pTexture == NULL);

	CDXTCImage image;

	if (image.LoadHeaderFromMemory((const BYTE*)c_pvBuf, bufSize))
	{
		return CreateDDSTexture(image, (const BYTE*)c_pvBuf);
	}

	// Try to load as TGA (WIC doesn't support TGA natively)
	if (HasExtension(fileName, ".tga"))
	{
		CTGAImage tgaImage;
		if (tgaImage.LoadFromMemory(bufSize, (const BYTE*)c_pvBuf))
		{
			UINT width = tgaImage.GetWidth();
			UINT height = tgaImage.GetHeight();
			DWORD* pPixels = tgaImage.GetBasePointer();

			if (width > 0 && height > 0 && pPixels)
			{
				ApplyColorKey(pPixels, width, height);

				D3D11_TEXTURE2D_DESC desc = {};
				desc.Width = width;
				desc.Height = height;
				desc.MipLevels = 1;
				desc.ArraySize = 1;
				desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
				desc.SampleDesc.Count = 1;
				desc.SampleDesc.Quality = 0;
				desc.Usage = D3D11_USAGE_DEFAULT;
				desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				desc.CPUAccessFlags = 0;
				desc.MiscFlags = 0;

				D3D11_SUBRESOURCE_DATA initData = {};
				initData.pSysMem = pPixels;
				initData.SysMemPitch = width * 4;

				HRESULT hr = ms_pDevice->CreateTexture2D(&desc, &initData, &m_pTexture);
				if (SUCCEEDED(hr))
				{
					if (CreateShaderResourceView())
					{
						m_width = width;
						m_height = height;
						m_Format = DXGI_FORMAT_B8G8R8A8_UNORM;
						m_bEmpty = false;
						return true;
					}
					else
					{
						TraceError("CreateFromMemoryFile: TGA SRV creation failed for %s", fileName);
						m_pTexture->Release();
						m_pTexture = nullptr;
					}
				}
				else
				{
					TraceError("CreateFromMemoryFile: TGA texture creation failed for %s (size: %ux%u, HRESULT: 0x%08X)", fileName, width, height, hr);
				}
			}
			else
			{
				TraceError("CreateFromMemoryFile: TGA invalid dimensions for %s (w=%u h=%u pPixels=%p)", fileName, width, height, pPixels);
			}
		}
		else
		{
			TraceError("CreateFromMemoryFile: TGA LoadFromMemory failed for %s", fileName);
		}
		// TGA loading failed, fall through to other loaders
	}

	if (HasExtension(fileName, ".jpg") || HasExtension(fileName, ".jpeg"))
	{
		std::vector<BYTE> jpegPixels;
		UINT jpegWidth = 0, jpegHeight = 0;

		if (LoadJPEGFromMemory((const BYTE*)c_pvBuf, bufSize, jpegPixels, jpegWidth, jpegHeight))
		{
			if (jpegWidth > 0 && jpegHeight > 0 && !jpegPixels.empty())
			{
				D3D11_TEXTURE2D_DESC desc = {};
				desc.Width = jpegWidth;
				desc.Height = jpegHeight;
				desc.MipLevels = 1;
				desc.ArraySize = 1;
				desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
				desc.SampleDesc.Count = 1;
				desc.SampleDesc.Quality = 0;
				desc.Usage = D3D11_USAGE_DEFAULT;
				desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				desc.CPUAccessFlags = 0;
				desc.MiscFlags = 0;

				D3D11_SUBRESOURCE_DATA initData = {};
				initData.pSysMem = jpegPixels.data();
				initData.SysMemPitch = jpegWidth * 4;

				HRESULT hr = ms_pDevice->CreateTexture2D(&desc, &initData, &m_pTexture);
				if (SUCCEEDED(hr))
				{
					if (CreateShaderResourceView())
					{
						m_width = jpegWidth;
						m_height = jpegHeight;
						m_Format = DXGI_FORMAT_B8G8R8A8_UNORM;
						m_bEmpty = false;
						return true;
					}
					else
					{
						TraceError("CreateFromMemoryFile: JPEG SRV creation failed for %s", fileName);
						m_pTexture->Release();
						m_pTexture = nullptr;
					}
				}
				else
				{
					TraceError("CreateFromMemoryFile: JPEG texture creation failed for %s (size: %ux%u, HRESULT: 0x%08X)", fileName, jpegWidth, jpegHeight, hr);
				}
			}
		}
		else
		{
			TraceError("CreateFromMemoryFile: JPEG decode failed for %s", fileName);
		}
		// JPEG loading failed - fall through to WIC as last resort
	}

	HRESULT hrCom = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	bool bComInitialized = SUCCEEDED(hrCom) || hrCom == RPC_E_CHANGED_MODE;

	IWICImagingFactory* pWIC = nullptr;
	HRESULT hr = CoCreateInstance(
		CLSID_WICImagingFactory,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&pWIC)
	);

	if (FAILED(hr))
	{
		TraceError("CreateFromMemoryFile: Failed to create WIC factory for %s (HRESULT: 0x%08X)", fileName, hr);
		if (bComInitialized && SUCCEEDED(hrCom)) CoUninitialize();
		return false;
	}

	// Create stream from memory
	IWICStream* pStream = nullptr;
	hr = pWIC->CreateStream(&pStream);
	if (FAILED(hr))
	{
		pWIC->Release();
		if (bComInitialized && SUCCEEDED(hrCom)) CoUninitialize();
		return false;
	}

	hr = pStream->InitializeFromMemory((BYTE*)c_pvBuf, bufSize);
	if (FAILED(hr))
	{
		pStream->Release();
		pWIC->Release();
		if (bComInitialized && SUCCEEDED(hrCom)) CoUninitialize();
		return false;
	}

	// Create decoder
	IWICBitmapDecoder* pDecoder = nullptr;
	hr = pWIC->CreateDecoderFromStream(pStream, nullptr, WICDecodeMetadataCacheOnDemand, &pDecoder);
	if (FAILED(hr))
	{
		pStream->Release();
		pWIC->Release();
		TraceError("CreateFromMemoryFile: Cannot decode %s", fileName);
		if (bComInitialized && SUCCEEDED(hrCom)) CoUninitialize();
		return false;
	}

	// Get first frame
	IWICBitmapFrameDecode* pFrame = nullptr;
	hr = pDecoder->GetFrame(0, &pFrame);
	if (FAILED(hr))
	{
		pDecoder->Release();
		pStream->Release();
		pWIC->Release();
		if (bComInitialized && SUCCEEDED(hrCom)) CoUninitialize();
		return false;
	}

	UINT width, height;
	pFrame->GetSize(&width, &height);

	// Convert to BGRA format
	IWICFormatConverter* pConverter = nullptr;
	hr = pWIC->CreateFormatConverter(&pConverter);
	if (FAILED(hr))
	{
		pFrame->Release();
		pDecoder->Release();
		pStream->Release();
		pWIC->Release();
		if (bComInitialized && SUCCEEDED(hrCom)) CoUninitialize();
		return false;
	}

	hr = pConverter->Initialize(
		pFrame,
		GUID_WICPixelFormat32bppBGRA,
		WICBitmapDitherTypeNone,
		nullptr,
		0.0f,
		WICBitmapPaletteTypeCustom
	);

	if (FAILED(hr))
	{
		pConverter->Release();
		pFrame->Release();
		pDecoder->Release();
		pStream->Release();
		pWIC->Release();
		if (bComInitialized && SUCCEEDED(hrCom)) CoUninitialize();
		return false;
	}

	// Get pixel data
	UINT stride = width * 4;
	UINT imageSize = stride * height;
	std::vector<BYTE> pixels(imageSize);

	hr = pConverter->CopyPixels(nullptr, stride, imageSize, pixels.data());

	pConverter->Release();
	pFrame->Release();
	pDecoder->Release();
	pStream->Release();
	pWIC->Release();

	if (FAILED(hr))
	{
		TraceError("CreateFromMemoryFile: Cannot copy pixels from %s (HRESULT: 0x%08X)", fileName, hr);
		if (bComInitialized && SUCCEEDED(hrCom)) CoUninitialize();
		return false;
	}

	ApplyColorKey((DWORD*)pixels.data(), width, height);

	// Validate device
	if (!ms_pDevice)
	{
		TraceError("CreateFromMemoryFile: Device is null for %s", fileName);
		if (bComInitialized && SUCCEEDED(hrCom)) CoUninitialize();
		return false;
	}

	// Create texture
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = pixels.data();
	initData.SysMemPitch = stride;

	hr = ms_pDevice->CreateTexture2D(&desc, &initData, &m_pTexture);
	if (FAILED(hr))
	{
		TraceError("CreateFromMemoryFile: Cannot create texture %s (size: %ux%u, HRESULT: 0x%08X)", fileName, width, height, hr);
		if (bComInitialized && SUCCEEDED(hrCom)) CoUninitialize();
		return false;
	}

	if (!CreateShaderResourceView())
	{
		m_pTexture->Release();
		m_pTexture = nullptr;
		if (bComInitialized && SUCCEEDED(hrCom)) CoUninitialize();
		return false;
	}

	m_width = width;
	m_height = height;
	m_Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	m_bEmpty = false;

	if (bComInitialized && SUCCEEDED(hrCom)) CoUninitialize();
	return true;
}

void CGraphicImageTexture::SetFileName(const char* c_szFileName)
{
	m_stFileName = c_szFileName;
}

bool CGraphicImageTexture::CreateFromDiskFile(const char* c_szFileName, DXGI_FORMAT dxgiFmt, DWORD dwFilter)
{
	Destroy();

	SetFileName(c_szFileName);

	m_dxgiFmt = dxgiFmt;
	m_dwFilter = dwFilter;
	return CreateDeviceObjects();
}

CGraphicImageTexture::CGraphicImageTexture()
{
	Initialize();
}

CGraphicImageTexture::~CGraphicImageTexture()
{
	Destroy();
}
