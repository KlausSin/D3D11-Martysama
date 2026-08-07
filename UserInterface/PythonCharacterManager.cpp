#include "stdafx.h"
#include "pythoncharactermanager.h"
#include "PythonBackground.h"
#include "PythonNonPlayer.h"
#include "AbstractPlayer.h"
#include "packet.h"

#include "../eterLib/Camera.h"
#include "../eterLib/ShaderManager.h"
#include "../eterLib/VTFInstanceManager.h"
#include "../GameLib/RaceManager.h"
#include "../eterLib/GrpScreen.h"
#include "../SphereLib/frustum.h"
#include "../EterGrnLib/LODController.h"
#include "../EterGrnLib/ModelInstance.h"
#include "../EterGrnLib/Model.h"


#ifdef ENABLE_RENDER_MODE_GROUPING
#include "../GameLib/ActorInstance.h"
#include "../EterGrnLib/Material.h"
#endif

#ifdef ENABLE_EFFECT_LIMIT
#include "../EffectLib/EffectManager.h"
#endif

#ifdef ENABLE_FRUSTUM_CULLING
int CPythonCharacterManager::ms_iCulledCount = 0;
int CPythonCharacterManager::ms_iRenderedCount = 0;
int CPythonCharacterManager::ms_iTotalCount = 0;
#endif

#ifdef ENABLE_CHAR_RENDER_LIMIT
#ifndef CHAR_RENDER_LIMIT_DEFAULT
#define CHAR_RENDER_LIMIT_DEFAULT 200
#endif
int CPythonCharacterManager::ms_iRenderLimit = CHAR_RENDER_LIMIT_DEFAULT;
int CPythonCharacterManager::ms_iSkippedByLimit = 0;
#endif

#ifdef ENABLE_RENDER_MODE_GROUPING
int CPythonCharacterManager::ms_iRenderModeNormal = 0;
int CPythonCharacterManager::ms_iRenderModeBlend = 0;
int CPythonCharacterManager::ms_iRenderModeAdd = 0;
int CPythonCharacterManager::ms_iRenderModeModulate = 0;
#endif

#ifdef ENABLE_UPDATE_CULLING
int CPythonCharacterManager::ms_iUpdateSkipped = 0;
int CPythonCharacterManager::ms_iUpdateFull = 0;
int CPythonCharacterManager::ms_iUpdateReduced = 0;
#endif

#if defined(ENABLE_UPDATE_CULLING) || defined(ENABLE_ANIMATION_LOD)
DWORD CPythonCharacterManager::ms_dwFrameCounter = 0;
#endif

#ifdef ENABLE_DEFORM_CULLING
int CPythonCharacterManager::ms_iDeformSkipped = 0;
int CPythonCharacterManager::ms_iDeformRendered = 0;
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
// Frame Process

int CHAR_STAGE_VIEW_BOUND = 200*100;

struct FCharacterManagerCharacterInstanceUpdate
{
	inline void operator () (const std::pair<DWORD,CInstanceBase *>& cr_Pair)
	{
		cr_Pair.second->Update();
	}
};

void CPythonCharacterManager::AdjustCollisionWithOtherObjects(CActorInstance* pInst )
{
	if( !pInst->IsPC() )
		return;

	CPythonCharacterManager& rkChrMgr=CPythonCharacterManager::Instance();
	for(CPythonCharacterManager::CharacterIterator i = rkChrMgr.CharacterInstanceBegin(); i!=rkChrMgr.CharacterInstanceEnd();++i)
	{
		CInstanceBase*  pkInstEach=*i;
		CActorInstance* rkActorEach=pkInstEach->GetGraphicThingInstancePtr();

		if (rkActorEach==pInst)
			continue;

		if( rkActorEach->IsPC() || rkActorEach->IsNPC() || rkActorEach->IsEnemy() )
			continue;

		if(pInst->TestPhysicsBlendingCollision(*rkActorEach) )
		{
			TPixelPosition curPos;
			pInst->GetPixelPosition(&curPos);
			pInst->SetBlendingPosition(curPos);
			//Tracef("!!!!!! Collision Adjusted\n");
			break;
		}
	}
}

void CPythonCharacterManager::EnableSortRendering(bool isEnable)
{
}

void CPythonCharacterManager::InsertPVPKey(DWORD dwVIDSrc, DWORD dwVIDDst)
{
	CInstanceBase::InsertPVPKey(dwVIDSrc, dwVIDDst);

	CInstanceBase* pkInstSrc=GetInstancePtr(dwVIDSrc);
	if (pkInstSrc)
		pkInstSrc->RefreshTextTail();

	CInstanceBase* pkInstDst=GetInstancePtr(dwVIDDst);
	if (pkInstDst)
		pkInstDst->RefreshTextTail();
}

void CPythonCharacterManager::RemovePVPKey(DWORD dwVIDSrc, DWORD dwVIDDst)
{
	CInstanceBase::RemovePVPKey(dwVIDSrc, dwVIDDst);

	CInstanceBase* pkInstSrc=GetInstancePtr(dwVIDSrc);
	if (pkInstSrc)
		pkInstSrc->RefreshTextTail();

	CInstanceBase* pkInstDst=GetInstancePtr(dwVIDDst);
	if (pkInstDst)
		pkInstDst->RefreshTextTail();
}

void CPythonCharacterManager::ChangeGVG(DWORD dwSrcGuildID, DWORD dwDstGuildID)
{
	TCharacterInstanceMap::iterator itor;
	for (itor = m_kAliveInstMap.begin(); itor != m_kAliveInstMap.end(); itor++)
	{
		CInstanceBase * pInstance = itor->second;

		DWORD dwInstanceGuildID = pInstance->GetGuildID();
		if (dwSrcGuildID == dwInstanceGuildID || dwDstGuildID == dwInstanceGuildID)
		{
			pInstance->RefreshTextTail();
		}
	}
}

void CPythonCharacterManager::ClearMainInstance()
{
	m_pkInstMain=NULL;
}

bool CPythonCharacterManager::SetMainInstance(DWORD dwVID)
{
	m_pkInstMain=GetInstancePtr(dwVID);

	if (!m_pkInstMain)
		return false;

	return true;
}

CInstanceBase* CPythonCharacterManager::GetMainInstancePtr()
{
	return m_pkInstMain;
}

void CPythonCharacterManager::GetInfo(std::string* pstInfo)
{
	pstInfo->append("Actor: ");

	CInstanceBase::GetInfo(pstInfo);

	char szInfo[256];
	sprintf(szInfo, "Container - Live %d, Dead %d", (int)m_kAliveInstMap.size(), (int)m_kDeadInstList.size());
	pstInfo->append(szInfo);
}

bool CPythonCharacterManager::IsCacheMode()
{
	static bool s_isOldCacheMode=false;

	bool isCacheMode=s_isOldCacheMode;
	if (s_isOldCacheMode)
	{
		if (m_kAliveInstMap.size()<30)
			isCacheMode=false;
	}
	else
	{
		if (m_kAliveInstMap.size()>40)
			isCacheMode=true;
	}
	s_isOldCacheMode=isCacheMode;

	return isCacheMode;
}

