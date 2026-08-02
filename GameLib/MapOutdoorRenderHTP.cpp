#include "StdAfx.h"
#include "MapOutdoor.h"

#include <unordered_map>

#include "../eterlib/ShaderManager.h"
#include "../eterlib/ShaderInit.h"

void CMapOutdoor::__RenderTerrain_RenderHardwareTransformPatch()
{
	DWORD dwFogColor;
	float fFogFarDistance;
	float fFogNearDistance;
	if (mc_pEnvironmentData)
	{
		dwFogColor=mc_pEnvironmentData->FogColor;
		fFogNearDistance=mc_pEnvironmentData->GetFogNearDistance();
		fFogFarDistance=mc_pEnvironmentData->GetFogFarDistance();
	}
	else
	{
		dwFogColor=0xffffffff;
		fFogNearDistance=5000.0f;
		fFogFarDistance=10000.0f;
	}

	// Always use shaders
	BeginTerrainShaderRender();


	SHADERMANAGER.SavePipelineState(PSTATE_BLENDENABLE, TRUE);

	bool bSavedAlphaTest = SHADERMANAGER.GetAlphaTestEnabled();
	SHADERMANAGER.SetAlphaTestEnabled(true);

	DWORD dwSavedParticleColor = SHADERMANAGER.GetParticleColor();
	SHADERMANAGER.SetParticleColor(dwFogColor);

	SHADERMANAGER.SetSamplerState(0, SAMPLER_ADDRESSU, ADDRESS_WRAP);
	SHADERMANAGER.SetSamplerState(0, SAMPLER_ADDRESSV, ADDRESS_WRAP);
	SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSU, ADDRESS_CLAMP);
	SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSV, ADDRESS_CLAMP);

#ifdef WORLD_EDITOR
	if (GetAsyncKeyState(VK_CAPITAL))
	{
		CSpeedTreeWrapper::ms_bSelfShadowOn = false;
	}
	else
	{
		CSpeedTreeWrapper::ms_bSelfShadowOn = true;
	}
#else
	CSpeedTreeWrapper::ms_bSelfShadowOn = true;
#endif
	SHADERMANAGER.SetBestFiltering(0);
	SHADERMANAGER.SetBestFiltering(1);

	m_matWorldForCommonUse._41 = 0.0f;
	m_matWorldForCommonUse._42 = 0.0f;
	SHADERMANAGER.SetMatrix(MATRIX_WORLD, &m_matWorldForCommonUse);

	SetShaderWorldMatrix(&m_matWorldForCommonUse);

	SHADERMANAGER.SaveTransform(MATRIX_TEXTURE0, &m_matWorldForCommonUse);
	SHADERMANAGER.SaveTransform(MATRIX_TEXTURE1, &m_matWorldForCommonUse);

	// Render State & TextureStageState
	//////////////////////////////////////////////////////////////////////////

	m_iRenderedSplatNumSqSum = 0;
	m_iRenderedPatchNum = 0;
	m_iRenderedSplatNum = 0;
	m_RenderedTextureNumVector.clear();

	std::pair<float, long> fog_far(fFogFarDistance+1600.0f, 0);
	std::pair<float, long> fog_near(fFogNearDistance-3200.0f, 0);

	std::vector<std::pair<float ,long> >::iterator far_it = std::upper_bound(m_PatchVector.begin(),m_PatchVector.end(),fog_far);
	std::vector<std::pair<float ,long> >::iterator near_it = std::upper_bound(m_PatchVector.begin(),m_PatchVector.end(),fog_near);

#ifdef WORLD_EDITOR
	near_it = m_PatchVector.begin();
	far_it = m_PatchVector.end();
#endif

	WORD wPrimitiveCount;
	EPrimitiveTopology ePrimitiveType;

	BYTE byCUrrentLODLevel = 0;

	float fLODLevel1Distance = __GetNoFogDistance();
	float fLODLevel2Distance = __GetFogDistance();

	{
		SelectIndexBuffer(0, &wPrimitiveCount, &ePrimitiveType);
	}

	bool bFogEnable = SHADERMANAGER.GetFogEnabled();
	std::vector<std::pair<float, long> >::iterator it = m_PatchVector.begin();

