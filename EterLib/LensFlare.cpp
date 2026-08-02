///////////////////////////////////////////////////////////////////////
//	CLensFlare Class
//
//	(c) 2003 IDV, Inc.
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization and may
//	not be copied or disclosed except in accordance with the terms of
//	that agreement.
//
//      Copyright (c) 2001-2003 IDV, Inc.
//      All Rights Reserved.
//
//		IDV, Inc.
//		1233 Washington St. Suite 610
//		Columbia, SC 29201
//		Voice: (803) 799-1699
//		Fax:   (803) 931-0320
//		Web:   http://www.idvinc.com
//

///////////////////////////////////////////////////////////////////////
//	Preprocessor
#include "StdAfx.h"
#include "LensFlare.h"
#include "Camera.h"
#include "ShaderManager.h"
#include "ResourceManager.h"

#include <math.h>
using namespace std;

///////////////////////////////////////////////////////////////////////
//	Variables

static string g_strFiles[] =
{
	"flare2.dds",
	"flare1.dds",
	"flare2.dds",
	"flare1.dds",
	"flare6.dds",
	"flare4.dds",
	"flare2.dds",
	"flare3.dds",
	""
};
static float g_fPosition[] =
{
	-0.55f,
	-0.5f,
	-0.45f,
	0.2f,
	0.3f,
	0.95f,
	0.9f,
	1.0f
};
static float g_fWidth[] =
{
	20.0f,
	32.0f,
	20.0f,
	32.0f,
	100.0f,
	32.0f,
	20.0f,
	250.0f
};

static float g_afColors[ ][4] =
{
    { 1.0f, 1.0f, 0.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f },
    { 0.0f, 1.0f, 0.0f, 0.8f },
	{ 0.3f, 0.5f, 1.0f, 0.9f },
	{ 0.3f, 0.5f, 1.0f, 0.6f },
	{ 1.0f, 0.6f, 0.9f, 0.4f },
	{ 1.0f, 0.0f, 0.0f, 0.5f },
	{ 1.0f, 0.6f, 0.3f, 0.4f }
};

///////////////////////////////////////////////////////////////////////
//	CLensFlare::CLensFlare

CLensFlare::CLensFlare() :
    m_fSunSize(0),
    m_fBeforeBright(0.0f),
    m_fAfterBright(0.0f),
    m_bFlareVisible(false),
    m_bDrawFlare(true),
    m_bDrawBrightScreen(true),
	m_bEnabled(true),
	m_bShowMainFlare(true),
	m_fMaxBrightness(1.0f),
	m_pDepthStagingTexture(nullptr)
{
    m_pControlPixels = new float[c_nDepthTestDimension * c_nDepthTestDimension];
    m_pTestPixels = new float[c_nDepthTestDimension * c_nDepthTestDimension];
	memset(m_pControlPixels, 0, sizeof(float) * c_nDepthTestDimension * c_nDepthTestDimension);
	memset(m_pTestPixels, 0, sizeof(float) * c_nDepthTestDimension * c_nDepthTestDimension);
	m_afColor[0] = m_afColor[1] = m_afColor[2] = 1.0f;
	m_afFlarePos[0] = m_afFlarePos[1] = 0.0f;
	m_afFlareWinPos[0] = m_afFlareWinPos[1] = 0.0f;
}

///////////////////////////////////////////////////////////////////////
//	CLensFlare::~CLensFlare

CLensFlare::~CLensFlare()
{
    delete[] m_pControlPixels;
    delete[] m_pTestPixels;

	if (m_pDepthStagingTexture)
	{
		m_pDepthStagingTexture->Release();
		m_pDepthStagingTexture = nullptr;
	}
}

///////////////////////////////////////////////////////////////////////
//	CLensFlare::Interpolate

float CLensFlare::Interpolate(float fStart, float fEnd, float fPercent)
{
	return fStart + (fEnd - fStart) * fPercent;
}

///////////////////////////////////////////////////////////////////////
//	CLensFlare::DrawBeforeFlare

