#include "StdAfx.h"
#include "../eterLib/ShaderManager.h"
#include "../EterPack/EterPackManager.h"
#include "../eterLib/ShaderInit.h"

#include "MapManager.h"
#include "MapOutdoor.h"

#include "PropertyLoader.h"

extern float MIN_FOG;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CMapManager::IsMapOutdoor()
{
	if (m_pkMap)
		return true;

	return false;
}

CMapOutdoor& CMapManager::GetMapOutdoorRef()
{
	assert(NULL!=m_pkMap);
	return *m_pkMap;
}

CMapManager::CMapManager() : mc_pcurEnvironmentData(NULL)
{
	m_pkMap = NULL;
	m_isSoftwareTilingEnableReserved=false;
#if defined(__BL_FOG_FIX__)
	m_isFogModeEnabled = false;
#endif

//	Initialize();
}

CMapManager::~CMapManager()
{
	Destroy();
}

bool CMapManager::IsSoftwareTilingEnable()
{
	return CTerrainPatch::SOFTWARE_TRANSFORM_PATCH_ENABLE;
}

void CMapManager::ReserveSoftwareTilingEnable(bool isEnable)
{
	m_isSoftwareTilingEnableReserved=isEnable;
}

void CMapManager::Initialize()
{
	mc_pcurEnvironmentData = NULL;
	__LoadMapInfoVector();
}

void CMapManager::Create()
{
	assert(NULL==m_pkMap && "CMapManager::Create");
	if (m_pkMap)
	{
		Clear();
		return;
	}

	CTerrainPatch::SOFTWARE_TRANSFORM_PATCH_ENABLE=m_isSoftwareTilingEnableReserved;

	m_pkMap = (CMapOutdoor*)AllocMap();

	assert(NULL!=m_pkMap && "CMapManager::Create MAP is NULL");
}

void CMapManager::Destroy()
{
	stl_wipe_second(m_EnvironmentDataMap);

	if (m_pkMap)
	{
		m_pkMap->Clear();
		delete m_pkMap;
		m_pkMap = NULL;
	}
}

void CMapManager::Clear()
{
	if (m_pkMap)
		m_pkMap->Clear();
}

CMapBase * CMapManager::AllocMap()
{
	return new CMapOutdoor;
}

//////////////////////////////////////////////////////////////////////////
// Map
//////////////////////////////////////////////////////////////////////////
void CMapManager::LoadProperty()
{
	CPropertyLoader PropertyLoader;
	PropertyLoader.SetPropertyManager(&m_PropertyManager);
	PropertyLoader.Create("*.*", "Property");
}

bool CMapManager::LoadMap(const std::string & c_rstrMapName, float x, float y, float z)
{
	CMapOutdoor& rkMap = GetMapOutdoorRef();

	rkMap.Leave();
	rkMap.SetName(c_rstrMapName);
	rkMap.LoadProperty();

	int mapType = rkMap.GetType();

	if ( CMapBase::MAPTYPE_INDOOR == mapType)
	{
		TraceError("CMapManager::LoadMap() Indoor Map Load Failed");
		return false;
	}
	else if (CMapBase::MAPTYPE_OUTDOOR == mapType)
	{
		if (!rkMap.Load(x, y, z))
		{
			TraceError("CMapManager::LoadMap() Outdoor Map Load Failed");
			return false;
		}

		RegisterEnvironmentData(0, rkMap.GetEnvironmentDataName().c_str());
		SetEnvironmentData(0);
	}
	else
	{
		TraceError("CMapManager::LoadMap() Invalid Map Type");
		return false;
	}

	rkMap.Enter();
	return true;
}

bool CMapManager::IsMapReady()
{
	if (!m_pkMap)
		return false;

	return m_pkMap->IsReady();
}

bool CMapManager::UnloadMap(const std::string c_strMapName)
{
	CMapOutdoor& rkMap=GetMapOutdoorRef();
	if (c_strMapName != rkMap.GetName() && "" != rkMap.GetName())
	{
		LogBoxf("%s: Unload Map Failed", c_strMapName.c_str());
		return false;
	}

	Clear();
	return true;
}

bool CMapManager::UpdateMap(float fx, float fy, float fz)
{
	if (!m_pkMap)
		return false;

	CMapOutdoor& rkMap=GetMapOutdoorRef();
	return rkMap.Update(fx, -fy, fz);
}

