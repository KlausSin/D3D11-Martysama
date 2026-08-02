#include "StdAfx.h"
#include <process.h>
#include "RaceManager.h"
#include "RaceMotionData.h"
#include "../EterPack/EterPackManager.h"

bool __IsGuildRace(unsigned race)
{
	if (race >= 14000 && race < 15000)
		return true;

	if (20043 == race)
		return true;

	return false;
}

bool __IsNPCRace(unsigned race)
{
	if (race > 9000)
		return true;

	return false;
}

void __GetRaceResourcePathes(unsigned race, std::vector <std::string> & vec_stPathes)
{
	if (__IsGuildRace(race))
	{
		vec_stPathes.emplace_back("d:/ymir work/guild/");
		vec_stPathes.emplace_back("d:/ymir work/npc/");
		vec_stPathes.emplace_back("d:/ymir work/npc2/");
		vec_stPathes.emplace_back("d:/ymir work/npc_pet/");
		vec_stPathes.emplace_back("d:/ymir work/npc_mount/");
		vec_stPathes.emplace_back("d:/ymir work/monster/");
		vec_stPathes.emplace_back("d:/ymir work/monster2/");
	}
	else if (__IsNPCRace(race))
	{
		if (race >= 30000)
		{
			if (race>=34028 && race<=34099) // last known 34072
			{
				vec_stPathes.emplace_back("d:/ymir work/npc_pet/");
				vec_stPathes.emplace_back("d:/ymir work/npc2/");
			}
			else
			{
				vec_stPathes.emplace_back("d:/ymir work/npc2/");
				vec_stPathes.emplace_back("d:/ymir work/npc_pet/");
			}
			vec_stPathes.emplace_back("d:/ymir work/npc/");
			vec_stPathes.emplace_back("d:/ymir work/npc_mount/");
			vec_stPathes.emplace_back("d:/ymir work/monster/");
			vec_stPathes.emplace_back("d:/ymir work/monster2/");
			vec_stPathes.emplace_back("d:/ymir work/guild/");
		}
		else
		{
			if (race>=20233 && race<=20299) // last known 20247
			{
				vec_stPathes.emplace_back("d:/ymir work/npc_mount/");
				vec_stPathes.emplace_back("d:/ymir work/npc/");
			}
			else
			{
				vec_stPathes.emplace_back("d:/ymir work/npc/");
				vec_stPathes.emplace_back("d:/ymir work/npc_mount/");
			}
			vec_stPathes.emplace_back("d:/ymir work/npc2/");
			vec_stPathes.emplace_back("d:/ymir work/npc_pet/");
			vec_stPathes.emplace_back("d:/ymir work/monster/");
			vec_stPathes.emplace_back("d:/ymir work/monster2/");
			vec_stPathes.emplace_back("d:/ymir work/guild/");
		}
	}
	else if (8507 == race || 8510 == race)
	{
		vec_stPathes.emplace_back("d:/ymir work/monster2/");
		vec_stPathes.emplace_back("d:/ymir work/monster/");
		vec_stPathes.emplace_back("d:/ymir work/npc/");
		vec_stPathes.emplace_back("d:/ymir work/npc2/");
		vec_stPathes.emplace_back("d:/ymir work/npc_pet/");
		vec_stPathes.emplace_back("d:/ymir work/npc_mount/");
		vec_stPathes.emplace_back("d:/ymir work/guild/");
	}
	else if (race > 8000)
	{
		vec_stPathes.emplace_back("d:/ymir work/monster/");
		vec_stPathes.emplace_back("d:/ymir work/monster2/");
		vec_stPathes.emplace_back("d:/ymir work/npc/");
		vec_stPathes.emplace_back("d:/ymir work/npc2/");
		vec_stPathes.emplace_back("d:/ymir work/npc_pet/");
		vec_stPathes.emplace_back("d:/ymir work/npc_mount/");
		vec_stPathes.emplace_back("d:/ymir work/guild/");
	}
	else if (race > 2000)
	{
		vec_stPathes.emplace_back("d:/ymir work/monster2/");
		vec_stPathes.emplace_back("d:/ymir work/monster/");
		vec_stPathes.emplace_back("d:/ymir work/npc/");
		vec_stPathes.emplace_back("d:/ymir work/npc2/");
		vec_stPathes.emplace_back("d:/ymir work/npc_pet/");
		vec_stPathes.emplace_back("d:/ymir work/npc_mount/");
		vec_stPathes.emplace_back("d:/ymir work/guild/");
	}
	else if (race>=1400 && race<=1700)
	{
		vec_stPathes.emplace_back("d:/ymir work/monster2/");
		vec_stPathes.emplace_back("d:/ymir work/monster/");
		vec_stPathes.emplace_back("d:/ymir work/npc/");
		vec_stPathes.emplace_back("d:/ymir work/npc2/");
		vec_stPathes.emplace_back("d:/ymir work/npc_pet/");
		vec_stPathes.emplace_back("d:/ymir work/npc_mount/");
		vec_stPathes.emplace_back("d:/ymir work/guild/");
	}
	else
	{
		vec_stPathes.emplace_back("d:/ymir work/monster/");
		vec_stPathes.emplace_back("d:/ymir work/monster2/");
		vec_stPathes.emplace_back("d:/ymir work/npc/");
		vec_stPathes.emplace_back("d:/ymir work/npc2/");
		vec_stPathes.emplace_back("d:/ymir work/npc_pet/");
		vec_stPathes.emplace_back("d:/ymir work/npc_mount/");
		vec_stPathes.emplace_back("d:/ymir work/guild/");
	}
	// @fixme021 add season folders
	vec_stPathes.emplace_back("#season1/npc/");
	vec_stPathes.emplace_back("#season2/npc/");
	vec_stPathes.emplace_back("#season1/monster/");
	vec_stPathes.emplace_back("#season2/monster/");
}