void CPythonCharacterManager::Update()
{
	// Process any completed async race data loads
	CRaceManager::Instance().ProcessAsyncLoads();

#ifdef __PERFORMANCE_CHECKER__
	DWORD t1=timeGetTime();
#endif
	CInstanceBase::ResetPerformanceCounter();

	CInstanceBase* pkInstMain=GetMainInstancePtr();
#ifdef __PERFORMANCE_CHECKER__
	DWORD t2=timeGetTime();
	DWORD dwDeadInstCount=0;
	DWORD dwForceVisibleInstCount=0;
#endif

#if defined(ENABLE_UPDATE_CULLING) || defined(ENABLE_ANIMATION_LOD)
	ms_dwFrameCounter++;
#endif

#ifdef ENABLE_UPDATE_CULLING
	ms_iUpdateSkipped = 0;
	ms_iUpdateFull = 0;
	ms_iUpdateReduced = 0;
#endif

	TCharacterInstanceMap::iterator i=m_kAliveInstMap.begin();
	while (m_kAliveInstMap.end()!=i)
	{
		TCharacterInstanceMap::iterator c=i++;

		CInstanceBase* pkInstEach=c->second;

		float fDistance = 0.0f;
		if (pkInstMain && pkInstEach != pkInstMain)
		{
			fDistance = pkInstEach->NEW_GetDistanceFromDestInstance(*pkInstMain);
			pkInstEach->m_fCachedDistanceFromMain = fDistance;
			pkInstEach->m_dwCachedDistanceFrame = ms_dwFrameCounter;
		}

#ifdef ENABLE_UPDATE_CULLING
		// Distance-based update throttling
		bool bShouldUpdate = true;
		bool bFullUpdate = true;

		if (pkInstMain && pkInstEach != pkInstMain)
		{
			if (fDistance > UPDATE_CULLING_DISTANCE_FAR)
			{
				// Very far: update only every N frames
				if ((ms_dwFrameCounter % UPDATE_CULLING_FRAME_SKIP_FAR) != (pkInstEach->GetVirtualID() % UPDATE_CULLING_FRAME_SKIP_FAR))
				{
					bShouldUpdate = false;
					ms_iUpdateSkipped++;
				}
				else
				{
					bFullUpdate = false;
					ms_iUpdateReduced++;
				}
			}
			else if (fDistance > UPDATE_CULLING_DISTANCE_MID)
			{
				// Mid range: update every N frames
				if ((ms_dwFrameCounter % UPDATE_CULLING_FRAME_SKIP_MID) != (pkInstEach->GetVirtualID() % UPDATE_CULLING_FRAME_SKIP_MID))
				{
					bShouldUpdate = false;
					ms_iUpdateSkipped++;
				}
				else
				{
					ms_iUpdateFull++;
				}
			}
			else
			{
				ms_iUpdateFull++;
			}
		}
		else
		{
			ms_iUpdateFull++;
		}

		// A spawn fade advances on the instance's local clock, which only ticks
		// inside Update(). Culling the update strands the actor part-transparent.
		if (!bShouldUpdate && pkInstEach->GetGraphicThingInstanceRef().IsBlendingAlpha())
		{
			bShouldUpdate = true;
			bFullUpdate = true;
			ms_iUpdateSkipped--;
			ms_iUpdateFull++;
		}

		if (bShouldUpdate)
		{
			pkInstEach->Update();
		}
#else
		pkInstEach->Update();
#endif

		// Backstop: nothing may leave an actor stuck mid-fade, whatever skipped it.
		{
			CActorInstance & rkActorBlend = pkInstEach->GetGraphicThingInstanceRef();
			if (rkActorBlend.IsAlphaBlendOverdue())
			{
				rkActorBlend.CompleteAlphaBlend();
			}
		}

		if (pkInstMain)
		{
#ifdef __PERFORMANCE_CHECKER__
			if (pkInstEach->IsForceVisible())
			{
				dwForceVisibleInstCount++;
				continue;
			}
#endif

			// Reuse cached distance instead of computing sqrt again
			if ((int)fDistance > CHAR_STAGE_VIEW_BOUND + 10)
			{
				__DeleteBlendOutInstance(pkInstEach);
				m_kAliveInstMap.erase(c);
#ifdef __PERFORMANCE_CHECKER__
				dwDeadInstCount++;
#endif
			}
		}
	}
#ifdef __PERFORMANCE_CHECKER__
	DWORD t3=timeGetTime();
#endif
	UpdateTransform();
#ifdef __PERFORMANCE_CHECKER__
	DWORD t4=timeGetTime();
#endif

	UpdateDeleting();
#ifdef __PERFORMANCE_CHECKER__
	DWORD t5=timeGetTime();
#endif

	__NEW_Pick();
#ifdef __PERFORMANCE_CHECKER__
	DWORD t6=timeGetTime();
#endif

#ifdef __PERFORMANCE_CHECKER__
	{
		static FILE* fp=fopen("perf_chrmgr_update.txt", "w");

		if (t6-t1>1)
		{
			fprintf(fp, "CU.Total %d (Time %d, Alive %d, Dead %d)\n",
				t6-t1, ELTimer_GetMSec(),
				m_kAliveInstMap.size(),
				m_kDeadInstList.size());
			fprintf(fp, "CU.Counter %d\n", t2-t1);
			fprintf(fp, "CU.ForEach %d\n", t3-t2);
			fprintf(fp, "CU.Trans %d\n", t4-t3);
			fprintf(fp, "CU.Del %d\n", t5-t4);
			fprintf(fp, "CU.Pick %d\n", t6-t5);
			fprintf(fp, "CU.AI %d\n", m_kAliveInstMap.size());
			fprintf(fp, "CU.DI %d\n", dwDeadInstCount);
			fprintf(fp, "CU.FVI %d\n", dwForceVisibleInstCount);
			fprintf(fp, "-------------------------------- \n");
			fflush(fp);
		}
	}
#endif
}

void CPythonCharacterManager::ShowPointEffect(DWORD ePoint, DWORD dwVID)
{
	CInstanceBase * pkInstSel = (dwVID == 0xffffffff) ? GetMainInstancePtr() : GetInstancePtr(dwVID);

	if (!pkInstSel)
		return;

	switch (ePoint)
	{
		case POINT_LEVEL:
			pkInstSel->LevelUp();
			break;
		case POINT_LEVEL_STEP:
			pkInstSel->SkillUp();
			break;
	}
}

bool CPythonCharacterManager::RegisterPointEffect(DWORD ePoint, const char* c_szFileName)
{
	if (ePoint>=POINT_MAX_NUM)
		return false;

	CEffectManager& rkEftMgr=CEffectManager::Instance();
	rkEftMgr.RegisterEffect2(c_szFileName, &m_adwPointEffect[ePoint]);

	return true;
}

