#include "StdAfx.h"
#include "MapOutdoor.h"
#include "TerrainPatch.h"
#include "AreaTerrain.h"
#include "TerrainQuadtree.h"

#include "../eterlib/Camera.h"
#include "../eterlib/ShaderManager.h"
#include "../eterlib/ShaderInit.h"
#include "../EterBase/StepTimer.h"
#include "../UserInterface/Locale_inc.h"

#define MAX_RENDER_SPALT 150

CArea::TCRCWithNumberVector m_dwRenderedCRCWithNumberVector;

CMapOutdoor::TTerrainNumVector CMapOutdoor::FSortPatchDrawStructWithTerrainNum::m_TerrainNumVector;

void CMapOutdoor::RenderTerrain()
{
	if (!IsVisiblePart(PART_TERRAIN))
		return;

	if (!m_bSettingTerrainVisible)
		return;

	if (!m_pTerrainPatchProxyList)
		return;

	CCamera * pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return;

	auto val = ms_matView * ms_matProj;
	BuildViewFrustum(val);

	Vector3 v3Eye = pCamera->GetEye();
	m_fXforDistanceCaculation = -v3Eye.x;
	m_fYforDistanceCaculation = -v3Eye.y;

	//////////////////////////////////////////////////////////////////////////
	// Push
	m_PatchVector.clear();

	__RenderTerrain_RecurseRenderQuadTree(m_pRootNode);

	std::sort(m_PatchVector.begin(),m_PatchVector.end());

	if (CTerrainPatch::SOFTWARE_TRANSFORM_PATCH_ENABLE)
	{
		__RenderTerrain_RenderSoftwareTransformPatch();
	}
	else
	{
		__RenderTerrain_RenderHardwareTransformPatch();
	}
}

void CMapOutdoor::__RenderTerrain_RecurseRenderQuadTree(CTerrainQuadtreeNode *Node, bool bCullCheckNeed)
{
	if (bCullCheckNeed)
	{
		switch (__RenderTerrain_RecurseRenderQuadTree_CheckBoundingCircle(Node->center, Node->radius))
		{
			case VIEW_ALL:
				// all child nodes need not cull check
				bCullCheckNeed = false;
				break;
			case VIEW_PART:
				break;
			case VIEW_NONE:
				// no need to render
				return;
		}
		// if no need cull check more
		// -> bCullCheckNeed = false;
	}

	if (Node->Size == 1)
	{
		Vector3 v3Center = Node->center;
		float fDistance = fMAX(fabs(v3Center.x + m_fXforDistanceCaculation), fabs(-v3Center.y + m_fYforDistanceCaculation));
		__RenderTerrain_AppendPatch(v3Center, fDistance, Node->PatchNum);
	}
	else
	{
		if (Node->NW_Node != NULL)
			__RenderTerrain_RecurseRenderQuadTree(Node->NW_Node, bCullCheckNeed);
		if (Node->NE_Node != NULL)
			__RenderTerrain_RecurseRenderQuadTree(Node->NE_Node, bCullCheckNeed);
		if (Node->SW_Node != NULL)
			__RenderTerrain_RecurseRenderQuadTree(Node->SW_Node, bCullCheckNeed);
		if (Node->SE_Node != NULL)
			__RenderTerrain_RecurseRenderQuadTree(Node->SE_Node, bCullCheckNeed);
	}
}

int	CMapOutdoor::__RenderTerrain_RecurseRenderQuadTree_CheckBoundingCircle(const Vector3 & c_v3Center, const float & c_fRadius)
{
	const int count = 6;

	Vector3 center = c_v3Center;
	center.y = -center.y;

	int i;

	float distance[count];
	for(i = 0; i < count; ++i)
	{
		distance[i] = PlaneDotCoord(&m_plane[i], &center);
		if (distance[i] <= -c_fRadius)
			return VIEW_NONE;
	}

	for(i = 0; i < count;++i)
	{
		if (distance[i] <= c_fRadius)
			return VIEW_PART;
	}

	return VIEW_ALL;
}

void CMapOutdoor::__RenderTerrain_AppendPatch(const Vector3& c_rv3Center, float fDistance, long lPatchNum)
{
	assert(NULL!=m_pTerrainPatchProxyList && "CMapOutdoor::__RenderTerrain_AppendPatch");
	if (!m_pTerrainPatchProxyList[lPatchNum].isUsed())
		return;

	m_pTerrainPatchProxyList[lPatchNum].SetCenterPosition(c_rv3Center);
	m_PatchVector.push_back(std::make_pair(fDistance, lPatchNum));
}

void CMapOutdoor::ApplyLight(DWORD dwVersion, const TLight& c_rkLight)
{
	m_kSTPD.m_dwLightVersion=dwVersion;
	SHADERMANAGER.SetLight(0, &c_rkLight);
}

void CMapOutdoor::InitializeVisibleParts()
{
	m_dwVisiblePartFlags=0xffffffff;
}

void CMapOutdoor::SetVisiblePart(int ePart, bool isVisible)
{
	DWORD dwMask=(1<<ePart);
	if (isVisible)
	{
		m_dwVisiblePartFlags|=dwMask;
	}
	else
	{
		DWORD dwReverseMask=~dwMask;
		m_dwVisiblePartFlags&=dwReverseMask;
	}
}

bool CMapOutdoor::IsVisiblePart(int ePart)
{
	DWORD dwMask=(1<<ePart);
	if (dwMask & m_dwVisiblePartFlags)
		return true;

	return false;
}

void CMapOutdoor::SetSplatLimit(int iSplatNum)
{
	m_iSplatLimit = iSplatNum;
}

std::vector<int> & CMapOutdoor::GetRenderedSplatNum(int * piPatch, int * piSplat, float * pfSplatRatio)
{
	*piPatch = m_iRenderedPatchNum;
	*piSplat = m_iRenderedSplatNum;
	*pfSplatRatio = m_iRenderedSplatNumSqSum/float(m_iRenderedPatchNum);

	return m_RenderedTextureNumVector;
}