void CMapManager::UpdateAroundAmbience(float fx, float fy, float fz)
{
	if (!m_pkMap)
		return;

	CMapOutdoor& rkMap=GetMapOutdoorRef();
	rkMap.UpdateAroundAmbience(fx, -fy, fz);
}

float CMapManager::GetHeight(float fx, float fy)
{
	if (!m_pkMap)
	{
		TraceError("CMapManager::GetHeight(%f, %f) - Accessing in a not initialized map", fx, fy);
		return 0.0f;
	}
	CMapOutdoor& rkMap=GetMapOutdoorRef();
	return rkMap.GetHeight(fx, fy);
}

float CMapManager::GetTerrainHeight(float fx, float fy)
{
	if (!m_pkMap)
	{
		TraceError("CMapManager::GetTerrainHeight(%f, %f) - Accessing in a not initialized map", fx, fy);
		return 0.0f;
	}
	CMapOutdoor& rkMap=GetMapOutdoorRef();
	return rkMap.GetTerrainHeight(fx, fy);
}

bool CMapManager::GetWaterHeight(int iX, int iY, long * plWaterHeight)
{
	if (!m_pkMap)
	{
		TraceError("CMapManager::GetTerrainHeight(%f, %f) - Accessing in a not initialized map", iX, iY);
		return false;
	}

	CMapOutdoor& rkMap=GetMapOutdoorRef();
	return rkMap.GetWaterHeight(iX, iY, plWaterHeight);
}

//////////////////////////////////////////////////////////////////////////
// Environment
//////////////////////////////////////////////////////////////////////////
void CMapManager::BeginEnvironment()
{
	if (!m_pkMap)
		return;

	if (!mc_pcurEnvironmentData)
		return;

	CMapOutdoor& rkMap=GetMapOutdoorRef();

	// Light always on
	bool bSavedLighting = SHADERMANAGER.GetLightingEnabled();
	SHADERMANAGER.SetLightingEnabled(true);

	// Fog
	bool bSavedFog = SHADERMANAGER.GetFogEnabled();

#ifdef ENABLE_HEIGHT_FOG
	{
		const Color& sunDiffuse = mc_pcurEnvironmentData->DirLights[ENV_DIRLIGHT_BACKGROUND].Diffuse;
		const Color& sunAmbient = mc_pcurEnvironmentData->DirLights[ENV_DIRLIGHT_BACKGROUND].Ambient;

		float intensity = (sunDiffuse.r + sunDiffuse.g + sunDiffuse.b
						+ sunAmbient.r + sunAmbient.g + sunAmbient.b) / 6.0f;
		intensity = max(0.15f, intensity);
		float fogR = sunDiffuse.r * 0.4f + sunAmbient.r * 0.2f + 0.3f * intensity;
		float fogG = sunDiffuse.g * 0.4f + sunAmbient.g * 0.2f + 0.35f * intensity;
		float fogB = sunDiffuse.b * 0.4f + sunAmbient.b * 0.2f + 0.4f * intensity;

		float maxC = max(fogR, max(fogG, fogB));
		if (maxC > 1.0f) { fogR /= maxC; fogG /= maxC; fogB /= maxC; }

		BYTE rr = (BYTE)(fogR * 255.0f);
		BYTE gg = (BYTE)(fogG * 255.0f);
		BYTE bb = (BYTE)(fogB * 255.0f);
		DWORD dwHeightFogColor = 0xFF000000 | (rr << 16) | (gg << 8) | bb;

		float fFogNear = MIN_FOG;
		float fFogFar  = MIN_FOG * 1.4f;

		SHADERMANAGER.SetFogEnabled(true);
		SHADERMANAGER.SetFogParams(fFogNear, fFogFar, dwHeightFogColor);

		CSpeedTreeForestDX11& rkForest = CSpeedTreeForestDX11::Instance();
		rkForest.SetFog(fFogNear, fFogFar);

		Color fogColor(dwHeightFogColor);
		UpdateShaderFogConstants(true, fFogNear, fFogFar, 0.0f, fogColor, true);
	}
#else
#if defined(__BL_FOG_FIX__)
	SHADERMANAGER.SetFogEnabled(m_isFogModeEnabled ? mc_pcurEnvironmentData->bFogEnable : false);
#else
	SHADERMANAGER.SetFogEnabled(mc_pcurEnvironmentData->bFogEnable != 0);
#endif

	if (mc_pcurEnvironmentData->bFogEnable)
	{
		DWORD dwFogColor = mc_pcurEnvironmentData->FogColor;
		SHADERMANAGER.SetFogColor(dwFogColor);

		if (mc_pcurEnvironmentData->bDensityFog)
		{
			float fDensity = 0.00015f;
			Color fogColor(dwFogColor);
			UpdateShaderFogConstants(true, 0.0f, 0.0f, fDensity, fogColor, false);
		}
		else
		{
			CSpeedTreeForestDX11& rkForest=CSpeedTreeForestDX11::Instance();
			rkForest.SetFog(
				mc_pcurEnvironmentData->GetFogNearDistance(),
				mc_pcurEnvironmentData->GetFogFarDistance()
			);

			float fFogNear=mc_pcurEnvironmentData->GetFogNearDistance();
			float fFogFar=mc_pcurEnvironmentData->GetFogFarDistance();
			SHADERMANAGER.SetFogParams(fFogNear, fFogFar, dwFogColor);

			Color fogColor(dwFogColor);
			UpdateShaderFogConstants(true, fFogNear, fFogFar, 0.0f, fogColor, true);
		}
	}
	else
	{
		UpdateShaderFogConstants(false, 0.0f, 0.0f, 0.0f, Color(1, 1, 1, 1), true);
	}
#endif

	rkMap.OnBeginEnvironment();
}