void CPythonCharacterManager::UpdateTransform()
{
#ifdef __PERFORMANCE_CHECKER__
	DWORD t1=timeGetTime();
	DWORD t2=timeGetTime();
#endif

	CInstanceBase * pMainInstance = GetMainInstancePtr();
	CPythonBackground& rkBG=CPythonBackground::Instance();

	for (TCharacterInstanceMap::iterator itor = m_kAliveInstMap.begin(); itor != m_kAliveInstMap.end(); ++itor)
	{
		CInstanceBase * pInstance = itor->second;

		if (pMainInstance)
		{
			pInstance->CheckAdvancing();

			if (pInstance->IsPushing())
				rkBG.CheckAdvancing(pInstance);
		}

		pInstance->Transform();
	}

#ifdef __PERFORMANCE_CHECKER__
	t2=timeGetTime();
	DWORD t3=timeGetTime();
#endif

	if (pMainInstance)
	{
#ifdef __MOVIE_MODE__
		if (!m_pkInstMain->IsMovieMode())
		{
			rkBG.CheckAdvancing(m_pkInstMain);
		}
#else
		rkBG.CheckAdvancing(m_pkInstMain);
#endif
	}

#ifdef __PERFORMANCE_CHECKER__
	DWORD t4=timeGetTime();
#endif

#ifdef __PERFORMANCE_CHECKER__
	{
		static FILE* fp=fopen("perf_chrmgr_updatetransform.txt", "w");

		if (t4-t1>5)
		{
			fprintf(fp, "CUT.Total %d (Time %f, Alive %d, Dead %d)\n",
				t4-t1, ELTimer_GetMSec()/1000.0f,
				m_kAliveInstMap.size(),
				m_kDeadInstList.size());
			fprintf(fp, "CUT.ChkAdvInst %d\n", t2-t1);
			fprintf(fp, "CUT.ChkAdvBG %d\n", t3-t2);
			fprintf(fp, "CUT.Trans %d\n", t4-t3);

			fprintf(fp, "-------------------------------- \n");
			fflush(fp);
		}

		fflush(fp);
	}
#endif
}
void CPythonCharacterManager::UpdateDeleting()
{
	TCharacterInstanceList::iterator itor = m_kDeadInstList.begin();
	for (; itor != m_kDeadInstList.end();)
	{
		CInstanceBase * pInstance = *itor;

		if (pInstance->UpdateDeleting())
		{
			++itor;
		}
		else
		{
			CInstanceBase::Delete(pInstance);
			itor = m_kDeadInstList.erase(itor);
		}
	}
}

struct FCharacterManagerCharacterInstanceDeform
{
	inline void operator () (const std::pair<DWORD,CInstanceBase *>& cr_Pair)
	{
		cr_Pair.second->Deform();
		//pInstance->Update();
	}
};
struct FCharacterManagerCharacterInstanceListDeform
{
	inline void operator () (CInstanceBase * pInstance)
	{
		pInstance->Deform();
	}
};

void CPythonCharacterManager::Deform()
{
#ifdef ENABLE_DEFORM_CULLING
	ms_iDeformSkipped = 0;
	ms_iDeformRendered = 0;

	CInstanceBase* pkInstMain = GetMainInstancePtr();

	for (TCharacterInstanceMap::iterator itor = m_kAliveInstMap.begin(); itor != m_kAliveInstMap.end(); ++itor)
	{
		CInstanceBase* pkInst = itor->second;

		// Always deform the main player
		if (pkInst == pkInstMain)
		{
			pkInst->Deform();
			ms_iDeformRendered++;
			continue;
		}


		if (pkInstMain)
		{
			float fDistance = (pkInst->m_dwCachedDistanceFrame == ms_dwFrameCounter)
				? pkInst->m_fCachedDistanceFromMain
				: pkInst->NEW_GetDistanceFromDestInstance(*pkInstMain);
			if (fDistance > UPDATE_CULLING_DISTANCE_FAR)
			{
				ms_iDeformSkipped++;
				continue;
			}

#ifdef ENABLE_ANIMATION_LOD
			DWORD dwVID = pkInst->GetVirtualID();
			if (fDistance > ANIMATION_LOD_DISTANCE_MID)
			{
				if ((ms_dwFrameCounter + dwVID) % ANIMATION_LOD_SKIP_FRAMES_FAR != 0)
				{
					ms_iDeformSkipped++;
					continue;
				}
			}
			else if (fDistance > ANIMATION_LOD_DISTANCE_HIGH)
			{
				if ((ms_dwFrameCounter + dwVID) % ANIMATION_LOD_SKIP_FRAMES_MID != 0)
				{
					ms_iDeformSkipped++;
					continue;
				}
			}
#endif
		}

		pkInst->Deform();
		ms_iDeformRendered++;
	}

	// Dead instances still need deform for fade-out animation
	std::for_each(m_kDeadInstList.begin(), m_kDeadInstList.end(), FCharacterManagerCharacterInstanceListDeform());
#else
	std::for_each(m_kAliveInstMap.begin(), m_kAliveInstMap.end(), FCharacterManagerCharacterInstanceDeform());
	std::for_each(m_kDeadInstList.begin(), m_kDeadInstList.end(), FCharacterManagerCharacterInstanceListDeform());
#endif
}

bool CPythonCharacterManager::OLD_GetPickedInstanceVID(DWORD* pdwPickedActorID)
{
	if (!m_pkInstPick)
		return false;

	*pdwPickedActorID=m_pkInstPick->GetVirtualID();
	return true;
}

CInstanceBase * CPythonCharacterManager::OLD_GetPickedInstancePtr()
{
	return m_pkInstPick;
}

Vector2 & CPythonCharacterManager::OLD_GetPickedInstPosReference()
{
	return m_v2PickedInstProjPos;
}

bool CPythonCharacterManager::IsRegisteredVID(DWORD dwVID)
{
	if (m_kAliveInstMap.end()==m_kAliveInstMap.find(dwVID))
		return false;

	return true;
}

bool CPythonCharacterManager::IsAliveVID(DWORD dwVID)
{
	return m_kAliveInstMap.find(dwVID)!=m_kAliveInstMap.end();
}

bool CPythonCharacterManager::IsDeadVID(DWORD dwVID)
{
	for (TCharacterInstanceList::iterator f=m_kDeadInstList.begin(); f!=m_kDeadInstList.end(); ++f)
	{
		if ((*f)->GetVirtualID()==dwVID)
			return true;
	}

	return false;
}

struct LessCharacterInstancePtrRenderOrder
{
	bool operator() (CInstanceBase* pkLeft, CInstanceBase* pkRight)
	{
		return pkLeft->LessRenderOrder(pkRight);
	}
};

