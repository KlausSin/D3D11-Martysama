#pragma once

#include "../eterBase/FileLoader.h"

#include <map>
#include <vector>

#include <d3d11.h>
#include <DirectXMath.h>
#include "GrpMathType.h"
#include "GrpMathFunc.h"

template<typename T>
class CTransitor
{
	public:
		CTransitor() {}
		~CTransitor() {}

		void SetActive(BOOL bActive = TRUE)
		{
			m_bActivated = bActive;
		}

		BOOL isActive()
		{
			return m_bActivated;
		}

		BOOL isActiveTime(float fcurTime)
		{
			if (fcurTime >= m_fEndTime)
				return FALSE;

			return TRUE;
		}

		DWORD GetID()
		{
			return m_dwID;
		}

		void SetID(DWORD dwID)
		{
			m_dwID = dwID;
		}

		void SetSourceValue(const T & c_rSourceValue)
		{
			m_SourceValue = c_rSourceValue;
		}

		void SetTransition(const T & c_rSourceValue, const T & c_rTargetValue, float fStartTime, float fBlendTime)
		{
			m_SourceValue = c_rSourceValue;
			m_TargetValue = c_rTargetValue;
			m_fStartTime = fStartTime;
			m_fEndTime = fStartTime + fBlendTime;
		}

		BOOL GetValue(float fcurTime, T * pValue)
		{
			if (fcurTime <= m_fStartTime)
				return FALSE;

			float fPercentage = (fcurTime - m_fStartTime) / (m_fEndTime - m_fStartTime);
			*pValue = m_SourceValue + (m_TargetValue - m_SourceValue) * fPercentage;
			return TRUE;
		}

	protected:
		DWORD	m_dwID;			// Public Transitor ID

		BOOL	m_bActivated;	// Have been started to blend?
		float	m_fStartTime;
		float	m_fEndTime;

		T		m_SourceValue;
		T		m_TargetValue;
};

typedef CTransitor<float>			TTransitorFloat;
typedef CTransitor<Vector3>			TTransitorVector3;
typedef CTransitor<Color>			TTransitorColor;

///////////////////////////////////////////////////////////////////////////////////////////////////

void PrintfTabs(FILE * File, int iTabCount, const char * c_szString, ...);

//typedef CTokenVector TTokenVector;

extern bool	LoadTextData(const char * c_szFileName, CTokenMap & rstTokenMap);
extern bool	LoadMultipleTextData(const char * c_szFileName, CTokenVectorMap & rstTokenVectorMap);

extern Vector3 TokenToVector(CTokenVector & rVector);
extern Color TokenToColor(CTokenVector & rVector);

#define GOTO_CHILD_NODE(TextFileLoader, Index) CTextFileLoader::CGotoChild Child(TextFileLoader, Index);

///////////////////////////////////////////////////////////////////////////////////////////////////

extern int CALLBACK EnumFontFamExProc(CONST LOGFONT* plogFont, CONST TEXTMETRIC* textMetric, DWORD dwWord, LPARAM lParam);
extern int GetCharsetFromCodePage(WORD codePage);
extern const char* GetFontFaceFromCodePageNT(WORD codePage);
extern const char* GetFontFaceFromCodePage9x(WORD codePage);
extern DWORD GetDefaultCodePage();
extern const char * GetDefaultFontFace();
extern const char*	GetFontFaceFromCodePage(WORD codePage);
extern void SetDefaultFontFace(const char* fontFace);
extern bool SetDefaultCodePage(DWORD codePage);
extern void base64_decode(const char * str,char * resultStr);

extern DWORD GetMaxTextureWidth();
extern DWORD GetMaxTextureHeight();
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
