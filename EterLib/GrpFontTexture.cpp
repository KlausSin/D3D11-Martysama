#include "StdAfx.h"
#include "GrpText.h"
#include "../eterBase/Stl.h"

#include "Util.h"

#pragma comment(lib, "dwrite.lib")

CGraphicFontTexture::CGraphicFontTexture()
{
	Initialize();
}

CGraphicFontTexture::~CGraphicFontTexture()
{
	Destroy();
}

void CGraphicFontTexture::Initialize()
{
	CGraphicTexture::Initialize();
	m_pDWriteFactory = NULL;
	m_pGdiInterop = NULL;
	m_atlasWidth = 0;
	m_atlasHeight = 0;
	m_isDirty = false;
	m_bItalic = false;
	m_fontSize = 0;
	memset(m_fontName, 0, sizeof(m_fontName));
}

bool CGraphicFontTexture::IsEmpty() const
{
	return m_fontFaceMap.empty();
}

void CGraphicFontTexture::Destroy()
{
	m_pSRV = NULL;
	CGraphicTexture::Destroy();
	stl_wipe(m_pFontTextureVector);
	m_charInfoMap.clear();

	for (auto& pair : m_fontFaceMap)
	{
		if (pair.second.pFace)
			pair.second.pFace->Release();
	}
	m_fontFaceMap.clear();

	if (m_pGdiInterop)
	{
		m_pGdiInterop->Release();
		m_pGdiInterop = NULL;
	}

	if (m_pDWriteFactory)
	{
		m_pDWriteFactory->Release();
		m_pDWriteFactory = NULL;
	}

	m_atlasBuffer.clear();

	Initialize();
}

bool CGraphicFontTexture::CreateDeviceObjects()
{
	return true;
}

void CGraphicFontTexture::DestroyDeviceObjects()
{
}

bool CGraphicFontTexture::Create(const char* c_szFontName, int fontSize, bool bItalic)
{
	Destroy();

	strncpy(m_fontName, c_szFontName, sizeof(m_fontName)-1);
	m_fontSize	= fontSize;
	m_bItalic	= bItalic;

	m_x = 0;
	m_y = 0;
	m_step = 0;

	DWORD width = 256,height = 256;
	if (GetMaxTextureWidth() > 512)
		width = 512;
	if (GetMaxTextureHeight() > 512)
		height = 512;

	// Initialize DirectWrite
	HRESULT hr = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(&m_pDWriteFactory));

	if (FAILED(hr) || !m_pDWriteFactory)
		return false;

	hr = m_pDWriteFactory->GetGdiInterop(&m_pGdiInterop);
	if (FAILED(hr) || !m_pGdiInterop)
		return false;

	// CPU atlas buffer (replaces GDI DIB)
	m_atlasWidth = width;
	m_atlasHeight = height;
	m_atlasBuffer.resize(width * height, 0);

	// Validate font for default code page
	if (!GetFontFaceData(GetDefaultCodePage()))
		return false;

	if (!AppendTexture())
		return false;

	return true;
}

CGraphicFontTexture::SFontFaceData* CGraphicFontTexture::GetFontFaceData(WORD codePage)
{
	auto it = m_fontFaceMap.find(codePage);
	if (it != m_fontFaceMap.end())
		return &it->second;

	// Build LOGFONTW (DirectWrite requires wide-char version)
	LOGFONTW logFont;
	memset(&logFont, 0, sizeof(LOGFONTW));

	logFont.lfHeight			= m_fontSize;
	logFont.lfEscapement		= 0;
	logFont.lfOrientation		= 0;
	logFont.lfWeight			= FW_NORMAL;
	logFont.lfItalic			= (BYTE) m_bItalic;
	logFont.lfUnderline			= FALSE;
	logFont.lfStrikeOut			= FALSE;
	logFont.lfCharSet			= GetCharsetFromCodePage(codePage);
	logFont.lfOutPrecision		= OUT_DEFAULT_PRECIS;
	logFont.lfClipPrecision		= CLIP_DEFAULT_PRECIS;
	logFont.lfQuality			= ANTIALIASED_QUALITY;
	logFont.lfPitchAndFamily	= DEFAULT_PITCH;
	MultiByteToWideChar(CP_ACP, 0, m_fontName, -1, logFont.lfFaceName, LF_FACESIZE);

	IDWriteFont* pFont = NULL;
	HRESULT hr = m_pGdiInterop->CreateFontFromLOGFONT(&logFont, &pFont);
	if (FAILED(hr) || !pFont)
		return NULL;

	IDWriteFontFace* pFace = NULL;
	hr = pFont->CreateFontFace(&pFace);
	pFont->Release();

	if (FAILED(hr) || !pFace)
		return NULL;

	// Compute em size from LOGFONT height
	DWRITE_FONT_METRICS fm;
	pFace->GetMetrics(&fm);

	float fEmSize;
	if (m_fontSize < 0)
	{
		// Negative lfHeight = em height in pixels
		fEmSize = (float)abs(m_fontSize);
	}
	else if (m_fontSize > 0)
	{
		// Positive lfHeight = cell height in pixels
		fEmSize = (float)m_fontSize * (float)fm.designUnitsPerEm / (float)(fm.ascent + fm.descent);
	}
	else
	{
		fEmSize = 12.0f;
	}

	float scale = fEmSize / (float)fm.designUnitsPerEm;

	SFontFaceData data;
	data.pFace = pFace;
	data.fEmSize = fEmSize;
	data.fAscent = fm.ascent * scale;
	data.fCellHeight = ceilf((fm.ascent + fm.descent) * scale);

	auto result = m_fontFaceMap.insert(TFontFaceMap::value_type(codePage, data));
	return &result.first->second;
}