CArea::TCRCWithNumberVector & CMapOutdoor::GetRenderedGraphicThingInstanceNum(DWORD * pdwGraphicThingInstanceNum, DWORD * pdwCRCNum)
{
	*pdwGraphicThingInstanceNum = m_dwRenderedGraphicThingInstanceNum;
	*pdwCRCNum = m_dwRenderedCRCNum;

	return m_dwRenderedCRCWithNumberVector;
}

void CMapOutdoor::RenderBeforeLensFlare()
{
	if (!mc_pEnvironmentData)
	{
		TraceError("CMapOutdoor::RenderBeforeLensFlare mc_pEnvironmentData is NULL");
		return;
	}

	m_LensFlare.Compute(mc_pEnvironmentData->DirLights[ENV_DIRLIGHT_BACKGROUND].Direction);
	m_LensFlare.DrawBeforeFlare();
}

void CMapOutdoor::RenderAfterLensFlare()
{
	m_LensFlare.AdjustBrightness();
	m_LensFlare.DrawFlare();

	// Render volumetric god rays
	if (m_bGodRaysEnabled)
		RenderGodRays();
}

void CMapOutdoor::SetGodRaysParams(float fDensity, float fDecay, float fExposure)
{
	m_fGodRaysDensity = fDensity;
	m_fGodRaysDecay = fDecay;
	m_fGodRaysExposure = fExposure;
}

void CMapOutdoor::RenderGodRays()
{
	if (!m_bGodRaysEnabled || !mc_pEnvironmentData)
	{
		SHADERMANAGER.SetGodRaysEnabled(false);
		return;
	}

	// Disable god rays at night — moon doesn't produce visible light scattering
	const Color& diffuse = mc_pEnvironmentData->DirLights[ENV_DIRLIGHT_BACKGROUND].Diffuse;
	bool bIsNight = ((diffuse.r + diffuse.g + diffuse.b) / 3.0f < 0.5f);
	if (bIsNight)
	{
		SHADERMANAGER.SetGodRaysEnabled(false);
		return;
	}

	CCamera* pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
	{
		SHADERMANAGER.SetGodRaysEnabled(false);
		return;
	}

	Vector3 vLightDir = mc_pEnvironmentData->DirLights[ENV_DIRLIGHT_BACKGROUND].Direction;
	float len = sqrtf(vLightDir.x * vLightDir.x + vLightDir.y * vLightDir.y + vLightDir.z * vLightDir.z);
	if (len < 0.001f)
	{
		SHADERMANAGER.SetGodRaysEnabled(false);
		return;
	}
	vLightDir.x /= len; vLightDir.y /= len; vLightDir.z /= len;

	float fBaseAngle = atan2f(-vLightDir.y, -vLightDir.x);
	DWORD dwTime = (DWORD)DX::StepTimer::instance().GetTotalMillieSeconds();
	float fTimeSec = (float)dwTime / 1000.0f;
	const float fOrbitSpeed = 6.2831853f / 1200.0f; // 2*PI / 1200 sec (must match SkyBox)
	float fTimeAngle = fTimeSec * fOrbitSpeed;
	float fAngle = fBaseAngle + fTimeAngle;

	float fCosA = cosf(fAngle);
	float fSinA = sinf(fAngle);

	const float fElevBase = 0.10f;
	const float fElevAmp = 0.04f;
	float fBodyZ = fElevBase + fElevAmp * sinf(fTimeAngle * 2.0f);

	const float fBodyDist = 0.88f;

	const Vector3& vScale = mc_pEnvironmentData->v3SkyBoxScale;
	Vector3 vSunWorld;
	vSunWorld.x = fCosA * fBodyDist * vScale.x;
	vSunWorld.y = fSinA * fBodyDist * vScale.y;
	vSunWorld.z = fBodyZ * vScale.z;

	float dirLen = sqrtf(vSunWorld.x * vSunWorld.x + vSunWorld.y * vSunWorld.y + vSunWorld.z * vSunWorld.z);
	if (dirLen < 0.001f)
	{
		SHADERMANAGER.SetGodRaysEnabled(false);
		return;
	}
	vSunWorld.x /= dirLen; vSunWorld.y /= dirLen; vSunWorld.z /= dirLen;

	const Vector3& vCameraPos = pCamera->GetEye();
	float fSunDistance = 50000.0f;
	Vector3 vSunPos;
	vSunPos.x = vCameraPos.x + vSunWorld.x * fSunDistance;
	vSunPos.y = vCameraPos.y + vSunWorld.y * fSunDistance;
	vSunPos.z = vCameraPos.z + vSunWorld.z * fSunDistance;

	// Transform to clip space
	const Matrix& matView = pCamera->GetViewMatrix();
	Matrix matProj;
	SHADERMANAGER.GetProjectionMatrix(&matProj);

	Matrix matViewProj;
	MatrixMultiply(&matViewProj, &matView, &matProj);

	Vector4 vWorldPos(vSunPos.x, vSunPos.y, vSunPos.z, 1.0f);
	Vector4 vClipPos;
	vClipPos.x = vWorldPos.x * matViewProj._11 + vWorldPos.y * matViewProj._21 + vWorldPos.z * matViewProj._31 + vWorldPos.w * matViewProj._41;
	vClipPos.y = vWorldPos.x * matViewProj._12 + vWorldPos.y * matViewProj._22 + vWorldPos.z * matViewProj._32 + vWorldPos.w * matViewProj._42;
	vClipPos.z = vWorldPos.x * matViewProj._13 + vWorldPos.y * matViewProj._23 + vWorldPos.z * matViewProj._33 + vWorldPos.w * matViewProj._43;
	vClipPos.w = vWorldPos.x * matViewProj._14 + vWorldPos.y * matViewProj._24 + vWorldPos.z * matViewProj._34 + vWorldPos.w * matViewProj._44;

	if (vClipPos.w <= 0.0f)
	{
		SHADERMANAGER.SetGodRaysEnabled(false);
		return;  // Sun is behind camera
	}

	float fNdcX = vClipPos.x / vClipPos.w;
	float fNdcY = vClipPos.y / vClipPos.w;

	if (fNdcX < -2.0f || fNdcX > 2.0f || fNdcY < -2.0f || fNdcY > 2.0f)
	{
		SHADERMANAGER.SetGodRaysEnabled(false);
		return;
	}

	float uvX = (fNdcX + 1.0f) * 0.5f;
	float uvY = (1.0f - fNdcY) * 0.5f;

	SHADERMANAGER.SetGodRaysEnabled(true);
	SHADERMANAGER.SetGodRaysParams(uvX, uvY, m_fGodRaysIntensity, m_fGodRaysDecay);
	SHADERMANAGER.SetGodRaysRayParams(m_fGodRaysDensity, 0.5f, m_fGodRaysExposure, 64);
	SHADERMANAGER.SetGodRaysColor(diffuse.r, diffuse.g, diffuse.b);
}

