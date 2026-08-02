#include "StdAfx.h"
#include "../eterBase/Random.h"
#include "../eterBase/Timer.h"
#include "../eterlib/ShaderManager.h"
#include "../eterlib/VTFInstanceManager.h"
#include "EffectManager.h"


#ifdef ENABLE_EFFECT_LIMIT
static const float EFFECT_CULL_DISTANCE_RENDER = 25000.0f;
static const float EFFECT_CULL_DISTANCE_FAR = 40000.0f;
static const float EFFECT_CULL_DISTANCE_MID = 25000.0f;
static const int EFFECT_FRAME_SKIP_FAR = 8;
static const int EFFECT_FRAME_SKIP_MID = 4;
#endif

void CEffectManager::GetInfo(std::string* pstInfo)
{
	char szInfo[256];

	sprintf(szInfo, "Effect: Inst - ED %d, EI %d Pool - PSI %d, MI %d, LI %d, PI %d, EI %d, ED %d, PSD %d, EM %d, LD %d",
		(int)m_kEftDataMap.size(),
		(int)m_kEftInstMap.size(),
		(int)CParticleSystemInstance::ms_kPool.GetCapacity(),
		(int)CEffectMeshInstance::ms_kPool.GetCapacity(),
		(int)CLightInstance::ms_kPool.GetCapacity(),
		(int)CParticleInstance::ms_kPool.GetCapacity(),
		//(int)CRayParticleInstance::ms_kPool.GetCapacity(),
		(int)CEffectInstance::ms_kPool.GetCapacity(),
		(int)CEffectData::ms_kPool.GetCapacity(),
		(int)CParticleSystemData::ms_kPool.GetCapacity(),
		(int)CEffectMeshScript::ms_kPool.GetCapacity(),
		(int)CLightData::ms_kPool.GetCapacity()
	);
	pstInfo->append(szInfo);
}

void CEffectManager::UpdateSound()
{
	for (TEffectInstanceMap::iterator itor = m_kEftInstMap.begin(); itor != m_kEftInstMap.end(); ++itor)
	{
		CEffectInstance * pEffectInstance = itor->second;

		pEffectInstance->UpdateSound();
	}
}

bool CEffectManager::IsAliveEffect(DWORD dwInstanceIndex)
{
	TEffectInstanceMap::iterator f = m_kEftInstMap.find(dwInstanceIndex);
	if (m_kEftInstMap.end()==f)
		return false;

	return f->second->isAlive() ? true : false;
}

static LARGE_INTEGER s_qpcFreq = {};
static DWORD64       s_frameEffectUpdateUs = 0;
static DWORD64       s_frameEffectRenderUs = 0;
static UINT          s_frameEffectCount = 0;  // alive effect instances sampled at Update end
static inline DWORD64 __qpcNow()
{
	if (!s_qpcFreq.QuadPart) QueryPerformanceFrequency(&s_qpcFreq);
	LARGE_INTEGER t; QueryPerformanceCounter(&t);
	return (DWORD64)t.QuadPart;
}
static inline DWORD64 __qpcToUs(DWORD64 elapsedTicks)
{
	return (elapsedTicks * 1000000ULL) / (DWORD64)s_qpcFreq.QuadPart;
}