bool CGraphicFontTexture::AppendTexture()
{
	CGraphicImageTexture * pNewTexture = new CGraphicImageTexture;

	if (!pNewTexture->Create(m_atlasWidth, m_atlasHeight, DXGI_FORMAT_R8G8B8A8_UNORM))
	{
		delete pNewTexture;
		return false;
	}

	m_pFontTextureVector.push_back(pNewTexture);
	return true;
}

bool CGraphicFontTexture::UpdateTexture()
{
	if(!m_isDirty)
		return true;

	m_isDirty = false;

	CGraphicImageTexture * pFontTexture = m_pFontTextureVector.back();

	if (!pFontTexture)
		return false;

	DWORD* pdwDst;
	int pitch;

	if (!pFontTexture->Lock(&pitch, (void**)&pdwDst))
		return false;

	// pitch is in bytes, convert to DWORD stride
	pitch /= 4;

	DWORD * pdwSrc = m_atlasBuffer.data();

	for (int y = 0; y < m_atlasHeight; ++y, pdwDst += pitch, pdwSrc += m_atlasWidth)
	{
		memcpy(pdwDst, pdwSrc, m_atlasWidth * sizeof(DWORD));
	}

	pFontTexture->Unlock();
	return true;
}

CGraphicFontTexture::TCharacterInfomation* CGraphicFontTexture::GetCharacterInfomation(WORD codePage, wchar_t keyValue)
{
	TCharacterKey code(codePage, keyValue);

	TCharacterInfomationMap::iterator f = m_charInfoMap.find(code);

	if (m_charInfoMap.end() == f)
	{
		return UpdateCharacterInfomation(code);
	}
	else
	{
		return &f->second;
	}
}

