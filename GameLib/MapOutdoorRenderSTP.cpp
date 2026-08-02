#include "StdAfx.h"
#include "MapOutdoor.h"
#include "TerrainPatch.h"
#include "TerrainQuadtree.h"

#include "../eterlib/Camera.h"
#include "../eterlib/ShaderManager.h"

struct SoftwareTransformPatch_SSplatVertex
{
	Vector4 kPosition;
	DWORD		dwDiffuse;
	DWORD		dwSpecular;
	Vector2 kTex1;
	Vector2 kTex2;
};

void CMapOutdoor::__RenderTerrain_RenderSoftwareTransformPatch()
{
	SoftwareTransformPatch_SRenderState kTPRS;

	bool bFogEnable = SHADERMANAGER.GetFogEnabled();

	__SoftwareTransformPatch_ApplyRenderState();

	__SoftwareTransformPatch_BuildPipeline(kTPRS);

	std::pair<float, long> fog_far(kTPRS.m_fFogFarDistance+800.0f, 0);
	std::pair<float, long> fog_near(kTPRS.m_fFogNearDistance-3200.0f, 0);

	std::vector<std::pair<float ,long> >::iterator far_it = std::upper_bound(m_PatchVector.begin(),m_PatchVector.end(),fog_far);
	std::vector<std::pair<float ,long> >::iterator near_it = std::upper_bound(m_PatchVector.begin(),m_PatchVector.end(),fog_near);

	WORD wPrimitiveCount;
	EPrimitiveTopology ePrimitiveType;

	BYTE byCUrrentLODLevel = 0;

	float fLODLevel1Distance = __GetNoFogDistance();
	float fLODLevel2Distance = __GetFogDistance();

	SelectIndexBuffer(0, &wPrimitiveCount, &ePrimitiveType);

	SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_TRANSFORMED);

	std::vector<std::pair<float, long> >::iterator it = m_PatchVector.begin();

	for( ; it != near_it; ++it)
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

		__SoftwareTransformPatch_RenderPatchSplat(kTPRS, it->second, wPrimitiveCount, ePrimitiveType, false);
		if (m_iRenderedSplatNum >= m_iSplatLimit)
			break;

	}

	if (m_iRenderedSplatNum < m_iSplatLimit)
	{
		for(it = near_it; it != far_it; ++it)
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

			__SoftwareTransformPatch_RenderPatchSplat(kTPRS, it->second, wPrimitiveCount, ePrimitiveType, true);

			if (m_iRenderedSplatNum >= m_iSplatLimit)
				break;

		}
	}

	SHADERMANAGER.SetDefaultTexture(0);
	SHADERMANAGER.SetShaderResource(1, NULL);


	SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_TRANSFORMED);

	if (byCUrrentLODLevel != 2)
	{
		byCUrrentLODLevel = 2;
		SelectIndexBuffer(2, &wPrimitiveCount, &ePrimitiveType);
	}

	if (m_iRenderedSplatNum < m_iSplatLimit)
	{
		for(it = far_it; it != m_PatchVector.end(); ++it)
		{
			__SoftwareTransformPatch_RenderPatchNone(kTPRS, it->second, wPrimitiveCount, ePrimitiveType);

			if (m_iRenderedSplatNum >= m_iSplatLimit)
				break;

		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Render State & TextureStageState
	__SoftwareTransformPatch_RestoreRenderState(bFogEnable);
}

void CMapOutdoor::__SoftwareTransformPatch_RenderPatchSplat(SoftwareTransformPatch_SRenderState& rkTPRS, long patchnum, WORD wPrimitiveCount, EPrimitiveTopology ePrimitiveType, bool isFogEnable)
{
	assert(NULL!=m_pTerrainPatchProxyList && "CMapOutdoor::__SoftwareTransformPatch_RenderPatchSplat");

	CTerrainPatchProxy * pTerrainPatchProxy = &m_pTerrainPatchProxyList[patchnum];

	if (!pTerrainPatchProxy->isUsed())
		return;

	bool isDynamicShadow = pTerrainPatchProxy->IsIn(rkTPRS.m_v3Player, 3000.0f);

	if (!m_bDrawChrShadow)
		isDynamicShadow = false;

	long sPatchNum = pTerrainPatchProxy->GetPatchNum();

	if (sPatchNum < 0)
		return;

	BYTE ucTerrainNum = pTerrainPatchProxy->GetTerrainNum();

	if (0xFF == ucTerrainNum)
		return;

	CTerrain * pTerrain;
	if (!GetTerrainPointer(ucTerrainNum, &pTerrain))
		return;

	WORD wCoordX, wCoordY;
	pTerrain->GetCoordinate(&wCoordX, &wCoordY);

	TTerrainSplatPatch & rTerrainSplatPatch = pTerrain->GetTerrainSplatPatch();

	SoftwareTransformPatch_STLVertex akTransVertex[CTerrainPatch::TERRAIN_VERTEX_COUNT];
	if (!__SoftwareTransformPatch_SetTransform(rkTPRS, akTransVertex, *pTerrainPatchProxy, wCoordX, wCoordY, isFogEnable, isDynamicShadow))
		return;

	if (!__SoftwareTransformPatch_SetSplatStream(akTransVertex))
		return;

	// isFogEnable determines blending behavior in the shader

	int iPrevRenderedSplatNum=m_iRenderedSplatNum;

	bool isFirst=true;
	for (DWORD j = 1; j < pTerrain->GetNumTextures(); ++j)
	{
		TTerainSplat & rSplat = rTerrainSplatPatch.Splats[j];

		if (!rSplat.Active)
			continue;

		if (rTerrainSplatPatch.PatchTileCount[sPatchNum][j] == 0)
			continue;

		const TTerrainTexture & rTexture = m_TextureSet.GetTexture(j);

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

	// Terrain shadow rendering
	if (m_bDrawShadow)
	{
		__SoftwareTransformPatch_SetShadowStream(akTransVertex);
		__SoftwareTransformPatch_ApplyStaticShadowRenderState();

		if (isDynamicShadow)
			__SoftwareTransformPatch_ApplyDynamicShadowRenderState();
		else
			__SoftwareTransformPatch_ApplyFogShadowRenderState();

		if (isFogEnable)
		{
			SHADERMANAGER.SetFogEnabled(true);
			SHADERMANAGER.SetFogColor(0xFFFFFFFF);
			SHADERMANAGER.SetDefaultTexture(0);
			SHADERMANAGER.DrawIndexed(ePrimitiveType, 0, m_iPatchTerrainVertexCount, 0, wPrimitiveCount);
			SHADERMANAGER.SetFogColor(rkTPRS.m_dwFogColor);
			SHADERMANAGER.SetFogEnabled(false);
		}
		else
		{
			SHADERMANAGER.SetDefaultTexture(0);
			SHADERMANAGER.DrawIndexed(ePrimitiveType, 0, m_iPatchTerrainVertexCount, 0, wPrimitiveCount);
		}

		if (isDynamicShadow)
			__SoftwareTransformPatch_RestoreDynamicShadowRenderState();
		else
			__SoftwareTransformPatch_RestoreFogShadowRenderState();

		ms_faceCount += wPrimitiveCount;
  		++m_iRenderedSplatNum;

		__SoftwareTransformPatch_RestoreStaticShadowRenderState();
	}

	++m_iRenderedPatchNum;

	int iCurRenderedSplatNum=m_iRenderedSplatNum-iPrevRenderedSplatNum;

	m_iRenderedSplatNumSqSum+=iCurRenderedSplatNum*iCurRenderedSplatNum;
}

void CMapOutdoor::__SoftwareTransformPatch_RenderPatchNone(SoftwareTransformPatch_SRenderState& rkTPRS, long patchnum, WORD wPrimitiveCount, EPrimitiveTopology ePrimitiveType)
{
	assert(NULL != m_pTerrainPatchProxyList && "CMapOutdoor::__SoftwareTransformPatch_RenderPatchNone");

	CTerrainPatchProxy* pTerrainPatchProxy = &m_pTerrainPatchProxyList[patchnum];

	if (!pTerrainPatchProxy->isUsed())
		return;

	long sPatchNum = pTerrainPatchProxy->GetPatchNum();
	if (sPatchNum < 0)
		return;

	BYTE ucTerrainNum = pTerrainPatchProxy->GetTerrainNum();
	if (0xFF == ucTerrainNum)
		return;

	CTerrain* pTerrain;
	if (!GetTerrainPointer(ucTerrainNum, &pTerrain))
		return;

	WORD wCoordX, wCoordY;
	pTerrain->GetCoordinate(&wCoordX, &wCoordY);

	SoftwareTransformPatch_SSourceVertex* akSrcVertex = pTerrainPatchProxy->SoftwareTransformPatch_GetTerrainVertexDataPtr();
	if (!akSrcVertex)
		return;

	float fScreenHalfWidth = rkTPRS.m_fScreenHalfWidth;
	float fScreenHalfHeight = rkTPRS.m_fScreenHalfHeight;

	Matrix m4Frustum = rkTPRS.m_m4Frustum;

	SoftwareTransformPatch_STVertex akTransVertex[CTerrainPatch::TERRAIN_VERTEX_COUNT];

	Vector4* akPosition = (Vector4*)akTransVertex;
	Vector4* pkPosition;
	for (UINT uIndex = 0; uIndex != CTerrainPatch::TERRAIN_VERTEX_COUNT; ++uIndex)
	{
		pkPosition = akPosition + uIndex;
		Vec3Transform(pkPosition, &akSrcVertex[uIndex].kPosition, &m4Frustum);
		pkPosition->w = 1.0f / pkPosition->w;
		pkPosition->z *= pkPosition->w;
		pkPosition->y = (pkPosition->y * pkPosition->w - 1.0f) * fScreenHalfHeight;
		pkPosition->x = (pkPosition->x * pkPosition->w + 1.0f) * fScreenHalfWidth;
	}


	ID3D11Buffer* pkVB = m_kSTPD.m_pkVBNone[m_kSTPD.m_dwNonePos++];
	m_kSTPD.m_dwNonePos %= SoftwareTransformPatch_SData::NONE_VB_NUM;
	if (!pkVB)
		return;

	DWORD dwVBSize = sizeof(SoftwareTransformPatch_STVertex) * CTerrainPatch::TERRAIN_VERTEX_COUNT;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	if (FAILED(ms_pContext->Map(pkVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
		return;

	memcpy(mappedResource.pData, akTransVertex, dwVBSize);

	ms_pContext->Unmap(pkVB, 0);

	SHADERMANAGER.SetVertexBuffer(0, pkVB, sizeof(SoftwareTransformPatch_STVertex));
	SHADERMANAGER.DrawIndexed(ePrimitiveType, 0, m_iPatchTerrainVertexCount, 0, wPrimitiveCount);
	ms_faceCount += wPrimitiveCount;
}

void CMapOutdoor::__SoftwareTransformPatch_ApplyStaticShadowRenderState()
{
	SHADERMANAGER.SetPipelineState(PSTATE_SRCBLEND, BLEND_ZERO);
	SHADERMANAGER.SetPipelineState(PSTATE_DESTBLEND, BLEND_SRCCOLOR);
}

void CMapOutdoor::__SoftwareTransformPatch_ApplyDynamicShadowRenderState()
{
 	SHADERMANAGER.SetShaderResource(1, m_lpCharacterShadowMapTexture);
	SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSU, ADDRESS_CLAMP);
	SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSV, ADDRESS_CLAMP);
}

void CMapOutdoor::__SoftwareTransformPatch_ApplyFogShadowRenderState()
{
	SHADERMANAGER.SetShaderResource(1, NULL);
}
void CMapOutdoor::__SoftwareTransformPatch_RestoreStaticShadowRenderState()
{
	SHADERMANAGER.SetSamplerState(0, SAMPLER_ADDRESSU, ADDRESS_WRAP);
	SHADERMANAGER.SetSamplerState(0, SAMPLER_ADDRESSV, ADDRESS_WRAP);

	SHADERMANAGER.SetPipelineState(PSTATE_SRCBLEND, BLEND_SRCALPHA);
	SHADERMANAGER.SetPipelineState(PSTATE_DESTBLEND, BLEND_INVSRCALPHA);
}

void CMapOutdoor::__SoftwareTransformPatch_RestoreDynamicShadowRenderState()
{
	SHADERMANAGER.SetShaderResource(1, NULL);
	SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSU, ADDRESS_CLAMP);
	SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSV, ADDRESS_CLAMP);
}

void CMapOutdoor::__SoftwareTransformPatch_RestoreFogShadowRenderState()
{
	SHADERMANAGER.SetFogEnabled(false);
	SHADERMANAGER.SetShaderResource(1, NULL);
}

void CMapOutdoor::__SoftwareTransformPatch_ApplyRenderState()
{
	DWORD dwFogColor=0xffffffff;
	if (mc_pEnvironmentData)
		dwFogColor=mc_pEnvironmentData->FogColor;

	BOOL isSoftwareVertexClipping=FALSE;
	if (!IsTLVertexClipping())
		isSoftwareVertexClipping=TRUE;

	SHADERMANAGER.SavePipelineState(PSTATE_BLENDENABLE, TRUE);

	bool bSavedAlphaTest = SHADERMANAGER.GetAlphaTestEnabled();
	SHADERMANAGER.SetAlphaTestEnabled(true);

	DWORD dwSavedTextureFactor = SHADERMANAGER.GetTextureFactor();
	SHADERMANAGER.SetTextureFactor(dwFogColor);
	SHADERMANAGER.SetLightingEnabled(false);
	SHADERMANAGER.SetFogEnabled(false);

	SHADERMANAGER.SetSamplerState(0, SAMPLER_ADDRESSU, ADDRESS_WRAP);
	SHADERMANAGER.SetSamplerState(0, SAMPLER_ADDRESSV, ADDRESS_WRAP);
	SHADERMANAGER.SetBestFiltering(0);

	SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSU, ADDRESS_CLAMP);
	SHADERMANAGER.SetSamplerState(1, SAMPLER_ADDRESSV, ADDRESS_CLAMP);
	SHADERMANAGER.SetBestFiltering(1);

	CSpeedTreeWrapper::ms_bSelfShadowOn = true;

	// Render State
	//////////////////////////////////////////////////////////////////////////

	m_iRenderedSplatNumSqSum = 0;
	m_iRenderedPatchNum = 0;
	m_iRenderedSplatNum = 0;
	m_RenderedTextureNumVector.clear();

	m_matWorldForCommonUse._41 = 0.0f;
	m_matWorldForCommonUse._42 = 0.0f;
	SHADERMANAGER.SetMatrix(MATRIX_WORLD, &m_matWorldForCommonUse);
}