void CLensFlare::Compute(const Vector3 & c_rv3LightDirection)
{
	CCamera* pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return;

	float afSunPos[3];

	Vector3 v3Target = pCamera->GetTarget();

	afSunPos[0]	= v3Target.x - c_rv3LightDirection.x * 99999999.0f;
	afSunPos[1]	= v3Target.y - c_rv3LightDirection.y * 99999999.0f;
	afSunPos[2]	= v3Target.z - c_rv3LightDirection.z * 99999999.0f;

	float fX, fY;
	ProjectPosition(afSunPos[0], afSunPos[1], afSunPos[2], &fX, &fY);

	// set flare location
	SetFlareLocation(fX, fY);

	float fSunVectorMagnitude = sqrtf(afSunPos[0] * afSunPos[0] +
		afSunPos[1] * afSunPos[1] +
		afSunPos[2] * afSunPos[2]);
	float afSunVector[3];
	afSunVector[0] = -afSunPos[0] / fSunVectorMagnitude;
	afSunVector[1] = -afSunPos[1] / fSunVectorMagnitude;
	afSunVector[2] = -afSunPos[2] / fSunVectorMagnitude;

	float afCameraDirection[3];
	afCameraDirection[0] = ms_matView._13;
	afCameraDirection[1] = ms_matView._23;
	afCameraDirection[2] = ms_matView._33;

	float fDotProduct =
		(afSunVector[0] * afCameraDirection[0]) +
		(afSunVector[1] * afCameraDirection[1]) +
		(afSunVector[2] * afCameraDirection[2]);

	// acosf(x) < PI/2 is equivalent to x > 0 (avoids expensive trig)
	SetVisible(fDotProduct > 0.0f);

	// set flare brightness
	fX /= ms_Viewport.Width;
	fY /= ms_Viewport.Height;

	float fDistance = sqrtf(((0.5f - fX) * (0.5f - fX)) + ((0.5f - fY) * (0.5f - fY)));
	float fBeforeBright = Interpolate(0.0f, c_fHalfMaxBright, 1.0f - (fDistance * c_fDistanceScale));
	float fAfterBright = Interpolate(0.0f, 1.0f, 1.0f - (fDistance * c_fDistanceScale));

	SetBrightnesses(fBeforeBright, fAfterBright);
}

///////////////////////////////////////////////////////////////////////
//	CLensFlare::DrawBeforeFlare