CGraphicFontTexture::TCharacterInfomation* CGraphicFontTexture::UpdateCharacterInfomation(TCharacterKey code)
{
	SFontFaceData* pFontData = GetFontFaceData(code.first);
	if (!pFontData || !pFontData->pFace)
		return NULL;

	wchar_t keyValue = code.second;

	if (keyValue == 0x08)
		keyValue = L' ';

	IDWriteFontFace* pFace = pFontData->pFace;
	float fEmSize = pFontData->fEmSize;
	float fAscent = pFontData->fAscent;
	int nCellHeight = (int)pFontData->fCellHeight;

	// Get glyph index
	UINT32 codePoint = (UINT32)keyValue;
	UINT16 glyphIndex = 0;
	HRESULT hr = pFace->GetGlyphIndices(&codePoint, 1, &glyphIndex);
	if (FAILED(hr))
		return NULL;

	DWRITE_FONT_METRICS fm;
	pFace->GetMetrics(&fm);
	float scale = fEmSize / (float)fm.designUnitsPerEm;

	DWRITE_GLYPH_METRICS gm;
	hr = pFace->GetGdiCompatibleGlyphMetrics(fEmSize, 1.0f, NULL, FALSE, &glyphIndex, 1, &gm);
	if (FAILED(hr))
		return NULL;

	// ABC widths (matching GDI's GetCharABCWidthsFloat logic)
	float A = (float)gm.leftSideBearing * scale;
	float B = (float)((INT32)gm.advanceWidth - gm.leftSideBearing - gm.rightSideBearing) * scale;
	float C = (float)gm.rightSideBearing * scale;

	int nChrWidth = (int)B;
	if (A > 0.0f) nChrWidth += (int)ceilf(A);
	if (C > 0.0f) nChrWidth += (int)ceilf(C);
	nChrWidth++;

	LONG lAdvance = (LONG)ceilf(A + B + C);

	int nChrHeight = nCellHeight;

	// Atlas packing (same logic as original)
	if (m_x + nChrWidth >= (m_atlasWidth - 1))
	{
		m_y += (m_step + 1);
		m_step = 0;
		m_x = 0;

		if (m_y + nChrHeight >= (m_atlasHeight - 1))
		{
			if (!UpdateTexture())
			{
				return NULL;
			}

			if (!AppendTexture())
				return NULL;

			// Clear atlas buffer for new texture page
			memset(m_atlasBuffer.data(), 0, m_atlasBuffer.size() * sizeof(DWORD));
			m_y = 0;
		}
	}

	// Rasterize glyph via DirectWrite
	FLOAT glyphAdvance = 0.0f;
	DWRITE_GLYPH_OFFSET glyphOffset = {0, 0};

	DWRITE_GLYPH_RUN glyphRun = {};
	glyphRun.fontFace = pFace;
	glyphRun.fontEmSize = fEmSize;
	glyphRun.glyphCount = 1;
	glyphRun.glyphIndices = &glyphIndex;
	glyphRun.glyphAdvances = &glyphAdvance;
	glyphRun.glyphOffsets = &glyphOffset;

	float baselineX = (float)m_x;
	float baselineY = (float)m_y + fAscent;

	IDWriteGlyphRunAnalysis* pAnalysis = NULL;
	hr = m_pDWriteFactory->CreateGlyphRunAnalysis(
		&glyphRun, 1.0f, NULL,
		DWRITE_RENDERING_MODE_ALIASED,
		DWRITE_MEASURING_MODE_GDI_CLASSIC,
		baselineX, baselineY,
		&pAnalysis);

	if (SUCCEEDED(hr) && pAnalysis)
	{
		RECT bounds;
		hr = pAnalysis->GetAlphaTextureBounds(DWRITE_TEXTURE_ALIASED_1x1, &bounds);

		if (SUCCEEDED(hr) && bounds.right > bounds.left && bounds.bottom > bounds.top)
		{
			int alphaW = bounds.right - bounds.left;
			int alphaH = bounds.bottom - bounds.top;

			std::vector<BYTE> alphaValues(alphaW * alphaH);
			hr = pAnalysis->CreateAlphaTexture(
				DWRITE_TEXTURE_ALIASED_1x1, &bounds,
				alphaValues.data(), (UINT32)alphaValues.size());

			if (SUCCEEDED(hr))
			{
				for (int gy = 0; gy < alphaH; ++gy)
				{
					int atlasY = bounds.top + gy;
					if (atlasY < 0 || atlasY >= m_atlasHeight) continue;

					for (int gx = 0; gx < alphaW; ++gx)
					{
						int atlasX = bounds.left + gx;
						if (atlasX < 0 || atlasX >= m_atlasWidth) continue;

						BYTE a = alphaValues[gy * alphaW + gx];
						m_atlasBuffer[atlasY * m_atlasWidth + atlasX] = a ? 0xFFFFFFFF : 0x00000000;
					}
				}
			}
		}

		pAnalysis->Release();
	}

	float rhwidth = 1.0f / float(m_atlasWidth);
	float rhheight = 1.0f / float(m_atlasHeight);

	TCharacterInfomation& rNewCharInfo = m_charInfoMap[code];

	rNewCharInfo.index = (short)(m_pFontTextureVector.size() - 1);
	rNewCharInfo.width = nChrWidth;
	rNewCharInfo.height = nChrHeight;
	rNewCharInfo.left = float(m_x) * rhwidth;
	rNewCharInfo.top = float(m_y) * rhheight;
	rNewCharInfo.right = float(m_x+nChrWidth) * rhwidth;
	rNewCharInfo.bottom = float(m_y+nChrHeight) * rhheight;
	rNewCharInfo.advance = (float) lAdvance;

	// @fixme050 BEGIN
	static constexpr auto CHAR_SPACING = 2;	 // appending empty space between characters
	m_x += nChrWidth + CHAR_SPACING;

	if (m_step < nChrHeight + CHAR_SPACING)
		m_step = nChrHeight + CHAR_SPACING;
	// @fixme050 END

	m_isDirty = true;

	return &rNewCharInfo;
}

bool CGraphicFontTexture::CheckTextureIndex(DWORD dwTexture)
{
	if (dwTexture >= m_pFontTextureVector.size())
		return false;

	return true;
}

void CGraphicFontTexture::SelectTexture(DWORD dwTexture)
{
	assert(CheckTextureIndex(dwTexture));
	m_pSRV = m_pFontTextureVector[dwTexture]->GetD3DTexture();
}

#if defined(ENABLE_FIX_MOBS_LAG) || defined(ENABLE_TEXT_BATCHING)
CGraphicImageTexture* CGraphicFontTexture::GetTexture(DWORD dwTexture)
{
	assert(CheckTextureIndex(dwTexture));
	return m_pFontTextureVector[dwTexture];
}
#endif