void CEffectManager::Update()
{
	DWORD64 t0 = __qpcNow();
#ifdef ENABLE_EFFECT_LIMIT
	++ms_dwUpdateFrameCounter;
#endif

	for (TEffectInstanceMap::iterator itor = m_kEftInstMap.begin(); itor != m_kEftInstMap.end();)
	{
		CEffectInstance * pEffectInstance = itor->second;

#ifdef ENABLE_EFFECT_LIMIT
		// Distance-based frame skipping for far effects only
		bool bShouldUpdate = true;
		{
			Vector3 v3Pos(pEffectInstance->GetGlobalMatrix()._41, pEffectInstance->GetGlobalMatrix()._42, pEffectInstance->GetGlobalMatrix()._43);
			float fDist = GetDistanceFromPlayer(v3Pos);
			pEffectInstance->m_fCachedDistFromPlayer = fDist;

			if (fDist > EFFECT_CULL_DISTANCE_FAR)
				bShouldUpdate = ((itor->first % EFFECT_FRAME_SKIP_FAR) == (ms_dwUpdateFrameCounter % EFFECT_FRAME_SKIP_FAR));
			else if (fDist > EFFECT_CULL_DISTANCE_MID)
				bShouldUpdate = ((itor->first % EFFECT_FRAME_SKIP_MID) == (ms_dwUpdateFrameCounter % EFFECT_FRAME_SKIP_MID));
		}

		if (bShouldUpdate)
#endif
		pEffectInstance->Update(/*fElapsedTime*/);

		if (!pEffectInstance->isAlive())
		{
			itor = m_kEftInstMap.erase(itor);

			CEffectInstance::Delete(pEffectInstance);

#ifdef ENABLE_EFFECT_LIMIT
			DecreaseActiveEffectCount();
#endif
		}
		else
		{
			++itor;
		}
	}
	s_frameEffectUpdateUs += __qpcToUs(__qpcNow() - t0);
	s_frameEffectCount = (UINT)m_kEftInstMap.size();

	static DWORD s_dwLastCompactTime = 0;
	DWORD dwNow = timeGetTime();
	if (dwNow - s_dwLastCompactTime > 30000)
	{
		s_dwLastCompactTime = dwNow;
		CEffectInstance::ms_kPool.Compact();
		CParticleSystemInstance::ms_kPool.Compact();
		CParticleInstance::ms_kPool.Compact();
		CEffectMeshInstance::ms_kPool.Compact();
		CLightInstance::ms_kPool.Compact();
	}
}

struct CEffectManager_LessEffectInstancePtrRenderOrder
{
	bool operator() (CEffectInstance* pkLeft, CEffectInstance* pkRight)
	{
		return pkLeft->LessRenderOrder(pkRight);
	}
};

struct CEffectManager_FEffectInstanceRender
{
	inline void operator () (CEffectInstance * pkEftInst)
	{
		pkEftInst->Render();
	}
};