CRaceData * CRaceManager::__LoadRaceData(DWORD dwRaceIndex)
{
	auto fRaceName=m_kMap_dwRaceKey_stRaceName.find(dwRaceIndex);
	if (m_kMap_dwRaceKey_stRaceName.end()==fRaceName)
		return NULL;

	const std::string & c_rstRaceName=fRaceName->second;

	if (c_rstRaceName.empty())
		return NULL;

	// LOAD_LOCAL_RESOURCE
	if (c_rstRaceName[0] == '#')
	{
		const char * pathName = c_rstRaceName.c_str() + 1;
		char shapeFileName[256];
		char motionListFileName[256];
		_snprintf(shapeFileName, sizeof(shapeFileName), "%sshape.msm", pathName);
		_snprintf(motionListFileName, sizeof(motionListFileName), "%smotlist.txt", pathName);

		CRaceData * pRaceData = CRaceData::New();
		pRaceData->SetRace(dwRaceIndex);
		if (!pRaceData->LoadRaceData(shapeFileName))
		{
			TraceError("CRaceManager::RegisterRacePath(race=%u).LoadRaceData(%s)", dwRaceIndex, shapeFileName);
			CRaceData::Delete(pRaceData);
			return NULL;
		}

		__LoadRaceMotionList(*pRaceData, pathName, motionListFileName);

		return pRaceData;
	}
	// END_OF_LOAD_LOCAL_RESOURCE
	std::vector <std::string> vec_stFullPathName;
	__GetRaceResourcePathes(dwRaceIndex, vec_stFullPathName);

	CRaceData * pRaceData = CRaceData::New();
	pRaceData->SetRace(dwRaceIndex);

	for (int i = 0; i < vec_stFullPathName.size(); i++)
	{
		std::string stFullPathName = vec_stFullPathName[i];
		{
			auto fRaceSrcName=m_kMap_stRaceName_stSrcName.find(c_rstRaceName);
			if (m_kMap_stRaceName_stSrcName.end()==fRaceSrcName)
				stFullPathName+=c_rstRaceName;
			else
				stFullPathName+=fRaceSrcName->second;
		}

		stFullPathName+="/";

		string stMSMFileName=stFullPathName;
		if (stFullPathName[0] == '#') // @fixme021 #season reads shape.msm only
		{
			stMSMFileName += "shape.msm";
			stFullPathName.erase(0, 1);
			stMSMFileName.erase(0, 1);
		}
		else
		{
			stMSMFileName += c_rstRaceName;
			stMSMFileName += ".msm";
		}

		if (!pRaceData->LoadRaceData(stMSMFileName.c_str()))
		{
			if (i != vec_stFullPathName.size() - 1)
			{
				Tracenf("CRaceManager::RegisterRacePath : RACE[%u] LOAD MSMFILE[%s] ERROR. Will Find Another Path.", dwRaceIndex, stMSMFileName.c_str()); // @warme668
				continue;
			}

			TraceError("CRaceManager::RegisterRacePath : RACE[%u] LOAD MSMFILE[%s] ERROR", dwRaceIndex, stMSMFileName.c_str());
			CRaceData::Delete(pRaceData);
			return NULL;
		}

		std::string stMotionListFileName=stFullPathName;
		stMotionListFileName+=pRaceData->GetMotionListFileName();

		__LoadRaceMotionList(*pRaceData, stFullPathName.c_str(), stMotionListFileName.c_str());

		return pRaceData;
	}
	TraceError("CRaceManager::RegisterRacePath : RACE[%u] HAVE NO PATH ERROR", dwRaceIndex);
	CRaceData::Delete(pRaceData);
	return NULL;
}

