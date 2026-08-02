#include "StdAfx.h"
#include "../eterLib/ShaderManager.h"
#include "../eterlib/Camera.h"
#include "../eterLib/ShaderInit.h"
#include "../EterBase/StepTimer.h"
#include "../UserInterface/Locale_inc.h"

#include "MapOutdoor.h"

static int recreate = false;

bool CMapOutdoor::CreateShadowMap(WORD size, ID3D11Texture2D** ppTexture, ID3D11ShaderResourceView** ppSRV,
								  ID3D11RenderTargetView** ppRTV, ID3D11DepthStencilView** ppDSV)
{
	if (!ms_pDevice)
		return false;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = size;
	texDesc.Height = size;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(ms_pDevice->CreateTexture2D(&texDesc, nullptr, ppTexture)))
		return false;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	if (FAILED(ms_pDevice->CreateShaderResourceView(*ppTexture, &srvDesc, ppSRV)))
	{
		SAFE_RELEASE(*ppTexture);
		return false;
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	if (FAILED(ms_pDevice->CreateDepthStencilView(*ppTexture, &dsvDesc, ppDSV)))
	{
		SAFE_RELEASE(*ppSRV);
		SAFE_RELEASE(*ppTexture);
		return false;
	}

	if (ppRTV)
		*ppRTV = nullptr;
	return true;
}

void CMapOutdoor::SetShadowTextureSize(WORD size)
{
	if (m_wShadowMapSize != size)
	{
		recreate = true;
		Tracenf("ShadowTextureSize changed %d -> %d", m_wShadowMapSize, size);
	}
	m_wShadowMapSize = size;
	if (size > 0)
		SHADERMANAGER.SetShadowTexelSize(1.0f / (float)size);   // real texel size for the PCF kernel
}

void CMapOutdoor::CreateCharacterShadowTexture()
{
	extern bool GRAPHICS_CAPS_CAN_NOT_DRAW_SHADOW;
	if (GRAPHICS_CAPS_CAN_NOT_DRAW_SHADOW)
		return;
	if (!ms_pDevice)
		return;

	ReleaseCharacterShadowTexture();

	for (int i = 0; i < CSM_NUM_CASCADES; ++i)
		m_bShadowCascadeValid[i] = false;

	if (IsLowTextureMemory())
		SetShadowTextureSize(128);

	m_fShadowOpacity = SHADOW_OPACITY_DEFAULT;

	m_ShadowMapViewport.TopLeftX = 1.0f;
	m_ShadowMapViewport.TopLeftY = 1.0f;
	m_ShadowMapViewport.Width = static_cast<float>(m_wShadowMapSize - 2);
	m_ShadowMapViewport.Height = static_cast<float>(m_wShadowMapSize - 2);
	m_ShadowMapViewport.MinDepth = 0.0f;
	m_ShadowMapViewport.MaxDepth = 1.0f;

	// Create 4 cascade shadow maps (R32F) — same proven CreateShadowMap pattern
	for (int i = 0; i < CSM_NUM_CASCADES; ++i)
	{
		if (!CreateShadowMap(m_wShadowMapSize, &m_lpShadowMapTexture[i], &m_lpShadowMapSRV[i],
			&m_lpShadowMapRTV[i], &m_lpShadowMapDSV[i]))
		{
			TraceError("CMapOutdoor: Failed to create cascade %d shadow map\n", i);
		}
	}

	// Legacy shadow map (for backwards compatibility)
	ID3D11Texture2D* pShadowTexture = nullptr;
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = m_wShadowMapSize;
	texDesc.Height = m_wShadowMapSize;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_B5G6R5_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = ms_pDevice->CreateTexture2D(&texDesc, nullptr, &pShadowTexture);
	if (FAILED(hr))
	{
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		hr = ms_pDevice->CreateTexture2D(&texDesc, nullptr, &pShadowTexture);
		if (FAILED(hr))
		{
			TraceError("CMapOutdoor Unable to create Character Shadow render target texture\n");
			return;
		}
	}

	if (FAILED(ms_pDevice->CreateShaderResourceView(pShadowTexture, nullptr, &m_lpCharacterShadowMapTexture)))
	{
		pShadowTexture->Release();
		return;
	}
	if (FAILED(ms_pDevice->CreateRenderTargetView(pShadowTexture, nullptr, &m_lpCharacterShadowMapRenderTargetSurface)))
	{
		SAFE_RELEASE(m_lpCharacterShadowMapTexture);
		pShadowTexture->Release();
		return;
	}
	pShadowTexture->Release();

	ID3D11Texture2D* pDepthTexture = nullptr;
	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = m_wShadowMapSize;
	depthDesc.Height = m_wShadowMapSize;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D16_UNORM;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	if (FAILED(ms_pDevice->CreateTexture2D(&depthDesc, nullptr, &pDepthTexture)))
		return;
	if (FAILED(ms_pDevice->CreateDepthStencilView(pDepthTexture, nullptr, &m_lpCharacterShadowMapDepthSurface)))
	{
		pDepthTexture->Release();
		return;
	}
	pDepthTexture->Release();

	Tracenf("CMapOutdoor: CSM shadow maps created (%dx%d, 4 cascades)", m_wShadowMapSize, m_wShadowMapSize);
}