void CEffectManager::Render()
{
	DWORD64 t0 = __qpcNow();
	SHADERMANAGER.SetShaderResource(0, NULL);
	SHADERMANAGER.SetShaderResource(1, NULL);

	SHADERMANAGER.SaveSamplerState(0, SAMPLER_MINFILTER, FILTER_ANISOTROPIC);
	SHADERMANAGER.SaveSamplerState(0, SAMPLER_MAGFILTER, FILTER_ANISOTROPIC);
	SHADERMANAGER.SavePipelineState(PSTATE_BLENDENABLE, TRUE);
	SHADERMANAGER.SavePipelineState(PSTATE_SRCBLEND, BLEND_SRCALPHA);
	SHADERMANAGER.SavePipelineState(PSTATE_DESTBLEND, BLEND_INVSRCALPHA);
	bool bSavedAlphaTest = SHADERMANAGER.GetAlphaTestEnabled();
	SHADERMANAGER.SetAlphaTestEnabled(false);
	SHADERMANAGER.SavePipelineState(PSTATE_CULLMODE, CULL_NONE);
	SHADERMANAGER.SavePipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
	SHADERMANAGER.SaveInputLayout(INPUT_LAYOUT_PT);
	CEffectInstance::ms_bBatchRenderState = true;

	SHADERMANAGER.ResetParticleBatcher();
	SHADERMANAGER.ResetParticleBatchStats();
	SHADERMANAGER.SetParticleBatchingActive(true);

#ifdef ENABLE_EFFECT_LIMIT
	bool bSkipSort = m_isDisableSortRendering ||
		(ms_iEffectLimit > 0 && ms_iActiveEffectCount > ms_iEffectLimit * 3 / 4);
#else
	bool bSkipSort = m_isDisableSortRendering;
#endif

	if (bSkipSort)
	{
		for (TEffectInstanceMap::iterator itor = m_kEftInstMap.begin(); itor != m_kEftInstMap.end(); ++itor)
		{
			CEffectInstance * pEffectInstance = itor->second;

#ifdef ENABLE_EFFECT_LIMIT
			if (pEffectInstance->m_fCachedDistFromPlayer > EFFECT_CULL_DISTANCE_RENDER)
				continue;
#endif
			pEffectInstance->Render();
		}
	}
	else
	{
		static std::vector<CEffectInstance*> s_kVct_pkEftInstSort;
		s_kVct_pkEftInstSort.clear();

		TEffectInstanceMap& rkMap_pkEftInstSrc=m_kEftInstMap;
		s_kVct_pkEftInstSort.reserve(rkMap_pkEftInstSrc.size());
		TEffectInstanceMap::iterator i;
		for (i=rkMap_pkEftInstSrc.begin(); i!=rkMap_pkEftInstSrc.end(); ++i)
		{
#ifdef ENABLE_EFFECT_LIMIT
			if (i->second->m_fCachedDistFromPlayer > EFFECT_CULL_DISTANCE_RENDER)
				continue;
#endif
			s_kVct_pkEftInstSort.emplace_back(i->second);
		}

		std::sort(s_kVct_pkEftInstSort.begin(), s_kVct_pkEftInstSort.end(), CEffectManager_LessEffectInstancePtrRenderOrder());
		std::for_each(s_kVct_pkEftInstSort.begin(), s_kVct_pkEftInstSort.end(), CEffectManager_FEffectInstanceRender());
	}

	SHADERMANAGER.FlushParticleBatches();
	SHADERMANAGER.SetParticleBatchingActive(false);

	s_frameEffectRenderUs += __qpcToUs(__qpcNow() - t0);

	// Restore batch state
	CEffectInstance::ms_bBatchRenderState = false;
	SHADERMANAGER.RestoreSamplerState(0, SAMPLER_MINFILTER);
	SHADERMANAGER.RestoreSamplerState(0, SAMPLER_MAGFILTER);
	SHADERMANAGER.RestorePipelineState(PSTATE_BLENDENABLE);
	SHADERMANAGER.RestorePipelineState(PSTATE_SRCBLEND);
	SHADERMANAGER.RestorePipelineState(PSTATE_DESTBLEND);
	SHADERMANAGER.SetAlphaTestEnabled(bSavedAlphaTest);
	SHADERMANAGER.RestorePipelineState(PSTATE_CULLMODE);
	SHADERMANAGER.RestorePipelineState(PSTATE_DEPTHWRITEMASK);
	SHADERMANAGER.RestoreInputLayout();
}

BOOL CEffectManager::RegisterEffect(const char * c_szFileName,bool isExistDelete,bool isNeedCache)
{
	std::string strFileName;
	StringPath(c_szFileName, strFileName);
	DWORD dwCRC = GetCaseCRC32(strFileName.c_str(), strFileName.length());

	TEffectDataMap::iterator itor = m_kEftDataMap.find(dwCRC);
	if (m_kEftDataMap.end() != itor)
	{
		if (isExistDelete)
		{
			CEffectData* pkEftData=itor->second;
			CEffectData::Delete(pkEftData);
			m_kEftDataMap.erase(itor);
		}
		else
		{
			//TraceError("CEffectManager::RegisterEffect - m_kEftDataMap.find [%s] Already Exist", c_szFileName);
			return TRUE;
		}
	}

	CEffectData * pkEftData = CEffectData::New();

	if (!pkEftData->LoadScript(c_szFileName))
	{
		TraceError("CEffectManager::RegisterEffect - LoadScript(%s) Error", c_szFileName);
		CEffectData::Delete(pkEftData);
		return FALSE;
	}

	m_kEftDataMap.emplace(dwCRC, pkEftData);

	if (isNeedCache)
	{
		if (m_kEftCacheMap.find(dwCRC)==m_kEftCacheMap.end())
		{
			CEffectInstance* pkNewEftInst=CEffectInstance::New();
			pkNewEftInst->SetEffectDataPointer(pkEftData);
			m_kEftCacheMap.emplace(dwCRC, pkNewEftInst);
		}
	}

	return TRUE;
}
BOOL CEffectManager::RegisterEffect2(const char * c_szFileName, DWORD* pdwRetCRC, bool isNeedCache)
{
	std::string strFileName;
	StringPath(c_szFileName, strFileName);
	DWORD dwCRC = GetCaseCRC32(strFileName.c_str(), strFileName.length());
	*pdwRetCRC=dwCRC;

	return RegisterEffect(c_szFileName,false,isNeedCache);
}