struct FCharacterManagerCharacterInstanceRender
{
	inline void operator () (const std::pair<DWORD,CInstanceBase *>& cr_Pair)
	{
		cr_Pair.second->Render();
		cr_Pair.second->RenderTrace();
	}
};
struct FCharacterInstanceRender
{
	inline void operator () (CInstanceBase * pInstance)
	{
		pInstance->Render();
	}
};
struct FCharacterInstanceRenderTrace
{
	inline void operator () (CInstanceBase * pInstance)
	{
		pInstance->RenderTrace();
	}
};

#if defined(ENABLE_FRUSTUM_CULLING) || defined(ENABLE_CHAR_RENDER_LIMIT) || defined(ENABLE_RENDER_MODE_GROUPING)
void CPythonCharacterManager::ResetCullingStats()
{
#ifdef ENABLE_FRUSTUM_CULLING
	ms_iCulledCount = 0;
	ms_iRenderedCount = 0;
	ms_iTotalCount = 0;
#endif
#ifdef ENABLE_CHAR_RENDER_LIMIT
	ms_iSkippedByLimit = 0;
#endif
#ifdef ENABLE_RENDER_MODE_GROUPING
	ms_iRenderModeNormal = 0;
	ms_iRenderModeBlend = 0;
	ms_iRenderModeAdd = 0;
	ms_iRenderModeModulate = 0;
#endif
}
#endif

void CPythonCharacterManager::__RenderSortedAliveActorList()
{
#if defined(ENABLE_FRUSTUM_CULLING) || defined(ENABLE_CHAR_RENDER_LIMIT) || defined(ENABLE_RENDER_MODE_GROUPING)
	ResetCullingStats();
#endif

	CInstanceBase* pkInstMain = GetMainInstancePtr();
	if (pkInstMain)
	{
		Vector3 v3PlayerPos = pkInstMain->GetGraphicThingInstanceRef().GetPosition();

#ifdef ENABLE_EFFECT_LIMIT
		CEffectManager::SetMainPlayerPosition(v3PlayerPos);
#endif
	}

	static std::vector<CInstanceBase*> s_kVct_pkInstAliveSort;
	s_kVct_pkInstAliveSort.clear();

	TCharacterInstanceMap& rkMap_pkInstAlive=m_kAliveInstMap;
	s_kVct_pkInstAliveSort.reserve(rkMap_pkInstAlive.size());
	TCharacterInstanceMap::iterator i;
	for (i=rkMap_pkInstAlive.begin(); i!=rkMap_pkInstAlive.end(); ++i)
		s_kVct_pkInstAliveSort.push_back(i->second);

#ifdef ENABLE_CHAR_RENDER_LIMIT
	if (ms_iRenderLimit > 0)
	{
		CCamera* pCamera = CCameraManager::Instance().GetCurrentCamera();
		if (pCamera)
		{
			Vector3 v3CameraPos = pCamera->GetEye();
			std::sort(s_kVct_pkInstAliveSort.begin(), s_kVct_pkInstAliveSort.end(),
				[&v3CameraPos](CInstanceBase* a, CInstanceBase* b) {
					const Vector3& v3PosA = a->GetGraphicThingInstanceRef().GetPosition();
					const Vector3& v3PosB = b->GetGraphicThingInstanceRef().GetPosition();
					float fDistA = (v3PosA.x - v3CameraPos.x) * (v3PosA.x - v3CameraPos.x) +
								   (v3PosA.y - v3CameraPos.y) * (v3PosA.y - v3CameraPos.y) +
								   (v3PosA.z - v3CameraPos.z) * (v3PosA.z - v3CameraPos.z);
					float fDistB = (v3PosB.x - v3CameraPos.x) * (v3PosB.x - v3CameraPos.x) +
								   (v3PosB.y - v3CameraPos.y) * (v3PosB.y - v3CameraPos.y) +
								   (v3PosB.z - v3CameraPos.z) * (v3PosB.z - v3CameraPos.z);
					return fDistA < fDistB;
				});
		}
		else
		{
			std::sort(s_kVct_pkInstAliveSort.begin(), s_kVct_pkInstAliveSort.end(), LessCharacterInstancePtrRenderOrder());
		}
	}
	else
#endif
	{
		std::sort(s_kVct_pkInstAliveSort.begin(), s_kVct_pkInstAliveSort.end(), LessCharacterInstancePtrRenderOrder());
	}

#ifdef ENABLE_RENDER_MODE_GROUPING
	// Group characters by render mode
	static std::vector<CInstanceBase*> s_kVct_pkInstNormal;
	static std::vector<CInstanceBase*> s_kVct_pkInstBlend;
	static std::vector<CInstanceBase*> s_kVct_pkInstAdd;
	static std::vector<CInstanceBase*> s_kVct_pkInstModulate;
	s_kVct_pkInstNormal.clear();
	s_kVct_pkInstBlend.clear();
	s_kVct_pkInstAdd.clear();
	s_kVct_pkInstModulate.clear();
#else
	static std::vector<CInstanceBase*> s_kVct_pkInstVTFDeferred;
	s_kVct_pkInstVTFDeferred.clear();
#endif

#ifdef ENABLE_CHAR_RENDER_LIMIT
	int iRenderCount = 0;
	int iLimit = ms_iRenderLimit;
#endif

	if (VTFMANAGER.IsInitialized())
		VTFMANAGER.ClearDeferredRigidBatches();

	for (auto it = s_kVct_pkInstAliveSort.begin(); it != s_kVct_pkInstAliveSort.end(); ++it)
	{
		CInstanceBase* pInstance = *it;

#ifdef ENABLE_CHAR_RENDER_LIMIT
		// Always render main player and PCs, only limit NPCs/mobs
		bool bIsImportant = (pInstance == pkInstMain) || pInstance->IsPC();

		// Check render limit (skip only non-important entities)
		if (!bIsImportant && iLimit > 0 && iRenderCount >= iLimit)
		{
			ms_iSkippedByLimit++;
			continue;
		}
#endif

#ifdef ENABLE_FRUSTUM_CULLING
		if (pkInstMain)
		{
			ms_iTotalCount++;

			// Frustum culling check
			Vector3 v3Center;
			float fRadius = pInstance->GetGraphicThingInstanceRef().GetBoundingSphereRadius();
			pInstance->GetGraphicThingInstanceRef().GetBoundingSpherePosition(&v3Center);

			// Use CScreen's frustum for visibility test
			if (CScreen::GetFrustum().ViewVolumeTest(Vector3d(v3Center.x, v3Center.y, v3Center.z), fRadius) == VS_OUTSIDE)
			{
				ms_iCulledCount++;
				continue;
			}

			ms_iRenderedCount++;
		}
#endif


#ifdef ENABLE_CHAR_RENDER_LIMIT
		iRenderCount++;
#endif

#ifdef ENABLE_RENDER_MODE_GROUPING
		// Group by render mode
		int iRenderMode = pInstance->GetGraphicThingInstanceRef().GetRenderMode();
		switch (iRenderMode)
		{
			case CActorInstance::RENDER_MODE_BLEND:
				s_kVct_pkInstBlend.push_back(pInstance);
				break;
			case CActorInstance::RENDER_MODE_ADD:
				s_kVct_pkInstAdd.push_back(pInstance);
				break;
			case CActorInstance::RENDER_MODE_MODULATE:
				s_kVct_pkInstModulate.push_back(pInstance);
				break;
			default:
				s_kVct_pkInstNormal.push_back(pInstance);
				break;
		}
#else
		if (VTFMANAGER.IsInitialized())
		{
			s_kVct_pkInstVTFDeferred.push_back(pInstance);
			pInstance->RenderWithRigidDefer();
		}
		else
			pInstance->Render();
		pInstance->RenderTrace();
#endif
	}

#ifndef ENABLE_RENDER_MODE_GROUPING
	SHADERMANAGER.SavePipelineState(PSTATE_CULLMODE, CULL_NONE);

	if (VTFMANAGER.IsInitialized() && VTFMANAGER.HasDeferredRigidBatches())
		VTFMANAGER.FlushDeferredRigidBatches();

	for (auto it = s_kVct_pkInstVTFDeferred.begin(); it != s_kVct_pkInstVTFDeferred.end(); ++it)
		(*it)->RenderBlendPassDeferred();

	SHADERMANAGER.RestorePipelineState(PSTATE_CULLMODE);
#endif

#ifdef ENABLE_RENDER_MODE_GROUPING
	if (VTFMANAGER.IsInitialized())
	{
		VTFMANAGER.ClearDeferredRigidBatches();

		for (auto it = s_kVct_pkInstNormal.begin(); it != s_kVct_pkInstNormal.end(); ++it)
		{
			(*it)->RenderWithRigidDefer();
			(*it)->RenderTrace();
		}

		SHADERMANAGER.SavePipelineState(PSTATE_CULLMODE, CULL_NONE);

		VTFMANAGER.FlushDeferredRigidBatches();

		for (auto it = s_kVct_pkInstNormal.begin(); it != s_kVct_pkInstNormal.end(); ++it)
			(*it)->RenderBlendPassDeferred();

		SHADERMANAGER.RestorePipelineState(PSTATE_CULLMODE);
	}
	else
	{
		for (auto it = s_kVct_pkInstNormal.begin(); it != s_kVct_pkInstNormal.end(); ++it)
		{
			(*it)->Render();
			(*it)->RenderTrace();
		}
	}

	if (VTFMANAGER.IsInitialized())
	{
		auto renderModeVTF = [](std::vector<CInstanceBase*>& vec) {
			if (vec.empty()) return;
			VTFMANAGER.ClearDeferredRigidBatches();
			for (auto it = vec.begin(); it != vec.end(); ++it)
			{
				(*it)->RenderWithRigidDefer();
				(*it)->RenderTrace();
			}
			SHADERMANAGER.SavePipelineState(PSTATE_CULLMODE, CULL_NONE);
			VTFMANAGER.FlushDeferredRigidBatches();
			for (auto it = vec.begin(); it != vec.end(); ++it)
				(*it)->RenderBlendPassDeferred();
			SHADERMANAGER.RestorePipelineState(PSTATE_CULLMODE);
		};
		renderModeVTF(s_kVct_pkInstBlend);
		renderModeVTF(s_kVct_pkInstAdd);
		renderModeVTF(s_kVct_pkInstModulate);
	}
	else
	{
		for (auto it = s_kVct_pkInstBlend.begin(); it != s_kVct_pkInstBlend.end(); ++it)
		{
			(*it)->Render();
			(*it)->RenderTrace();
		}
		for (auto it = s_kVct_pkInstAdd.begin(); it != s_kVct_pkInstAdd.end(); ++it)
		{
			(*it)->Render();
			(*it)->RenderTrace();
		}
		for (auto it = s_kVct_pkInstModulate.begin(); it != s_kVct_pkInstModulate.end(); ++it)
		{
			(*it)->Render();
			(*it)->RenderTrace();
		}
	}

	// Update stats
	ms_iRenderModeNormal = (int)s_kVct_pkInstNormal.size();
	ms_iRenderModeBlend = (int)s_kVct_pkInstBlend.size();
	ms_iRenderModeAdd = (int)s_kVct_pkInstAdd.size();
	ms_iRenderModeModulate = (int)s_kVct_pkInstModulate.size();
#endif
}

