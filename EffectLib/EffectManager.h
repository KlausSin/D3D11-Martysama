#pragma once

#include "EffectInstance.h"

class CEffectManager : public CScreen, public CSingleton<CEffectManager>
{
	public:
		enum EEffectType
		{
			EFFECT_TYPE_NONE				= 0,
			EFFECT_TYPE_PARTICLE			= 1,
			EFFECT_TYPE_ANIMATION_TEXTURE	= 2,
			EFFECT_TYPE_MESH				= 3,
			EFFECT_TYPE_SIMPLE_LIGHT		= 4,

			EFFECT_TYPE_MAX_NUM				= 4,
		};

		typedef std::map<DWORD, CEffectData*> TEffectDataMap;
		typedef std::map<DWORD, CEffectInstance*> TEffectInstanceMap;

	public:
		CEffectManager();
		virtual ~CEffectManager();

		void Destroy();

		void UpdateSound();
		void Update();
		void Render();


		void GetInfo(std::string* pstInfo);

		bool IsAliveEffect(DWORD dwInstanceIndex);

		// Register
		BOOL RegisterEffect(const char * c_szFileName,bool isExistDelete=false,bool isNeedCache=false);
		BOOL RegisterEffect2(const char * c_szFileName, DWORD* pdwRetCRC, bool isNeedCache=false);

		void DeleteAllInstances();

		// Usage
		int CreateEffect(DWORD dwID, const Vector3 & c_rv3Position, const Vector3 & c_rv3Rotation);
		int CreateEffect(const char * c_szFileName, const Vector3 & c_rv3Position, const Vector3 & c_rv3Rotation);

		void CreateEffectInstance(DWORD dwInstanceIndex, DWORD dwID);
		BOOL SelectEffectInstance(DWORD dwInstanceIndex);
		bool DestroyEffectInstance(DWORD dwInstanceIndex);
		void DeactiveEffectInstance(DWORD dwInstanceIndex);

		void SetEffectTextures(DWORD dwID,std::vector<string> textures);
		void SetEffectInstancePosition(const Vector3 & c_rv3Position);
		void SetEffectInstanceRotation(const Vector3 & c_rv3Rotation);
		void SetEffectInstanceGlobalMatrix(const Matrix & c_rmatGlobal);

		void ShowEffect();
		void HideEffect();

#ifdef __ENABLE_STEALTH_FIX__
		void ApplyAlwaysHidden();
		void ReleaseAlwaysHidden();
#endif

		// Temporary function
		DWORD GetRandomEffect();
		int GetEmptyIndex();
		bool GetEffectData(DWORD dwID, CEffectData ** ppEffect);
		bool GetEffectData(DWORD dwID, const CEffectData ** c_ppEffect);

		void CreateUnsafeEffectInstance(DWORD dwEffectDataID, CEffectInstance ** ppEffectInstance);
		bool DestroyUnsafeEffectInstance(CEffectInstance * pEffectInstance);

		int GetRenderingEffectCount();

#ifdef ENABLE_EFFECT_LIMIT
		static void SetEffectLimit(int iLimit) { ms_iEffectLimit = iLimit; }
		static int GetEffectLimit() { return ms_iEffectLimit; }
		static int GetActiveEffectCount() { return ms_iActiveEffectCount; }
		static void SetMainPlayerPosition(const Vector3& v3Pos) { ms_v3PlayerPosition = v3Pos; }
		static float GetDistanceFromPlayer(const Vector3& v3Pos);
		static bool CanCreateEffect(const Vector3& v3Pos);
		static void IncreaseActiveEffectCount() { ms_iActiveEffectCount++; }
		static void DecreaseActiveEffectCount() { if (ms_iActiveEffectCount > 0) ms_iActiveEffectCount--; }
#endif

	protected:
		void __Initialize();

		void __DestroyEffectInstanceMap();
		void __DestroyEffectCacheMap();
		void __DestroyEffectDataMap();

	protected:
		bool m_isDisableSortRendering;
		TEffectDataMap					m_kEftDataMap;
		TEffectInstanceMap				m_kEftInstMap;
		TEffectInstanceMap				m_kEftCacheMap;

		CEffectInstance *				m_pSelectedEffectInstance;

#ifdef ENABLE_EFFECT_LIMIT
		static int ms_iEffectLimit;
		static int ms_iActiveEffectCount;
		static Vector3 ms_v3PlayerPosition;
	static DWORD ms_dwUpdateFrameCounter;
#endif
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