#ifdef WORLD_EDITOR
	SHADERMANAGER.SetFogEnabled(false);

	for( ; it != near_it; ++it)
	{
		{
			if (byCUrrentLODLevel == 0 && fLODLevel1Distance <= it->first)
			{
				byCUrrentLODLevel = 1;
				SelectIndexBuffer(1, &wPrimitiveCount, &ePrimitiveType);
			}
			else if (byCUrrentLODLevel == 1 && fLODLevel2Distance <= it->first)
			{
				byCUrrentLODLevel = 2;
				SelectIndexBuffer(2, &wPrimitiveCount, &ePrimitiveType);
			}
		}

		__HardwareTransformPatch_RenderPatchSplat(it->second, wPrimitiveCount, ePrimitiveType);
		if (m_iRenderedSplatNum >= m_iSplatLimit)
			break;

 		if (m_bDrawWireFrame)
			DrawWireFrame(it->second, wPrimitiveCount, ePrimitiveType);
	}

	SHADERMANAGER.SetFogEnabled(bFogEnable);

	if (m_iRenderedSplatNum < m_iSplatLimit)
	{
		for(it = near_it; it != far_it; ++it)
		{
			{
				if (byCUrrentLODLevel == 0 && fLODLevel1Distance <= it->first)
				{
					byCUrrentLODLevel = 1;
					SelectIndexBuffer(1, &wPrimitiveCount, &ePrimitiveType);
				}
				else if (byCUrrentLODLevel == 1 && fLODLevel2Distance <= it->first)
				{
					byCUrrentLODLevel = 2;
					SelectIndexBuffer(2, &wPrimitiveCount, &ePrimitiveType);
				}
			}

			__HardwareTransformPatch_RenderPatchSplat(it->second, wPrimitiveCount, ePrimitiveType);

			if (m_iRenderedSplatNum >= m_iSplatLimit)
				break;

			if (m_bDrawWireFrame)
				DrawWireFrame(it->second, wPrimitiveCount, ePrimitiveType);
		}
	}

#else
	{
		std::vector<SPatchRenderInfo> allPatches;
		allPatches.reserve(m_PatchVector.size());

		// Collect no-fog patches (before near_it)
		byCUrrentLODLevel = 0;
		for (it = m_PatchVector.begin(); it != near_it; ++it)
		{
			{
				if (byCUrrentLODLevel == 0 && fLODLevel1Distance <= it->first)
					byCUrrentLODLevel = 1;
				else if (byCUrrentLODLevel == 1 && fLODLevel2Distance <= it->first)
					byCUrrentLODLevel = 2;
			}

			SPatchRenderInfo info;
			info.patchNum = it->second;
			info.byLODLevel = byCUrrentLODLevel;
			info.bFogEnabled = false;
			allPatches.push_back(info);
		}

		// Collect fog-enabled patches (near_it to far_it)
		for (it = near_it; it != far_it; ++it)
		{
			{
				if (byCUrrentLODLevel == 0 && fLODLevel1Distance <= it->first)
					byCUrrentLODLevel = 1;
				else if (byCUrrentLODLevel == 1 && fLODLevel2Distance <= it->first)
					byCUrrentLODLevel = 2;
			}

			SPatchRenderInfo info;
			info.patchNum = it->second;
			info.byLODLevel = byCUrrentLODLevel;
			info.bFogEnabled = true;
			allPatches.push_back(info);
		}

		__HardwareTransformPatch_RenderBatchedByTexture(allPatches);

		// Shadow pass: set up state once, then render per-patch
		if (m_bDrawShadow && m_bDrawChrShadow && m_lpCharacterShadowMapTexture && !allPatches.empty())
		{
			SHADERMANAGER.SetLightingEnabled(true);
			SHADERMANAGER.SetFogColor(0xFFFFFFFF);
			SHADERMANAGER.SetPipelineState(PSTATE_SRCBLEND, BLEND_ZERO);
			SHADERMANAGER.SetPipelineState(PSTATE_DESTBLEND, BLEND_SRCCOLOR);

			SHADERMANAGER.SetDefaultTexture(0);
			SHADERMANAGER.SetSamplerState(0, SAMPLER_ADDRESSU, ADDRESS_CLAMP);
			SHADERMANAGER.SetSamplerState(0, SAMPLER_ADDRESSV, ADDRESS_CLAMP);

			SHADERMANAGER.SetMatrix(MATRIX_TEXTURE1, &m_matDynamicShadow);
			SHADERMANAGER.SetShaderResource(1, m_lpCharacterShadowMapTexture);
			SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSU, ADDRESS_CLAMP);
			SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSV, ADDRESS_CLAMP);

			Matrix matIdentity;
			MatrixIdentity(&matIdentity);
			SetTerrainShaderTextureTransforms(&matIdentity, &m_matDynamicShadow);

			SHADERMANAGER.SetMaterialParams(0.0f, 0.0f, 1.0f, 0.0f);

			SHADERMANAGER.SetShadowTextures(m_lpShadowMapSRV[3], m_lpShadowMapSRV[0]);
			SHADERMANAGER.SetShadowMidFarTextures(m_lpShadowMapSRV[1], m_lpShadowMapSRV[2]);

			// Use LOD 0 for shadow pass
			SelectIndexBuffer(0, &wPrimitiveCount, &ePrimitiveType);

			for (const auto& info : allPatches)
			{
				__RenderTerrainShadowPass(info.patchNum, wPrimitiveCount, ePrimitiveType);
			}

			// Restore state after shadow batch
			SHADERMANAGER.SetMaterialParams(0.0f, 0.0f, 0.0f, 0.0f);
			SHADERMANAGER.SetShaderResource(1, NULL);
			SHADERMANAGER.SetShadowTextures(nullptr, nullptr);
			SHADERMANAGER.SetShadowMidFarTextures(nullptr, nullptr);

			SHADERMANAGER.SetSamplerState(0, SAMPLER_ADDRESSU, ADDRESS_WRAP);
			SHADERMANAGER.SetSamplerState(0, SAMPLER_ADDRESSV, ADDRESS_WRAP);
			SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSU, ADDRESS_CLAMP);
			SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSV, ADDRESS_CLAMP);

			SHADERMANAGER.SetPipelineState(PSTATE_SRCBLEND, BLEND_SRCALPHA);
			SHADERMANAGER.SetPipelineState(PSTATE_DESTBLEND, BLEND_INVSRCALPHA);
			SHADERMANAGER.SetFogColor(dwFogColor);
			SHADERMANAGER.SetLightingEnabled(false);
		}

		// Wireframe debug
		if (m_bDrawWireFrame)
		{
			BYTE byWFLOD = 0;
			SelectIndexBuffer(0, &wPrimitiveCount, &ePrimitiveType);
			for (const auto& info : allPatches)
			{
				if (info.byLODLevel != byWFLOD)
				{
					byWFLOD = info.byLODLevel;
					SelectIndexBuffer(byWFLOD, &wPrimitiveCount, &ePrimitiveType);
				}
				DrawWireFrame(info.patchNum, wPrimitiveCount, ePrimitiveType);
			}
		}
	}