bool CRaceManager::__LoadRaceMotionList(CRaceData & rkRaceData, const char * pathName, const char * motionListFileName)
{
	static std::map<std::string, DWORD> s_kMap_stType_dwIndex;
	static bool s_isInit=false;

	if (!s_isInit)
	{
		s_isInit=true;

		s_kMap_stType_dwIndex.emplace("SPAWN", CRaceMotionData::NAME_SPAWN);
		s_kMap_stType_dwIndex.emplace("WAIT", CRaceMotionData::NAME_WAIT);
		s_kMap_stType_dwIndex.emplace("WAIT1", CRaceMotionData::NAME_WAIT);
		s_kMap_stType_dwIndex.emplace("WAIT2", CRaceMotionData::NAME_WAIT);
		s_kMap_stType_dwIndex.emplace("WALK", CRaceMotionData::NAME_WALK);
		s_kMap_stType_dwIndex.emplace("WALK1", CRaceMotionData::NAME_WALK);
		s_kMap_stType_dwIndex.emplace("WALK2", CRaceMotionData::NAME_WALK);
		s_kMap_stType_dwIndex.emplace("RUN", CRaceMotionData::NAME_RUN);
		s_kMap_stType_dwIndex.emplace("RUN1", CRaceMotionData::NAME_RUN);
		s_kMap_stType_dwIndex.emplace("RUN2", CRaceMotionData::NAME_RUN);
		s_kMap_stType_dwIndex.emplace("STOP", CRaceMotionData::NAME_STOP);
		s_kMap_stType_dwIndex.emplace("DEAD", CRaceMotionData::NAME_DEAD);
		s_kMap_stType_dwIndex.emplace("COMBO_ATTACK", CRaceMotionData::NAME_COMBO_ATTACK_1);
		s_kMap_stType_dwIndex.emplace("COMBO_ATTACK1", CRaceMotionData::NAME_COMBO_ATTACK_2);
		s_kMap_stType_dwIndex.emplace("COMBO_ATTACK2", CRaceMotionData::NAME_COMBO_ATTACK_3);
		s_kMap_stType_dwIndex.emplace("NORMAL_ATTACK", CRaceMotionData::NAME_NORMAL_ATTACK);
		s_kMap_stType_dwIndex.emplace("NORMAL_ATTACK1", CRaceMotionData::NAME_NORMAL_ATTACK);
		s_kMap_stType_dwIndex.emplace("NORMAL_ATTACK2", CRaceMotionData::NAME_NORMAL_ATTACK);
		s_kMap_stType_dwIndex.emplace("FRONT_DAMAGE", CRaceMotionData::NAME_DAMAGE);
		s_kMap_stType_dwIndex.emplace("FRONT_DAMAGE1", CRaceMotionData::NAME_DAMAGE);
		s_kMap_stType_dwIndex.emplace("FRONT_DAMAGE2", CRaceMotionData::NAME_DAMAGE);
		s_kMap_stType_dwIndex.emplace("FRONT_DAMAGE3", CRaceMotionData::NAME_DAMAGE);
		s_kMap_stType_dwIndex.emplace("FRONT_DEAD", CRaceMotionData::NAME_DEAD);
		s_kMap_stType_dwIndex.emplace("FRONT_DEAD1", CRaceMotionData::NAME_DEAD);
		s_kMap_stType_dwIndex.emplace("FRONT_DEAD2", CRaceMotionData::NAME_DEAD);
		s_kMap_stType_dwIndex.emplace("FRONT_KNOCKDOWN", CRaceMotionData::NAME_DAMAGE_FLYING);
		s_kMap_stType_dwIndex.emplace("FRONT_KNOCKDOWN1", CRaceMotionData::NAME_DAMAGE_FLYING);
		s_kMap_stType_dwIndex.emplace("FRONT_STANDUP", CRaceMotionData::NAME_STAND_UP);
		s_kMap_stType_dwIndex.emplace("FRONT_STANDUP1", CRaceMotionData::NAME_STAND_UP);
		s_kMap_stType_dwIndex.emplace("BACK_DAMAGE", CRaceMotionData::NAME_DAMAGE_BACK);
		s_kMap_stType_dwIndex.emplace("BACK_DAMAGE1", CRaceMotionData::NAME_DAMAGE_BACK);
		s_kMap_stType_dwIndex.emplace("BACK_DEAD", CRaceMotionData::NAME_DEAD_BACK);
		s_kMap_stType_dwIndex.emplace("BACK_DEAD1", CRaceMotionData::NAME_DEAD_BACK);
		s_kMap_stType_dwIndex.emplace("BACK_DEAD2", CRaceMotionData::NAME_DEAD_BACK);
		s_kMap_stType_dwIndex.emplace("BACK_KNOCKDOWN", CRaceMotionData::NAME_DAMAGE_FLYING_BACK);
		s_kMap_stType_dwIndex.emplace("BACK_KNOCKDOWN1", CRaceMotionData::NAME_DAMAGE_FLYING_BACK);
		s_kMap_stType_dwIndex.emplace("BACK_STANDUP", CRaceMotionData::NAME_STAND_UP_BACK);
		s_kMap_stType_dwIndex.emplace("BACK_STANDUP1", CRaceMotionData::NAME_STAND_UP_BACK);
		s_kMap_stType_dwIndex.emplace("SPECIAL", CRaceMotionData::NAME_SPECIAL_1);
		s_kMap_stType_dwIndex.emplace("SPECIAL1", CRaceMotionData::NAME_SPECIAL_2);
		s_kMap_stType_dwIndex.emplace("SPECIAL2", CRaceMotionData::NAME_SPECIAL_3);
		s_kMap_stType_dwIndex.emplace("SPECIAL3", CRaceMotionData::NAME_SPECIAL_4);
		s_kMap_stType_dwIndex.emplace("SPECIAL4", CRaceMotionData::NAME_SPECIAL_5);
		s_kMap_stType_dwIndex.emplace("SPECIAL5", CRaceMotionData::NAME_SPECIAL_6);
		s_kMap_stType_dwIndex.emplace("SKILL1", CRaceMotionData::NAME_SKILL+121);
		s_kMap_stType_dwIndex.emplace("SKILL2", CRaceMotionData::NAME_SKILL+122);
		s_kMap_stType_dwIndex.emplace("SKILL3", CRaceMotionData::NAME_SKILL+123);
		s_kMap_stType_dwIndex.emplace("SKILL4", CRaceMotionData::NAME_SKILL+124);
		s_kMap_stType_dwIndex.emplace("SKILL5", CRaceMotionData::NAME_SKILL+125);
	}

	const void * pvData;
	CMappedFile kMappedFile;
	if (!CEterPackManager::Instance().Get(kMappedFile, motionListFileName, &pvData))
		return false;

	CMemoryTextFileLoader kTextFileLoader;
	kTextFileLoader.Bind(kMappedFile.Size(), pvData);

	rkRaceData.RegisterMotionMode(CRaceMotionData::MODE_GENERAL);

	char szMode[256];
	char szType[256];
	char szFile[256];
	int nPercent = 0;

	bool isSpawn=false;

	static std::string stSpawnMotionFileName;
	static std::string stMotionFileName;

	stSpawnMotionFileName = "";
	stMotionFileName = "";

	UINT uLineCount=kTextFileLoader.GetLineCount();
	for (UINT uLineIndex=0; uLineIndex<uLineCount; ++uLineIndex)
	{
		DWORD motionType = CRaceMotionData::NAME_NONE;

		const std::string & c_rstLine=kTextFileLoader.GetLineString(uLineIndex);
		sscanf(c_rstLine.c_str(), "%s %s %s %d", szMode, szType, szFile, &nPercent);

		auto fTypeIndex=s_kMap_stType_dwIndex.find(szType);

		if (s_kMap_stType_dwIndex.end() == fTypeIndex)
		{
			const size_t c_cutLengthLimit = 2;
			bool bFound = false;

			if (c_cutLengthLimit < strlen(szType) + 1)
			{
				for (size_t i = 1; i <= c_cutLengthLimit; ++i)
				{
					std::string typeName = std::string(szType).substr(0, strlen(szType) - i);
					fTypeIndex = s_kMap_stType_dwIndex.find(typeName);

					if (s_kMap_stType_dwIndex.end() != fTypeIndex)
					{
						bFound = true;
						break;
					}
				}
			}

			if (false == bFound)
				continue;
		}

		motionType = fTypeIndex->second;

		stMotionFileName = pathName;
		stMotionFileName += szFile;

		rkRaceData.RegisterMotionData(CRaceMotionData::MODE_GENERAL, motionType, stMotionFileName.c_str(), nPercent);

		switch (motionType)
		{
			case CRaceMotionData::NAME_SPAWN:
				isSpawn=true;
				break;
			case CRaceMotionData::NAME_DAMAGE:
				stSpawnMotionFileName=stMotionFileName;
				break;
		}
	}

	if (!isSpawn && stSpawnMotionFileName!="")
		rkRaceData.RegisterMotionData(CRaceMotionData::MODE_GENERAL, CRaceMotionData::NAME_SPAWN, stSpawnMotionFileName.c_str(), nPercent);

	rkRaceData.RegisterNormalAttack(CRaceMotionData::MODE_GENERAL, CRaceMotionData::NAME_NORMAL_ATTACK);

	return true;
}