void CMapOutdoor::__SoftwareTransformPatch_RestoreRenderState(bool bFogEnable)
{
	SHADERMANAGER.SetLightingEnabled(true);


	SHADERMANAGER.SetFogEnabled(bFogEnable);

	std::sort(m_RenderedTextureNumVector.begin(),m_RenderedTextureNumVector.end());


	SHADERMANAGER.RestorePipelineState(PSTATE_BLENDENABLE);

	// Render State
	//////////////////////////////////////////////////////////////////////////
}

void CMapOutdoor::__SoftwareTransformPatch_BuildPipeline(SoftwareTransformPatch_SRenderState& rkTPRS)
{
	memset(&rkTPRS, 0, sizeof(rkTPRS));

	if (mc_pEnvironmentData)
	{
		rkTPRS.m_dwFogColor = mc_pEnvironmentData->FogColor;
		rkTPRS.m_fFogNearDistance = mc_pEnvironmentData->GetFogNearDistance();
		rkTPRS.m_fFogFarDistance = mc_pEnvironmentData->GetFogFarDistance();
	}
	else
	{
		rkTPRS.m_dwFogColor = 0xffffffff;
		rkTPRS.m_fFogNearDistance = 5000.0f;
		rkTPRS.m_fFogFarDistance = 10000.0f;
	}

	UINT uScreenWidth;
	UINT uScreenHeight;
	CScreen::GetBackBufferSize(&uScreenWidth, &uScreenHeight);

	rkTPRS.m_fScreenHalfWidth = +float(uScreenWidth) / 2.0f;
	rkTPRS.m_fScreenHalfHeight = -float(uScreenHeight) / 2.0f;

	SHADERMANAGER.GetLight(0, &rkTPRS.m_kLight);
	SHADERMANAGER.GetMaterial(&rkTPRS.m_kMtrl);

	Matrix m4View; SHADERMANAGER.GetMatrix(MATRIX_VIEW, &m4View);
	Matrix m4Proj; SHADERMANAGER.GetMatrix(MATRIX_PROJECTION, &m4Proj);

	MatrixMultiply(&rkTPRS.m_m4Frustum, &m4View, &m4Proj);

	rkTPRS.m_v3Player.x = +m_v3Player.x;
	rkTPRS.m_v3Player.y = -m_v3Player.y;
	rkTPRS.m_v3Player.z = +m_v3Player.z;

	rkTPRS.m_m4Proj = m4Proj;
	rkTPRS.m_m4DynamicShadow = m_matLightView * m_matDynamicShadowScale;


	Vector3 kFogNearVector;
	Vector3 kFogNearVector2;
	Vector3(0.0f, 0.0f, -rkTPRS.m_fFogNearDistance) = kFogNearVector2;
	Vec3TransformCoord(&kFogNearVector, &kFogNearVector2, &rkTPRS.m_m4Proj);

	Vector3 kFogFarVector;
	Vec3TransformCoord(&kFogFarVector, &kFogNearVector2, &rkTPRS.m_m4Proj);

	float fFogNear = kFogNearVector.z;
	float fFogFar = kFogFarVector.z;
	float fFogLenInv = 1.0f / (fFogFar - fFogNear);

	rkTPRS.m_fFogNearTransZ = fFogNear;
	rkTPRS.m_fFogFarTransZ = fFogFar;
	rkTPRS.m_fFogLenInv = fFogLenInv;

}

bool CMapOutdoor::__SoftwareTransformPatch_SetTransform(SoftwareTransformPatch_SRenderState& rkTPRS, SoftwareTransformPatch_STLVertex* akTransVertex, CTerrainPatchProxy& rkTerrainPatchProxy, UINT uTerrainX, UINT uTerrainY, bool isFogEnable, bool isDynamicShadow)
{
	SoftwareTransformPatch_SSourceVertex* akSrcVertex=rkTerrainPatchProxy.SoftwareTransformPatch_GetTerrainVertexDataPtr();
	if (!akSrcVertex)
		return false;

	rkTerrainPatchProxy.SoftwareTransformPatch_UpdateTerrainLighting(
		m_kSTPD.m_dwLightVersion,
		rkTPRS.m_kLight, rkTPRS.m_kMtrl);

	Vector3* pkSrcPosition;

	float fTilePatternX=+1/640.0f;
	float fTilePatternY=-1/640.0f;

	float fTerrainBaseX=-(float) (uTerrainX * CTerrainImpl::TERRAIN_XSIZE)+m_fTerrainTexCoordBase * 12.30769f;
	float fTerrainBaseY=+(float) (uTerrainY * CTerrainImpl::TERRAIN_YSIZE)+m_fTerrainTexCoordBase * 12.30769f;

	float fScreenHalfWidth=rkTPRS.m_fScreenHalfWidth;
	float fScreenHalfHeight=rkTPRS.m_fScreenHalfHeight;

	float fAlphaPatternX=m_matSplatAlpha._11;
	float fAlphaPatternY=m_matSplatAlpha._22;
	float fAlphaBiasX=m_matSplatAlpha._41;
	float fAlphaBiasY=m_matSplatAlpha._42;
	float fShadowPatternX=+m_fTerrainTexCoordBase * ((float) CTerrainImpl::PATCH_XSIZE / (float)(CTerrainImpl::XSIZE));
	float fShadowPatternY=-m_fTerrainTexCoordBase * ((float) CTerrainImpl::PATCH_YSIZE / (float)(CTerrainImpl::YSIZE));

	Matrix m4Frustum=rkTPRS.m_m4Frustum;

	if (isFogEnable)
	{
		float fFogCur;
		float fFogFar=rkTPRS.m_fFogFarTransZ;
		float fFogLenInv=rkTPRS.m_fFogLenInv;

		float fLocalX;
		float fLocalY;

		SoftwareTransformPatch_STLVertex kWorkVertex;
		for (UINT uIndex=0; uIndex!=CTerrainPatch::TERRAIN_VERTEX_COUNT; ++uIndex)
		{
			pkSrcPosition=&akSrcVertex[uIndex].kPosition;
			Vec3Transform(&kWorkVertex.kPosition, pkSrcPosition, &m4Frustum);
			fLocalX=pkSrcPosition->x+fTerrainBaseX;
			fLocalY=pkSrcPosition->y+fTerrainBaseY;
			kWorkVertex.kPosition.w=1.0f/kWorkVertex.kPosition.w;
			kWorkVertex.kPosition.x*=kWorkVertex.kPosition.w;
			kWorkVertex.kPosition.y*=kWorkVertex.kPosition.w;
			kWorkVertex.kPosition.z*=kWorkVertex.kPosition.w;
			kWorkVertex.kPosition.x=(kWorkVertex.kPosition.x+1.0f)*fScreenHalfWidth;
			kWorkVertex.kPosition.y=(kWorkVertex.kPosition.y-1.0f)*fScreenHalfHeight;
			kWorkVertex.dwDiffuse=akSrcVertex[uIndex].dwDiffuse;
			kWorkVertex.kTexTile.x=pkSrcPosition->x*fTilePatternX;
			kWorkVertex.kTexTile.y=pkSrcPosition->y*fTilePatternY;
			kWorkVertex.kTexAlpha.x = max(0.001f, min(0.999f, fLocalX*fAlphaPatternX+fAlphaBiasX));
			kWorkVertex.kTexAlpha.y = max(0.001f, min(0.999f, fLocalY*fAlphaPatternY+fAlphaBiasY));
			kWorkVertex.kTexStaticShadow.x=fLocalX*fShadowPatternX;
			kWorkVertex.kTexStaticShadow.y=fLocalY*fShadowPatternY;
			kWorkVertex.kTexDynamicShadow.x=0.0f;
			kWorkVertex.kTexDynamicShadow.y=0.0f;

			fFogCur=(fFogFar-kWorkVertex.kPosition.z)*fFogLenInv;
			if (fFogCur<0.0f)
				kWorkVertex.dwFog=kWorkVertex.dwDiffuse=0x0000000|(kWorkVertex.dwDiffuse&0xffffff);
			else if (fFogCur>1.0f)
				kWorkVertex.dwFog=kWorkVertex.dwDiffuse=0xFF000000|(kWorkVertex.dwDiffuse&0xffffff);
			else
				kWorkVertex.dwFog=kWorkVertex.dwDiffuse=BYTE(255.0f*fFogCur)<<24|(kWorkVertex.dwDiffuse&0xffffff);

			*(akTransVertex+uIndex)=kWorkVertex;
		}
	}
	else
	{
		float fLocalX;
		float fLocalY;

		SoftwareTransformPatch_STLVertex kWorkVertex;
		for (UINT uIndex=0; uIndex!=CTerrainPatch::TERRAIN_VERTEX_COUNT; ++uIndex)
		{
			pkSrcPosition=&akSrcVertex[uIndex].kPosition;
			Vec3Transform(&kWorkVertex.kPosition, pkSrcPosition, &m4Frustum);
			fLocalX=pkSrcPosition->x+fTerrainBaseX;
			fLocalY=pkSrcPosition->y+fTerrainBaseY;
			kWorkVertex.kPosition.w=1.0f/kWorkVertex.kPosition.w;
			kWorkVertex.kPosition.x*=kWorkVertex.kPosition.w;
			kWorkVertex.kPosition.y*=kWorkVertex.kPosition.w;
			kWorkVertex.kPosition.z*=kWorkVertex.kPosition.w;
			kWorkVertex.kPosition.x=(kWorkVertex.kPosition.x+1.0f)*fScreenHalfWidth;
			kWorkVertex.kPosition.y=(kWorkVertex.kPosition.y-1.0f)*fScreenHalfHeight;
			kWorkVertex.dwDiffuse=akSrcVertex[uIndex].dwDiffuse;
			kWorkVertex.dwFog=0xffffffff;
			kWorkVertex.kTexTile.x=pkSrcPosition->x*fTilePatternX;
			kWorkVertex.kTexTile.y=pkSrcPosition->y*fTilePatternY;
			kWorkVertex.kTexAlpha.x = max(0.001f, min(0.999f, fLocalX*fAlphaPatternX+fAlphaBiasX));
			kWorkVertex.kTexAlpha.y = max(0.001f, min(0.999f, fLocalY*fAlphaPatternY+fAlphaBiasY));
			kWorkVertex.kTexStaticShadow.x=fLocalX*fShadowPatternX;
			kWorkVertex.kTexStaticShadow.y=fLocalY*fShadowPatternY;
			kWorkVertex.kTexDynamicShadow.x=0.0f;
			kWorkVertex.kTexDynamicShadow.y=0.0f;

			*(akTransVertex+uIndex)=kWorkVertex;
		}
	}

	if (isDynamicShadow)
	{
		Matrix m4DynamicShadow=rkTPRS.m_m4DynamicShadow;

		Vector3 v3Shadow;
		for (UINT uIndex=0; uIndex!=CTerrainPatch::TERRAIN_VERTEX_COUNT; ++uIndex)
		{
			Vec3TransformCoord(&v3Shadow, &akSrcVertex[uIndex].kPosition, &m4DynamicShadow);
			akTransVertex[uIndex].kTexDynamicShadow.x=v3Shadow.x;
			akTransVertex[uIndex].kTexDynamicShadow.y=v3Shadow.y;
		}
	}

	return true;
}