int CEffectManager::CreateEffect(const char * c_szFileName, const Vector3 & c_rv3Position, const Vector3 & c_rv3Rotation)
{
	std::string strFileName;
	StringPath(c_szFileName, strFileName); //@fixme030
	DWORD dwID = GetCaseCRC32(strFileName.c_str(), strFileName.size());
	return CreateEffect(dwID, c_rv3Position, c_rv3Rotation);
}

int CEffectManager::CreateEffect(DWORD dwID, const Vector3 & c_rv3Position, const Vector3 & c_rv3Rotation)
{
#ifdef ENABLE_EFFECT_LIMIT
	if (!CanCreateEffect(c_rv3Position))
		return -1;
#endif

	int iInstanceIndex = GetEmptyIndex();

	CreateEffectInstance(iInstanceIndex, dwID);
	SelectEffectInstance(iInstanceIndex);
	Matrix mat;
	MatrixRotationYawPitchRoll(&mat,ToRadian(c_rv3Rotation.x),ToRadian(c_rv3Rotation.y),ToRadian(c_rv3Rotation.z));
	mat._41 = c_rv3Position.x;
	mat._42 = c_rv3Position.y;
	mat._43 = c_rv3Position.z;
	SetEffectInstanceGlobalMatrix(mat);

	return iInstanceIndex;
}

void CEffectManager::CreateEffectInstance(DWORD dwInstanceIndex, DWORD dwID)
{
	if (!dwID)
		return;

#ifdef ENABLE_EFFECT_LIMIT
	if (ms_iEffectLimit > 0 && ms_iActiveEffectCount >= ms_iEffectLimit)
		return;
#endif

	CEffectData * pEffect;
	if (!GetEffectData(dwID, &pEffect))
	{
		Tracef("CEffectManager::CreateEffectInstance - NO DATA :%d\n", dwID);
		return;
	}

	CEffectInstance * pEffectInstance = CEffectInstance::New();
	pEffectInstance->SetEffectDataPointer(pEffect);

	m_kEftInstMap.emplace(dwInstanceIndex, pEffectInstance);

#ifdef ENABLE_EFFECT_LIMIT
	IncreaseActiveEffectCount();
#endif
}

bool CEffectManager::DestroyEffectInstance(DWORD dwInstanceIndex)
{
	TEffectInstanceMap::iterator itor = m_kEftInstMap.find(dwInstanceIndex);

	if (itor == m_kEftInstMap.end())
		return false;

	CEffectInstance * pEffectInstance = itor->second;

	m_kEftInstMap.erase(itor);

	CEffectInstance::Delete(pEffectInstance);

#ifdef ENABLE_EFFECT_LIMIT
	DecreaseActiveEffectCount();
#endif

	return true;
}

void CEffectManager::DeactiveEffectInstance(DWORD dwInstanceIndex)
{
	TEffectInstanceMap::iterator itor = m_kEftInstMap.find(dwInstanceIndex);

	if (itor == m_kEftInstMap.end())
		return;

	CEffectInstance * pEffectInstance = itor->second;
	pEffectInstance->SetDeactive();
}

