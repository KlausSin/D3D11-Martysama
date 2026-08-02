#include "StdAfx.h"
#include "PythonQuest.h"

#include "../EterBase/StepTimer.h"

void CPythonQuest::RegisterQuestInstance(const SQuestInstance& c_rQuestInstance)
{
	DeleteQuestInstance(c_rQuestInstance.dwIndex);
	m_QuestInstanceContainer.push_back(c_rQuestInstance);

	/////

	SQuestInstance& rQuestInstance = *m_QuestInstanceContainer.rbegin();
	rQuestInstance.iStartTime = int(DX::StepTimer::instance().GetTotalSeconds());
}

struct FQuestInstanceCompare
{
	DWORD dwSearchIndex;
	FQuestInstanceCompare(DWORD dwIndex) : dwSearchIndex(dwIndex) {}
	bool operator () (const CPythonQuest::SQuestInstance& rQuestInstance)
	{
		return dwSearchIndex == rQuestInstance.dwIndex;
	}
};

void CPythonQuest::DeleteQuestInstance(DWORD dwIndex)
{
	TQuestInstanceContainer::iterator itor = std::find_if(m_QuestInstanceContainer.begin(), m_QuestInstanceContainer.end(), FQuestInstanceCompare(dwIndex));
	if (itor == m_QuestInstanceContainer.end())
		return;

	m_QuestInstanceContainer.erase(itor);
}

bool CPythonQuest::IsQuest(DWORD dwIndex)
{
	TQuestInstanceContainer::iterator itor = std::find_if(m_QuestInstanceContainer.begin(), m_QuestInstanceContainer.end(), FQuestInstanceCompare(dwIndex));
	return itor != m_QuestInstanceContainer.end();
}

// STEP 1: Modified MakeQuest function signature
void CPythonQuest::MakeQuest(DWORD dwIndex
#if defined(ENABLE_QUEST_RENEWAL)
	, BYTE bType
	, bool bIsConfirmed
#endif
)
{
	DeleteQuestInstance(dwIndex);
	m_QuestInstanceContainer.push_back(SQuestInstance());

	/////

	SQuestInstance& rQuestInstance = *m_QuestInstanceContainer.rbegin();
	rQuestInstance.dwIndex = dwIndex;
	rQuestInstance.iStartTime = int(DX::StepTimer::instance().GetTotalSeconds());

	// STEP 2: Add new members
#if defined(ENABLE_QUEST_RENEWAL)
	rQuestInstance.bType = bType;
	rQuestInstance.bIsConfirmed = bIsConfirmed;
#endif
}

void CPythonQuest::SetQuestTitle(DWORD dwIndex, const char* c_szTitle)
{
	SQuestInstance* pQuestInstance;
	if (!__GetQuestInstancePtr(dwIndex, &pQuestInstance))
		return;

	pQuestInstance->strTitle = c_szTitle;
}

void CPythonQuest::SetQuestClockName(DWORD dwIndex, const char* c_szClockName)
{
	SQuestInstance* pQuestInstance;
	if (!__GetQuestInstancePtr(dwIndex, &pQuestInstance))
		return;

	pQuestInstance->strClockName = c_szClockName;
}

void CPythonQuest::SetQuestCounterName(DWORD dwIndex, const char* c_szCounterName)
{
	SQuestInstance* pQuestInstance;
	if (!__GetQuestInstancePtr(dwIndex, &pQuestInstance))
		return;

	pQuestInstance->strCounterName = c_szCounterName;
}

void CPythonQuest::SetQuestClockValue(DWORD dwIndex, int iClockValue)
{
	SQuestInstance* pQuestInstance;
	if (!__GetQuestInstancePtr(dwIndex, &pQuestInstance))
		return;

	pQuestInstance->iClockValue = iClockValue;
	pQuestInstance->iStartTime = int(DX::StepTimer::instance().GetTotalSeconds());
}

void CPythonQuest::SetQuestCounterValue(DWORD dwIndex, int iCounterValue)
{
	SQuestInstance* pQuestInstance;
	if (!__GetQuestInstancePtr(dwIndex, &pQuestInstance))
		return;

	pQuestInstance->iCounterValue = iCounterValue;
}

void CPythonQuest::SetQuestIconFileName(DWORD dwIndex, const char* c_szIconFileName)
{
	SQuestInstance* pQuestInstance;
	if (!__GetQuestInstancePtr(dwIndex, &pQuestInstance))
		return;

	pQuestInstance->strIconFileName = c_szIconFileName;
}

#if defined(ENABLE_QUEST_RENEWAL)
void CPythonQuest::SetQuestIsConfirmed(DWORD dwIndex, bool bIsConfirmed)
{
	SQuestInstance* pQuestInstance;
	if (!__GetQuestInstancePtr(dwIndex, &pQuestInstance))
		return;

	pQuestInstance->bIsConfirmed = bIsConfirmed;
}
#endif