void CRaceManager::RegisterRaceSrcName(const char * c_szName, const char * c_szSrcName)
{
	m_kMap_stRaceName_stSrcName.emplace(c_szName, c_szSrcName);
}

void CRaceManager::RegisterRaceName(DWORD dwRaceIndex, const char * c_szName)
{
	m_kMap_dwRaceKey_stRaceName.emplace(dwRaceIndex, c_szName);
}

void CRaceManager::CreateRace(DWORD dwRaceIndex)
{
	if (m_RaceDataMap.end() != m_RaceDataMap.find(dwRaceIndex))
	{
		TraceError("RaceManager::CreateRace : Race %u already created", dwRaceIndex);
		return;
	}

	CRaceData * pRaceData = CRaceData::New();
	pRaceData->SetRace(dwRaceIndex);
	m_RaceDataMap.emplace(dwRaceIndex, pRaceData);

	Tracenf("CRaceManager::CreateRace(dwRaceIndex=%d)", dwRaceIndex);
}

void CRaceManager::SelectRace(DWORD dwRaceIndex)
{
	TRaceDataIterator itor = m_RaceDataMap.find(dwRaceIndex);
	if (m_RaceDataMap.end() == itor)
	{
		assert(!"Failed to select race data!");
		return;
	}

	m_pSelectedRaceData = itor->second;
}