void CEffectManager::CreateUnsafeEffectInstance(DWORD dwEffectDataID, CEffectInstance ** ppEffectInstance)
{
	CEffectData * pEffect{nullptr}; //@fixme030
	if (!GetEffectData(dwEffectDataID, &pEffect))
	{
		Tracef("CEffectManager::CreateUnsafeEffectInstance - NO DATA :%d\n", dwEffectDataID);
		return;
	}

	CEffectInstance* pkEftInstNew=CEffectInstance::New();
	pkEftInstNew->SetEffectDataPointer(pEffect);

	*ppEffectInstance = pkEftInstNew;
}

bool CEffectManager::DestroyUnsafeEffectInstance(CEffectInstance * pEffectInstance)
{
	if (!pEffectInstance)
		return false;

	CEffectInstance::Delete(pEffectInstance);

	return true;
}

BOOL CEffectManager::SelectEffectInstance(DWORD dwInstanceIndex)
{
	TEffectInstanceMap::iterator itor = m_kEftInstMap.find(dwInstanceIndex);

	m_pSelectedEffectInstance = NULL;

	if (m_kEftInstMap.end() == itor)
		return FALSE;

	m_pSelectedEffectInstance = itor->second;

	return TRUE;
}

void CEffectManager::SetEffectTextures(DWORD dwID,std::vector<string> textures)
{
	CEffectData * pEffectData;
	if (!GetEffectData(dwID, &pEffectData))
	{
		Tracef("CEffectManager::SetEffectTextures - NO DATA :%d\n", dwID);
		return;
	}

	for(DWORD i = 0; i < textures.size(); i++)
	{
		CParticleSystemData * pParticle = pEffectData->GetParticlePointer(i);
		pParticle->ChangeTexture(textures.at(i).c_str());
	}
}

void CEffectManager::SetEffectInstancePosition(const Vector3 & c_rv3Position)
{
	if (!m_pSelectedEffectInstance)
	{
//		assert(!"Instance to use is not yet set!");
		return;
	}

	m_pSelectedEffectInstance->SetPosition(c_rv3Position);
}

void CEffectManager::SetEffectInstanceRotation(const Vector3 & c_rv3Rotation)
{
	if (!m_pSelectedEffectInstance)
	{
//		assert(!"Instance to use is not yet set!");
		return;
	}

	m_pSelectedEffectInstance->SetRotation(c_rv3Rotation.x,c_rv3Rotation.y,c_rv3Rotation.z);
}

void CEffectManager::SetEffectInstanceGlobalMatrix(const Matrix & c_rmatGlobal)
{
	if (!m_pSelectedEffectInstance)
		return;

	m_pSelectedEffectInstance->SetGlobalMatrix(c_rmatGlobal);
}

void CEffectManager::ShowEffect()
{
	if (!m_pSelectedEffectInstance)
		return;

	m_pSelectedEffectInstance->Show();
}

void CEffectManager::HideEffect()
{
	if (!m_pSelectedEffectInstance)
		return;

	m_pSelectedEffectInstance->Hide();
}

#ifdef __ENABLE_STEALTH_FIX__
void CEffectManager::ApplyAlwaysHidden()
{
	if (!m_pSelectedEffectInstance)
		return;
	m_pSelectedEffectInstance->ApplyAlwaysHidden();
}

void CEffectManager::ReleaseAlwaysHidden()
{
	if (!m_pSelectedEffectInstance)
		return;
	m_pSelectedEffectInstance->ReleaseAlwaysHidden();
}
#endif

bool CEffectManager::GetEffectData(DWORD dwID, CEffectData ** ppEffect)
{
	TEffectDataMap::iterator itor = m_kEftDataMap.find(dwID);

	if (itor == m_kEftDataMap.end())
		return false;

	*ppEffect = itor->second;

	return true;
}