void CMapOutdoor::RenderCollision()
{
	for (int i = 0; i < AROUND_AREA_NUM; ++i)
	{
		CArea * pArea;
		if (GetAreaPointer(i, &pArea))
			pArea->RenderCollision();
	}
}

void CMapOutdoor::RenderScreenFiltering()
{
	m_ScreenFilter.Render();
}

void CMapOutdoor::RenderSky()
{
	if (IsVisiblePart(PART_SKY))
	{
#ifdef ENABLE_CELESTIAL_BODY
		// Set light direction before Render() — celestial body renders inline
		if (mc_pEnvironmentData)
		{
			const Vector3& lightDir = mc_pEnvironmentData->DirLights[ENV_DIRLIGHT_BACKGROUND].Direction;
			const Color& diffuse = mc_pEnvironmentData->DirLights[ENV_DIRLIGHT_BACKGROUND].Diffuse;
			bool bIsNight = ((diffuse.r + diffuse.g + diffuse.b) / 3.0f < 0.5f);
			m_SkyBox.SetLightDirection(lightDir, bIsNight);
		}
#endif
		m_SkyBox.Render();
	}
}

void CMapOutdoor::RenderCelestialBody()
{
}

void CMapOutdoor::RenderCloud()
{
	if (IsVisiblePart(PART_CLOUD))
	{
		if (mc_pEnvironmentData)
		{
			const Vector3& sunDir = mc_pEnvironmentData->DirLights[ENV_DIRLIGHT_BACKGROUND].Direction;
			SHADERMANAGER.SetSunDirection(sunDir.x, sunDir.y, sunDir.z, 1.0f);
		}
		m_SkyBox.RenderCloud();
	}
}

void CMapOutdoor::RenderTree()
{
	if (IsVisiblePart(PART_TREE))
		CSpeedTreeForestDX11::Instance().Render();
}

void CMapOutdoor::SetInverseViewAndDynamicShaodwMatrices()
{
	CCamera * pCamera = CCameraManager::Instance().GetCurrentCamera();

	if (!pCamera)
		return;

	m_matViewInverse = pCamera->GetInverseViewMatrix();

	Vector3 v3Target = pCamera->GetTarget();

	Vector3 v3LightEye;
	if (mc_pEnvironmentData)
	{
		Vector3 vLightDir = mc_pEnvironmentData->DirLights[ENV_DIRLIGHT_BACKGROUND].Direction;
		float len = sqrtf(vLightDir.x * vLightDir.x + vLightDir.y * vLightDir.y + vLightDir.z * vLightDir.z);
		if (len > 0.001f)
		{
			vLightDir.x /= len; vLightDir.y /= len; vLightDir.z /= len;

			// Same orbit calculation as god rays / SkyBox::Render()
			float fBaseAngle = atan2f(-vLightDir.y, -vLightDir.x);
			DWORD dwTime = (DWORD)DX::StepTimer::instance().GetTotalMillieSeconds();
			float fTimeSec = (float)dwTime / 1000.0f;
			const float fOrbitSpeed = 6.2831853f / 1200.0f;
			float fTimeAngle = fTimeSec * fOrbitSpeed;
			float fAngle = fBaseAngle + fTimeAngle;

			float fSunX = cosf(fAngle);
			float fSunY = sinf(fAngle);

			const Color& sunDiffuse = mc_pEnvironmentData->DirLights[ENV_DIRLIGHT_BACKGROUND].Diffuse;
			bool bIsNight = ((sunDiffuse.r + sunDiffuse.g + sunDiffuse.b) / 3.0f < 0.5f);

			// At night, flip 180 degrees — shadows come from the moon (opposite side)
			if (bIsNight)
			{
				fSunX = -fSunX;
				fSunY = -fSunY;
			}

			const float fElevBase = 0.10f;
			const float fElevAmp = 0.04f;
			float fBodyZ = fElevBase + fElevAmp * sinf(fTimeAngle * 2.0f);
			const Vector3& vScale = mc_pEnvironmentData->v3SkyBoxScale;

			Vector3 vSunDir;
			vSunDir.x = fSunX * 0.88f * vScale.x;
			vSunDir.y = fSunY * 0.88f * vScale.y;
			vSunDir.z = fBodyZ * vScale.z;

			// Enforce minimum elevation — prevents giant stretched shadows
			float horizLen = sqrtf(vSunDir.x * vSunDir.x + vSunDir.y * vSunDir.y);
			if (vSunDir.z < horizLen * 0.8f)
				vSunDir.z = horizLen * 0.8f;  // min ~39 degree elevation

			float dirLen = sqrtf(vSunDir.x * vSunDir.x + vSunDir.y * vSunDir.y + vSunDir.z * vSunDir.z);
			if (dirLen > 0.001f)
			{
				vSunDir.x /= dirLen; vSunDir.y /= dirLen; vSunDir.z /= dirLen;
				float dist = SHADOW_LIGHT_OFFSET * 2.0f;
				v3LightEye.x = v3Target.x + vSunDir.x * dist;
				v3LightEye.y = v3Target.y + vSunDir.y * dist;
				v3LightEye.z = v3Target.z + vSunDir.z * dist;
			}
			else
			{
				v3LightEye.x = v3Target.x - SHADOW_LIGHT_DIR_X * SHADOW_LIGHT_OFFSET;
				v3LightEye.y = v3Target.y - SHADOW_LIGHT_OFFSET;
				v3LightEye.z = v3Target.z + SHADOW_LIGHT_DIR_Z * SHADOW_LIGHT_OFFSET;
			}
		}
		else
		{
			v3LightEye.x = v3Target.x - SHADOW_LIGHT_DIR_X * SHADOW_LIGHT_OFFSET;
			v3LightEye.y = v3Target.y - SHADOW_LIGHT_OFFSET;
			v3LightEye.z = v3Target.z + SHADOW_LIGHT_DIR_Z * SHADOW_LIGHT_OFFSET;
		}
	}
	else
	{
		v3LightEye.x = v3Target.x - SHADOW_LIGHT_DIR_X * SHADOW_LIGHT_OFFSET;
		v3LightEye.y = v3Target.y - SHADOW_LIGHT_OFFSET;
		v3LightEye.z = v3Target.z + SHADOW_LIGHT_DIR_Z * SHADOW_LIGHT_OFFSET;
	}

	auto val = Vector3(0.0f, 0.0f, 1.0f);
	MatrixLookAtRH(&m_matLightView, &v3LightEye, &v3Target, &val);
	m_matDynamicShadow = m_matLightView * m_matDynamicShadowScale;

	SHADERMANAGER.SetShadowOpacity(SHADOW_OPACITY_DEFAULT);
	SHADERMANAGER.SetShadowMatrices(&m_matShadowCascade[3], &m_matShadowCascade[0]);
	SHADERMANAGER.SetShadowMidFarMatrices(&m_matShadowCascade[1], &m_matShadowCascade[2]);

	// Cascade split distances (view-space depth thresholds)
	// Objects closer than split[0] use cascade 0, etc.
	SHADERMANAGER.SetCascadeSplits(
		m_fCascadeSize[0] * 0.4f,   // ~600 — use cascade 0 for very near
		m_fCascadeSize[1] * 0.4f,   // ~1300 — cascade 1 for mid-near
		m_fCascadeSize[2] * 0.4f,   // ~2000 — cascade 2 for mid-far
		m_fCascadeSize[3] * 0.4f    // ~4000 — cascade 3 for far
	);
}