void CPythonCharacterManager::__RenderSortedDeadActorList()
{
	static std::vector<CInstanceBase*> s_kVct_pkInstDeadSort;
	s_kVct_pkInstDeadSort.clear();

	TCharacterInstanceList& rkLst_pkInstDead=m_kDeadInstList;
	TCharacterInstanceList::iterator i;

	CCamera* pDeadCamera = CCameraManager::Instance().GetCurrentCamera();
	if (pDeadCamera)
	{
		const Vector3 v3Eye = pDeadCamera->GetEye();
		const float fMaxSq = kDeadActorMaxDistance * kDeadActorMaxDistance;
		for (i=rkLst_pkInstDead.begin(); i!=rkLst_pkInstDead.end(); ++i)
		{
			const Vector3& v3Pos = (*i)->GetGraphicThingInstanceRef().GetPosition();
			const float dx = v3Pos.x - v3Eye.x, dy = v3Pos.y - v3Eye.y, dz = v3Pos.z - v3Eye.z;
			if ((dx*dx + dy*dy + dz*dz) <= fMaxSq)
				s_kVct_pkInstDeadSort.push_back(*i);
		}

		if (s_kVct_pkInstDeadSort.size() > kDeadActorMaxRender)
		{
			std::partial_sort(s_kVct_pkInstDeadSort.begin(),
			                  s_kVct_pkInstDeadSort.begin() + kDeadActorMaxRender,
			                  s_kVct_pkInstDeadSort.end(),
			                  [&v3Eye](CInstanceBase* a, CInstanceBase* b) {
				const Vector3& pa = a->GetGraphicThingInstanceRef().GetPosition();
				const Vector3& pb = b->GetGraphicThingInstanceRef().GetPosition();
				const float da = (pa.x-v3Eye.x)*(pa.x-v3Eye.x) + (pa.y-v3Eye.y)*(pa.y-v3Eye.y) + (pa.z-v3Eye.z)*(pa.z-v3Eye.z);
				const float db = (pb.x-v3Eye.x)*(pb.x-v3Eye.x) + (pb.y-v3Eye.y)*(pb.y-v3Eye.y) + (pb.z-v3Eye.z)*(pb.z-v3Eye.z);
				return da < db;
			});
			s_kVct_pkInstDeadSort.resize(kDeadActorMaxRender);
		}
	}
	else
	{
		for (i=rkLst_pkInstDead.begin(); i!=rkLst_pkInstDead.end(); ++i)
			s_kVct_pkInstDeadSort.push_back(*i);
	}

	std::sort(s_kVct_pkInstDeadSort.begin(), s_kVct_pkInstDeadSort.end(), LessCharacterInstancePtrRenderOrder());
	std::for_each(s_kVct_pkInstDeadSort.begin(), s_kVct_pkInstDeadSort.end(), FCharacterInstanceRender());
}

