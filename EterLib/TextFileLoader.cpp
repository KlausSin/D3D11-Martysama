#include "StdAfx.h"
#include "../eterBase/CRC32.h"
#include "../EterPack/EterPackManager.h"
#include "Pool.h"
#include "TextFileLoader.h"

#include <string>
#include <algorithm>
#include <memory>
#include <vector>
#include <string_view>
#include <cerrno>
#include <cctype>
#include <cstdlib>

std::map< DWORD, CTextFileLoader* > CTextFileLoader::ms_kMap_dwNameKey_pkTextFileLoader;
bool CTextFileLoader::ms_isCacheMode = false;
CDynamicPool< CTextFileLoader::SGroupNode > CTextFileLoader::SGroupNode::ms_kPool;

static bool SafeParseFloat(const std::string& str, float* outVal)
{
	if (str.empty())
		return false;

	char* end;
	errno = 0;
	*outVal = std::strtof(str.c_str(), &end);

	return (end == str.c_str() + str.size() && errno == 0);
}

static bool SafeParseInt(const std::string& str, int* outVal)
{
	if (str.empty())
		return false;

	char* end;
	errno = 0;
	*outVal = std::strtol(str.c_str(), &end, 10);

	return (end == str.c_str() + str.size() && errno == 0);
}

static void FastSplitLine(const char* line, size_t len, std::vector< std::string_view >& outTokens)
{
	outTokens.clear();

	if (!line || len == 0)
		return;

	const char* ptr = line;
	const char* end = line + len;

	while (ptr < end)
	{
		while (ptr < end && (unsigned char)*ptr <= ' ')
			++ptr;

		if (ptr >= end)
			break;

		if (*ptr == '"')
		{
			++ptr;
			const char* startToken = ptr;

			while (ptr < end && *ptr != '"')
				++ptr;

			outTokens.emplace_back(startToken, ptr - startToken);

			if (ptr < end)
				++ptr;
		}
		else
		{
			const char* startToken = ptr;

			while (ptr < end && (unsigned char)*ptr > ' ')
				++ptr;

			outTokens.emplace_back(startToken, ptr - startToken);
		}
	}
}

template< size_t N >
static bool IsEqualLower(std::string_view sv, const char(&literal)[N])
{
	constexpr size_t len = N - 1;

	if (sv.length() != len)
		return false;

	for (size_t i = 0; i < len; ++i)
	{
		if (std::tolower((unsigned char)sv[i]) != literal[i])
			return false;
	}

	return true;
}

DWORD CTextFileLoader::SGroupNode::GenNameKey(const char* c_szGroupName, size_t uGroupNameLen)
{
	return GetCRC32(c_szGroupName, uGroupNameLen);
}

DWORD CTextFileLoader::SGroupNode::GenNameKey(std::string_view sv)
{
	return GetCRC32(sv.data(), sv.length());
}

CTokenVector* CTextFileLoader::SGroupNode::GetTokenVector(const std::string& c_rstGroupName)
{
	return GetTokenVector(GenNameKey(c_rstGroupName.c_str(), c_rstGroupName.length()));
}

CTokenVector* CTextFileLoader::SGroupNode::GetTokenVector(DWORD dwGroupNameKey)
{
	auto it = m_kMap_dwKey_kVct_stToken.find(dwGroupNameKey);

	if (it != m_kMap_dwKey_kVct_stToken.end())
		return &it->second;

	return nullptr;
}

bool CTextFileLoader::SGroupNode::IsExistTokenVector(const std::string& c_rstGroupName)
{
	return IsExistTokenVector(GenNameKey(c_rstGroupName.c_str(), c_rstGroupName.length()));
}

bool CTextFileLoader::SGroupNode::IsExistTokenVector(DWORD dwGroupNameKey)
{
	return m_kMap_dwKey_kVct_stToken.find(dwGroupNameKey) != m_kMap_dwKey_kVct_stToken.end();
}

void CTextFileLoader::SGroupNode::InsertTokenVector(const std::string& c_rstGroupName, const CTokenVector& c_rkVct_stToken)
{
	DWORD dwGroupNameKey = GenNameKey(c_rstGroupName.c_str(), c_rstGroupName.length());
	m_kMap_dwKey_kVct_stToken.emplace(dwGroupNameKey, c_rkVct_stToken);
}

