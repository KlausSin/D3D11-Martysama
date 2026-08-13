#include "StdAfx.h"
#include "ScreenFilter.h"
#include "ShaderManager.h"

void CScreenFilter::Render()
{
	if (!m_bEnable)
		return;

	SHADERMANAGER.PushState();

	SHADERMANAGER.SetMatrix(MATRIX_PROJECTION, &ms_matIdentity);
	SHADERMANAGER.SetMatrix(MATRIX_VIEW, &ms_matIdentity);
	SHADERMANAGER.SetMatrix(MATRIX_WORLD, &ms_matIdentity);

	SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, TRUE);
	SHADERMANAGER.SetPipelineState(PSTATE_SRCBLEND, m_bySrcType);
	SHADERMANAGER.SetPipelineState(PSTATE_DESTBLEND, m_byDestType);

	SetOrtho2D(CScreen::ms_iWidth, CScreen::ms_iHeight, 400.0f);
	SetDiffuseColor(m_Color.r, m_Color.g, m_Color.b, m_Color.a);
	RenderBar2d(0, 0, CScreen::ms_iWidth, CScreen::ms_iHeight);

	SHADERMANAGER.PopState();
}

void CScreenFilter::SetEnable(BOOL /*bFlag*/)
{
	m_bEnable = FALSE;
}

void CScreenFilter::SetBlendType(BYTE bySrcType, BYTE byDestType)
{
	m_bySrcType = bySrcType;
	m_byDestType = byDestType;
}
void CScreenFilter::SetColor(const Color & c_rColor)
{
	m_Color = c_rColor;
}

CScreenFilter::CScreenFilter()
{
	m_bEnable = FALSE;
	m_bySrcType = BLEND_SRCALPHA;
	m_byDestType = BLEND_INVSRCALPHA;
	m_Color = Color(0.0f, 0.0f, 0.0f, 0.0f);
}
CScreenFilter::~CScreenFilter()
{
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