#endif

	SHADERMANAGER.SetFogEnabled(false);
	SHADERMANAGER.SetLightingEnabled(false);

	SHADERMANAGER.SetDefaultTexture(0);
	SHADERMANAGER.SetShaderResource(1, NULL);

	{
		float fr = ((dwFogColor >> 16) & 0xFF) / 255.0f;
		float fg = ((dwFogColor >> 8) & 0xFF) / 255.0f;
		float fb = (dwFogColor & 0xFF) / 255.0f;
		SHADERMANAGER.SetDiffuseColor(fr, fg, fb, 1.0f);
		SHADERMANAGER.CommitChanges();
	}

	SHADERMANAGER.SetFogEnabled(bFogEnable);
	SHADERMANAGER.SetLightingEnabled(true);

	std::sort(m_RenderedTextureNumVector.begin(),m_RenderedTextureNumVector.end());

	//////////////////////////////////////////////////////////////////////////
	// Restore Render State

	SHADERMANAGER.SetParticleColor(dwSavedParticleColor);

	SHADERMANAGER.RestoreTransform(MATRIX_TEXTURE0);
	SHADERMANAGER.RestoreTransform(MATRIX_TEXTURE1);

	SHADERMANAGER.RestorePipelineState(PSTATE_BLENDENABLE);
	SHADERMANAGER.SetAlphaTestEnabled(bSavedAlphaTest);

	EndTerrainShaderRender();

	// Render State & TextureStageState
	//////////////////////////////////////////////////////////////////////////
}