void CLensFlare::DrawBeforeFlare()
{
    if (!m_bFlareVisible || !m_bEnabled || !m_bShowMainFlare)
        return;

	if (m_SunFlareImageInstance.IsEmpty())
		return;

	Matrix matProj;
	MatrixOrthoOffCenterRH(&matProj, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f);
	SHADERMANAGER.SaveTransform(MATRIX_PROJECTION, &matProj);
	SHADERMANAGER.SaveTransform(MATRIX_VIEW, &ms_matIdentity);

	Matrix matWorld;
	MatrixTranslation(&matWorld, m_afFlarePos[0], m_afFlarePos[1], 0.0f);
	SHADERMANAGER.SetMatrix(MATRIX_WORLD, &matWorld);

	// Save and set lighting state
	m_bSavedLighting = SHADERMANAGER.GetLightingEnabled();
	SHADERMANAGER.SetLightingEnabled(false);

	SHADERMANAGER.SavePipelineState(PSTATE_DEPTHENABLE, FALSE);					// glDisable(GL_DEPTH_TEST);
	SHADERMANAGER.SavePipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
	SHADERMANAGER.SavePipelineState(PSTATE_CULLMODE, CULL_NONE);			// glDisable(GL_CULL_FACE);
	// RS_SHADEMODE removed - not needed in DX11

	// Save and set alpha test state
	m_bSavedAlphaTest = SHADERMANAGER.GetAlphaTestEnabled();
	SHADERMANAGER.SetAlphaTestEnabled(false);

	SHADERMANAGER.SavePipelineState(PSTATE_BLENDENABLE, TRUE);			// glEnable(GL_BLEND);
	SHADERMANAGER.SavePipelineState(PSTATE_SRCBLEND, BLEND_SRCALPHA);
	SHADERMANAGER.SavePipelineState(PSTATE_DESTBLEND, BLEND_INVSRCALPHA);

	float fAspectRatio = ms_Viewport.Width / float(ms_Viewport.Height);
	float fHeight = m_fSunSize * fAspectRatio;
	Color color(1.0f, 1.0f, 1.0f, 1.0f);

	SVertex vertices[4];
	vertices[0].x = -m_fSunSize;
	vertices[0].y = -fHeight;
	vertices[0].z = 0.0f;
	vertices[0].color = color;
	vertices[0].u = 0.0f;
	vertices[0].v = 0.0f;

	vertices[1].x = -m_fSunSize;
	vertices[1].y = fHeight;
	vertices[1].z = 0.0f;
	vertices[1].color = color;
	vertices[1].u = 0.0f;
	vertices[1].v = 1.0f;

	vertices[2].x = m_fSunSize;
	vertices[2].y = -fHeight;
	vertices[2].z = 0.0f;
	vertices[2].color = color;
	vertices[2].u = 1.0f;
	vertices[2].v = 0.0f;

	vertices[3].x = m_fSunSize;
	vertices[3].y = fHeight;
	vertices[3].z = 0.0f;
	vertices[3].color = color;
	vertices[3].u = 1.0f;
	vertices[3].v = 1.0f;

	CGraphicTexture* pTexture = m_SunFlareImageInstance.GetTexturePointer();
	if (pTexture)
	{
		SHADERMANAGER.SetShaderResource(0, pTexture->GetD3DTexture());
		SHADERMANAGER.SetShaderResource(1, NULL);

		// Bind UI shader for screen-space rendering
		SHADERMANAGER.BeginUI();
		SHADERMANAGER.CommitChanges();
		SHADERMANAGER.DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, vertices, sizeof(SVertex));
	}

	// Restore lighting state
	SHADERMANAGER.SetLightingEnabled(m_bSavedLighting);

	SHADERMANAGER.RestorePipelineState(PSTATE_DEPTHENABLE); // glDisable(GL_DEPTH_TEST);
	SHADERMANAGER.RestorePipelineState(PSTATE_DEPTHWRITEMASK);
	SHADERMANAGER.RestorePipelineState(PSTATE_CULLMODE); // glDisable(GL_CULL_FACE);
	// RS_SHADEMODE removed - not needed in DX11

	// Restore alpha test state
	SHADERMANAGER.SetAlphaTestEnabled(m_bSavedAlphaTest);

	SHADERMANAGER.RestorePipelineState(PSTATE_BLENDENABLE); // glEnable(GL_BLEND);
	SHADERMANAGER.RestorePipelineState(PSTATE_SRCBLEND);
	SHADERMANAGER.RestorePipelineState(PSTATE_DESTBLEND);

	SHADERMANAGER.RestoreTransform(MATRIX_VIEW);
	SHADERMANAGER.RestoreTransform(MATRIX_PROJECTION);
}

///////////////////////////////////////////////////////////////////////
//	CLensFlare::DrawAfterFlare

void CLensFlare::DrawAfterFlare()
{
	if (m_bEnabled && m_fAfterBright != 0.0f && m_bDrawBrightScreen)
	{
		SetDiffuseColor(m_afColor[0], m_afColor[1], m_afColor[2], m_fAfterBright);
		RenderBar2d(0.0f, 0.0f, 1024.0f, 1024.0f);
	}
}

///////////////////////////////////////////////////////////////////////
//	CLensFlare::SetMainFlare

void CLensFlare::SetMainFlare(string strSunFile, float fSunSize)
{
	if (m_bEnabled && m_bShowMainFlare)
	{
		m_fSunSize = fSunSize;
		CResource * pResource = CResourceManager::Instance().GetResourcePointer(strSunFile.c_str());

		if (!pResource || !pResource->IsType(CGraphicImage::Type()))
			return;

		m_SunFlareImageInstance.SetImagePointer(static_cast<CGraphicImage *> (pResource));
	}
}

///////////////////////////////////////////////////////////////////////
//	CLensFlare::DrawFlare