void CPythonCharacterManager::Render()
{
#ifdef ENABLE_RENDER_MODE_GROUPING
	CGrannyMaterial::ResetRenderStateCache();
#endif

	SHADERMANAGER.SetLightingEnabled(true);

	SHADERMANAGER.SetDefaultTexture(0);

	SHADERMANAGER.SetShaderResource(1, NULL);
	SHADERMANAGER.SetTwoTextureBlend(false);

	bool bShadowEnabled = false;
	CPythonBackground& rkBG = CPythonBackground::Instance();
	if (rkBG.IsMapReady())
	{
		CMapOutdoor& rkMap = rkBG.GetMapOutdoorRef();
		bShadowEnabled = rkMap.IsCharacterShadowEnabled();
	}

	__RenderSortedAliveActorList();
	const size_t nDeferAfterAlive = VTFMANAGER.GetDeferredRigidCount();
	__RenderSortedDeadActorList();

	{
		static int  s_probeTick = 0;
		static bool s_probeOn   = false;
		if ((s_probeTick % 60) == 0)
		{
			s_probeOn = false;
			if (FILE* fp = fopen("chrprobe.txt", "r")) { s_probeOn = true; fclose(fp); }
		}
		if (s_probeOn && (s_probeTick % 60) == 0)
		{
			TraceError("[CHRPROBE] alive=%u dead=%u deferAfterAlive=%u deferAfterDead=%u deferShadow=%u",
			           (unsigned)m_kAliveInstMap.size(), (unsigned)m_kDeadInstList.size(),
			           (unsigned)nDeferAfterAlive,
			           (unsigned)VTFMANAGER.GetDeferredRigidCount(),
			           (unsigned)VTFMANAGER.GetDeferredShadowCount());

			for (TCharacterInstanceMap::iterator it = m_kAliveInstMap.begin(); it != m_kAliveInstMap.end(); ++it)
			{
				CInstanceBase* p = it->second;
				if (!p) continue;
				const Vector3& v = p->GetGraphicThingInstanceRef().GetPosition();
				TraceError("[CHRPROBE]   ALIVE vid=%u race=%u hair=%u pos=%.1f,%.1f,%.1f",
				           (unsigned)p->GetVirtualID(), (unsigned)p->GetRace(),
				           (unsigned)p->GetPart(CRaceData::PART_HAIR), v.x, v.y, v.z);
			}
			for (TCharacterInstanceList::iterator it = m_kDeadInstList.begin(); it != m_kDeadInstList.end(); ++it)
			{
				CInstanceBase* p = *it;
				if (!p) continue;
				const Vector3& v = p->GetGraphicThingInstanceRef().GetPosition();
				TraceError("[CHRPROBE]   DEAD  vid=%u race=%u hair=%u pos=%.1f,%.1f,%.1f",
				           (unsigned)p->GetVirtualID(), (unsigned)p->GetRace(),
				           (unsigned)p->GetPart(CRaceData::PART_HAIR), v.x, v.y, v.z);
			}
		}
		++s_probeTick;
	}


	CInstanceBase * pkPickedInst = OLD_GetPickedInstancePtr();
	if (pkPickedInst)
	{
		const Vector3 & c_rv3Position = pkPickedInst->GetGraphicThingInstanceRef().GetPosition();
		CPythonGraphic::Instance().ProjectPosition(c_rv3Position.x, c_rv3Position.y, c_rv3Position.z, &m_v2PickedInstProjPos.x, &m_v2PickedInstProjPos.y);
	}
}

void CPythonCharacterManager::RenderShadowMainInstance()
{
	CInstanceBase* pkInstMain=GetMainInstancePtr();
	if (pkInstMain)
		pkInstMain->RenderToShadowMap();
}

struct FCharacterManagerCharacterInstanceRenderToShadowMap
{
	inline void operator () (const std::pair<DWORD,CInstanceBase *>& cr_Pair)
	{
		cr_Pair.second->RenderToShadowMap();
	}
};

namespace
{
	void __DeferShadowRigidParts(CInstanceBase* pkInst)
	{
		if (!pkInst) return;
		CActorInstance& rkActor = pkInst->GetGraphicThingInstanceRef();
		for (DWORD idx = 0; idx < rkActor.GetLODControllerCount(); ++idx)
		{
			CGrannyLODController* pLOD = rkActor.GetLODControllerPointer(idx);
			if (!pLOD || !pLOD->isModelInstance()) continue;
			CGrannyModelInstance* pModelInst = pLOD->GetCurrentModelInstance();
			if (!pModelInst || pModelInst->IsEmpty()) continue;
			if (pModelInst->HasRigidMeshes())
			{
				CGrannyModel* pModel = pModelInst->GetModel();
				if (pModel)
					VTFMANAGER.DeferRigidModelInstanceForShadow(pModel, pModelInst);
			}
		}
	}
}

void CPythonCharacterManager::CollectShadowCastersForFrame()
{
	m_kVct_pkShadowCasters.clear();
	VTFMANAGER.ClearDeferredShadowBatches();

#ifdef ENABLE_SHADOW_RENDER_LIMIT
	CInstanceBase* pkInstMain = GetMainInstancePtr();
	if (pkInstMain)
	{
		m_kVct_pkShadowCasters.push_back(pkInstMain);
		__DeferShadowRigidParts(pkInstMain);
	}

	static const float SHADOW_MAX_DISTANCE = 3000.0f;
	int iShadowCount = 0;
	for (TCharacterInstanceMap::iterator itor = m_kAliveInstMap.begin();
		 itor != m_kAliveInstMap.end(); ++itor)
	{
		CInstanceBase* pkInst = itor->second;
		if (pkInst == pkInstMain)
			continue;
		if (pkInst->m_dwCachedDistanceFrame == ms_dwFrameCounter &&
			pkInst->m_fCachedDistanceFromMain > SHADOW_MAX_DISTANCE)
			continue;

		m_kVct_pkShadowCasters.push_back(pkInst);
		__DeferShadowRigidParts(pkInst);
		if (++iShadowCount >= SHADOW_RENDER_LIMIT_DEFAULT)
			break;
	}
#else
	for (TCharacterInstanceMap::iterator itor = m_kAliveInstMap.begin();
		 itor != m_kAliveInstMap.end(); ++itor)
	{
		m_kVct_pkShadowCasters.push_back(itor->second);
		__DeferShadowRigidParts(itor->second);
	}
#endif
}

void CPythonCharacterManager::EndShadowCastersForFrame()
{
	m_kVct_pkShadowCasters.clear();
	VTFMANAGER.ClearDeferredShadowBatches();
}

void CPythonCharacterManager::RenderShadowAllInstances()
{
	CGrannyModelInstance::ms_bShadowSkipRigid = true;

	for (auto* pkInst : m_kVct_pkShadowCasters)
		pkInst->RenderToShadowMap();

	CGrannyModelInstance::ms_bShadowSkipRigid = false;

	VTFMANAGER.FlushDeferredShadowBatches();
}