void CMapOutdoor::__HardwareTransformPatch_RenderPatchSplat(long patchnum, WORD wPrimitiveCount, EPrimitiveTopology ePrimitiveType)
{
	assert(NULL!=m_pTerrainPatchProxyList && "__HardwareTransformPatch_RenderPatchSplat");
	CTerrainPatchProxy * pTerrainPatchProxy = &m_pTerrainPatchProxyList[patchnum];

	if (!pTerrainPatchProxy->isUsed())
		return;

	long sPatchNum = pTerrainPatchProxy->GetPatchNum();
	if (sPatchNum < 0)
		return;

	BYTE ucTerrainNum = pTerrainPatchProxy->GetTerrainNum();
	if (0xFF == ucTerrainNum)
		return;

	CTerrain * pTerrain;
	if (!GetTerrainPointer(ucTerrainNum, &pTerrain))
		return;

	DWORD dwFogColor;
	if (mc_pEnvironmentData)
		dwFogColor=mc_pEnvironmentData->FogColor;
	else
		dwFogColor=0xffffffff;

	WORD wCoordX, wCoordY;
	pTerrain->GetCoordinate(&wCoordX, &wCoordY);

	TTerrainSplatPatch & rTerrainSplatPatch = pTerrain->GetTerrainSplatPatch();

	Matrix matTexTransform, matSplatAlphaTexTransform, matSplatColorTexTransform;
	m_matWorldForCommonUse._41 = -(float) (wCoordX * CTerrainImpl::TERRAIN_XSIZE);
	m_matWorldForCommonUse._42 = (float) (wCoordY * CTerrainImpl::TERRAIN_YSIZE);
	MatrixMultiply(&matTexTransform, &m_matViewInverse, &m_matWorldForCommonUse);
	MatrixMultiply(&matSplatAlphaTexTransform, &matTexTransform, &m_matSplatAlpha);
	SHADERMANAGER.SetMatrix(MATRIX_TEXTURE1, &matSplatAlphaTexTransform);

	Matrix matTiling;
	MatrixScaling(&matTiling, 1.0f/640.0f, -1.0f/640.0f, 0.0f);
	matTiling._41=0.0f;
	matTiling._42=0.0f;

	MatrixMultiply(&matSplatColorTexTransform, &m_matViewInverse, &matTiling);
	SHADERMANAGER.SetMatrix(MATRIX_TEXTURE0, &matSplatColorTexTransform);

	SHADERMANAGER.SetMaterialParams(1.0f/640.0f, -1.0f/640.0f, 0.0f, 0.0f);

	Matrix matWorldOffsetForShader;
	MatrixIdentity(&matWorldOffsetForShader);
	matWorldOffsetForShader._41 = -(float)(wCoordX * CTerrainImpl::TERRAIN_XSIZE);  // Offset X to tile-local
	matWorldOffsetForShader._42 = (float)(wCoordY * CTerrainImpl::TERRAIN_YSIZE);   // Offset Y (positive, because worldPos.y is already negative)

	Matrix matSplatAlphaForShader;
	MatrixMultiply(&matSplatAlphaForShader, &matWorldOffsetForShader, &m_matSplatAlpha);
	SetTerrainShaderTextureTransforms(&matSplatColorTexTransform, &matSplatAlphaForShader);

	CGraphicVertexBuffer* pkVB=pTerrainPatchProxy->HardwareTransformPatch_GetVertexBufferPtr();
	if (!pkVB)
		return;

	SHADERMANAGER.SetVertexBuffer(0, pkVB->GetD3DVertexBuffer(), m_iPatchTerrainVertexSize);

	SHADERMANAGER.SetLightingEnabled(true);

	int iPrevRenderedSplatNum=m_iRenderedSplatNum;

#ifdef WORLD_EDITOR

	int nRenderTextureCount = 0;

//	if (!m_bShowEntirePatchTextureCount && !(GetAsyncKeyState(VK_LCONTROL) & 0x8000) )
	if (1)
	{
		for (DWORD j = 1; j < pTerrain->GetNumTextures(); ++j)
		{
			TTerainSplat & rSplat = rTerrainSplatPatch.Splats[j];

			if (!rSplat.Active)
				continue;

			if (rTerrainSplatPatch.PatchTileCount[sPatchNum][j] == 0)
				continue;

			++nRenderTextureCount;
		}

		DWORD dwParticleColor = SHADERMANAGER.GetParticleColor();

		static int DefaultTCT = 8;
		int TextureCountThreshold = DefaultTCT;
		DWORD dwTFactor = 0xFFFFFFFF;

		if (GetAsyncKeyState(VK_LSHIFT) & 0x8000)
		{
			if (GetAsyncKeyState(VK_1) & 0x8000)
			{
				TextureCountThreshold = 2;
				dwTFactor = 0xFF0000FF;
			}
			else if (GetAsyncKeyState(VK_2) & 0x8000)
			{
				TextureCountThreshold = 3;
				dwTFactor = 0xFF00FF00;
			}
			else if (GetAsyncKeyState(VK_3) & 0x8000)
			{
				TextureCountThreshold = 4;
				dwTFactor = 0xFF00FFFF;
			}
			else if (GetAsyncKeyState(VK_4) & 0x8000)
			{
				TextureCountThreshold = 5;
				dwTFactor = 0xFFFF0000;
			}
			else if (GetAsyncKeyState(VK_5) & 0x8000)
			{
				TextureCountThreshold = 6;
				dwTFactor = 0xFFFFFF00;
			}
			else if (GetAsyncKeyState(VK_6) & 0x8000)
			{
				TextureCountThreshold = 7;
				dwTFactor = 0xFFFF00ff;
			}
			// new stuff
			else if (GetAsyncKeyState(VK_8) & 0x8000)
			{
				TextureCountThreshold = DefaultTCT = 8;
				dwTFactor = 0xFFFFFFFF;
			}
			else if (GetAsyncKeyState(VK_0) & 0x8000)
			{
				TextureCountThreshold = DefaultTCT = 255;
				dwTFactor = 0xFFFFFFFF;
			}

		}

		if (nRenderTextureCount>=TextureCountThreshold)
		{
			SHADERMANAGER.SetParticleColor( dwTFactor);
			SHADERMANAGER.DrawIndexed(ePrimitiveType, 0, m_iPatchTerrainVertexCount, 0, wPrimitiveCount);
			SHADERMANAGER.SetParticleColor( dwParticleColor);
		}
		else
		{
			if ( 0 < rTerrainSplatPatch.PatchTileCount[sPatchNum][0] )
			{
					DWORD dwParticleColorFor0Texture = SHADERMANAGER.GetParticleColor();
				SHADERMANAGER.SetParticleColor( 0xFF88FF88);
				SHADERMANAGER.DrawIndexed(ePrimitiveType, 0, m_iPatchTerrainVertexCount, 0, wPrimitiveCount);
				SHADERMANAGER.SetParticleColor( dwParticleColorFor0Texture);
			}

			for (DWORD j = 1; j < pTerrain->GetNumTextures(); ++j)
			{
				TTerainSplat & rSplat = rTerrainSplatPatch.Splats[j];

				if (!rSplat.Active)
					continue;

				DWORD dwTextureCount = rTerrainSplatPatch.PatchTileCount[sPatchNum][j];
				if (dwTextureCount == 0)
					continue;

				DWORD dwParticleColorForTextureBalance = 0xFFFFFFFF;

				if (!(GetAsyncKeyState(VK_LSHIFT) & 0x8000))
				{
					const TTerrainTexture & rTexture = m_TextureSet.GetTexture(j);

					MatrixMultiply(&matSplatColorTexTransform, &m_matViewInverse, &rTexture.m_matTransform);
					SHADERMANAGER.SetMatrix(MATRIX_TEXTURE0, &matSplatColorTexTransform);

					SHADERMANAGER.SetMaterialParams(rTexture.m_matTransform._11, rTexture.m_matTransform._22, 0.0f, 0.0f);

					SHADERMANAGER.SetShaderResource(0, rTexture.pd3dTexture);
					SHADERMANAGER.SetShaderResource(1, rSplat.pd3dTexture);
					SHADERMANAGER.DrawIndexed(ePrimitiveType, 0, m_iPatchTerrainVertexCount, 0, wPrimitiveCount);
				}
				else
				{
							if (dwTextureCount < 71)
					{
						dwParticleColorForTextureBalance = SHADERMANAGER.GetParticleColor();
						if (dwTextureCount < 51)
							SHADERMANAGER.SetParticleColor( 0xFFFF0000);
						else
							SHADERMANAGER.SetParticleColor( 0xFF0000FF);
						SHADERMANAGER.SetDefaultTexture(0);
					}
					else
					{
						const TTerrainTexture & rTexture = m_TextureSet.GetTexture(j);

						MatrixMultiply(&matSplatColorTexTransform, &m_matViewInverse, &rTexture.m_matTransform);
						SHADERMANAGER.SetMatrix(MATRIX_TEXTURE0, &matSplatColorTexTransform);

							SHADERMANAGER.SetMaterialParams(rTexture.m_matTransform._11, rTexture.m_matTransform._22, 0.0f, 0.0f);

						SHADERMANAGER.SetShaderResource(0, rTexture.pd3dTexture);
					}
					SHADERMANAGER.SetShaderResource(1, rSplat.pd3dTexture);
					SHADERMANAGER.DrawIndexed(ePrimitiveType, 0, m_iPatchTerrainVertexCount, 0, wPrimitiveCount);
					if (dwTextureCount < 71)
					{
						SHADERMANAGER.SetParticleColor( dwParticleColorForTextureBalance);
					}
				}

				std::vector<int>::iterator aIterator = std::find(m_RenderedTextureNumVector.begin(), m_RenderedTextureNumVector.end(), (int)j);
				if (aIterator == m_RenderedTextureNumVector.end())
					m_RenderedTextureNumVector.push_back(j);
				++m_iRenderedSplatNum;
				if (m_iRenderedSplatNum >= m_iSplatLimit)
					break;
			}
		}
	}
	else
	{
		int TextureCountThreshold = 6;
		DWORD dwTFactor = 0xFFFF00FF;

		if (GetAsyncKeyState(VK_LSHIFT) & 0x8000)
		{
			if (GetAsyncKeyState(VK_1) & 0x8000)
			{
				TextureCountThreshold = 1;
				dwTFactor = 0xFF0000FF;
			}
			else if (GetAsyncKeyState(VK_2) & 0x8000)
			{
				TextureCountThreshold = 2;
				dwTFactor = 0xFF00FF00;
			}
			else if (GetAsyncKeyState(VK_3) & 0x8000)
			{
				TextureCountThreshold = 3;
				dwTFactor = 0xFF00FFFF;
			}
			else if (GetAsyncKeyState(VK_4) & 0x8000)
			{
				TextureCountThreshold = 4;
				dwTFactor = 0xFFFF0000;
			}
			else if (GetAsyncKeyState(VK_5) & 0x8000)
			{
				TextureCountThreshold = 5;
				dwTFactor = 0xFFFFFF00;
			}
		}

		for (DWORD j = 1; j < pTerrain->GetNumTextures(); ++j)
		{
			TTerainSplat & rSplat = rTerrainSplatPatch.Splats[j];

			if (!rSplat.Active)
				continue;

			if (rTerrainSplatPatch.PatchTileCount[sPatchNum][j] == 0)
				continue;

			DWORD dwParticleColor;

			if (nRenderTextureCount>=TextureCountThreshold)
			{
					dwParticleColor = SHADERMANAGER.GetParticleColor();
				SHADERMANAGER.SetParticleColor( dwTFactor);
				SHADERMANAGER.SetDefaultTexture(0);
			}
			else
			{
				const TTerrainTexture & rTexture = m_TextureSet.GetTexture(j);

				MatrixMultiply(&matSplatColorTexTransform, &m_matViewInverse, &rTexture.m_matTransform);
				SHADERMANAGER.SetMatrix(MATRIX_TEXTURE0, &matSplatColorTexTransform);

						SHADERMANAGER.SetMaterialParams(rTexture.m_matTransform._11, rTexture.m_matTransform._22, 0.0f, 0.0f);

				SHADERMANAGER.SetShaderResource(0, rTexture.pd3dTexture);
			}
			SHADERMANAGER.SetShaderResource(1, rSplat.pd3dTexture);
			SHADERMANAGER.DrawIndexed(ePrimitiveType, 0, m_iPatchTerrainVertexCount, 0, wPrimitiveCount);
			if (nRenderTextureCount>=TextureCountThreshold)
			{
				SHADERMANAGER.SetParticleColor( dwParticleColor);
			}

			++nRenderTextureCount;

			std::vector<int>::iterator aIterator = std::find(m_RenderedTextureNumVector.begin(), m_RenderedTextureNumVector.end(), (int)j);
			if (aIterator == m_RenderedTextureNumVector.end())
				m_RenderedTextureNumVector.push_back(j);
			++m_iRenderedSplatNum;
			if (m_iRenderedSplatNum >= m_iSplatLimit)
				break;

		}
	}

#else

	bool isFirst=true;
	for (DWORD j = 1; j < pTerrain->GetNumTextures(); ++j)
	{
		TTerainSplat & rSplat = rTerrainSplatPatch.Splats[j];

		if (!rSplat.Active)
			continue;

		if (rTerrainSplatPatch.PatchTileCount[sPatchNum][j] == 0)
			continue;

		const TTerrainTexture & rTexture = m_TextureSet.GetTexture(j);

		MatrixMultiply(&matSplatColorTexTransform, &m_matViewInverse, &rTexture.m_matTransform);
		SHADERMANAGER.SetMatrix(MATRIX_TEXTURE0, &matSplatColorTexTransform);

		SHADERMANAGER.SetMaterialParams(rTexture.m_matTransform._11, rTexture.m_matTransform._22, 0.0f, isFirst ? 1.0f : 0.0f);

		SetTerrainShaderTextureTransforms(&matSplatColorTexTransform, NULL);

		SHADERMANAGER.SetShaderResource(0, rTexture.pd3dTexture);
		SHADERMANAGER.SetShaderResource(1, rSplat.pd3dTexture);
		SHADERMANAGER.DrawIndexed(ePrimitiveType, 0, m_iPatchTerrainVertexCount, 0, wPrimitiveCount);
		isFirst = false;

		std::vector<int>::iterator aIterator = std::find(m_RenderedTextureNumVector.begin(), m_RenderedTextureNumVector.end(), (int)j);
		if (aIterator == m_RenderedTextureNumVector.end())
			m_RenderedTextureNumVector.push_back(j);
		++m_iRenderedSplatNum;
		if (m_iRenderedSplatNum >= m_iSplatLimit)
			break;

	}

#endif

#ifdef WORLD_EDITOR
	if (m_bDrawShadow && m_bDrawChrShadow && m_lpCharacterShadowMapTexture)
	{
		SHADERMANAGER.SetLightingEnabled(true);

		SHADERMANAGER.SetFogColor(0xFFFFFFFF);
		SHADERMANAGER.SetPipelineState(PSTATE_SRCBLEND, BLEND_ZERO);
		SHADERMANAGER.SetPipelineState(PSTATE_DESTBLEND, BLEND_SRCCOLOR);

		SHADERMANAGER.SetDefaultTexture(0);
		SHADERMANAGER.SetSamplerState(0, SAMPLER_ADDRESSU, ADDRESS_CLAMP);
		SHADERMANAGER.SetSamplerState(0, SAMPLER_ADDRESSV, ADDRESS_CLAMP);

		SHADERMANAGER.SetMatrix(MATRIX_TEXTURE1, &m_matDynamicShadow);
		SHADERMANAGER.SetShaderResource(1, m_lpCharacterShadowMapTexture);
		SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSU, ADDRESS_CLAMP);
		SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSV, ADDRESS_CLAMP);

		Matrix matIdentity;
		MatrixIdentity(&matIdentity);
		SetTerrainShaderTextureTransforms(&matIdentity, &m_matDynamicShadow);

		SHADERMANAGER.SetMaterialParams(0.0f, 0.0f, 1.0f, 0.0f);

		SHADERMANAGER.SetShadowTextures(m_lpShadowMapSRV[3], m_lpShadowMapSRV[0]);
			SHADERMANAGER.SetShadowMidFarTextures(m_lpShadowMapSRV[1], m_lpShadowMapSRV[2]);

		SHADERMANAGER.CommitChanges();

		ms_faceCount += wPrimitiveCount;
		SHADERMANAGER.DrawIndexed(ePrimitiveType, 0, m_iPatchTerrainVertexCount, 0, wPrimitiveCount);
		++m_iRenderedSplatNum;

		SHADERMANAGER.SetMaterialParams(0.0f, 0.0f, 0.0f, 0.0f);
		SHADERMANAGER.SetShaderResource(1, NULL);
		SHADERMANAGER.SetShadowTextures(nullptr, nullptr);
			SHADERMANAGER.SetShadowMidFarTextures(nullptr, nullptr);

		SHADERMANAGER.SetSamplerState(0, SAMPLER_ADDRESSU, ADDRESS_WRAP);
		SHADERMANAGER.SetSamplerState(0, SAMPLER_ADDRESSV, ADDRESS_WRAP);
		SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSU, ADDRESS_CLAMP);
		SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSV, ADDRESS_CLAMP);

		SHADERMANAGER.SetPipelineState(PSTATE_SRCBLEND, BLEND_SRCALPHA);
		SHADERMANAGER.SetPipelineState(PSTATE_DESTBLEND, BLEND_INVSRCALPHA);
		SHADERMANAGER.SetFogColor(dwFogColor);

		SHADERMANAGER.SetLightingEnabled(false);
	}