#if defined(__BL_FOG_FIX__)
void CMapManager::SetFogMode(bool bEnable)
{
	m_isFogModeEnabled = bEnable;
}

bool CMapManager::GetFogMode() const
{
	return m_isFogModeEnabled;
}
#endif

void CMapManager::EndEnvironment()
{
	if (!mc_pcurEnvironmentData)
		return;

	// States will be restored by next BeginEnvironment call
}

void CMapManager::SetEnvironmentData(int nEnvDataIndex)
{
	const TEnvironmentData * c_pEnvironmenData;

	if (GetEnvironmentData(nEnvDataIndex, &c_pEnvironmenData))
		SetEnvironmentDataPtr(c_pEnvironmenData);
}

void CMapManager::SetEnvironmentDataPtr(const TEnvironmentData * c_pEnvironmentData)
{
	if (!m_pkMap)
		return;

	if (!c_pEnvironmentData)
	{
		assert(!"null environment data");
		TraceError("null environment data");
		return;
	}

	CMapOutdoor& rkMap=GetMapOutdoorRef();

	mc_pcurEnvironmentData = c_pEnvironmentData;

	rkMap.SetEnvironmentDataPtr(mc_pcurEnvironmentData);
}

void CMapManager::ResetEnvironmentDataPtr(const TEnvironmentData * c_pEnvironmentData)
{
	if (!m_pkMap)
		return;

	if (!c_pEnvironmentData)
	{
		assert(!"null environment data");
		TraceError("null environment data");
		return;
	}

	CMapOutdoor& rkMap=GetMapOutdoorRef();

	mc_pcurEnvironmentData = c_pEnvironmentData;
	rkMap.ResetEnvironmentDataPtr(mc_pcurEnvironmentData);
}

void CMapManager::BlendEnvironmentData(const TEnvironmentData * c_pEnvironmentData, int iTransitionTime)
{
}

bool CMapManager::RegisterEnvironmentData(DWORD dwIndex, const char * c_szFileName)
{
	TEnvironmentData * pEnvironmentData = AllocEnvironmentData();

	if (!LoadEnvironmentData(c_szFileName, pEnvironmentData))
	{
		DeleteEnvironmentData(pEnvironmentData);
		return false;
	}

	TEnvironmentDataMap::iterator f=m_EnvironmentDataMap.find(dwIndex);
	if (m_EnvironmentDataMap.end()==f)
	{
		m_EnvironmentDataMap.insert(TEnvironmentDataMap::value_type(dwIndex, pEnvironmentData));
	}
	else
	{
		delete f->second;
		f->second=pEnvironmentData;
	}
	return true;
}

void CMapManager::GetCurrentEnvironmentData(const TEnvironmentData ** c_ppEnvironmentData)
{
	*c_ppEnvironmentData = mc_pcurEnvironmentData;
}