void CMapOutdoor::ReleaseCharacterShadowTexture()
{
	SAFE_RELEASE(m_lpCharacterShadowMapRenderTargetSurface);
	SAFE_RELEASE(m_lpCharacterShadowMapDepthSurface);
	SAFE_RELEASE(m_lpCharacterShadowMapTexture);

	for (int i = 0; i < CSM_NUM_CASCADES; ++i)
	{
		SAFE_RELEASE(m_lpShadowMapRTV[i]);
		SAFE_RELEASE(m_lpShadowMapDSV[i]);
		SAFE_RELEASE(m_lpShadowMapSRV[i]);
		SAFE_RELEASE(m_lpShadowMapTexture[i]);
	}
}

DWORD dwLightEnable = FALSE;
DWORD dwSavedTextureFactor = 0xFFFFFFFF;

bool CMapOutdoor::BeginRenderCharacterShadowToTexture(int cascadeIndex)
{
	if (!ms_pContext)
	{
		m_bShadowRenderActive = false;
		return false;
	}

	if (cascadeIndex < 0 || cascadeIndex >= CSM_NUM_CASCADES)
	{
		m_bShadowRenderActive = false;
		return false;
	}

	CCamera* pCurrentCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCurrentCamera)
	{
		m_bShadowRenderActive = false;
		return false;
	}

	if (recreate)
	{
		CreateCharacterShadowTexture();
		recreate = false;
	}

	ID3D11RenderTargetView* pRTV = m_lpShadowMapRTV[cascadeIndex];
	ID3D11DepthStencilView* pDSV = m_lpShadowMapDSV[cascadeIndex];

	if (!pDSV)   // depth-only now: pRTV is intentionally null
	{
		m_bShadowRenderActive = false;
		return false;
	}

	m_bShadowRenderActive = true;

	if (!m_bShadowSunLatched)
	{
		const float SHADOW_SUN_ELEV_Z = 0.625f;
		const float fHoriz = sqrtf(1.0f - SHADOW_SUN_ELEV_Z * SHADOW_SUN_ELEV_Z);

		float fAzimuth = 3.9269908f;   // 225 deg, used only if the map has no usable light direction
		if (mc_pEnvironmentData)
		{
			const Vector3& vLightDir = mc_pEnvironmentData->DirLights[ENV_DIRLIGHT_BACKGROUND].Direction;
			if (fabsf(vLightDir.x) > 0.0001f || fabsf(vLightDir.y) > 0.0001f)
				fAzimuth = atan2f(-vLightDir.y, -vLightDir.x);
		}

		m_v3ShadowSunDir.x = cosf(fAzimuth) * fHoriz;
		m_v3ShadowSunDir.y = sinf(fAzimuth) * fHoriz;
		m_v3ShadowSunDir.z = SHADOW_SUN_ELEV_Z;
		m_bShadowSunLatched = true;
	}

	const float SHADOW_SUN_DIR_X = m_v3ShadowSunDir.x;
	const float SHADOW_SUN_DIR_Y = m_v3ShadowSunDir.y;
	const float SHADOW_SUN_DIR_Z = m_v3ShadowSunDir.z;

	Vector3 v3Target = pCurrentCamera->GetTarget();

	// Ortho projection sized by this cascade
	float mapSize = m_fCascadeSize[cascadeIndex];

	if (m_wShadowMapSize > 0)
	{
		Vector3 vZ(SHADOW_SUN_DIR_X, SHADOW_SUN_DIR_Y, SHADOW_SUN_DIR_Z);
		Vector3 vX(-vZ.y, vZ.x, 0.0f);						// cross(worldUp, vZ)
		float fLen = sqrtf(vX.x * vX.x + vX.y * vX.y);
		if (fLen > 0.001f) { vX.x /= fLen; vX.y /= fLen; }
		Vector3 vY(vZ.y * vX.z - vZ.z * vX.y,				// cross(vZ, vX)
				   vZ.z * vX.x - vZ.x * vX.z,
				   vZ.x * vX.y - vZ.y * vX.x);

		const float fTexel = mapSize / (float)m_wShadowMapSize;
		float fU = v3Target.x * vX.x + v3Target.y * vX.y + v3Target.z * vX.z;
		float fV = v3Target.x * vY.x + v3Target.y * vY.y + v3Target.z * vY.z;
		const float fW = v3Target.x * vZ.x + v3Target.y * vZ.y + v3Target.z * vZ.z;

		fU = floorf(fU / fTexel) * fTexel;
		fV = floorf(fV / fTexel) * fTexel;

		v3Target.x = vX.x * fU + vY.x * fV + vZ.x * fW;
		v3Target.y = vX.y * fU + vY.y * fV + vZ.y * fW;
		v3Target.z = vX.z * fU + vY.z * fV + vZ.z * fW;
	}

	if (cascadeIndex == 0)
		++m_dwShadowFrameIndex;

	{
		static const DWORD s_adwInterval[CSM_NUM_CASCADES] = { 1, 1, 2, 4 };
		static const DWORD s_adwPhase[CSM_NUM_CASCADES]    = { 0, 0, 0, 1 };

		const Vector3& v3Prev = m_v3ShadowCascadeCenter[cascadeIndex];
		const bool bMoved = (v3Target.x != v3Prev.x) || (v3Target.y != v3Prev.y) || (v3Target.z != v3Prev.z);
		const bool bDue   = ((m_dwShadowFrameIndex % s_adwInterval[cascadeIndex]) == s_adwPhase[cascadeIndex]);

		if (m_bShadowCascadeValid[cascadeIndex] && !bMoved && !bDue)
		{
			SHADERMANAGER.SetShadowCullPlanes(NULL);   // nothing renders, so leave no stale planes behind
			m_bShadowRenderActive = false;
			return false;
		}

		m_v3ShadowCascadeCenter[cascadeIndex] = v3Target;
		m_bShadowCascadeValid[cascadeIndex] = true;
	}

	Vector3 v3LightEye;
	const float fLightDist = SHADOW_LIGHT_OFFSET * 2.0f;
	v3LightEye.x = v3Target.x + SHADOW_SUN_DIR_X * fLightDist;
	v3LightEye.y = v3Target.y + SHADOW_SUN_DIR_Y * fLightDist;
	v3LightEye.z = v3Target.z + SHADOW_SUN_DIR_Z * fLightDist;

	auto val = Vector3(0.0f, 0.0f, 1.0f);
	MatrixLookAtRH(&m_matLightView, &v3LightEye, &v3Target, &val);

	Matrix matLightProj;
	MatrixOrthoRH(&matLightProj, mapSize, mapSize, SHADOW_NEAR_PLANE, SHADOW_FAR_PLANE);

	// Store the cascade matrix for shader sampling (no snap — must match rendering exactly)
	MatrixMultiply(&m_matShadowCascade[cascadeIndex], &m_matLightView, &matLightProj);

	{
		const Matrix& M = m_matShadowCascade[cascadeIndex];
		float afPlane[4][4] =
		{
			{  M._11,  M._21,  M._31,  M._41 + 1.0f },   // left   ( clip.x + clip.w >= 0 )
			{ -M._11, -M._21, -M._31, -M._41 + 1.0f },   // right  ( clip.w - clip.x >= 0 )
			{  M._12,  M._22,  M._32,  M._42 + 1.0f },   // bottom ( clip.y + clip.w >= 0 )
			{ -M._12, -M._22, -M._32, -M._42 + 1.0f },   // top    ( clip.w - clip.y >= 0 )
		};

		for (int i = 0; i < 4; ++i)
		{
			float fLen = sqrtf(afPlane[i][0] * afPlane[i][0] +
							   afPlane[i][1] * afPlane[i][1] +
							   afPlane[i][2] * afPlane[i][2]);
			if (fLen > 0.000001f)
			{
				afPlane[i][0] /= fLen; afPlane[i][1] /= fLen;
				afPlane[i][2] /= fLen; afPlane[i][3] /= fLen;
			}
		}

		SHADERMANAGER.SetShadowCullPlanes(&afPlane[0][0]);
	}

	SHADERMANAGER.SaveTransform(MATRIX_VIEW, &m_matLightView);
	SHADERMANAGER.SaveTransform(MATRIX_PROJECTION, &matLightProj);

	dwLightEnable = SHADERMANAGER.GetLightingEnabled() ? TRUE : FALSE;
	SHADERMANAGER.SetLightingEnabled(false);

	dwSavedTextureFactor = SHADERMANAGER.GetTextureFactor();
	SHADERMANAGER.SetTextureFactor(0xFF808080);
	SHADERMANAGER.SetCharacterShadowPass(true);

	BeginShaderShadowRender();

	// Unbind shadow SRVs before using as render targets
	SHADERMANAGER.SetShadowTextures(nullptr, nullptr);
	SHADERMANAGER.SetShadowMidFarTextures(nullptr, nullptr);

	bool bSuccess = true;

	ms_pContext->OMGetRenderTargets(1, &m_lpBackupRenderTargetSurface, &m_lpBackupDepthSurface);
	if (!m_lpBackupRenderTargetSurface)
	{
		TraceError("CMapOutdoor::BeginRenderCharacterShadowToTexture : Unable to Save Window Render Target\n");
		bSuccess = false;
	}

	ms_pContext->OMSetRenderTargets(0, nullptr, pDSV);

	ms_pContext->ClearDepthStencilView(pDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

	SHADERMANAGER.SavePipelineState(PSTATE_DEPTHENABLE, TRUE);
	SHADERMANAGER.SavePipelineState(PSTATE_DEPTHWRITEMASK, TRUE);
	SHADERMANAGER.SavePipelineState(PSTATE_DEPTHFUNC, COMPARISON_LESSEQUAL);

	const float fSlopeScaledBias = 2.0f;
	SHADERMANAGER.SavePipelineState(PSTATE_SLOPESCALEDDEPTHBIAS, *(const DWORD*)&fSlopeScaledBias);

	UINT numViewports = 1;
	ms_pContext->RSGetViewports(&numViewports, &m_BackupViewport);
	ms_pContext->RSSetViewports(1, &m_ShadowMapViewport);

	return bSuccess;
}