void CMapOutdoor::OnRender()
{
#ifdef __PERFORMANCE_CHECKER__
	DWORD t1=ELTimer_GetMSec();
	SetInverseViewAndDynamicShaodwMatrices();

	SetBlendOperation();
	DWORD t2=ELTimer_GetMSec();
	RenderArea();
	DWORD t3=ELTimer_GetMSec();
	if (!m_bEnableTerrainOnlyForHeight)
		RenderTerrain();
	DWORD t4=ELTimer_GetMSec();
	RenderTree();
	DWORD t5=ELTimer_GetMSec();
	DWORD tEnd=ELTimer_GetMSec();

	if (tEnd-t1<7)
		return;

	static FILE* fp=fopen("perf_map_render.txt", "w");
 	fprintf(fp, "MAP.Total %d (Time %d)\n", tEnd-t1, ELTimer_GetMSec());
	fprintf(fp, "MAP.ENV %d\n", t2-t1);
	fprintf(fp, "MAP.OBJ %d\n", t3-t2);
	fprintf(fp, "MAP.TRN %d\n", t4-t3);
	fprintf(fp, "MAP.TRE %d\n", t5-t4);

#else
	SetInverseViewAndDynamicShaodwMatrices();

	SetBlendOperation();

	if (!m_bEnableTerrainOnlyForHeight)
	{
		RenderTerrain();
	}

	RenderArea();

	RenderTree();

	RenderBlendArea();
#endif
}

struct FAreaRenderShadow
{
	void operator () (CGraphicObjectInstance * pInstance)
	{
		pInstance->RenderShadow();
		pInstance->Hide();
	}
};

struct FPCBlockerHide
{
	void operator () (CGraphicObjectInstance * pInstance)
	{
		pInstance->Hide();
	}
};

struct FRenderPCBlocker
{
	void operator () (CGraphicObjectInstance * pInstance)
	{
		pInstance->Show();
		CGraphicThingInstance* pThingInstance = dynamic_cast <CGraphicThingInstance*> (pInstance);
		if (pThingInstance != NULL)
		{
			if (pThingInstance->HaveBlendThing())
			{
				pThingInstance->BlendRender();
				return;
			}
		}

		pInstance->RenderPCBlocker();
	}
};

void CMapOutdoor::RenderEffect()
{
	if (!IsVisiblePart(PART_OBJECT))
		return;
	for (int i = 0; i < AROUND_AREA_NUM; ++i)
	{
		CArea * pArea;
		if (GetAreaPointer(i, &pArea))
		{
			pArea->RenderEffect();
		}
	}
}

struct CMapOutdoor_LessThingInstancePtrRenderOrder
{
	bool operator() (CGraphicThingInstance* pkLeft, CGraphicThingInstance* pkRight)
	{
		CCamera * pCurrentCamera = CCameraManager::Instance().GetCurrentCamera();
		const Vector3 & c_rv3CameraPos = pCurrentCamera->GetEye();
		const Vector3 & c_v3LeftPos  = pkLeft->GetPosition();
		const Vector3 & c_v3RightPos = pkRight->GetPosition();

		auto val1 = Vector3(c_rv3CameraPos - c_v3LeftPos);
		auto val2 = Vector3(c_rv3CameraPos - c_v3RightPos);
		return Vec3LengthSq(&val1) < Vec3LengthSq(&val2 );
	}
};

struct CMapOutdoor_FOpaqueThingInstanceRender
{
	inline void operator () (CGraphicThingInstance * pkThingInst)
	{
		pkThingInst->Render();
	}
};
struct CMapOutdoor_FBlendThingInstanceRender
{
	inline void operator () (CGraphicThingInstance * pkThingInst)
	{
		pkThingInst->BlendRender();
	}
};

void CMapOutdoor::RenderArea(bool bRenderAmbience)
{
	if (!IsVisiblePart(PART_OBJECT))
		return;

	m_dwRenderedCRCNum = 0;
	m_dwRenderedGraphicThingInstanceNum = 0;
	m_dwRenderedCRCWithNumberVector.clear();

	for (int j = 0; j < AROUND_AREA_NUM; ++j)
	{
		CArea * pArea;
		if (GetAreaPointer(j, &pArea))
		{
			pArea->RenderDungeon();
		}
	}

#ifndef WORLD_EDITOR
	// PCBlocker
	std::for_each(m_PCBlockerVector.begin(), m_PCBlockerVector.end(), FPCBlockerHide());

	// Shadow Receiver
	if (m_bDrawShadow && m_bDrawChrShadow)
	{
		if (mc_pEnvironmentData != NULL)
			SHADERMANAGER.SetFogColor(0xFFFFFFFF);

		SHADERMANAGER.SaveTransform(MATRIX_TEXTURE1, &m_matDynamicShadow);
		SHADERMANAGER.SetShaderResource(1, m_lpCharacterShadowMapTexture);
		SHADERMANAGER.SaveSamplerState(1, SAMPLER_ADDRESSU, ADDRESS_BORDER);
		SHADERMANAGER.SaveSamplerState(1, SAMPLER_ADDRESSV, ADDRESS_BORDER);
		SHADERMANAGER.SaveSamplerState(1, SAMPLER_BORDERCOLOR, 0xFFFFFFFF);

		SHADERMANAGER.SetShadowTextures(m_lpShadowMapSRV[3], m_lpShadowMapSRV[0]);
		SHADERMANAGER.SetShadowMidFarTextures(m_lpShadowMapSRV[1], m_lpShadowMapSRV[2]);

		SHADERMANAGER.SetTwoTextureBlend(true);

		std::for_each(m_ShadowReceiverVector.begin(), m_ShadowReceiverVector.end(), FAreaRenderShadow());

		SHADERMANAGER.SetTwoTextureBlend(false);

		SHADERMANAGER.SetShaderResource(1, NULL);  // Legacy shadow map (slot 1)
		SHADERMANAGER.SetShadowTextures(nullptr, nullptr);
		SHADERMANAGER.SetShadowMidFarTextures(nullptr, nullptr);

		SHADERMANAGER.RestoreSamplerState(1, SAMPLER_ADDRESSU);
		SHADERMANAGER.RestoreSamplerState(1, SAMPLER_ADDRESSV);
		SHADERMANAGER.RestoreSamplerState(1, SAMPLER_BORDERCOLOR);

		SHADERMANAGER.RestoreTransform(MATRIX_TEXTURE1);

		if (mc_pEnvironmentData != NULL)
			SHADERMANAGER.SetFogColor(mc_pEnvironmentData->FogColor);
	}
#endif

	SHADERMANAGER.SavePipelineState(PSTATE_DEPTHWRITEMASK, TRUE);

	bool m_isDisableSortRendering=false;

	if (m_isDisableSortRendering)
	{
		for (int i = 0; i < AROUND_AREA_NUM; ++i)
		{
			CArea * pArea;
			if (GetAreaPointer(i, &pArea))
			{
				pArea->Render();

				m_dwRenderedCRCNum += pArea->DEBUG_GetRenderedCRCNum();
				m_dwRenderedGraphicThingInstanceNum += pArea->DEBUG_GetRenderedGrapphicThingInstanceNum();

				CArea::TCRCWithNumberVector & rCRCWithNumberVector = pArea->DEBUG_GetRenderedCRCWithNumVector();

				CArea::TCRCWithNumberVector::iterator aIterator = rCRCWithNumberVector.begin();
				while (aIterator != rCRCWithNumberVector.end())
				{
					DWORD dwCRC = (*aIterator++).dwCRC;

					CArea::TCRCWithNumberVector::iterator aCRCWithNumberVectorIterator =
						std::find_if(m_dwRenderedCRCWithNumberVector.begin(), m_dwRenderedCRCWithNumberVector.end(), CArea::FFindIfCRC(dwCRC));

					if ( m_dwRenderedCRCWithNumberVector.end() == aCRCWithNumberVectorIterator)
					{
						CArea::TCRCWithNumber aCRCWithNumber;
						aCRCWithNumber.dwCRC = dwCRC;
						aCRCWithNumber.dwNumber = 1;
						m_dwRenderedCRCWithNumberVector.push_back(aCRCWithNumber);
					}
					else
					{
						CArea::TCRCWithNumber & rCRCWithNumber = *aCRCWithNumberVectorIterator;
						rCRCWithNumber.dwNumber += 1;
					}
				}
	#ifdef WORLD_EDITOR
				if (bRenderAmbience)
					pArea->RenderAmbience();
	#endif
			}
		}

		std::sort(m_dwRenderedCRCWithNumberVector.begin(), m_dwRenderedCRCWithNumberVector.end(), CArea::CRCNumComp());
	}
	else
	{
		static std::vector<CGraphicThingInstance*> s_kVct_pkOpaqueThingInstSort;
		s_kVct_pkOpaqueThingInstSort.clear();

		for (int i = 0; i < AROUND_AREA_NUM; ++i)
		{
			CArea * pArea;
			if (GetAreaPointer(i, &pArea))
			{
				pArea->CollectRenderingObject(s_kVct_pkOpaqueThingInstSort);
#ifdef WORLD_EDITOR
				if (bRenderAmbience)
					pArea->RenderAmbience();
#endif
			}

		}

		std::sort(s_kVct_pkOpaqueThingInstSort.begin(), s_kVct_pkOpaqueThingInstSort.end(), CMapOutdoor_LessThingInstancePtrRenderOrder());
		std::for_each(s_kVct_pkOpaqueThingInstSort.begin(), s_kVct_pkOpaqueThingInstSort.end(), CMapOutdoor_FOpaqueThingInstanceRender());
	}

	SHADERMANAGER.RestorePipelineState(PSTATE_DEPTHWRITEMASK);

#ifndef WORLD_EDITOR
	// Shadow Receiver
	if (m_bDrawShadow && m_bDrawChrShadow)
	{
		std::for_each(m_ShadowReceiverVector.begin(), m_ShadowReceiverVector.end(), std::void_mem_fun(&CGraphicObjectInstance::Show));
	}
#endif
}

void CMapOutdoor::RenderBlendArea()
{
	if (!IsVisiblePart(PART_OBJECT))
		return;

	static std::vector<CGraphicThingInstance*> s_kVct_pkBlendThingInstSort;
	s_kVct_pkBlendThingInstSort.clear();

	for (int i = 0; i < AROUND_AREA_NUM; ++i)
	{
		CArea * pArea;
		if (GetAreaPointer(i, &pArea))
		{
			pArea->CollectBlendRenderingObject(s_kVct_pkBlendThingInstSort);
		}
	}

	if (s_kVct_pkBlendThingInstSort.size() != 0)
	{
		// Shader handles texture blending

		std::sort(s_kVct_pkBlendThingInstSort.begin(), s_kVct_pkBlendThingInstSort.end(), CMapOutdoor_LessThingInstancePtrRenderOrder());

		SHADERMANAGER.SavePipelineState(PSTATE_DEPTHWRITEMASK, TRUE);
		SHADERMANAGER.SavePipelineState(PSTATE_BLENDENABLE, TRUE);
		SHADERMANAGER.SavePipelineState(PSTATE_SRCBLEND, BLEND_SRCALPHA);
		SHADERMANAGER.SavePipelineState(PSTATE_DESTBLEND, BLEND_INVSRCALPHA);

		std::for_each(s_kVct_pkBlendThingInstSort.begin(), s_kVct_pkBlendThingInstSort.end(), CMapOutdoor_FBlendThingInstanceRender());

		SHADERMANAGER.RestorePipelineState(PSTATE_BLENDENABLE);
		SHADERMANAGER.RestorePipelineState(PSTATE_SRCBLEND);
		SHADERMANAGER.RestorePipelineState(PSTATE_DESTBLEND);
		SHADERMANAGER.RestorePipelineState(PSTATE_DEPTHWRITEMASK);
	}
}
void CMapOutdoor::RenderDungeon()
{
	for (int i = 0; i < AROUND_AREA_NUM; ++i)
	{
		CArea * pArea;
		if (!GetAreaPointer(i, &pArea))
			continue;
		pArea->RenderDungeon();
	}
}

void CMapOutdoor::RenderPCBlocker()
{
#ifndef WORLD_EDITOR
	// PCBlocker
	if (m_PCBlockerVector.size() != 0)
	{
		SHADERMANAGER.SetDefaultTexture(0);
		SHADERMANAGER.SavePipelineState(PSTATE_BLENDENABLE, TRUE);
		SHADERMANAGER.SaveSamplerState(1, SAMPLER_ADDRESSU, ADDRESS_CLAMP);
		SHADERMANAGER.SaveSamplerState(1, SAMPLER_ADDRESSV, ADDRESS_CLAMP);

		SHADERMANAGER.SaveTransform(MATRIX_TEXTURE1, &m_matBuildingTransparent);
		CGraphicTexture* pBuildingTex = m_BuildingTransparentImageInstance.GetTexturePointer();
		if (pBuildingTex)
			SHADERMANAGER.SetShaderResource(1, pBuildingTex->GetD3DTexture());

		std::for_each(m_PCBlockerVector.begin(), m_PCBlockerVector.end(), FRenderPCBlocker());

		SHADERMANAGER.SetShaderResource(1, NULL);
		SHADERMANAGER.RestoreTransform(MATRIX_TEXTURE1);
		SHADERMANAGER.RestoreSamplerState(1, SAMPLER_ADDRESSU);
		SHADERMANAGER.RestoreSamplerState(1, SAMPLER_ADDRESSV);
		SHADERMANAGER.RestorePipelineState(PSTATE_BLENDENABLE);
	}
#endif
}

void CMapOutdoor::SelectIndexBuffer(BYTE byLODLevel, WORD * pwPrimitiveCount, EPrimitiveTopology * pePrimitiveType)
{
#ifdef WORLD_EDITOR
	*pwPrimitiveCount = m_wNumIndices - 2;
	*pePrimitiveType = TOPOLOGY_TRIANGLESTRIP;
	SHADERMANAGER.SetIndexBuffer(m_IndexBuffer.GetD3DIndexBuffer());
#else
	if (0 == byLODLevel)
	{
		*pwPrimitiveCount = m_wNumIndices[byLODLevel] - 2;
		*pePrimitiveType = TOPOLOGY_TRIANGLESTRIP;
	}
	else
	{
		*pwPrimitiveCount =  m_wNumIndices[byLODLevel]/3;
		*pePrimitiveType = TOPOLOGY_TRIANGLELIST;
	}
	SHADERMANAGER.SetIndexBuffer(m_IndexBuffer[byLODLevel].GetD3DIndexBuffer());
#endif
}