void CLensFlare::DrawFlare()
{
	if (m_bEnabled && m_bFlareVisible && m_bDrawFlare && m_fAfterBright != 0.0f)
	{
        //glPushAttrib(GL_ENABLE_BIT);
		// Save and set lighting state
		m_bSavedLightingFlare = SHADERMANAGER.GetLightingEnabled();
		SHADERMANAGER.SetLightingEnabled(false);

		SHADERMANAGER.SavePipelineState(PSTATE_DEPTHENABLE, FALSE); // glDisable(GL_DEPTH_TEST);
		SHADERMANAGER.SavePipelineState(PSTATE_CULLMODE, CULL_NONE); // glDisable(GL_CULL_FACE);

		// Save and set alpha test state
		m_bSavedAlphaTestFlare = SHADERMANAGER.GetAlphaTestEnabled();
		SHADERMANAGER.SetAlphaTestEnabled(false);

		SHADERMANAGER.SavePipelineState(PSTATE_BLENDENABLE, TRUE); // glEnable(GL_BLEND);

		Matrix matProj;
		MatrixOrthoOffCenterRH(&matProj, 0.0f, ms_Viewport.Width, ms_Viewport.Height, 0.0f, -1.0f, 1.0f);
		SHADERMANAGER.SaveTransform(MATRIX_PROJECTION, &matProj);
		SHADERMANAGER.SaveTransform(MATRIX_VIEW, &ms_matIdentity);

		SHADERMANAGER.SetMatrix(MATRIX_WORLD, &ms_matIdentity);
		//glMatrixMode(GL_MODELVIEW);
		//glLoadIdentity();

		//glDisable(GL_TEXTURE_2D);
		DrawAfterFlare();

		//glEnable(GL_TEXTURE_2D);
		m_cFlare.Draw(m_fAfterBright,
					  ms_Viewport.Width,
					  ms_Viewport.Height,
					  static_cast<int>(m_afFlareWinPos[0]),
					  static_cast<int>(m_afFlareWinPos[1]));

		// Restore lighting state
		SHADERMANAGER.SetLightingEnabled(m_bSavedLightingFlare);

		SHADERMANAGER.RestorePipelineState(PSTATE_DEPTHENABLE); // glDisable(GL_DEPTH_TEST);
		SHADERMANAGER.RestorePipelineState(PSTATE_CULLMODE); // glDisable(GL_CULL_FACE);
		SHADERMANAGER.RestorePipelineState(PSTATE_BLENDENABLE); // glEnable(GL_BLEND);

		// Restore alpha test state
		SHADERMANAGER.SetAlphaTestEnabled(m_bSavedAlphaTestFlare);

		SHADERMANAGER.RestoreTransform(MATRIX_PROJECTION);
		SHADERMANAGER.RestoreTransform(MATRIX_VIEW);
		//glDisable(GL_TEXTURE_2D);
        //glPopAttrib();
	}
}

///////////////////////////////////////////////////////////////////////
//	CLensFlare::CharacterizeFlare
void CLensFlare::CharacterizeFlare(bool bEnabled, bool bShowMainFlare, float fMaxBrightness, const Color & c_rColor)
{
	m_bEnabled = bEnabled;
	m_bShowMainFlare = bShowMainFlare;
	m_fMaxBrightness = fMaxBrightness;

	m_afColor[0] = c_rColor.r;
	m_afColor[1] = c_rColor.g;
	m_afColor[2] = c_rColor.b;
}

///////////////////////////////////////////////////////////////////////
//	CLensFlare::Initialize
void CLensFlare::Initialize(std::string strPath)
{
	if (m_bEnabled)
		m_cFlare.Init(strPath);
}

///////////////////////////////////////////////////////////////////////
//	CLensFlare::SetFlareLocation
void CLensFlare::SetFlareLocation(double dX, double dY)
{
	if (m_bEnabled)
	{
		m_afFlareWinPos[0] = float(dX);
		m_afFlareWinPos[1] = float(dY);

		m_afFlarePos[0] = float(dX) / ms_Viewport.Width;
		m_afFlarePos[1] = float(dY) / ms_Viewport.Height;
	}
}

///////////////////////////////////////////////////////////////////////
//	CLensFlare::SetBrightnesses

void CLensFlare::SetBrightnesses(float fBeforeBright, float fAfterBright)
{
	if (m_bEnabled)
	{
	    m_fBeforeBright = fBeforeBright;
	    m_fAfterBright = fAfterBright;

		ClampBrightness();
	}
}

///////////////////////////////////////////////////////////////////////
//	CLensFlare::ReadControlPixels

void CLensFlare::ReadControlPixels()
{
	if (m_bEnabled)
		ReadDepthPixels(m_pControlPixels);
}

