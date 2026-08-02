#pragma once

#include "Resource.h"
#include "FileLoaderThread.h"

#include <set>
#include <map>
#include <unordered_map>
#include <string>

#if defined(__BL_GR2_FILE_LOAD_IMPROVE__)
#include "../EterGrnLib/Thing.h"
#endif

class CResourceManager : public CSingleton<CResourceManager>
{
	public:
		CResourceManager();
		virtual ~CResourceManager();

		void		LoadStaticCache(const char* c_szFileName);

		void		DestroyDeletingList();
		void		Destroy();

		void		BeginThreadLoading();
		void		EndThreadLoading();

		CResource *	InsertResourcePointer(DWORD dwFileCRC, CResource* pResource);
		CResource *	FindResourcePointer(DWORD dwFileCRC);
		CResource *	GetResourcePointer(const char * c_szFileName);
		CResource *	GetTypeResourcePointer(const char * c_szFileName, int iType=-1);

		bool		isResourcePointerData(DWORD dwFileCRC);

		void		RegisterResourceNewFunctionPointer(const char* c_szFileExt, CResource* (*pResNewFunc)(const char* c_szFileName));
		void		RegisterResourceNewFunctionByTypePointer(int iType, CResource* (*pNewFunc) (const char* c_szFileName));

		void		DumpFileListToTextFile(const char* c_szFileName);
		bool		IsFileExist(const char * c_szFileName);

		void		Update();
		void		ReserveDeletingResource(CResource * pResource);

#if defined(__BL_GR2_FILE_LOAD_IMPROVE__)
		bool		LoadGraphicThing(CGraphicThing* pResource, int iSize, const void* c_pvBuf, const std::string& sFileName);
#endif

	public:
		void		ProcessBackgroundLoading();
		void		PushBackgroundLoadingSet(std::set<std::string> & LoadingSet);

	protected:
		void		__DestroyDeletingResourceMap();
		void		__DestroyResourceMap();
		void		__DestroyCacheMap();

		DWORD		__GetFileCRC(const char * c_szFileName, const char ** c_pszLowerFile = NULL);

	protected:
		typedef std::unordered_map<DWORD, CResource *>							TResourcePointerMap;
		typedef std::map<std::string, CResource* (*)(const char*)>				TResourceNewFunctionPointerMap;
		typedef std::map<int, CResource* (*)(const char*)>						TResourceNewFunctionByTypePointerMap;
		typedef std::unordered_map<CResource *, DWORD>							TResourceDeletingMap;
		typedef std::unordered_map<DWORD, std::string>							TResourceRequestMap;
		typedef std::map<long, CResource*>										TResourceRefDecreaseWaitingMap;

#if defined(__BL_GR2_FILE_LOAD_IMPROVE__)
		typedef std::map<std::string, granny_file*>								TResourceGrannyCacheMap;
#endif

	protected:
		TResourcePointerMap						m_pCacheMap;
		TResourcePointerMap						m_pResMap;
		TResourceNewFunctionPointerMap			m_pResNewFuncMap;
		TResourceNewFunctionByTypePointerMap	m_pResNewFuncByTypeMap;
		TResourceDeletingMap					m_ResourceDeletingMap;
		TResourceRequestMap						m_RequestMap;
		TResourceRequestMap						m_WaitingMap;
		TResourceRefDecreaseWaitingMap			m_pResRefDecreaseWaitingMap;

#if defined(__BL_GR2_FILE_LOAD_IMPROVE__)
		TResourceGrannyCacheMap					m_pGrannyCacheMap;
#endif

		static CFileLoaderThread				ms_loadingThread;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