#endif
	++m_iRenderedPatchNum;

	int iCurRenderedSplatNum=m_iRenderedSplatNum-iPrevRenderedSplatNum;

	m_iRenderedSplatNumSqSum+=iCurRenderedSplatNum*iCurRenderedSplatNum;
}

void CMapOutdoor::__HardwareTransformPatch_RenderPatchNone(long patchnum, WORD wPrimitiveCount, EPrimitiveTopology ePrimitiveType)
{
	assert(NULL!=m_pTerrainPatchProxyList && "__HardwareTransformPatch_RenderPatchNone");
	CTerrainPatchProxy * pTerrainPatchProxy = &m_pTerrainPatchProxyList[patchnum];

	if (!pTerrainPatchProxy->isUsed())
		return;

	CGraphicVertexBuffer* pkVB=pTerrainPatchProxy->HardwareTransformPatch_GetVertexBufferPtr();
	if (!pkVB)
		return;

	SHADERMANAGER.SetVertexBuffer(0, pkVB->GetD3DVertexBuffer(), m_iPatchTerrainVertexSize);
	SHADERMANAGER.DrawIndexed(ePrimitiveType, 0, m_iPatchTerrainVertexCount, 0, wPrimitiveCount);
}

void CMapOutdoor::__RenderTerrainShadowPass(long patchnum, WORD wPrimitiveCount, EPrimitiveTopology ePrimitiveType)
{
	if (!m_pTerrainPatchProxyList)
		return;

	CTerrainPatchProxy* pProxy = &m_pTerrainPatchProxyList[patchnum];
	if (!pProxy->isUsed())
		return;

	CGraphicVertexBuffer* pkVB = pProxy->HardwareTransformPatch_GetVertexBufferPtr();
	if (!pkVB)
		return;

	SHADERMANAGER.SetVertexBuffer(0, pkVB->GetD3DVertexBuffer(), m_iPatchTerrainVertexSize);
	SHADERMANAGER.CommitChanges();

	ms_faceCount += wPrimitiveCount;
	SHADERMANAGER.DrawIndexed(ePrimitiveType, 0, m_iPatchTerrainVertexCount, 0, wPrimitiveCount);
	++m_iRenderedSplatNum;
}