void CMapOutdoor::EndRenderCharacterShadowToTexture()
{
	if (!m_bShadowRenderActive)
		return;
	if (!ms_pContext)
	{
		m_bShadowRenderActive = false;
		return;
	}

	m_bShadowRenderActive = false;

	SHADERMANAGER.SetShadowCullPlanes(NULL);   // culling applies to the shadow pass only

	SHADERMANAGER.RestorePipelineState(PSTATE_SLOPESCALEDDEPTHBIAS);
	SHADERMANAGER.RestorePipelineState(PSTATE_DEPTHFUNC);
	SHADERMANAGER.RestorePipelineState(PSTATE_DEPTHWRITEMASK);
	SHADERMANAGER.RestorePipelineState(PSTATE_DEPTHENABLE);

	EndShaderShadowRender();

	ms_pContext->RSSetViewports(1, &m_BackupViewport);
	ms_pContext->OMSetRenderTargets(1, &m_lpBackupRenderTargetSurface, m_lpBackupDepthSurface);

	SAFE_RELEASE(m_lpBackupRenderTargetSurface);
	SAFE_RELEASE(m_lpBackupDepthSurface);

	SHADERMANAGER.RestoreTransform(MATRIX_VIEW);
	SHADERMANAGER.RestoreTransform(MATRIX_PROJECTION);

	SHADERMANAGER.SetLightingEnabled(dwLightEnable != FALSE);
	SHADERMANAGER.SetTextureFactor(dwSavedTextureFactor);
	SHADERMANAGER.SetCharacterShadowPass(false);
}

void CMapOutdoor::RenderObjectShadowsToTexture()
{
	SHADERMANAGER.BeginShadow();

	for (int i = 0; i < AROUND_AREA_NUM; ++i)
	{
		CArea* pArea = m_pArea[i];
		if (pArea)
		{
			pArea->RenderToShadowMapVTF();
			pArea->RenderDungeonShadowVTFBatched();
		}
	}
}

void CMapOutdoor::RenderTreeShadowsToTexture()
{
	CSpeedTreeForestDX11& rkForest = CSpeedTreeForestDX11::Instance();
	rkForest.RenderToShadowMap();
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