const std::string& CTextFileLoader::SGroupNode::GetGroupName()
{
	return m_strGroupName;
}

bool CTextFileLoader::SGroupNode::IsGroupNameKey(DWORD dwGroupNameKey)
{
	return dwGroupNameKey == m_dwGroupNameKey;
}

void CTextFileLoader::SGroupNode::SetGroupName(const std::string& c_rstGroupName)
{
	m_strGroupName = c_rstGroupName;
	stl_lowers(m_strGroupName);
	m_dwGroupNameKey = GenNameKey(m_strGroupName.c_str(), m_strGroupName.length());
}

CTextFileLoader::SGroupNode* CTextFileLoader::SGroupNode::New()
{
	return ms_kPool.Alloc();
}

void CTextFileLoader::SGroupNode::Delete(SGroupNode* pkNode)
{
	pkNode->m_kMap_dwKey_kVct_stToken.clear();
	pkNode->ChildNodeVector.clear();
	pkNode->m_strGroupName.clear();
	pkNode->m_dwGroupNameKey = 0;
	ms_kPool.Free(pkNode);
}

void CTextFileLoader::SGroupNode::DestroySystem()
{
	ms_kPool.Destroy();
}

CTextFileLoader* CTextFileLoader::Cache(const char* c_szFileName)
{
	DWORD dwNameKey = GetCRC32(c_szFileName, strlen(c_szFileName));

	if (auto f = ms_kMap_dwNameKey_pkTextFileLoader.find(dwNameKey); f != ms_kMap_dwNameKey_pkTextFileLoader.end())
	{
		if (!ms_isCacheMode)
		{
			CTextFileLoader* pkNewTextFileLoader = new CTextFileLoader;

			if (!pkNewTextFileLoader->Load(c_szFileName))
			{
				delete pkNewTextFileLoader;
				f->second->SetTop();
				return f->second;
			}

			delete f->second;
			f->second = pkNewTextFileLoader;
		}

		f->second->SetTop();
		return f->second;
	}

	CTextFileLoader* pkNewTextFileLoader = new CTextFileLoader;

	if (!pkNewTextFileLoader->Load(c_szFileName))
	{
		delete pkNewTextFileLoader;
		return nullptr;
	}

	ms_kMap_dwNameKey_pkTextFileLoader.emplace(dwNameKey, pkNewTextFileLoader);
	return pkNewTextFileLoader;
}

void CTextFileLoader::SetCacheMode()
{
	ms_isCacheMode = true;
}

void CTextFileLoader::DestroySystem()
{
	for (auto& pair : ms_kMap_dwNameKey_pkTextFileLoader)
		delete pair.second;

	ms_kMap_dwNameKey_pkTextFileLoader.clear();
	SGroupNode::DestroySystem();
}

void CTextFileLoader::Destroy()
{
	__DestroyGroupNodeVector();
	m_GlobalNode.m_kMap_dwKey_kVct_stToken.clear();
	m_GlobalNode.ChildNodeVector.clear();
}

CTextFileLoader::CTextFileLoader()
{
	m_GlobalNode.m_strGroupName = "global";
	m_GlobalNode.pParentNode = nullptr;

	SetTop();

	m_dwBufSize = 0;
	m_dwBufCapacity = 0;

	m_kVct_pkNode.reserve(128);
}

CTextFileLoader::~CTextFileLoader()
{
	Destroy();
}

void CTextFileLoader::__DestroyGroupNodeVector()
{
	for (auto* node : m_kVct_pkNode)
		SGroupNode::Delete(node);

	m_kVct_pkNode.clear();
}

const char* CTextFileLoader::GetFileName()
{
	return m_strFileName.c_str();
}

bool CTextFileLoader::IsEmpty()
{
	return m_strFileName.empty();
}

