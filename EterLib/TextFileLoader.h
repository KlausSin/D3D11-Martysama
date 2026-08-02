#pragma once

#include "../eterBase/FileLoader.h"
#include "../eterBase/MappedFile.h"
#include "../eterLib/Util.h"
#include "../eterLib/Pool.h"

class CTextFileLoader {
public:
    typedef struct SGroupNode {
        static DWORD GenNameKey(const char* c_szGroupName, size_t uGroupNameLen);
        static DWORD GenNameKey(std::string_view sv);

        void SetGroupName(const std::string& c_rstGroupName);
        bool IsGroupNameKey(DWORD dwGroupNameKey);

        const std::string& GetGroupName();

        CTokenVector* GetTokenVector(DWORD dwGroupNameKey);
        CTokenVector* GetTokenVector(const std::string& c_rstGroupName);

        bool IsExistTokenVector(DWORD dwGroupNameKey);
        bool IsExistTokenVector(const std::string& c_rstGroupName);

        void InsertTokenVector(const std::string& c_rstGroupName,
            const CTokenVector& c_rkVct_stToken);

        DWORD m_dwGroupNameKey;
        std::string m_strGroupName;

        std::map < DWORD, CTokenVector > m_kMap_dwKey_kVct_stToken;

        SGroupNode* pParentNode;
        std::vector < SGroupNode* > ChildNodeVector;

        static SGroupNode* New();
        static void Delete(SGroupNode* pkNode);

        static void DestroySystem();
        static CDynamicPool < SGroupNode > ms_kPool;
    }
    TGroupNode;

    typedef std::vector < TGroupNode* > TGroupNodeVector;

    class CGotoChild {
    public: CGotoChild(CTextFileLoader* pOwner,
        const char* c_szKey) : m_pOwner(pOwner) {
        m_pOwner->SetChildNode(c_szKey);
    }

          CGotoChild(CTextFileLoader* pOwner, DWORD dwIndex) : m_pOwner(pOwner) {
              m_pOwner->SetChildNode(dwIndex);
          }

          ~CGotoChild() {
              m_pOwner->SetParentNode();
          }

          CTextFileLoader* m_pOwner;
    };

public:
    static void DestroySystem();
    static void SetCacheMode();
    static CTextFileLoader* Cache(const char* c_szFileName);

public:
    CTextFileLoader();
    virtual~CTextFileLoader();

    void Destroy();

    bool Load(const char* c_szFileName);
    const char* GetFileName();
    bool IsEmpty();

    void SetTop() {
        m_pcurNode = &m_GlobalNode;
    }

    DWORD GetChildNodeCount() {
        if (!m_pcurNode)
            return 0;

        return (DWORD)m_pcurNode->ChildNodeVector.size();
    }

    BOOL SetChildNode(const char* c_szKey) {
        if (!m_pcurNode)
            return FALSE;

        DWORD dwKey = SGroupNode::GenNameKey(c_szKey, strlen(c_szKey));

        for (TGroupNode* node : m_pcurNode->ChildNodeVector) {
            if (node->IsGroupNameKey(dwKey)) {
                m_pcurNode = node;
                return TRUE;
            }
        }

        return FALSE;
    }

    BOOL SetChildNode(DWORD dwIndex) {
        if (!m_pcurNode)
            return FALSE;

        if (dwIndex >= m_pcurNode->ChildNodeVector.size())
            return FALSE;

        m_pcurNode = m_pcurNode->ChildNodeVector[dwIndex];
        return TRUE;
    }

    BOOL SetChildNode(const std::string& c_rstrKeyHead, DWORD dwIndex);

    BOOL SetParentNode() {
        if (!m_pcurNode || !m_pcurNode->pParentNode)
            return FALSE;

        m_pcurNode = m_pcurNode->pParentNode;
        return TRUE;
    }

    BOOL GetCurrentNodeName(std::string* pstrName) {
        if (!m_pcurNode)
            return FALSE;

        *pstrName = m_pcurNode->GetGroupName();
        return TRUE;
    }

    BOOL IsToken(const char* c_szKey) {
        if (!m_pcurNode)
            return FALSE;

        return m_pcurNode->IsExistTokenVector(SGroupNode::GenNameKey(c_szKey, strlen(c_szKey)));
    }

