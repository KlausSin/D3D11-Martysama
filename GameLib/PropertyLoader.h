#pragma once

#include "../eterbase/FileDir.h"

class CPropertyManager;

class CPropertyLoader : public CDir
{
	public:
		CPropertyLoader();
		virtual ~CPropertyLoader();

		void			SetPropertyManager(CPropertyManager * pPropertyManager);
		DWORD			RegisterFile(const char* c_szPathName, const char* c_szFileName);

		virtual bool	OnFolder(const char* c_szFilter, const char* c_szPathName, const char* c_szFileName);
		virtual bool	OnFile(const char* c_szPathName, const char* c_szFileName);

	protected:
		CPropertyManager * m_pPropertyManager;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