int CPythonQuest::GetQuestCount()
{
	return (int)(m_QuestInstanceContainer.size());
}

#if defined(ENABLE_QUEST_RENEWAL)
int CPythonQuest::GetQuestButtonNoticeCount(BYTE bQuestType)
{
	int count = 0;
	for (TQuestInstanceContainer::value_type& it : m_QuestInstanceContainer)
	{
		if (bQuestType == QUEST_TYPE_MAX)
			count += 1;
		else
			if (it.bType == bQuestType)
				count += 1;
	}
	return count;
}
#endif

// STEP 5: Replace GetQuestInstancePtr function
#if defined(ENABLE_QUEST_RENEWAL)
bool CPythonQuest::GetQuestInstancePtr(DWORD dwQuestIndex, SQuestInstance** ppQuestInstance)
{
	TQuestInstanceContainer::iterator itor = std::find_if(m_QuestInstanceContainer.begin(), m_QuestInstanceContainer.end(), FQuestInstanceCompare(dwQuestIndex));
	if (itor == m_QuestInstanceContainer.end())
		return false;

	const DWORD dwVectorIndex = std::distance(m_QuestInstanceContainer.begin(), itor);
	*ppQuestInstance = &m_QuestInstanceContainer[dwVectorIndex];
	return true;
}
#else
bool CPythonQuest::GetQuestInstancePtr(DWORD dwArrayIndex, SQuestInstance** ppQuestInstance)
{
	if (dwArrayIndex >= m_QuestInstanceContainer.size())
		return false;

	*ppQuestInstance = &m_QuestInstanceContainer[dwArrayIndex];

	return true;
}
#endif

bool CPythonQuest::__GetQuestInstancePtr(DWORD dwQuestIndex, SQuestInstance** ppQuestInstance)
{
	TQuestInstanceContainer::iterator itor = std::find_if(m_QuestInstanceContainer.begin(), m_QuestInstanceContainer.end(), FQuestInstanceCompare(dwQuestIndex));
	if (itor == m_QuestInstanceContainer.end())
		return false;

	*ppQuestInstance = &(*itor);

	return true;
}

void CPythonQuest::__Initialize()
{
}

void CPythonQuest::Clear()
{
	m_QuestInstanceContainer.clear();
}

CPythonQuest::CPythonQuest()
{
	__Initialize();
}