struct FCharacterManagerCharacterInstanceRenderCollision
{
	inline void operator () (const std::pair<DWORD,CInstanceBase *>& cr_Pair)
	{
		cr_Pair.second->RenderCollision();
	}
};

void CPythonCharacterManager::RenderCollision()
{
 	std::for_each(m_kAliveInstMap.begin(), m_kAliveInstMap.end(), FCharacterManagerCharacterInstanceRenderCollision());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Managing Process

CInstanceBase * CPythonCharacterManager::CreateInstance(const CInstanceBase::SCreateData& c_rkCreateData)
{
	CInstanceBase * pCharacterInstance = RegisterInstance(c_rkCreateData.m_dwVID);
	if (!pCharacterInstance)
	{
		TraceError("CPythonCharacterManager::CreateInstance: VID[%d] - ALREADY EXIST\n", c_rkCreateData.m_dwVID); // @fixme010
		return NULL;
	}

	if (!pCharacterInstance->Create(c_rkCreateData))
	{
		TraceError("CPythonCharacterManager::CreateInstance VID[%d] Race[%d]", c_rkCreateData.m_dwVID, c_rkCreateData.m_dwRace);
		DeleteInstance(c_rkCreateData.m_dwVID);
		return NULL;
	}

	if (c_rkCreateData.m_isMain)
		SelectInstance(c_rkCreateData.m_dwVID);

	return (pCharacterInstance);
}

CInstanceBase * CPythonCharacterManager::RegisterInstance(DWORD VirtualID)
{
	TCharacterInstanceMap::iterator itor = m_kAliveInstMap.find(VirtualID);

	if (m_kAliveInstMap.end() != itor)
	{
		return NULL;
	}

	CInstanceBase * pCharacterInstance = CInstanceBase::New();
	m_kAliveInstMap.insert(TCharacterInstanceMap::value_type(VirtualID, pCharacterInstance));

	return (pCharacterInstance);
}

void CPythonCharacterManager::DeleteInstance(DWORD dwDelVID)
{
	TCharacterInstanceMap::iterator itor = m_kAliveInstMap.find(dwDelVID);

	if (m_kAliveInstMap.end() == itor)
	{
		Tracef("DeleteCharacterInstance: no vid by %d\n", dwDelVID);
		return;
	}

	CInstanceBase * pkInstDel = itor->second;

	if (pkInstDel == m_pkInstBind)
		m_pkInstBind = NULL;

	if (pkInstDel == m_pkInstMain)
		m_pkInstMain = NULL;

	if (pkInstDel == m_pkInstPick)
		m_pkInstPick = NULL;

	CInstanceBase::Delete(pkInstDel);

	m_kAliveInstMap.erase(itor);
}

void CPythonCharacterManager::__DeleteBlendOutInstance(CInstanceBase* pkInstDel)
{
	pkInstDel->DeleteBlendOut();
	m_kDeadInstList.push_back(pkInstDel);

	IAbstractPlayer& rkPlayer=IAbstractPlayer::GetSingleton();
	rkPlayer.NotifyCharacterDead(pkInstDel->GetVirtualID());
}

void CPythonCharacterManager::DeleteInstanceByFade(DWORD dwVID)
{
	TCharacterInstanceMap::iterator f = m_kAliveInstMap.find(dwVID);
	if (m_kAliveInstMap.end() == f)
	{
		return;
	}
	__DeleteBlendOutInstance(f->second);
	m_kAliveInstMap.erase(f);
}

void CPythonCharacterManager::SelectInstance(DWORD VirtualID)
{
	TCharacterInstanceMap::iterator itor = m_kAliveInstMap.find(VirtualID);

	if (m_kAliveInstMap.end() == itor)
	{
		Tracef("SelectCharacterInstance: no vid by %d\n", VirtualID);
		return;
	}

	m_pkInstBind = itor->second;
}

CInstanceBase * CPythonCharacterManager::GetInstancePtr(DWORD VirtualID)
{
	TCharacterInstanceMap::iterator itor = m_kAliveInstMap.find(VirtualID);

	if (m_kAliveInstMap.end() == itor)
		return NULL;

	return itor->second;
}

CInstanceBase * CPythonCharacterManager::GetInstancePtrByName(const char *name)
{
	TCharacterInstanceMap::iterator itor;

	for (itor = m_kAliveInstMap.begin(); itor != m_kAliveInstMap.end(); itor++)
	{
		CInstanceBase * pInstance = itor->second;

		if (!strcmp(pInstance->GetNameString(), name))
			return pInstance;
	}

	return NULL;
}

CInstanceBase * CPythonCharacterManager::GetSelectedInstancePtr()
{
	return m_pkInstBind;
}

CInstanceBase* CPythonCharacterManager::FindClickableInstancePtr()
{
	return NULL;
}

void CPythonCharacterManager::__UpdateSortPickedActorList()
{
	__UpdatePickedActorList();
	__SortPickedActorList();
}

void CPythonCharacterManager::__UpdatePickedActorList()
{
	m_kVct_pkInstPicked.clear();

	static const float PICK_MAX_DISTANCE = 5000.0f;

	TCharacterInstanceMap::iterator i;
	for (i=m_kAliveInstMap.begin(); i!=m_kAliveInstMap.end(); ++i)
	{
		CInstanceBase* pkInstEach=i->second;

		if (pkInstEach->m_dwCachedDistanceFrame == ms_dwFrameCounter &&
			pkInstEach->m_fCachedDistanceFromMain > PICK_MAX_DISTANCE)
			continue;

		if (pkInstEach->CanPickInstance())
		{
			if (pkInstEach->IsDead())
			{
				if (pkInstEach->IntersectBoundingBox())
					m_kVct_pkInstPicked.push_back(pkInstEach);
			}
			else
			{
				if (pkInstEach->IntersectDefendingSphere())
					m_kVct_pkInstPicked.push_back(pkInstEach);
			}
		}
	}
}

struct CInstanceBase_SLessCameraDistance
{
	TPixelPosition m_kPPosEye;

	bool operator() (CInstanceBase* pkInstLeft, CInstanceBase* pkInstRight)
	{
		int nLeftDeadPoint=pkInstLeft->IsDead();
		int nRightDeadPoint=pkInstRight->IsDead();

		if (nLeftDeadPoint<nRightDeadPoint)
			return true;

		if (pkInstLeft->CalculateDistanceSq3d(m_kPPosEye)<pkInstRight->CalculateDistanceSq3d(m_kPPosEye))
			return true;

		return false;
	}
};

void CPythonCharacterManager::__SortPickedActorList()
{
	CCamera * pCamera = CCameraManager::Instance().GetCurrentCamera();
	const Vector3& c_rv3EyePos=pCamera->GetEye();

	CInstanceBase_SLessCameraDistance kLess;
	kLess.m_kPPosEye=TPixelPosition(+c_rv3EyePos.x, -c_rv3EyePos.y, +c_rv3EyePos.z);

	std::sort(m_kVct_pkInstPicked.begin(), m_kVct_pkInstPicked.end(), kLess);
}

void CPythonCharacterManager::__NEW_Pick()
{
	__UpdateSortPickedActorList();

	CInstanceBase* pkInstMain=GetMainInstancePtr();

#ifdef __MOVIE_MODE
	if (pkInstMain)
		if (pkInstMain->IsMovieMode())
		{
			if (m_pkInstPick)
				m_pkInstPick->OnUnselected();
			return;
		}
#endif

	bool bMainInPickedList = false;

	{
		std::vector<CInstanceBase*>::iterator f;
		for (f=m_kVct_pkInstPicked.begin(); f!=m_kVct_pkInstPicked.end(); ++f)
		{
			CInstanceBase* pkInstEach=*f;
			if (pkInstEach == pkInstMain)
			{
				bMainInPickedList = true;
				continue;
			}
			if (pkInstEach->IntersectBoundingBox())
			{
				if (m_pkInstPick)
					if (m_pkInstPick!=pkInstEach)
						m_pkInstPick->OnUnselected();

				if (pkInstEach->CanPickInstance())
				{
					m_pkInstPick = pkInstEach;
					m_pkInstPick->OnSelected();
					return;
				}
			}
		}
	}

	{
		std::vector<CInstanceBase*>::iterator f;
		for (f=m_kVct_pkInstPicked.begin(); f!=m_kVct_pkInstPicked.end(); ++f)
		{
			CInstanceBase* pkInstEach=*f;
			if (pkInstEach!=pkInstMain)
			{
				if (m_pkInstPick)
					if (m_pkInstPick!=pkInstEach)
						m_pkInstPick->OnUnselected();

				if (pkInstEach->CanPickInstance())
				{
					m_pkInstPick = pkInstEach;
					m_pkInstPick->OnSelected();
					return;
				}
			}
		}
	}

	if (pkInstMain && bMainInPickedList)
	if (pkInstMain->CanPickInstance())
	{
		if (m_pkInstPick)
			if (m_pkInstPick!=pkInstMain)
				m_pkInstPick->OnUnselected();

		m_pkInstPick = pkInstMain;
		m_pkInstPick->OnSelected();
		return;
	}

	if (m_pkInstPick)
	{
		m_pkInstPick->OnUnselected();
		m_pkInstPick=NULL;
	}
}

void CPythonCharacterManager::__OLD_Pick()
{
	for (TCharacterInstanceMap::iterator itor = m_kAliveInstMap.begin(); itor != m_kAliveInstMap.end(); ++itor)
	{
		CInstanceBase * pkInstEach = itor->second;

		if (pkInstEach == m_pkInstMain)
			continue;

		if (pkInstEach->IntersectDefendingSphere())
		{
			if (m_pkInstPick)
				if (m_pkInstPick!=pkInstEach)
					m_pkInstPick->OnUnselected();

			m_pkInstPick = pkInstEach;
			m_pkInstPick->OnSelected();

			return;
		}
	}

	if (m_pkInstPick)
	{
		m_pkInstPick->OnUnselected();
		m_pkInstPick=NULL;
	}
}

int CPythonCharacterManager::PickAll()
{
	for (TCharacterInstanceMap::iterator itor = m_kAliveInstMap.begin(); itor != m_kAliveInstMap.end(); ++itor)
	{
		CInstanceBase * pInstance = itor->second;

		if (pInstance->IntersectDefendingSphere())
			return pInstance->GetVirtualID();
	}

	return -1;
}

CInstanceBase * CPythonCharacterManager::GetCloseInstance(CInstanceBase * pInstance)
{
	float fMinDistance = 10000.0f;
	CInstanceBase * pCloseInstance = NULL;

	TCharacterInstanceMap::iterator itor = m_kAliveInstMap.begin();
	for (; itor != m_kAliveInstMap.end(); ++itor)
	{
		CInstanceBase * pTargetInstance = itor->second;

		if (pTargetInstance == pInstance)
			continue;

		DWORD dwVirtualNumber = pTargetInstance->GetVirtualNumber();
		if (CPythonNonPlayer::ON_CLICK_EVENT_BATTLE != CPythonNonPlayer::Instance().GetEventType(dwVirtualNumber))
			continue;

		float fDistance = pInstance->GetDistance(pTargetInstance);
		if (fDistance < fMinDistance)
		{
			fMinDistance = fDistance;
			pCloseInstance = pTargetInstance;
		}
	}

	return pCloseInstance;
}

void CPythonCharacterManager::RefreshAllPCTextTail()
{
	CPythonCharacterManager::CharacterIterator itor = CharacterInstanceBegin();
	CPythonCharacterManager::CharacterIterator itorEnd = CharacterInstanceEnd();
	for (; itor != itorEnd; ++itor)
	{
		CInstanceBase * pInstance = *itor;
		if (!pInstance->IsPC())
			continue;

		pInstance->RefreshTextTail();
	}
}

void CPythonCharacterManager::RefreshAllGuildMark()
{
	CPythonCharacterManager::CharacterIterator itor = CharacterInstanceBegin();
	CPythonCharacterManager::CharacterIterator itorEnd = CharacterInstanceEnd();
	for (; itor != itorEnd; ++itor)
	{
		CInstanceBase * pInstance = *itor;
		if (!pInstance->IsPC())
			continue;

		pInstance->ChangeGuild(pInstance->GetGuildID());
		pInstance->RefreshTextTail();
	}
}

void CPythonCharacterManager::DeleteAllInstances()
{
	DestroyAliveInstanceMap();
	DestroyDeadInstanceList();
}

void CPythonCharacterManager::DestroyAliveInstanceMap()
{
	for (TCharacterInstanceMap::iterator i = m_kAliveInstMap.begin(); i != m_kAliveInstMap.end(); ++i)
		CInstanceBase::Delete(i->second);

	m_kAliveInstMap.clear();
}

void CPythonCharacterManager::DestroyDeadInstanceList()
{
	std::for_each(m_kDeadInstList.begin(), m_kDeadInstList.end(), CInstanceBase::Delete);
	m_kDeadInstList.clear();
}

void CPythonCharacterManager::Destroy()
{
	DeleteAllInstances();

	CInstanceBase::DestroySystem();

	__Initialize();
}

void CPythonCharacterManager::__Initialize()
{
	memset(m_adwPointEffect, 0, sizeof(m_adwPointEffect));
	m_pkInstMain = NULL;
	m_pkInstBind = NULL;
	m_pkInstPick = NULL;
	m_v2PickedInstProjPos = Vector2(0.0f, 0.0f);
}

CPythonCharacterManager::CPythonCharacterManager()
{
	__Initialize();
}

CPythonCharacterManager::~CPythonCharacterManager()
{
	Destroy();
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
