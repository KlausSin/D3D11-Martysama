#include "StdAfx.h"
#include "../eterPack/EterPackManager.h"

#include "PropertyManager.h"
#include "Property.h"

#include <sstream>

CPropertyManager::CPropertyManager() : m_isFileMode(true)
{
#if defined(__BL_AUTO_LANTERN_EFFECT__)
	m_dwLanternCRC = m_dwLanternEffectCRC = 0xffffffff;
	m_dwLanternCRC2 = m_dwLanternEffectCRC2 = 0xffffffff;
#endif
}

CPropertyManager::~CPropertyManager()
{
	Clear();
}

bool CPropertyManager::Initialize(const char * c_pszPackFileName)
{
	if (c_pszPackFileName)
	{
		// Load reserved CRCs if exists
		if (CEterPackManager::Instance().isExist("property/reserve"))
		{
			LoadReservedCRC("property/reserve");
		}

		CMappedFile kListFile;
		PBYTE pbListData;
		if (CEterPackManager::Instance().Get(kListFile, "property/list", (LPCVOID*)&pbListData))
		{
			std::stringstream kSS;
			kSS << (char*)pbListData;

			std::string strLine;
			while (std::getline(kSS, strLine))
			{
				// Trim whitespace
				std::size_t posRight = strLine.find_last_not_of(" \t\r\n");
				strLine = (posRight == std::string::npos ? "" : strLine.substr(0, posRight + 1));
				std::size_t posLeft = strLine.find_first_not_of(" \t\r\n");
				strLine = (posLeft == std::string::npos ? "" : strLine.substr(posLeft, std::string::npos));

				if (strLine.empty())
					continue;

				if (!Register(strLine.c_str(), NULL))
				{
					TraceError("CPropertyManager::Initialize: Cannot register property '%s'!", strLine.c_str());
				}
			}
		}

		m_isFileMode = false;
	}
	else
	{
		m_isFileMode = true;
	}

	return true;
}

bool CPropertyManager::BuildPack()
{
	return false;
}

bool CPropertyManager::LoadReservedCRC(const char * c_pszFileName)
{
	CMappedFile file;
	LPCVOID c_pvData;

	if (!CEterPackManager::Instance().Get(file, c_pszFileName, &c_pvData))
	{
		return false;
	}

	CMemoryTextFileLoader textFileLoader;
	textFileLoader.Bind(file.Size(), c_pvData);

	for (DWORD i = 0; i < textFileLoader.GetLineCount(); ++i)
	{
		const char * pszLine = textFileLoader.GetLineString(i).c_str();

		if (!pszLine || !*pszLine)
			continue;

		ReserveCRC(atoi(pszLine));
	}

	return true;
}

void CPropertyManager::ReserveCRC(DWORD dwCRC)
{
	m_ReservedCRCSet.insert(dwCRC);
}

DWORD CPropertyManager::GetUniqueCRC(const char * c_szSeed)
{
	std::string stTmp = c_szSeed;

	while (1)
	{
		DWORD dwCRC = GetCRC32(stTmp.c_str(), stTmp.length());

		if (m_ReservedCRCSet.find(dwCRC) == m_ReservedCRCSet.end() &&
			m_PropertyByCRCMap.find(dwCRC) == m_PropertyByCRCMap.end())
		{
			return dwCRC;
		}

		char szAdd[2];
		_snprintf(szAdd, sizeof(szAdd), "%d", random() % 10);
		stTmp += szAdd;
	}
}

bool CPropertyManager::Register(const char * c_pszFileName, CProperty ** ppProperty)
{
	CMappedFile file;
	LPCVOID c_pvData;

	if (!CEterPackManager::Instance().Get(file, c_pszFileName, &c_pvData))
	{
		return false;
	}

	CProperty * pProperty = new CProperty(c_pszFileName);

	if (!pProperty->ReadFromMemory(c_pvData, file.Size(), c_pszFileName))
	{
		delete pProperty;
		return false;
	}

	DWORD dwCRC = pProperty->GetCRC();

#if defined(__BL_AUTO_LANTERN_EFFECT__)
	const std::string strFileName(pProperty->GetFileName());

	if (m_dwLanternCRC == 0xffffffff && strFileName.find("ob-b1-013-lamp02.prb") != std::string::npos)
		m_dwLanternCRC = dwCRC;
	else if (m_dwLanternEffectCRC == 0xffffffff && strFileName.find("fire_ob-b1-013-lamp02.mse.pre") != std::string::npos)
		m_dwLanternEffectCRC = dwCRC;

	if (m_dwLanternCRC2 == 0xffffffff && strFileName.find("obj-0010.prb") != std::string::npos)
		m_dwLanternCRC2 = dwCRC;
	else if (m_dwLanternEffectCRC2 == 0xffffffff && strFileName.find("fire_obj-0010.mse.pre") != std::string::npos)
		m_dwLanternEffectCRC2 = dwCRC;
#endif

	TPropertyCRCMap::iterator itor = m_PropertyByCRCMap.find(dwCRC);

	if (m_PropertyByCRCMap.end() != itor)
	{
		Tracef("Property already registered, replace %s to %s\n",
			itor->second->GetFileName(),
			c_pszFileName);

		delete itor->second;
		itor->second = pProperty;
	}
	else
	{
		m_PropertyByCRCMap.insert(TPropertyCRCMap::value_type(dwCRC, pProperty));
	}

	if (ppProperty)
		*ppProperty = pProperty;

	return true;
}

bool CPropertyManager::Get(const char * c_pszFileName, CProperty ** ppProperty)
{
	return Register(c_pszFileName, ppProperty);
}

bool CPropertyManager::Get(DWORD dwCRC, CProperty ** ppProperty)
{
	TPropertyCRCMap::iterator itor = m_PropertyByCRCMap.find(dwCRC);

	if (m_PropertyByCRCMap.end() == itor)
		return false;

	*ppProperty = itor->second;
	return true;
}

bool CPropertyManager::Put(const char * c_pszFileName, const char * c_pszSourceFileName)
{
	if (!CopyFile(c_pszSourceFileName, c_pszFileName, FALSE))
		return false;

	Register(c_pszFileName);
	return true;
}

bool CPropertyManager::Erase(DWORD dwCRC)
{
	TPropertyCRCMap::iterator itor = m_PropertyByCRCMap.find(dwCRC);

	if (m_PropertyByCRCMap.end() == itor)
		return false;

	CProperty * pProperty = itor->second;
	m_PropertyByCRCMap.erase(itor);

	DeleteFile(pProperty->GetFileName());
	ReserveCRC(pProperty->GetCRC());

	FILE * fp = fopen("property/reserve", "a+");

	if (!fp)
		LogBox("Cannot open reserve CRC file.");
	else
	{
		char szCRC[64 + 1];
		_snprintf(szCRC, sizeof(szCRC), "%u\r\n", pProperty->GetCRC());

		fputs(szCRC, fp);
		fclose(fp);
	}

	delete pProperty;
	return true;
}

bool CPropertyManager::Erase(const char * c_pszFileName)
{
	CProperty * pProperty = NULL;

	if (Get(c_pszFileName, &pProperty))
		return Erase(pProperty->GetCRC());

	return false;
}

void CPropertyManager::Clear()
{
	stl_wipe_second(m_PropertyByCRCMap);
}