bool CEffectManager::GetEffectData(DWORD dwID, const CEffectData ** c_ppEffect)
{
	TEffectDataMap::iterator itor = m_kEftDataMap.find(dwID);

	if (itor == m_kEftDataMap.end())
		return false;

	*c_ppEffect = itor->second;

	return true;
}

int CEffectManager::GetRenderingEffectCount()
{
	return CEffectInstance::GetRenderingEffectCount();
}

DWORD CEffectManager::GetRandomEffect()
{
	int iIndex = random() % m_kEftDataMap.size();

	TEffectDataMap::iterator itor = m_kEftDataMap.begin();
	for (int i = 0; i < iIndex; ++i, ++itor);

	return itor->first;
}

int CEffectManager::GetEmptyIndex()
{
	static int iMaxIndex=1;

	if (iMaxIndex>2100000000)
		iMaxIndex = 1;

	int iNextIndex = iMaxIndex++;
	while(m_kEftInstMap.find(iNextIndex) != m_kEftInstMap.end())
		iNextIndex++;

	return iNextIndex;
}

void CEffectManager::DeleteAllInstances()
{
	__DestroyEffectInstanceMap();
}

void CEffectManager::__DestroyEffectInstanceMap()
{
	for (TEffectInstanceMap::iterator i = m_kEftInstMap.begin(); i != m_kEftInstMap.end(); ++i)
	{
		CEffectInstance * pkEftInst = i->second;
		CEffectInstance::Delete(pkEftInst);
	}

	m_kEftInstMap.clear();

#ifdef ENABLE_EFFECT_LIMIT
	ms_iActiveEffectCount = 0;
#endif
}

void CEffectManager::__DestroyEffectCacheMap()
{
	for (TEffectInstanceMap::iterator i = m_kEftCacheMap.begin(); i != m_kEftCacheMap.end(); ++i)
	{
		CEffectInstance * pkEftInst = i->second;
		CEffectInstance::Delete(pkEftInst);
	}

	m_kEftCacheMap.clear();
}

void CEffectManager::__DestroyEffectDataMap()
{
	for (TEffectDataMap::iterator i = m_kEftDataMap.begin(); i != m_kEftDataMap.end(); ++i)
	{
		CEffectData * pData = i->second;
		CEffectData::Delete(pData);
	}

	m_kEftDataMap.clear();
}

void CEffectManager::Destroy()
{
	__DestroyEffectInstanceMap();
	__DestroyEffectCacheMap();
	__DestroyEffectDataMap();

	__Initialize();
}

void CEffectManager::__Initialize()
{
	m_pSelectedEffectInstance = NULL;
	m_isDisableSortRendering = false;
}

CEffectManager::CEffectManager()
{
	__Initialize();
}

CEffectManager::~CEffectManager()
{
	Destroy();
}

#ifdef ENABLE_EFFECT_LIMIT
#ifndef EFFECT_LIMIT_DEFAULT
#define EFFECT_LIMIT_DEFAULT 300
#endif
#ifndef EFFECT_LIMIT_DISTANCE_DEFAULT
#define EFFECT_LIMIT_DISTANCE_DEFAULT 2500.0f
#endif

int CEffectManager::ms_iEffectLimit = EFFECT_LIMIT_DEFAULT;
int CEffectManager::ms_iActiveEffectCount = 0;
Vector3 CEffectManager::ms_v3PlayerPosition(0.0f, 0.0f, 0.0f);
DWORD CEffectManager::ms_dwUpdateFrameCounter = 0;

float CEffectManager::GetDistanceFromPlayer(const Vector3& v3Pos)
{
	Vector3 v3Diff = v3Pos - ms_v3PlayerPosition;
	return Vec3Length(&v3Diff);
}

bool CEffectManager::CanCreateEffect(const Vector3& v3Pos)
{
	if (ms_iEffectLimit <= 0)
		return true;  // No limit

	return ms_iActiveEffectCount < ms_iEffectLimit;
}
#endif

// just for map effect
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
