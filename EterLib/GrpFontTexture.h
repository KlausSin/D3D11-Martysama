#pragma once

#include "GrpTexture.h"
#include "GrpImageTexture.h"

#include <dwrite.h>
#include <vector>
#include <map>

class CGraphicFontTexture : public CGraphicTexture
{
public:
	typedef std::pair<WORD,wchar_t> TCharacterKey;

	typedef struct SCharacterInfomation
	{
		short index;
		short width;
		short height;
		float left;
		float top;
		float right;
		float bottom;
		float advance;
	} TCharacterInfomation;

	typedef std::vector<TCharacterInfomation*>		TPCharacterInfomationVector;

public:
	CGraphicFontTexture();
	virtual ~CGraphicFontTexture();

	void Destroy();
	bool Create(const char* c_szFontName, int fontSize, bool bItalic);

	bool CreateDeviceObjects();
	void DestroyDeviceObjects();

	bool CheckTextureIndex(DWORD dwTexture);
#if defined(ENABLE_FIX_MOBS_LAG) || defined(ENABLE_TEXT_BATCHING)
	CGraphicImageTexture* GetTexture(DWORD dwTexture);
#endif
	void SelectTexture(DWORD dwTexture);

	bool UpdateTexture();

	TCharacterInfomation* GetCharacterInfomation(WORD codePage, wchar_t keyValue);
	TCharacterInfomation* UpdateCharacterInfomation(TCharacterKey code);

	bool IsEmpty() const;

protected:
	void Initialize();

	bool AppendTexture();

	struct SFontFaceData
	{
		IDWriteFontFace* pFace;
		float fEmSize;
		float fAscent;
		float fCellHeight;
	};

	SFontFaceData* GetFontFaceData(WORD codePage);

protected:
	typedef std::vector<CGraphicImageTexture*>				TGraphicImageTexturePointerVector;
	typedef std::map<TCharacterKey, TCharacterInfomation>	TCharacterInfomationMap;
	typedef std::map<WORD, SFontFaceData>					TFontFaceMap;

protected:
	// DirectWrite
	IDWriteFactory*		m_pDWriteFactory;
	IDWriteGdiInterop*	m_pGdiInterop;

	// CPU atlas buffer (replaces GDI DIB)
	std::vector<DWORD>	m_atlasBuffer;
	int					m_atlasWidth;
	int					m_atlasHeight;

	TGraphicImageTexturePointerVector m_pFontTextureVector;

	TCharacterInfomationMap m_charInfoMap;

	TFontFaceMap m_fontFaceMap;

	int m_x;
	int m_y;
	int m_step;
	bool m_isDirty;

	TCHAR	m_fontName[LF_FACESIZE];
	LONG	m_fontSize;
	bool	m_bItalic;
};