bool CTextFileLoader::Load(const char* c_szFileName)
{
	__DestroyGroupNodeVector();
	m_GlobalNode.m_kMap_dwKey_kVct_stToken.clear();
	m_GlobalNode.ChildNodeVector.clear();
	m_strFileName.clear();

	CMappedFile kFile;
	LPCVOID pData = nullptr;

	if (!CEterPackManager::Instance().Get(kFile, c_szFileName, &pData))
		return false;

	m_dwBufSize = kFile.Size();

	if (m_dwBufCapacity < m_dwBufSize)
	{
		m_dwBufCapacity = m_dwBufSize;
		m_acBufData = std::make_unique< char[] >(m_dwBufCapacity);
	}

	if (m_dwBufSize > 0 && pData != nullptr)
	{
		memcpy(m_acBufData.get(), pData, m_dwBufSize);
	}
	else
	{
		TraceError("CTextFileLoader::Load: File %s found but empty", c_szFileName);
		return false;
	}

	m_strFileName = c_szFileName;
	m_dwcurLineIndex = 0;

	m_textFileLoader.Bind(m_dwBufSize, m_acBufData.get());
	return LoadGroup(&m_GlobalNode);
}

bool CTextFileLoader::LoadGroup(TGroupNode* pGroupNode)
{
	std::vector< std::string_view > tokens;
	tokens.reserve(32);

	CTokenVector stTempValues;
	stTempValues.reserve(32);

	int nLocalGroupDepth = 0;
	DWORD lineCount = m_textFileLoader.GetLineCount();

	for (; m_dwcurLineIndex < lineCount; ++m_dwcurLineIndex)
	{
		const std::string& lineStr = m_textFileLoader.GetLineString(m_dwcurLineIndex);

		if (lineStr.empty())
			continue;

		FastSplitLine(lineStr.c_str(), lineStr.length(), tokens);

		if (tokens.empty())
			continue;

		std::string_view keyToken = tokens[0];

		if (keyToken.empty())
			continue;

		if (keyToken[0] == '{')
		{
			nLocalGroupDepth++;
			continue;
		}

		if (keyToken[0] == '}')
		{
			nLocalGroupDepth--;
			break;
		}

		if (IsEqualLower(keyToken, "group"))
		{
			if (tokens.size() < 2)
			{
				TraceError("CTextFileLoader::LoadGroup: Missing group name in %s:%u", m_strFileName.c_str(), m_dwcurLineIndex);
				continue;
			}

			TGroupNode* pNewNode = TGroupNode::New();
			pNewNode->pParentNode = pGroupNode;
			pNewNode->SetGroupName(std::string(tokens[1]));

			++m_dwcurLineIndex;

			if (!LoadGroup(pNewNode))
			{
				TGroupNode::Delete(pNewNode);
				return false;
			}

			m_kVct_pkNode.push_back(pNewNode);
			pGroupNode->ChildNodeVector.push_back(pNewNode);
		}
		else if (IsEqualLower(keyToken, "list"))
		{
			if (tokens.size() < 2)
			{
				TraceError("CTextFileLoader::LoadGroup: Missing list name in %s:%u", m_strFileName.c_str(), m_dwcurLineIndex);
				continue;
			}

			std::string listKey(tokens[1]);
			stl_lowers(listKey);

			stTempValues.clear();

			++m_dwcurLineIndex;

			for (; m_dwcurLineIndex < lineCount; ++m_dwcurLineIndex)
			{
				const std::string& subLineStr = m_textFileLoader.GetLineString(m_dwcurLineIndex);
				FastSplitLine(subLineStr.c_str(), subLineStr.length(), tokens);

				if (tokens.empty())
					continue;

				char firstChar = tokens[0][0];

				if (firstChar == '{')
					continue;

				if (firstChar == '}')
					break;

				for (auto& sv : tokens)
					stTempValues.emplace_back(sv);
			}

			pGroupNode->InsertTokenVector(listKey, stTempValues);
		}
		else
		{
			if (tokens.size() < 2)
				continue;

			stTempValues.clear();

			for (size_t i = 1; i < tokens.size(); ++i)
				stTempValues.emplace_back(tokens[i]);

			std::string keyStr(keyToken);
			stl_lowers(keyStr);

			pGroupNode->InsertTokenVector(keyStr, stTempValues);
		}
	}

	return (nLocalGroupDepth == 0);
}