///////////////////////////////////////////////////////////////////////
//	CLensFlare::AdjustBrightness

void CLensFlare::AdjustBrightness()
{
	if (m_bEnabled)
	{
		ReadDepthPixels(m_pTestPixels);

		int nDifferent = 0;

		for (int i = 0; i < c_nDepthTestDimension * c_nDepthTestDimension; ++i)
			if (m_pTestPixels[i] != m_pControlPixels[i])
				++nDifferent;

		float fAdjust = (static_cast<float>(nDifferent) / (c_nDepthTestDimension * c_nDepthTestDimension));
		fAdjust = sqrtf(fAdjust) * 0.85f;
		m_fAfterBright *= 1.0f - fAdjust;
	}
}

///////////////////////////////////////////////////////////////////////
//	CLensFlare::ReadDepthPixels

void CLensFlare::ReadDepthPixels(float * pPixels)
{
	if (!ms_pDevice || !ms_pContext || !ms_pDepthStencilBuffer)
		return;

	if (!m_pDepthStagingTexture)
	{
		D3D11_TEXTURE2D_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Width = c_nDepthTestDimension;
		desc.Height = c_nDepthTestDimension;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
#ifdef ENABLE_SSAO
		desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
#else
		desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
#endif
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		HRESULT hr = ms_pDevice->CreateTexture2D(&desc, nullptr, &m_pDepthStagingTexture);
		if (FAILED(hr))
			return;
	}

	// Compute source region centered on sun screen position
	int nCenterX = static_cast<int>(m_afFlareWinPos[0]);
	int nCenterY = static_cast<int>(m_afFlareWinPos[1]);
	int nHalf = c_nDepthTestDimension / 2;

	int nSrcX = nCenterX - nHalf;
	int nSrcY = nCenterY - nHalf;

	// Clamp to screen bounds
	int nScreenW = static_cast<int>(ms_Viewport.Width);
	int nScreenH = static_cast<int>(ms_Viewport.Height);

	if (nSrcX < 0) nSrcX = 0;
	if (nSrcY < 0) nSrcY = 0;
	if (nSrcX + c_nDepthTestDimension > nScreenW) nSrcX = nScreenW - c_nDepthTestDimension;
	if (nSrcY + c_nDepthTestDimension > nScreenH) nSrcY = nScreenH - c_nDepthTestDimension;

	if (nSrcX < 0 || nSrcY < 0)
		return;

	// Copy sub-region from depth buffer to staging texture
	D3D11_BOX srcBox;
	srcBox.left = nSrcX;
	srcBox.top = nSrcY;
	srcBox.right = nSrcX + c_nDepthTestDimension;
	srcBox.bottom = nSrcY + c_nDepthTestDimension;
	srcBox.front = 0;
	srcBox.back = 1;

	ms_pContext->CopySubresourceRegion(m_pDepthStagingTexture, 0, 0, 0, 0,
		ms_pDepthStencilBuffer, 0, &srcBox);

	// Map and read depth values
	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = ms_pContext->Map(m_pDepthStagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
	if (FAILED(hr))
		return;

	for (int y = 0; y < c_nDepthTestDimension; ++y)
	{
		const DWORD* pRow = reinterpret_cast<const DWORD*>(
			static_cast<const BYTE*>(mapped.pData) + y * mapped.RowPitch);

		for (int x = 0; x < c_nDepthTestDimension; ++x)
		{
			DWORD dwDepthStencil = pRow[x];
			// Extract 24-bit depth and normalize to [0,1]
			pPixels[y * c_nDepthTestDimension + x] = static_cast<float>(dwDepthStencil >> 8) / 16777215.0f;
		}
	}

	ms_pContext->Unmap(m_pDepthStagingTexture, 0);
}

///////////////////////////////////////////////////////////////////////
//	CLensFlare::ClampBrightness

void CLensFlare::ClampBrightness()
{
	// before
    if (m_fBeforeBright < 0.0f)
        m_fBeforeBright = 0.0f;
    else if (m_fBeforeBright > 1.0f)
        m_fBeforeBright = 1.0f;

	m_fBeforeBright *= m_fMaxBrightness;

    if (m_fAfterBright < 0.0f)
        m_fAfterBright = 0.0f;
    else if (m_fAfterBright > 1.0f)
        m_fAfterBright = 1.0f;

	m_fAfterBright *= m_fMaxBrightness;
}

///////////////////////////////////////////////////////////////////////
//	CFlare implementation
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
//	CFlare::CFlare

CFlare::CFlare()
{
}

///////////////////////////////////////////////////////////////////////
//	CFlare::~CFlare

CFlare::~CFlare()
{
}

///////////////////////////////////////////////////////////////////////
//	CFlare::Init

void CFlare::Init(std::string strPath)
{
	int i = 0;

	while (g_strFiles[i] != "")
	{
		CResource * pResource = CResourceManager::Instance().GetResourcePointer((strPath + "/" + string(g_strFiles[i])).c_str());

		if (!pResource->IsType(CGraphicImage::Type()))
			assert(false);

		SFlarePiece * pPiece = new SFlarePiece;

		pPiece->m_imageInstance.SetImagePointer(static_cast<CGraphicImage *> (pResource));
		pPiece->m_fPosition = g_fPosition[i];
		pPiece->m_fWidth = g_fWidth[i];
		pPiece->m_pColor = g_afColors[i];

		m_vFlares.push_back(pPiece);
		i++;
	}
}

///////////////////////////////////////////////////////////////////////
//	CFlare::Draw
void CFlare::Draw(float fBrightScale, int nWidth, int nHeight, int nX, int nY)
{
	SHADERMANAGER.SavePipelineState(PSTATE_SRCBLEND, BLEND_SRCALPHA);
	SHADERMANAGER.SavePipelineState(PSTATE_DESTBLEND, BLEND_ONE);

	float fDX = float(nX) - float(nWidth) / 2.0f;
	float fDY = float(nY) - float(nHeight) / 2.0f;

	SHADERMANAGER.SetShaderResource(1, NULL);


	for (unsigned int i = 0; i < m_vFlares.size(); i++)
	{
		float fCenterX = float(nX) - (m_vFlares[i]->m_fPosition + 1.0f) * fDX;
		float fCenterY = float(nY) - (m_vFlares[i]->m_fPosition + 1.0f) * fDY;
		float fW = m_vFlares[i]->m_fWidth;

		Color flareColor(m_vFlares[i]->m_pColor[0] * fBrightScale,
						   m_vFlares[i]->m_pColor[1] * fBrightScale,
						   m_vFlares[i]->m_pColor[2] * fBrightScale,
						   m_vFlares[i]->m_pColor[3] * fBrightScale);

		CGraphicTexture* pFlareTexture = m_vFlares[i]->m_imageInstance.GetTexturePointer();
		if (!pFlareTexture)
			continue;

		SHADERMANAGER.SetShaderResource(0, pFlareTexture->GetD3DTexture());

		TVertex vertices[4];

		vertices[0].u = 0.0f;
		vertices[0].v = 0.0f;
		vertices[0].x = fCenterX - fW;
		vertices[0].y = fCenterY - fW;
		vertices[0].z = 0.0f;
		vertices[0].color = flareColor;

		vertices[1].u = 0.0f;
		vertices[1].v = 1.0f;
		vertices[1].x = fCenterX - fW;
		vertices[1].y = fCenterY + fW;
		vertices[1].z = 0.0f;
		vertices[1].color = flareColor;

		vertices[2].u = 1.0f;
		vertices[2].v = 0.0f;
		vertices[2].x = fCenterX + fW;
		vertices[2].y = fCenterY - fW;
		vertices[2].z = 0.0f;
		vertices[2].color = flareColor;

		vertices[3].u = 1.0f;
		vertices[3].v = 1.0f;
		vertices[3].x = fCenterX + fW;
		vertices[3].y = fCenterY + fW;
		vertices[3].z = 0.0f;
		vertices[3].color = flareColor;

		// Bind UI shader for screen-space rendering
		SHADERMANAGER.BeginUI();
		SHADERMANAGER.CommitChanges();
		SHADERMANAGER.DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, vertices, sizeof(TVertex));
	}

	SHADERMANAGER.RestorePipelineState(PSTATE_SRCBLEND);
	SHADERMANAGER.RestorePipelineState(PSTATE_DESTBLEND);
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