CRaceData * CRaceManager::GetSelectedRaceDataPointer()
{
	return m_pSelectedRaceData;
}

BOOL CRaceManager::GetRaceDataPointer(DWORD dwRaceIndex, CRaceData ** ppRaceData)
{
	TRaceDataIterator itor = m_RaceDataMap.find(dwRaceIndex);

	if (m_RaceDataMap.end() == itor)
	{
		// Check if async loading is available
		if (m_hAsyncThread)
		{
			__RequestAsyncLoad(dwRaceIndex);
			return FALSE;
		}

		// Fallback to synchronous loading if thread not available
		CRaceData * pRaceData = __LoadRaceData(dwRaceIndex);

		if (pRaceData)
		{
			m_RaceDataMap.emplace(dwRaceIndex, pRaceData);
			*ppRaceData = pRaceData;
			return TRUE;
		}

		TraceError("CRaceManager::GetRaceDataPointer: cannot load data by dwRaceIndex %lu", dwRaceIndex);
		return FALSE;
	}

	*ppRaceData = itor->second;
	return TRUE;
}

void CRaceManager::SetPathName(const char * c_szPathName)
{
	m_strPathName = c_szPathName;
}

const char * CRaceManager::GetFullPathFileName(const char * c_szFileName)
{
	static std::string s_stFileName;

	if (c_szFileName[0] != '.')
	{
		s_stFileName = m_strPathName;
		s_stFileName += c_szFileName;
	}
	else
		s_stFileName = c_szFileName;

	return s_stFileName.c_str();
}