BOOL CTextFileLoader::SetChildNode(const std::string& c_rstrKeyHead, DWORD dwIndex)
{
	char szKey[64];
	_snprintf(szKey, sizeof(szKey), "%s%02u", c_rstrKeyHead.c_str(), dwIndex);

	return SetChildNode(szKey);
}

BOOL CTextFileLoader::GetTokenBoolean(const char* c_szKey, BOOL* pData)
{
	CTokenVector* pTokenVector;

	if (!GetTokenVector(c_szKey, &pTokenVector))
		return FALSE;

	if (pTokenVector->empty())
		return FALSE;

	int val;

	if (SafeParseInt(pTokenVector->at(0), &val))
	{
		*pData = (val != 0);
		return TRUE;
	}

	return FALSE;
}

BOOL CTextFileLoader::GetTokenByte(const char* c_szKey, BYTE* pData)
{
	CTokenVector* pTokenVector;

	if (!GetTokenVector(c_szKey, &pTokenVector))
		return FALSE;

	if (pTokenVector->empty())
		return FALSE;

	int val;

	if (SafeParseInt(pTokenVector->at(0), &val))
	{
		*pData = (BYTE)val;
		return TRUE;
	}

	return FALSE;
}

BOOL CTextFileLoader::GetTokenWord(const char* c_szKey, WORD* pData)
{
	CTokenVector* pTokenVector;

	if (!GetTokenVector(c_szKey, &pTokenVector))
		return FALSE;

	if (pTokenVector->empty())
		return FALSE;

	int val;

	if (SafeParseInt(pTokenVector->at(0), &val))
	{
		*pData = (WORD)val;
		return TRUE;
	}

	return FALSE;
}

BOOL CTextFileLoader::GetTokenInteger(const char* c_szKey, int* pData)
{
	CTokenVector* pTokenVector;

	if (!GetTokenVector(c_szKey, &pTokenVector))
		return FALSE;

	if (pTokenVector->empty())
		return FALSE;

	return SafeParseInt(pTokenVector->at(0), pData);
}

BOOL CTextFileLoader::GetTokenDoubleWord(const char* c_szKey, DWORD* pData)
{
	CTokenVector* pTokenVector;

	if (!GetTokenVector(c_szKey, &pTokenVector))
		return FALSE;

	if (pTokenVector->empty())
		return FALSE;

	int val;

	if (SafeParseInt(pTokenVector->at(0), &val))
	{
		*pData = (DWORD)val;
		return TRUE;
	}

	return FALSE;
}

BOOL CTextFileLoader::GetTokenFloat(const char* c_szKey, float* pData)
{
	CTokenVector* pTokenVector;

	if (!GetTokenVector(c_szKey, &pTokenVector))
		return FALSE;

	if (pTokenVector->empty())
		return FALSE;

	return SafeParseFloat(pTokenVector->at(0), pData);
}

BOOL CTextFileLoader::GetTokenBoolean(const std::string& c_rstrKey, BOOL* pData)
{
	return GetTokenBoolean(c_rstrKey.c_str(), pData);
}

BOOL CTextFileLoader::GetTokenByte(const std::string& c_rstrKey, BYTE* pData)
{
	return GetTokenByte(c_rstrKey.c_str(), pData);
}

BOOL CTextFileLoader::GetTokenWord(const std::string& c_rstrKey, WORD* pData)
{
	return GetTokenWord(c_rstrKey.c_str(), pData);
}

BOOL CTextFileLoader::GetTokenInteger(const std::string& c_rstrKey, int* pData)
{
	return GetTokenInteger(c_rstrKey.c_str(), pData);
}

BOOL CTextFileLoader::GetTokenDoubleWord(const std::string& c_rstrKey, DWORD* pData)
{
	return GetTokenDoubleWord(c_rstrKey.c_str(), pData);
}

BOOL CTextFileLoader::GetTokenFloat(const std::string& c_rstrKey, float* pData)
{
	return GetTokenFloat(c_rstrKey.c_str(), pData);
}