CPythonQuest::~CPythonQuest()
{
	Clear();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

PyObject* questGetQuestCount(PyObject* poSelf, PyObject* poArgs)
{
	return Py_BuildValue("i", CPythonQuest::Instance().GetQuestCount());
}

PyObject* questGetQuestData(PyObject* poSelf, PyObject* poArgs)
{
	int iIndex;
	if (!PyTuple_GetInteger(poArgs, 0, &iIndex))
		return Py_BadArgument();

	CPythonQuest::SQuestInstance* pQuestInstance;
	if (!CPythonQuest::Instance().GetQuestInstancePtr(iIndex, &pQuestInstance))
		return Py_BuildException("Failed to find quest by index %d", iIndex);

	CGraphicImage* pImage = NULL;
	if (!pQuestInstance->strIconFileName.empty())
	{
		std::string strIconFileName;
		strIconFileName = "d:/ymir work/ui/game/quest/questicon/";
		strIconFileName += pQuestInstance->strIconFileName.c_str();
		pImage = (CGraphicImage*)CResourceManager::Instance().GetResourcePointer(strIconFileName.c_str());
	}
	else
	{
		{
			std::string strIconFileName = "season1/icon/scroll_open.tga";
			pImage = (CGraphicImage*)CResourceManager::Instance().GetResourcePointer(strIconFileName.c_str());
		}
	}

	// STEP 6: Modified return value
#if defined(ENABLE_QUEST_RENEWAL)
	return Py_BuildValue("ibsisi",
		pQuestInstance->bType,
		pQuestInstance->bIsConfirmed,
		pQuestInstance->strTitle.c_str(),
		pImage,
		pQuestInstance->strCounterName.c_str(),
		pQuestInstance->iCounterValue);
#else
	return Py_BuildValue("sisi", pQuestInstance->strTitle.c_str(),
		pImage,
		pQuestInstance->strCounterName.c_str(),
		pQuestInstance->iCounterValue);
#endif
}

PyObject* questGetQuestIndex(PyObject* poSelf, PyObject* poArgs)
{
	int iIndex;
	if (!PyTuple_GetInteger(poArgs, 0, &iIndex))
		return Py_BadArgument();

	CPythonQuest::SQuestInstance* pQuestInstance;
	if (!CPythonQuest::Instance().GetQuestInstancePtr(iIndex, &pQuestInstance))
		return Py_BuildException("Failed to find quest by index %d", iIndex);

	return Py_BuildValue("i", pQuestInstance->dwIndex);
}

PyObject* questGetQuestLastTime(PyObject* poSelf, PyObject* poArgs)
{
	int iIndex;
	if (!PyTuple_GetInteger(poArgs, 0, &iIndex))
		return Py_BadArgument();

	CPythonQuest::SQuestInstance* pQuestInstance;
	if (!CPythonQuest::Instance().GetQuestInstancePtr(iIndex, &pQuestInstance))
		return Py_BuildException("Failed to find quest by index %d", iIndex);

	int iLastTime = 0;

	if (pQuestInstance->iClockValue >= 0)
		iLastTime = (pQuestInstance->iStartTime + pQuestInstance->iClockValue) - int(DX::StepTimer::instance().GetTotalSeconds());

	return Py_BuildValue("si", pQuestInstance->strClockName.c_str(), iLastTime);
}

PyObject* questClear(PyObject* poSelf, PyObject* poArgs)
{
	CPythonQuest::Instance().Clear();
	return Py_BuildNone();
}

// STEP 7: Add new Python functions above initquest
#if defined(ENABLE_QUEST_RENEWAL)
PyObject* questGetQuestCounterData(PyObject* poSelf, PyObject* poArgs) { return Py_BuildNone(); }
PyObject* questGetQuestButtonNoticeCount(PyObject* poSelf, PyObject* poArgs)
{
	BYTE bType;
	if (!PyTuple_GetInteger(poArgs, 0, &bType))
		return Py_BadArgument();

	return Py_BuildValue("i", CPythonQuest::Instance().GetQuestButtonNoticeCount(bType));
}
#endif

void initquest()
{
	static PyMethodDef s_methods[] =
	{
		{ "GetQuestCount",				questGetQuestCount,				METH_VARARGS },
		{ "GetQuestData",				questGetQuestData,				METH_VARARGS },
		{ "GetQuestIndex",				questGetQuestIndex,				METH_VARARGS },
		{ "GetQuestLastTime",			questGetQuestLastTime,			METH_VARARGS },
		{ "Clear",						questClear,						METH_VARARGS },
		// STEP 8: Add new methods
#if defined(ENABLE_QUEST_RENEWAL)
		{ "GetQuestCounterData",		questGetQuestCounterData,		METH_VARARGS },
		{ "GetQuestButtonNoticeCount",	questGetQuestButtonNoticeCount,	METH_VARARGS },
#endif
		{ NULL,							NULL,							NULL },
	};

	PyObject* poModule = Py_InitModule("quest", s_methods);
	PyModule_AddIntConstant(poModule, "QUEST_MAX_NUM", 5);

	// STEP 9: Add constants at bottom
#if defined(ENABLE_QUEST_RENEWAL)
	PyModule_AddIntConstant(poModule, "QUEST_TYPE_MAIN", CPythonQuest::QUEST_TYPE_MAIN);
	PyModule_AddIntConstant(poModule, "QUEST_TYPE_SUB", CPythonQuest::QUEST_TYPE_SUB);
	PyModule_AddIntConstant(poModule, "QUEST_TYPE_LEVELUP", CPythonQuest::QUEST_TYPE_LEVELUP);
	PyModule_AddIntConstant(poModule, "QUEST_TYPE_EVENT", CPythonQuest::QUEST_TYPE_EVENT);
	PyModule_AddIntConstant(poModule, "QUEST_TYPE_COLLECTION", CPythonQuest::QUEST_TYPE_COLLECTION);
	PyModule_AddIntConstant(poModule, "QUEST_TYPE_SYSTEM", CPythonQuest::QUEST_TYPE_SYSTEM);
	PyModule_AddIntConstant(poModule, "QUEST_TYPE_SCROLL", CPythonQuest::QUEST_TYPE_SCROLL);
	PyModule_AddIntConstant(poModule, "QUEST_TYPE_DAILY", CPythonQuest::QUEST_TYPE_DAILY);
	PyModule_AddIntConstant(poModule, "QUEST_TYPE_UNEXPOSED", CPythonQuest::QUEST_TYPE_UNEXPOSED);
	PyModule_AddIntConstant(poModule, "QUEST_TYPE_MAX", CPythonQuest::QUEST_TYPE_MAX);

	PyModule_AddIntConstant(poModule, "QUEST_STRING_TYPE_NORMAL", CPythonQuest::QUEST_STRING_TYPE_NORMAL);
	PyModule_AddIntConstant(poModule, "QUEST_STRING_TYPE_CLOCK", CPythonQuest::QUEST_STRING_TYPE_CLOCK);
	PyModule_AddIntConstant(poModule, "QUEST_STRING_TYPE_COUNT", CPythonQuest::QUEST_STRING_TYPE_COUNT);
#endif
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec