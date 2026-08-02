#pragma once
#include "../UserInterface/Locale_inc.h"

class CPropertyManager : public CSingleton<CPropertyManager>
{
	public:
		CPropertyManager();
		virtual ~CPropertyManager();

		void			Clear();

		bool			BuildPack();

		bool			LoadReservedCRC(const char * c_pszFileName);
		void			ReserveCRC(DWORD dwCRC);
		DWORD			GetUniqueCRC(const char * c_szSeed);

		bool			Initialize(const char * c_pszPackFileName = NULL);
		bool			Register(const char * c_pszFileName, CProperty ** ppProperty = NULL);

		bool			Get(DWORD dwCRC, CProperty ** ppProperty);
		bool			Get(const char * c_pszFileName, CProperty ** ppProperty);

		bool			Put(const char * c_pszFileName, const char * c_pszSourceFileName);

		bool			Erase(DWORD dwCRC);
		bool			Erase(const char * c_pszFileName);

	protected:
		typedef std::map<DWORD, CProperty *>		TPropertyCRCMap;
		typedef std::set<DWORD>						TCRCSet;

		bool										m_isFileMode;
		TPropertyCRCMap								m_PropertyByCRCMap;
		TCRCSet										m_ReservedCRCSet;

#if defined(__BL_AUTO_LANTERN_EFFECT__)
public:
	DWORD	GetLanternCRC() const { return m_dwLanternCRC; }
	DWORD	GetLanternEffectCRC() const { return m_dwLanternEffectCRC; }
	DWORD	GetLanternCRC2() const { return m_dwLanternCRC2; }
	DWORD	GetLanternEffectCRC2() const { return m_dwLanternEffectCRC2; }

private:
	DWORD	m_dwLanternCRC;
	DWORD	m_dwLanternEffectCRC;
	DWORD	m_dwLanternCRC2;
	DWORD	m_dwLanternEffectCRC2;
#endif
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