void CMapOutdoor::__HardwareTransformPatch_RenderBatchedByTexture(
	const std::vector<SPatchRenderInfo>& patches)
{
	// Extended texture/patch pair with LOD and fog info
	struct STexturePatchPair
	{
		DWORD dwTextureIdx;
		long  patchNum;
		ID3D11ShaderResourceView* pColorTex;
		ID3D11ShaderResourceView* pSplatTex;
		Matrix matColorTransform;
		float fTileScaleU;
		float fTileScaleV;
		BYTE  byLODLevel;
		bool  bFogEnabled;
	};

	std::vector<STexturePatchPair> texPatchPairs;
	texPatchPairs.reserve(patches.size() * 4);  // ~4 textures per patch average

	for (const auto& patchInfo : patches)
	{
		long patchnum = patchInfo.patchNum;
		if (!m_pTerrainPatchProxyList)
			continue;

		CTerrainPatchProxy* pProxy = &m_pTerrainPatchProxyList[patchnum];
		if (!pProxy->isUsed())
			continue;

		long sPatchNum = pProxy->GetPatchNum();
		if (sPatchNum < 0)
			continue;

		BYTE ucTerrainNum = pProxy->GetTerrainNum();
		if (ucTerrainNum == 0xFF)
			continue;

		CTerrain* pTerrain;
		if (!GetTerrainPointer(ucTerrainNum, &pTerrain))
			continue;

		TTerrainSplatPatch& rSplatPatch = pTerrain->GetTerrainSplatPatch();

		for (DWORD j = 1; j < pTerrain->GetNumTextures(); ++j)
		{
			TTerainSplat& rSplat = rSplatPatch.Splats[j];
			if (!rSplat.Active)
				continue;
			if (rSplatPatch.PatchTileCount[sPatchNum][j] == 0)
				continue;

			const TTerrainTexture& rTexture = m_TextureSet.GetTexture(j);

			STexturePatchPair pair;
			pair.dwTextureIdx = j;
			pair.patchNum = patchnum;
			pair.pColorTex = rTexture.pd3dTexture;
			pair.pSplatTex = rSplat.pd3dTexture;
			pair.fTileScaleU = rTexture.m_matTransform._11;
			pair.fTileScaleV = rTexture.m_matTransform._22;
			pair.byLODLevel = patchInfo.byLODLevel;
			pair.bFogEnabled = patchInfo.bFogEnabled;

			MatrixMultiply(&pair.matColorTransform, &m_matViewInverse, &rTexture.m_matTransform);

			texPatchPairs.push_back(pair);
		}
	}

	std::unordered_map<long, DWORD> firstTexForPatch;
	firstTexForPatch.reserve(texPatchPairs.size());
	for (const auto& p : texPatchPairs)
	{
		auto it = firstTexForPatch.find(p.patchNum);
		if (it == firstTexForPatch.end() || p.dwTextureIdx < it->second)
			firstTexForPatch[p.patchNum] = p.dwTextureIdx;
	}

	std::sort(texPatchPairs.begin(), texPatchPairs.end(),
		[&firstTexForPatch](const STexturePatchPair& a, const STexturePatchPair& b) {
			bool aFirst = (firstTexForPatch[a.patchNum] == a.dwTextureIdx);
			bool bFirst = (firstTexForPatch[b.patchNum] == b.dwTextureIdx);
			if (aFirst != bFirst)
				return aFirst;  // first-pass pairs before secondary pairs
			if (a.bFogEnabled != b.bFogEnabled)
				return !a.bFogEnabled;  // fog-off before fog-on
			if (a.byLODLevel != b.byLODLevel)
				return a.byLODLevel < b.byLODLevel;
			return a.dwTextureIdx < b.dwTextureIdx;
		});

	bool bFogEnable = SHADERMANAGER.GetFogEnabled();
	bool bCurrentFog = false;  // Start with fog off (first group is no-fog)
	SHADERMANAGER.SetFogEnabled(false);

	BYTE byCurrentLOD = 0;
	WORD wPrimitiveCount;
	EPrimitiveTopology ePrimitiveType;
	SelectIndexBuffer(0, &wPrimitiveCount, &ePrimitiveType);

	ID3D11ShaderResourceView* pLastColorTex = nullptr;
	ID3D11ShaderResourceView* pLastSplatTex = nullptr;

	SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, FALSE);
	bool bBlendOn = false;

	for (const auto& pair : texPatchPairs)
	{
		// Switch fog state when it changes
		if (pair.bFogEnabled != bCurrentFog)
		{
			bCurrentFog = pair.bFogEnabled;
			SHADERMANAGER.SetFogEnabled(bCurrentFog ? bFogEnable : false);
		}

		// Switch LOD when it changes
		if (pair.byLODLevel != byCurrentLOD)
		{
			byCurrentLOD = pair.byLODLevel;
			SelectIndexBuffer(byCurrentLOD, &wPrimitiveCount, &ePrimitiveType);
		}

		auto itFirstTex = firstTexForPatch.find(pair.patchNum);
		bool bIsFirstForPatch = (itFirstTex != firstTexForPatch.end()
			&& pair.dwTextureIdx == itFirstTex->second);

		// Toggle alpha blend based on first-pass vs secondary pair
		if (!bIsFirstForPatch && !bBlendOn)
		{
			SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, TRUE);
			bBlendOn = true;
		}

		// Only rebind the color texture when it changes
		if (pair.pColorTex != pLastColorTex)
		{
			SHADERMANAGER.SetShaderResource(0, pair.pColorTex);
			SHADERMANAGER.SetMatrix(MATRIX_TEXTURE0, &pair.matColorTransform);
			SHADERMANAGER.SetMaterialParams(pair.fTileScaleU, pair.fTileScaleV, 0.0f, 0.0f);
			SetTerrainShaderTextureTransforms(&pair.matColorTransform, NULL);
			pLastColorTex = pair.pColorTex;
		}

		if (pair.pSplatTex != pLastSplatTex)
		{
			SHADERMANAGER.SetShaderResource(1, pair.pSplatTex);
			pLastSplatTex = pair.pSplatTex;
		}

		// Bind this patch's vertex buffer and draw
		CTerrainPatchProxy* pProxy = &m_pTerrainPatchProxyList[pair.patchNum];
		CGraphicVertexBuffer* pkVB = pProxy->HardwareTransformPatch_GetVertexBufferPtr();
		if (!pkVB)
			continue;

		// Set up per-patch splat alpha transform
		BYTE ucTerrainNum = pProxy->GetTerrainNum();
		CTerrain* pTerrain;
		if (GetTerrainPointer(ucTerrainNum, &pTerrain))
		{
			WORD wCoordX, wCoordY;
			pTerrain->GetCoordinate(&wCoordX, &wCoordY);

			Matrix matWorldOffsetForShader;
			MatrixIdentity(&matWorldOffsetForShader);
			matWorldOffsetForShader._41 = -(float)(wCoordX * CTerrainImpl::TERRAIN_XSIZE);
			matWorldOffsetForShader._42 = (float)(wCoordY * CTerrainImpl::TERRAIN_YSIZE);

			Matrix matSplatAlphaForShader;
			MatrixMultiply(&matSplatAlphaForShader, &matWorldOffsetForShader, &m_matSplatAlpha);
			SHADERMANAGER.SetMatrix(MATRIX_TEXTURE1, &matSplatAlphaForShader);
			SetTerrainShaderTextureTransforms(NULL, &matSplatAlphaForShader);
		}

		SHADERMANAGER.SetVertexBuffer(0, pkVB->GetD3DVertexBuffer(), m_iPatchTerrainVertexSize);
		SHADERMANAGER.SetLightingEnabled(true);
		SHADERMANAGER.DrawIndexed(ePrimitiveType, 0, m_iPatchTerrainVertexCount, 0, wPrimitiveCount);

		++m_iRenderedPatchNum;
		if (!bIsFirstForPatch)
		{
			++m_iRenderedSplatNum;
			if (m_iRenderedSplatNum >= m_iSplatLimit)
				break;
		}
	}

	// Restore fog state after terrain patch rendering
	SHADERMANAGER.SetFogEnabled(bFogEnable);

	if (!bBlendOn)
		SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, TRUE);
}