bool CMapManager::GetEnvironmentData(DWORD dwIndex, const TEnvironmentData ** c_ppEnvironmentData)
{
	TEnvironmentDataMap::iterator itor = m_EnvironmentDataMap.find(dwIndex);

	if (m_EnvironmentDataMap.end() == itor)
	{
		*c_ppEnvironmentData = NULL;
		return false;
	}

	*c_ppEnvironmentData = itor->second;
	return true;
}

void CMapManager::RefreshPortal()
{
	if (!IsMapReady())
		return;

	CMapOutdoor & rMap = GetMapOutdoorRef();
	for (int i = 0; i < AROUND_AREA_NUM; ++i)
	{
		CArea * pArea;
		if (!rMap.GetAreaPointer(i, &pArea))
			continue;

		pArea->RefreshPortal();
	}
}

void CMapManager::ClearPortal()
{
	if (!IsMapReady())
		return;

	CMapOutdoor & rMap = GetMapOutdoorRef();
	for (int i = 0; i < AROUND_AREA_NUM; ++i)
	{
		CArea * pArea;
		if (!rMap.GetAreaPointer(i, &pArea))
			continue;

		pArea->ClearPortal();
	}
}

void CMapManager::AddShowingPortalID(int iID)
{
	if (!IsMapReady())
		return;

	CMapOutdoor & rMap = GetMapOutdoorRef();
	for (int i = 0; i < AROUND_AREA_NUM; ++i)
	{
		CArea * pArea;
		if (!rMap.GetAreaPointer(i, &pArea))
			continue;

		pArea->AddShowingPortalID(iID);
	}
}

TEnvironmentData * CMapManager::AllocEnvironmentData()
{
	TEnvironmentData * pEnvironmentData = new TEnvironmentData;
	Environment_Init(*pEnvironmentData);
	return pEnvironmentData;
}

void CMapManager::DeleteEnvironmentData(TEnvironmentData * pEnvironmentData)
{
	delete pEnvironmentData;
	pEnvironmentData = NULL;
}

BOOL CMapManager::LoadEnvironmentData(const char * c_szFileName, TEnvironmentData * pEnvironmentData)
{
	if (!pEnvironmentData)
		return FALSE;

	return (BOOL)Environment_Load(*pEnvironmentData, c_szFileName);
}

DWORD CMapManager::GetShadowMapColor(float fx, float fy)
{
	if (!IsMapReady())
		return 0xFFFFFFFF;

	CMapOutdoor& rkMap=GetMapOutdoorRef();
	return rkMap.GetShadowMapColor(fx, fy);
}

std::vector<int> & CMapManager::GetRenderedSplatNum(int * piPatch, int * piSplat, float * pfSplatRatio)
{
	if (!m_pkMap)
	{
		static std::vector<int> s_emptyVector;
		*piPatch = 0;
		*piSplat = 0;
		return s_emptyVector;
	}

	CMapOutdoor& rkMap=GetMapOutdoorRef();
	return rkMap.GetRenderedSplatNum(piPatch, piSplat, pfSplatRatio);
}

CArea::TCRCWithNumberVector & CMapManager::GetRenderedGraphicThingInstanceNum(DWORD * pdwGraphicThingInstanceNum, DWORD * pdwCRCNum)
{
	if (!m_pkMap)
	{
		static CArea::TCRCWithNumberVector s_emptyVector;
		*pdwGraphicThingInstanceNum = 0;
		*pdwCRCNum = 0;
		return s_emptyVector;
	}

	CMapOutdoor& rkMap=GetMapOutdoorRef();
	return rkMap.GetRenderedGraphicThingInstanceNum(pdwGraphicThingInstanceNum, pdwCRCNum);
}

bool CMapManager::GetNormal(int ix, int iy, Vector3 * pv3Normal)
{
	if (!IsMapReady())
		return false;

	CMapOutdoor& rkMap=GetMapOutdoorRef();
	return rkMap.GetNormal(ix, iy, pv3Normal);
}

bool CMapManager::isPhysicalCollision(const Vector3 & c_rvCheckPosition)
{
	if (!IsMapReady())
		return false;

	CMapOutdoor& rkMap=GetMapOutdoorRef();
	return rkMap.isAttrOn(c_rvCheckPosition.x, -c_rvCheckPosition.y, CTerrainImpl::ATTRIBUTE_BLOCK);
}

bool CMapManager::isAttrOn(float fX, float fY, BYTE byAttr)
{
	if (!IsMapReady())
		return false;

	CMapOutdoor& rkMap=GetMapOutdoorRef();
	return rkMap.isAttrOn(fX, fY, byAttr);
}