void CRaceManager::Create()
{
	CRaceMotionData::CreateSystem(2048);
	CRaceData::CreateSystem(256, 512);

	// Start async loader thread
	m_bAsyncShutdown = false;
	m_hAsyncSemaphore = CreateSemaphore(NULL, 0, 65535, NULL);
	if (m_hAsyncSemaphore)
	{
		m_hAsyncThread = (HANDLE)_beginthreadex(NULL, 0, __AsyncLoaderThreadEntry, this, 0, &m_uAsyncThreadID);
		if (m_hAsyncThread)
			SetThreadPriority(m_hAsyncThread, THREAD_PRIORITY_BELOW_NORMAL);
	}
}

void CRaceManager::__Initialize()
{
	m_pSelectedRaceData = NULL;
	m_hAsyncThread = NULL;
	m_uAsyncThreadID = 0;
	m_hAsyncSemaphore = NULL;
	m_bAsyncShutdown = false;
}

void CRaceManager::__DestroyRaceDataMap()
{
	TRaceDataMap::iterator i;
	for (i=m_RaceDataMap.begin(); i!=m_RaceDataMap.end(); ++i)
		CRaceData::Delete(i->second);

	m_RaceDataMap.clear();
}

void CRaceManager::Destroy()
{
	__ShutdownAsyncLoader();
	__DestroyRaceDataMap();

	__Initialize();
}

CRaceManager::CRaceManager()
{
	__Initialize();
}

CRaceManager::~CRaceManager()
{
	Destroy();
}

// Async loading implementation
UINT CALLBACK CRaceManager::__AsyncLoaderThreadEntry(void* pThis)
{
	return static_cast<CRaceManager*>(pThis)->__AsyncLoaderThread();
}

UINT CRaceManager::__AsyncLoaderThread()
{
	while (!m_bAsyncShutdown)
	{
		DWORD dwWaitResult = WaitForSingleObject(m_hAsyncSemaphore, INFINITE);

		if (m_bAsyncShutdown)
			break;

		if (dwWaitResult == WAIT_OBJECT_0)
		{
			// Get next request from queue
			DWORD dwRaceIndex = 0;
			{
				m_AsyncRequestMutex.Lock();
				if (!m_AsyncRequestQueue.empty())
				{
					dwRaceIndex = m_AsyncRequestQueue.front();
					m_AsyncRequestQueue.pop_front();
				}
				m_AsyncRequestMutex.Unlock();
			}

			if (dwRaceIndex != 0)
			{
				// Load the race data (this is the slow part)
				CRaceData* pRaceData = __LoadRaceData(dwRaceIndex);

				// Add to complete queue
				SAsyncLoadResult result;
				result.dwRaceIndex = dwRaceIndex;
				result.pRaceData = pRaceData;

				m_AsyncCompleteMutex.Lock();
				m_AsyncCompleteQueue.emplace_back(result);
				m_AsyncCompleteMutex.Unlock();

				// Remove from loading set
				m_AsyncLoadingMutex.Lock();
				m_AsyncLoadingSet.erase(dwRaceIndex);
				m_AsyncLoadingMutex.Unlock();
			}
		}
	}

	return 0;
}