BOOL CTextFileLoader::GetTokenVector2(const char* c_szKey, Vector2* pVector2)
{
	CTokenVector* pTokenVector;

	if (!GetTokenVector(c_szKey, &pTokenVector))
		return FALSE;

	if (pTokenVector->size() != 2)
		return FALSE;

	if (!SafeParseFloat(pTokenVector->at(0), &pVector2->x) ||
		!SafeParseFloat(pTokenVector->at(1), &pVector2->y))
		return FALSE;

	return TRUE;
}

BOOL CTextFileLoader::GetTokenVector3(const char* c_szKey, Vector3* pVector3)
{
	CTokenVector* pTokenVector;

	if (!GetTokenVector(c_szKey, &pTokenVector))
		return FALSE;

	if (pTokenVector->size() != 3)
		return FALSE;

	if (!SafeParseFloat(pTokenVector->at(0), &pVector3->x) ||
		!SafeParseFloat(pTokenVector->at(1), &pVector3->y) ||
		!SafeParseFloat(pTokenVector->at(2), &pVector3->z))
		return FALSE;

	return TRUE;
}

BOOL CTextFileLoader::GetTokenVector4(const char* c_szKey, Vector4* pVector4)
{
	CTokenVector* pTokenVector;

	if (!GetTokenVector(c_szKey, &pTokenVector))
		return FALSE;

	if (pTokenVector->size() != 4)
		return FALSE;

	if (!SafeParseFloat(pTokenVector->at(0), &pVector4->x) ||
		!SafeParseFloat(pTokenVector->at(1), &pVector4->y) ||
		!SafeParseFloat(pTokenVector->at(2), &pVector4->z) ||
		!SafeParseFloat(pTokenVector->at(3), &pVector4->w))
		return FALSE;

	return TRUE;
}

BOOL CTextFileLoader::GetTokenPosition(const char* c_szKey, Vector3* pVector)
{
	return GetTokenVector3(c_szKey, pVector);
}

BOOL CTextFileLoader::GetTokenQuaternion(const char* c_szKey, Quaternion* pQ)
{
	CTokenVector* pTokenVector;

	if (!GetTokenVector(c_szKey, &pTokenVector))
		return FALSE;

	if (pTokenVector->size() != 4)
		return FALSE;

	if (!SafeParseFloat(pTokenVector->at(0), &pQ->x) ||
		!SafeParseFloat(pTokenVector->at(1), &pQ->y) ||
		!SafeParseFloat(pTokenVector->at(2), &pQ->z) ||
		!SafeParseFloat(pTokenVector->at(3), &pQ->w))
		return FALSE;

	return TRUE;
}

BOOL CTextFileLoader::GetTokenDirection(const char* c_szKey, Vector3* pVector)
{
	CTokenVector* pTokenVector;

	if (!GetTokenVector(c_szKey, &pTokenVector))
		return FALSE;

	if (pTokenVector->size() != 3)
		return FALSE;

	if (!SafeParseFloat(pTokenVector->at(0), &pVector->x) ||
		!SafeParseFloat(pTokenVector->at(1), &pVector->y) ||
		!SafeParseFloat(pTokenVector->at(2), &pVector->z))
		return FALSE;

	return TRUE;
}

BOOL CTextFileLoader::GetTokenColor(const char* c_szKey, Color* pColor)
{
	CTokenVector* pTokenVector;

	if (!GetTokenVector(c_szKey, &pTokenVector))
		return FALSE;

	if (pTokenVector->size() != 4)
		return FALSE;

	if (!SafeParseFloat(pTokenVector->at(0), &pColor->r) ||
		!SafeParseFloat(pTokenVector->at(1), &pColor->g) ||
		!SafeParseFloat(pTokenVector->at(2), &pColor->b) ||
		!SafeParseFloat(pTokenVector->at(3), &pColor->a))
		return FALSE;

	return TRUE;
}

BOOL CTextFileLoader::GetTokenString(const char* c_szKey, std::string* pString)
{
	CTokenVector* pTokenVector;

	if (!GetTokenVector(c_szKey, &pTokenVector))
		return FALSE;

	if (pTokenVector->empty())
		return FALSE;

	*pString = pTokenVector->at(0);
	return TRUE;
}