void CMapOutdoor::SetPatchDrawVector()
{
	assert(NULL!=m_pTerrainPatchProxyList && "CMapOutdoor::__SetPatchDrawVector");

	m_PatchDrawStructVector.clear();

	std::vector<std::pair<float, long> >::iterator aDistancePatchVectorIterator;

	TPatchDrawStruct aPatchDrawStruct;

	aDistancePatchVectorIterator = m_PatchVector.begin();
	while(aDistancePatchVectorIterator != m_PatchVector.end())
	{
		std::pair<float, long> adistancePatchPair = *aDistancePatchVectorIterator;

		CTerrainPatchProxy * pTerrainPatchProxy = &m_pTerrainPatchProxyList[adistancePatchPair.second];

		if (!pTerrainPatchProxy->isUsed())
		{
			++aDistancePatchVectorIterator;
			continue;
		}

		long lPatchNum = pTerrainPatchProxy->GetPatchNum();
		if (lPatchNum < 0)
		{
			++aDistancePatchVectorIterator;
			continue;
		}

		BYTE byTerrainNum = pTerrainPatchProxy->GetTerrainNum();
		if (0xFF == byTerrainNum)
		{
			++aDistancePatchVectorIterator;
			continue;
		}

		CTerrain * pTerrain;
		if (!GetTerrainPointer(byTerrainNum, &pTerrain))
		{
			++aDistancePatchVectorIterator;
			continue;
		}

		aPatchDrawStruct.fDistance				= adistancePatchPair.first;
		aPatchDrawStruct.byTerrainNum			= byTerrainNum;
		aPatchDrawStruct.lPatchNum				= lPatchNum;
		aPatchDrawStruct.pTerrainPatchProxy		= pTerrainPatchProxy;

		m_PatchDrawStructVector.push_back(aPatchDrawStruct);

		++aDistancePatchVectorIterator;
	}

	std::stable_sort(m_PatchDrawStructVector.begin(), m_PatchDrawStructVector.end(), FSortPatchDrawStructWithTerrainNum());
}

float CMapOutdoor::__GetNoFogDistance()
{
	return (float)(CTerrainImpl::CELLSCALE * m_lViewRadius) * 0.5f;
}

float CMapOutdoor::__GetFogDistance()
{
	return (float)(CTerrainImpl::CELLSCALE * m_lViewRadius) * 0.75f;
}

struct FPatchNumMatch
{
	long m_lPatchNumToCheck;
	FPatchNumMatch(long lPatchNum)
	{
		m_lPatchNumToCheck = lPatchNum;
	}
	bool operator() (std::pair<long, BYTE> aPair)
	{
		return m_lPatchNumToCheck == aPair.first;
	}
};

void CMapOutdoor::NEW_DrawWireFrame(CTerrainPatchProxy * pTerrainPatchProxy, WORD wPrimitiveCount, EPrimitiveTopology ePrimitiveType)
{
	DWORD dwFillMode = SHADERMANAGER.GetPipelineState(PSTATE_FILLMODE);
	SHADERMANAGER.SetPipelineState(PSTATE_FILLMODE, FILL_WIREFRAME);

	bool bFogEnable = SHADERMANAGER.GetFogEnabled();
	SHADERMANAGER.SetFogEnabled(false);

	SHADERMANAGER.SetDefaultTexture(0);
	SHADERMANAGER.SetShaderResource(1, NULL);

	SHADERMANAGER.DrawIndexed(ePrimitiveType, 0, m_iPatchTerrainVertexCount, 0, wPrimitiveCount);

	SHADERMANAGER.SetPipelineState(PSTATE_FILLMODE, dwFillMode);
	SHADERMANAGER.SetFogEnabled(bFogEnable);
}

void CMapOutdoor::DrawWireFrame(long patchnum, WORD wPrimitiveCount, EPrimitiveTopology ePrimitiveType)
{
	assert(NULL!=m_pTerrainPatchProxyList && "CMapOutdoor::DrawWireFrame");

	CTerrainPatchProxy * pTerrainPatchProxy= &m_pTerrainPatchProxyList[patchnum];

	if (!pTerrainPatchProxy->isUsed())
		return;

	long sPatchNum = pTerrainPatchProxy->GetPatchNum();
	if (sPatchNum < 0)
		return;
	BYTE ucTerrainNum = pTerrainPatchProxy->GetTerrainNum();
	if (0xFF == ucTerrainNum)
		return;

	DWORD dwFillMode = SHADERMANAGER.GetPipelineState(PSTATE_FILLMODE);
	SHADERMANAGER.SetPipelineState(PSTATE_FILLMODE, FILL_WIREFRAME);

	bool bFogEnable2 = SHADERMANAGER.GetFogEnabled();
	SHADERMANAGER.SetFogEnabled(false);

	SHADERMANAGER.SetDefaultTexture(0);
	SHADERMANAGER.SetShaderResource(1, NULL);

	SHADERMANAGER.DrawIndexed(ePrimitiveType, 0, m_iPatchTerrainVertexCount, 0, wPrimitiveCount);

	SHADERMANAGER.SetPipelineState(PSTATE_FILLMODE, dwFillMode);
	SHADERMANAGER.SetFogEnabled(bFogEnable2);
}

