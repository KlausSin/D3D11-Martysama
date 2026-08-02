#pragma once

#include "../EffectLib/StdAfx.h"
#include "../eterlib/Resource.h"
#include "../eterlib/ResourceManager.h"

enum EResourceTypes
{
	RES_TYPE_UNKNOWN,
};

class CPythonResource : public CSingleton<CPythonResource>
{
	public:
		CPythonResource();
		virtual ~CPythonResource();

		void Destroy();

		void DumpFileList(const char * c_szFileName);

	protected:
		CResourceManager m_resManager;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
