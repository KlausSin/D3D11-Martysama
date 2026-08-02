#pragma once

#include <deque>
#include <set>
#include "RaceData.h"
#include "../EterLib/Mutex.h"

class CRaceManager : public CSingleton<CRaceManager>
{
	public:
		typedef std::map<DWORD, CRaceData *> TRaceDataMap;
		typedef TRaceDataMap::iterator TRaceDataIterator;

	public:
		CRaceManager();
		virtual ~CRaceManager();

		void Create();
		void Destroy();

		void RegisterRaceName(DWORD dwRaceIndex, const char * c_szName);
		void RegisterRaceSrcName(const char * c_szName, const char * c_szSrcName);

		void SetPathName(const char * c_szPathName);
		const char * GetFullPathFileName(const char* c_szFileName);

		// Handling
		void CreateRace(DWORD dwRaceIndex);
		void SelectRace(DWORD dwRaceIndex);
		CRaceData * GetSelectedRaceDataPointer();
		// Handling

		BOOL GetRaceDataPointer(DWORD dwRaceIndex, CRaceData ** ppRaceData);

		// Async loading - call ProcessAsyncLoads() from game loop
		void ProcessAsyncLoads();
		bool IsAsyncLoadPending() const { return !m_AsyncCompleteQueue.empty() || !m_AsyncLoadingSet.empty(); }

	protected:
		CRaceData* __LoadRaceData(DWORD dwRaceIndex);
		bool __LoadRaceMotionList(CRaceData& rkRaceData, const char* pathName, const char* motionListFileName);

		void __Initialize();
		void __DestroyRaceDataMap();

	protected:
		TRaceDataMap					m_RaceDataMap;

		std::map<std::string, std::string> m_kMap_stRaceName_stSrcName;
		std::map<DWORD, std::string>	m_kMap_dwRaceKey_stRaceName;

	private:
		std::string						m_strPathName;
		CRaceData *						m_pSelectedRaceData;

		// Async race loading
		struct SAsyncLoadResult
		{
			DWORD dwRaceIndex;
			CRaceData* pRaceData;
		};

		static UINT CALLBACK __AsyncLoaderThreadEntry(void* pThis);
		UINT __AsyncLoaderThread();
		void __RequestAsyncLoad(DWORD dwRaceIndex);
		void __ShutdownAsyncLoader();

		HANDLE							m_hAsyncThread;
		unsigned						m_uAsyncThreadID;
		HANDLE							m_hAsyncSemaphore;
		bool							m_bAsyncShutdown;

		std::deque<DWORD>				m_AsyncRequestQueue;
		Mutex							m_AsyncRequestMutex;

		std::set<DWORD>					m_AsyncLoadingSet;
		Mutex							m_AsyncLoadingMutex;

		std::deque<SAsyncLoadResult>	m_AsyncCompleteQueue;
		Mutex							m_AsyncCompleteMutex;

#ifdef ENABLE_RACE_HEIGHT
	public:
		void SetRaceHeight(int iVnum, float fHeight);
		float GetRaceHeight(int iVnum);
	private:
		std::map<int, float>			m_kMap_iRaceKey_fRaceAdditionalHeight;
#endif
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