    BOOL IsToken(const std::string& c_rstrKey) {
        if (!m_pcurNode)
            return FALSE;

        return m_pcurNode->IsExistTokenVector(c_rstrKey);
    }

    BOOL GetTokenVector(const char* c_szKey, CTokenVector** ppTokenVector) {
        if (!m_pcurNode)
            return FALSE;

        CTokenVector* pkRet = m_pcurNode->GetTokenVector(SGroupNode::GenNameKey(c_szKey, strlen(c_szKey)));

        if (!pkRet)
            return FALSE;

        *ppTokenVector = pkRet;
        return TRUE;
    }

    BOOL GetTokenVector(const std::string& c_rstrKey, CTokenVector** ppTokenVector) {
        if (!m_pcurNode)
            return FALSE;

        CTokenVector* pkRet = m_pcurNode->GetTokenVector(c_rstrKey);

        if (!pkRet)
            return FALSE;

        *ppTokenVector = pkRet;
        return TRUE;
    }

    BOOL GetTokenBoolean(const char* c_szKey, BOOL* pData);
    BOOL GetTokenByte(const char* c_szKey, BYTE* pData);
    BOOL GetTokenWord(const char* c_szKey, WORD* pData);
    BOOL GetTokenInteger(const char* c_szKey, int* pData);
    BOOL GetTokenDoubleWord(const char* c_szKey, DWORD* pData);
    BOOL GetTokenFloat(const char* c_szKey, float* pData);

    BOOL GetTokenBoolean(const std::string& c_rstrKey, BOOL* pData);
    BOOL GetTokenByte(const std::string& c_rstrKey, BYTE* pData);
    BOOL GetTokenWord(const std::string& c_rstrKey, WORD* pData);
    BOOL GetTokenInteger(const std::string& c_rstrKey, int* pData);
    BOOL GetTokenDoubleWord(const std::string& c_rstrKey, DWORD* pData);
    BOOL GetTokenFloat(const std::string& c_rstrKey, float* pData);

    BOOL GetTokenVector2(const char* c_szKey, Vector2* pVector2);
    BOOL GetTokenVector3(const char* c_szKey, Vector3* pVector3);
    BOOL GetTokenVector4(const char* c_szKey, Vector4* pVector4);
    BOOL GetTokenPosition(const char* c_szKey, Vector3* pVector);
    BOOL GetTokenQuaternion(const char* c_szKey, Quaternion* pQ);
    BOOL GetTokenDirection(const char* c_szKey, Vector3* pVector);
    BOOL GetTokenColor(const char* c_szKey, Color* pColor);
    BOOL GetTokenString(const char* c_szKey, std::string* pString);

    BOOL GetTokenVector2(const std::string& k, Vector2* v) {
        return GetTokenVector2(k.c_str(), v);
    }
    BOOL GetTokenVector3(const std::string& k, Vector3* v) {
        return GetTokenVector3(k.c_str(), v);
    }
    BOOL GetTokenVector4(const std::string& k, Vector4* v) {
        return GetTokenVector4(k.c_str(), v);
    }
    BOOL GetTokenPosition(const std::string& k, Vector3* v) {
        return GetTokenPosition(k.c_str(), v);
    }
    BOOL GetTokenQuaternion(const std::string& k, Quaternion* q) {
        return GetTokenQuaternion(k.c_str(), q);
    }
    BOOL GetTokenDirection(const std::string& k, Vector3* v) {
        return GetTokenDirection(k.c_str(), v);
    }
    BOOL GetTokenColor(const std::string& k, Color* c) {
        return GetTokenColor(k.c_str(), c);
    }
    BOOL GetTokenString(const std::string& k, std::string* s) {
        return GetTokenString(k.c_str(), s);
    }

protected:
    void __DestroyGroupNodeVector();
    bool LoadGroup(TGroupNode* pGroupNode);

protected:
    std::string m_strFileName;

    std::unique_ptr < char[] > m_acBufData;
    DWORD m_dwBufSize;
    DWORD m_dwBufCapacity;

    DWORD m_dwcurLineIndex;

    CMemoryTextFileLoader m_textFileLoader;

    TGroupNode m_GlobalNode;
    TGroupNode* m_pcurNode;

    std::vector < SGroupNode* > m_kVct_pkNode;

protected:
    static std::map < DWORD,
        CTextFileLoader* > ms_kMap_dwNameKey_pkTextFileLoader;
    static bool ms_isCacheMode;
};