bool CMapOutdoor::__SoftwareTransformPatch_SetSplatStream(SoftwareTransformPatch_STLVertex* akSrcVertex)
{
	ID3D11Buffer* pkVB=m_kSTPD.m_pkVBSplat[m_kSTPD.m_dwSplatPos++];
	m_kSTPD.m_dwSplatPos%=SoftwareTransformPatch_SData::SPLAT_VB_NUM;
	if (!pkVB)
		return false;

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	if (FAILED(ms_pContext->Map(pkVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
		return false;

	SoftwareTransformPatch_SSplatVertex* akDstVertex = (SoftwareTransformPatch_SSplatVertex*)mappedResource.pData;
	for (UINT uIndex=0; uIndex!=CTerrainPatch::TERRAIN_VERTEX_COUNT; ++uIndex)
		*(akDstVertex+uIndex)=*((SoftwareTransformPatch_SSplatVertex*)(akSrcVertex+uIndex));

	ms_pContext->Unmap(pkVB, 0);

	SHADERMANAGER.SetVertexBuffer(0, pkVB, sizeof(SoftwareTransformPatch_SSplatVertex));
	return true;
}

bool CMapOutdoor::__SoftwareTransformPatch_SetShadowStream(SoftwareTransformPatch_STLVertex* akSrcVertex)
{
	ID3D11Buffer* pkVB=m_kSTPD.m_pkVBSplat[m_kSTPD.m_dwSplatPos++];
	m_kSTPD.m_dwSplatPos%=SoftwareTransformPatch_SData::SPLAT_VB_NUM;
	if (!pkVB)
		return false;

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	if (FAILED(ms_pContext->Map(pkVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
		return false;

	SoftwareTransformPatch_SSplatVertex* akDstVertex = (SoftwareTransformPatch_SSplatVertex*)mappedResource.pData;
	SoftwareTransformPatch_STLVertex* pkSrcVertex;
	SoftwareTransformPatch_SSplatVertex* pkDstVertex;
	for (UINT uIndex=0; uIndex!=CTerrainPatch::TERRAIN_VERTEX_COUNT; ++uIndex)
	{
		pkSrcVertex=akSrcVertex+uIndex;
		pkDstVertex=akDstVertex+uIndex;
		pkDstVertex->kPosition=pkSrcVertex->kPosition;
		pkDstVertex->dwDiffuse=pkSrcVertex->dwDiffuse;
		pkDstVertex->dwSpecular=pkSrcVertex->dwFog;
		pkDstVertex->kTex1=pkSrcVertex->kTexStaticShadow;
		pkDstVertex->kTex2=pkSrcVertex->kTexDynamicShadow;
	}
	ms_pContext->Unmap(pkVB, 0);

	SHADERMANAGER.SetVertexBuffer(0, pkVB, sizeof(SoftwareTransformPatch_SSplatVertex));
	return true;
}

void CMapOutdoor::__SoftwareTransformPatch_Initialize()
{
	{
		for (UINT uIndex=0; uIndex!=SoftwareTransformPatch_SData::SPLAT_VB_NUM; ++uIndex)
			m_kSTPD.m_pkVBSplat[uIndex]=NULL;
		m_kSTPD.m_dwSplatPos=0;
	}

	{
		for (UINT uIndex=0; uIndex!=SoftwareTransformPatch_SData::NONE_VB_NUM; ++uIndex)
			m_kSTPD.m_pkVBNone[uIndex]=NULL;
		m_kSTPD.m_dwNonePos=0;
	}
}

bool CMapOutdoor::__SoftwareTransformPatch_Create()
{
	if (!ms_pDevice)
		return false;

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = sizeof(SoftwareTransformPatch_SSplatVertex) * CTerrainPatch::TERRAIN_VERTEX_COUNT;
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		for (UINT uIndex=0; uIndex!=SoftwareTransformPatch_SData::SPLAT_VB_NUM; ++uIndex)
		{
			assert(NULL==m_kSTPD.m_pkVBSplat[uIndex]);
			if (FAILED(ms_pDevice->CreateBuffer(&bufferDesc, nullptr, &m_kSTPD.m_pkVBSplat[uIndex])))
				 return false;
		}
	}

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = sizeof(SoftwareTransformPatch_STVertex) * CTerrainPatch::TERRAIN_VERTEX_COUNT;
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		for (UINT uIndex=0; uIndex!=SoftwareTransformPatch_SData::NONE_VB_NUM; ++uIndex)
		{
			assert(NULL==m_kSTPD.m_pkVBNone[uIndex]);
			if (FAILED(ms_pDevice->CreateBuffer(&bufferDesc, nullptr, &m_kSTPD.m_pkVBNone[uIndex])))
				return false;
		}
	}
	return true;
}

void CMapOutdoor::__SoftwareTransformPatch_Destroy()
{
	{
		for (UINT uIndex=0; uIndex!=SoftwareTransformPatch_SData::SPLAT_VB_NUM; ++uIndex)
		{
			if (m_kSTPD.m_pkVBSplat[uIndex])
				m_kSTPD.m_pkVBSplat[uIndex]->Release();
		}
	}

	{
		for (UINT uIndex=0; uIndex!=SoftwareTransformPatch_SData::NONE_VB_NUM; ++uIndex)
		{
			if (m_kSTPD.m_pkVBNone[uIndex])
				m_kSTPD.m_pkVBNone[uIndex]->Release();
		}
	}
	__SoftwareTransformPatch_Initialize();
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