void CRaceManager::__RequestAsyncLoad(DWORD dwRaceIndex)
{
	// Check if already loaded
	if (m_RaceDataMap.find(dwRaceIndex) != m_RaceDataMap.end())
		return;

	// Check if already loading
	m_AsyncLoadingMutex.Lock();
	if (m_AsyncLoadingSet.find(dwRaceIndex) != m_AsyncLoadingSet.end())
	{
		m_AsyncLoadingMutex.Unlock();
		return;
	}
	m_AsyncLoadingSet.insert(dwRaceIndex);
	m_AsyncLoadingMutex.Unlock();

	// Add to request queue
	m_AsyncRequestMutex.Lock();
	m_AsyncRequestQueue.emplace_back(dwRaceIndex);
	m_AsyncRequestMutex.Unlock();

	// Signal the thread
	ReleaseSemaphore(m_hAsyncSemaphore, 1, NULL);
}

void CRaceManager::ProcessAsyncLoads()
{
	// Process all completed loads
	m_AsyncCompleteMutex.Lock();
	while (!m_AsyncCompleteQueue.empty())
	{
		SAsyncLoadResult result = m_AsyncCompleteQueue.front();
		m_AsyncCompleteQueue.pop_front();
		m_AsyncCompleteMutex.Unlock();

		if (result.pRaceData)
		{
			if (m_RaceDataMap.find(result.dwRaceIndex) == m_RaceDataMap.end())
			{
				m_RaceDataMap.emplace(result.dwRaceIndex, result.pRaceData);
				Tracenf("CRaceManager: Async loaded race %u", result.dwRaceIndex);
			}
			else
			{
				// Race was loaded by another path, delete duplicate
				CRaceData::Delete(result.pRaceData);
			}
		}

		m_AsyncCompleteMutex.Lock();
	}
	m_AsyncCompleteMutex.Unlock();
}

void CRaceManager::__ShutdownAsyncLoader()
{
	if (!m_hAsyncThread)
		return;

	m_bAsyncShutdown = true;

	// Signal thread to wake up
	if (m_hAsyncSemaphore)
		ReleaseSemaphore(m_hAsyncSemaphore, 1, NULL);

	// Wait for thread to finish
	WaitForSingleObject(m_hAsyncThread, 5000);
	CloseHandle(m_hAsyncThread);
	m_hAsyncThread = NULL;

	if (m_hAsyncSemaphore)
	{
		CloseHandle(m_hAsyncSemaphore);
		m_hAsyncSemaphore = NULL;
	}

	// Clear any pending requests
	m_AsyncRequestQueue.clear();
	m_AsyncLoadingSet.clear();

	// Process any remaining completed loads
	for (auto& result : m_AsyncCompleteQueue)
	{
		if (result.pRaceData)
			CRaceData::Delete(result.pRaceData);
	}
	m_AsyncCompleteQueue.clear();
}

#ifdef ENABLE_RACE_HEIGHT
void CRaceManager::SetRaceHeight(int iVnum, float fHeight)
{
	m_kMap_iRaceKey_fRaceAdditionalHeight.emplace(iVnum, fHeight);
}

float CRaceManager::GetRaceHeight(int iVnum)
{
	auto it = m_kMap_iRaceKey_fRaceAdditionalHeight.find(iVnum);
	if (m_kMap_iRaceKey_fRaceAdditionalHeight.end() == it)
		return 0.0f;

	return it->second;
}
#endif
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
