#pragma once

#include "GrpScreen.h"

class CScreenFilter : public CScreen
{
	public:
		CScreenFilter();
		virtual ~CScreenFilter();

		void SetEnable(BOOL bFlag);
		void SetBlendType(BYTE bySrcType, BYTE byDestType);
		void SetColor(const Color & c_rColor);

		void Render();

	protected:
		BOOL m_bEnable;
		BYTE m_bySrcType;
		BYTE m_byDestType;
		Color m_Color;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