// Attr
void CMapOutdoor::RenderMarkedArea()
{
	if (!m_pTerrainPatchProxyList)
		return;

	m_matWorldForCommonUse._41 = 0.0f;
	m_matWorldForCommonUse._42 = 0.0f;
	SHADERMANAGER.SetMatrix(MATRIX_WORLD, &m_matWorldForCommonUse);

	WORD wPrimitiveCount;
	EPrimitiveTopology eType;
	SelectIndexBuffer(0, &wPrimitiveCount, &eType);

	Matrix matTexTransform, matTexTransformTemp;

	MatrixScaling(&matTexTransform, m_fTerrainTexCoordBase * 32.0f, -m_fTerrainTexCoordBase * 32.0f, 0.0f);
	MatrixMultiply(&matTexTransform, &m_matViewInverse, &matTexTransform);
	SHADERMANAGER.SaveTransform(MATRIX_TEXTURE0, &matTexTransform);
	SHADERMANAGER.SaveTransform(MATRIX_TEXTURE1, &matTexTransform);

	SHADERMANAGER.SavePipelineState(PSTATE_BLENDENABLE, TRUE);
	SHADERMANAGER.SavePipelineState(PSTATE_SRCBLEND, BLEND_SRCALPHA);
	SHADERMANAGER.SavePipelineState(PSTATE_DESTBLEND, BLEND_INVSRCALPHA);

	static long lStartTime = timeGetTime();
	float fTime = float((timeGetTime() - lStartTime)%3000) / 3000.0f;
	float fAlpha = fabs(fTime - 0.5f) / 2.0f + 0.1f;
	if (SHADERMANAGER.IsInitialized())
		SHADERMANAGER.SetParticleColor(Color(1.0f, 1.0f, 1.0f, fAlpha));
	SHADERMANAGER.SaveSamplerState(1, SAMPLER_MINFILTER, FILTER_ANISOTROPIC);
	SHADERMANAGER.SaveSamplerState(1, SAMPLER_MAGFILTER, FILTER_ANISOTROPIC);
	SHADERMANAGER.SaveSamplerState(1, SAMPLER_MIPFILTER, FILTER_ANISOTROPIC);
	SHADERMANAGER.SaveSamplerState(1, SAMPLER_ADDRESSU, ADDRESS_CLAMP);
	SHADERMANAGER.SaveSamplerState(1, SAMPLER_ADDRESSV, ADDRESS_CLAMP);

	SHADERMANAGER.SetShaderResource(0, m_attrImageInstance.GetTexturePointer()->GetD3DTexture());

	RecurseRenderAttr(m_pRootNode);

	SHADERMANAGER.RestoreSamplerState(1, SAMPLER_MINFILTER);
	SHADERMANAGER.RestoreSamplerState(1, SAMPLER_MAGFILTER);
	SHADERMANAGER.RestoreSamplerState(1, SAMPLER_MIPFILTER);
	SHADERMANAGER.RestoreSamplerState(1, SAMPLER_ADDRESSU);
	SHADERMANAGER.RestoreSamplerState(1, SAMPLER_ADDRESSV);

	SHADERMANAGER.RestoreTransform(MATRIX_TEXTURE0);
	SHADERMANAGER.RestoreTransform(MATRIX_TEXTURE1);

	SHADERMANAGER.RestorePipelineState(PSTATE_BLENDENABLE);
	SHADERMANAGER.RestorePipelineState(PSTATE_SRCBLEND);
	SHADERMANAGER.RestorePipelineState(PSTATE_DESTBLEND);
}

void CMapOutdoor::RecurseRenderAttr(CTerrainQuadtreeNode *Node, bool bCullEnable)
{
	if (bCullEnable)
	{
		if (__RenderTerrain_RecurseRenderQuadTree_CheckBoundingCircle(Node->center, Node->radius)==VIEW_NONE)
			return;
	}

	{
		if (Node->Size == 1)
		{
			DrawPatchAttr(Node->PatchNum);
		}
		else
		{
			if (Node->NW_Node != NULL)
				RecurseRenderAttr(Node->NW_Node, bCullEnable);
			if (Node->NE_Node != NULL)
				RecurseRenderAttr(Node->NE_Node, bCullEnable);
			if (Node->SW_Node != NULL)
				RecurseRenderAttr(Node->SW_Node, bCullEnable);
			if (Node->SE_Node != NULL)
				RecurseRenderAttr(Node->SE_Node, bCullEnable);
		}
 	}
}

void CMapOutdoor::DrawPatchAttr(long patchnum)
{
	CTerrainPatchProxy * pTerrainPatchProxy = &m_pTerrainPatchProxyList[patchnum];
	if (!pTerrainPatchProxy->isUsed())
		return;

	long sPatchNum = pTerrainPatchProxy->GetPatchNum();
	if (sPatchNum < 0)
		return;

	BYTE ucTerrainNum = pTerrainPatchProxy->GetTerrainNum();
	if (0xFF == ucTerrainNum)
		return;

	// Deal with this material buffer
	CTerrain * pTerrain;
	if (!GetTerrainPointer(ucTerrainNum, &pTerrain))
		return;

	if (!pTerrain->IsMarked())
		return;

	WORD wCoordX, wCoordY;
	pTerrain->GetCoordinate(&wCoordX, &wCoordY);

	m_matWorldForCommonUse._41 = -(float) (wCoordX * CTerrainImpl::XSIZE * CTerrainImpl::CELLSCALE);
	m_matWorldForCommonUse._42 = (float) (wCoordY * CTerrainImpl::YSIZE * CTerrainImpl::CELLSCALE);

	Matrix matTexTransform, matTexTransformTemp;
	MatrixMultiply(&matTexTransform, &m_matViewInverse, &m_matWorldForCommonUse);
	MatrixMultiply(&matTexTransform, &matTexTransform, &m_matStaticShadow);
	SHADERMANAGER.SetMatrix(MATRIX_TEXTURE1, &matTexTransform);

	TTerrainSplatPatch & rAttrSplatPatch = pTerrain->GetMarkedSplatPatch();
 	SHADERMANAGER.SetShaderResource(1, rAttrSplatPatch.Splats[0].pd3dTexture);

	SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_PN);
	SHADERMANAGER.SetVertexBuffer(0, pTerrainPatchProxy->HardwareTransformPatch_GetVertexBufferPtr()->GetD3DVertexBuffer(), m_iPatchTerrainVertexSize);

#ifdef WORLD_EDITOR
	SHADERMANAGER.DrawIndexed(TOPOLOGY_TRIANGLESTRIP, 0, m_iPatchTerrainVertexCount, 0, m_wNumIndices - 2);
#else
	SHADERMANAGER.DrawIndexed(TOPOLOGY_TRIANGLESTRIP, 0, m_iPatchTerrainVertexCount, 0, m_wNumIndices[0] - 2);
#endif
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