bool CMapManager::GetAttr(float fX, float fY, BYTE * pbyAttr)
{
	if (!IsMapReady())
		return false;

	CMapOutdoor& rkMap=GetMapOutdoorRef();
	return rkMap.GetAttr(fX, fY, pbyAttr);
}

bool CMapManager::isAttrOn(int iX, int iY, BYTE byAttr)
{
	if (!IsMapReady())
		return false;

	CMapOutdoor& rkMap=GetMapOutdoorRef();
	return rkMap.isAttrOn(iX, iY, byAttr);
}

bool CMapManager::GetAttr(int iX, int iY, BYTE * pbyAttr)
{
	if (!IsMapReady())
		return false;

	CMapOutdoor& rkMap=GetMapOutdoorRef();
	return rkMap.GetAttr(iX, iY, pbyAttr);
}

// 2004.10.14.myevan.TEMP_CAreaLoaderThread
/*
bool CMapManager::BGLoadingEnable()
{
	if (!IsMapReady())
		return false;
	return ((CMapOutdoor*)m_pMap)->BGLoadingEnable();
}

void CMapManager::BGLoadingEnable(bool bBGLoadingEnable)
{
	if (!IsMapReady())
		return;
	((CMapOutdoor*)m_pMap)->BGLoadingEnable(bBGLoadingEnable);
}
*/

void CMapManager::SetTerrainRenderSort(CMapOutdoor::ETerrainRenderSort eTerrainRenderSort)
{
	if (!IsMapReady())
		return;

	CMapOutdoor& rkMap=GetMapOutdoorRef();
	rkMap.SetTerrainRenderSort(eTerrainRenderSort);
}

void CMapManager::SetTransparentTree(bool bTransparenTree)
{
	if (!IsMapReady())
		return;

	CMapOutdoor& rkMap=GetMapOutdoorRef();
	rkMap.SetTransparentTree(bTransparenTree);
}

CMapOutdoor::ETerrainRenderSort CMapManager::GetTerrainRenderSort()
{
	if (!IsMapReady())
		return CMapOutdoor::DISTANCE_SORT;

	CMapOutdoor& rkMap=GetMapOutdoorRef();
	return rkMap.GetTerrainRenderSort();
}

void CMapManager::GetBaseXY(DWORD * pdwBaseX, DWORD * pdwBaseY)
{
	if (!IsMapReady())
	{
		*pdwBaseX = 0;
		*pdwBaseY = 0;
	}

	CMapOutdoor& rkMap=GetMapOutdoorRef();
	rkMap.GetBaseXY(pdwBaseX, pdwBaseY);
}

void CMapManager::__LoadMapInfoVector()
{
	CMappedFile kFile;
	LPCVOID pData;
	if (!CEterPackManager::Instance().Get(kFile, m_stAtlasInfoFileName.c_str(), &pData))
		if (!CEterPackManager::Instance().Get(kFile, "AtlasInfo.txt", &pData))
			return;

	CMemoryTextFileLoader textFileLoader;
	textFileLoader.Bind(kFile.Size(), pData);

	char szMapName[256];
	int x, y;
	int width, height;
	for (UINT uLineIndex=0; uLineIndex<textFileLoader.GetLineCount(); ++uLineIndex)
	{
		const std::string& c_rstLine=textFileLoader.GetLineString(uLineIndex);
		sscanf(c_rstLine.c_str(), "%s %d %d %d %d",
			szMapName,
			&x, &y, &width, &height);

		if ('\0'==szMapName[0])
			continue;

		TMapInfo kMapInfo;
		kMapInfo.m_strName = szMapName;
		kMapInfo.m_dwBaseX = x;
		kMapInfo.m_dwBaseY = y;

		kMapInfo.m_dwSizeX = width;
		kMapInfo.m_dwSizeY = height;

		kMapInfo.m_dwEndX = kMapInfo.m_dwBaseX + kMapInfo.m_dwSizeX * CTerrainImpl::TERRAIN_XSIZE;
		kMapInfo.m_dwEndY = kMapInfo.m_dwBaseY + kMapInfo.m_dwSizeY * CTerrainImpl::TERRAIN_YSIZE;

		m_kVct_kMapInfo.push_back(kMapInfo);
	}

	return;
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
