#pragma once

#include "AbstractCharacterManager.h"
#include "InstanceBase.h"
#include "../GameLib/PhysicsObject.h"

class CPythonCharacterManager : public CSingleton<CPythonCharacterManager>, public IAbstractCharacterManager, public IObjectManager
{
	public:
		// Character List
		typedef std::list<CInstanceBase *>			TCharacterInstanceList;
		typedef std::map<DWORD, CInstanceBase *>	TCharacterInstanceMap;

		class CharacterIterator;

	public:
		CPythonCharacterManager();
		virtual ~CPythonCharacterManager();

		virtual void AdjustCollisionWithOtherObjects(CActorInstance* pInst );

		void EnableSortRendering(bool isEnable);

		bool IsRegisteredVID(DWORD dwVID);
		bool IsAliveVID(DWORD dwVID);
		bool IsDeadVID(DWORD dwVID);
		bool IsCacheMode();

		bool OLD_GetPickedInstanceVID(DWORD* pdwPickedActorID);
		CInstanceBase* OLD_GetPickedInstancePtr();
		Vector2& OLD_GetPickedInstPosReference();

		CInstanceBase* FindClickableInstancePtr();

		void InsertPVPKey(DWORD dwVIDSrc, DWORD dwVIDDst);
		void RemovePVPKey(DWORD dwVIDSrc, DWORD dwVIDDst);
		void ChangeGVG(DWORD dwSrcGuildID, DWORD dwDstGuildID);

		void GetInfo(std::string* pstInfo);

		void ClearMainInstance();
		bool SetMainInstance(DWORD dwVID);
		CInstanceBase* GetMainInstancePtr();

		void								SCRIPT_SetAffect(DWORD dwVID, DWORD eAffect, BOOL isVisible);
		void								SetEmoticon(DWORD dwVID, DWORD eEmoticon);
		bool								IsPossibleEmoticon(DWORD dwVID);
		void								ShowPointEffect(DWORD dwVID, DWORD ePoint);
		bool								RegisterPointEffect(DWORD ePoint, const char* c_szFileName);

		// System
		void								Destroy();

		void								DeleteAllInstances();

		bool								CreateDeviceObjects();
		void								DestroyDeviceObjects();

		void								Update();
		void								Deform();
		void								Render();
		void								RenderShadowMainInstance();
		void								RenderShadowAllInstances();
		void								CollectShadowCastersForFrame();
		void								EndShadowCastersForFrame();
		void								RenderCollision();

		// Create/Delete Instance
		CInstanceBase *						CreateInstance(const CInstanceBase::SCreateData& c_rkCreateData);
		CInstanceBase *						RegisterInstance(DWORD VirtualID);

		void								DeleteInstance(DWORD VirtualID);
		void								DeleteInstanceByFade(DWORD VirtualID);
		void								DeleteVehicleInstance(DWORD VirtualID);

		void 								DestroyAliveInstanceMap();
		void 								DestroyDeadInstanceList();

		inline CharacterIterator			CharacterInstanceBegin() { return CharacterIterator(m_kAliveInstMap.begin());}
		inline CharacterIterator			CharacterInstanceEnd() { return CharacterIterator(m_kAliveInstMap.end());}

		// Access Instance
		void								SelectInstance(DWORD VirtualID);
		CInstanceBase *						GetSelectedInstancePtr();

		CInstanceBase *						GetInstancePtr(DWORD VirtualID);
		CInstanceBase *						GetInstancePtrByName(const char *name);

		// Pick
		int									PickAll();
		CInstanceBase *						GetCloseInstance(CInstanceBase * pInstance);

		// Refresh TextTail
		void								RefreshAllPCTextTail();
		void								RefreshAllGuildMark();

	protected:
		void								UpdateTransform();
		void								UpdateDeleting();

	protected:
		void __Initialize();

		void __DeleteBlendOutInstance(CInstanceBase* pkInstDel);

		void __OLD_Pick();
		void __NEW_Pick();

		void __UpdateSortPickedActorList();
		void __UpdatePickedActorList();
		void __SortPickedActorList();

		void __RenderSortedAliveActorList();
		void __RenderSortedDeadActorList();

	protected:
		CInstanceBase *						m_pkInstMain;
		CInstanceBase *						m_pkInstPick;
		CInstanceBase *						m_pkInstBind;
		Vector2							m_v2PickedInstProjPos;

		TCharacterInstanceMap				m_kAliveInstMap;
		TCharacterInstanceList				m_kDeadInstList;

		std::vector<CInstanceBase*>			m_kVct_pkInstPicked;

		std::vector<CInstanceBase*>			m_kVct_pkShadowCasters;

		DWORD								m_adwPointEffect[POINT_MAX_NUM];

	// =====================================================
	// OPTIMIZATION SYSTEMS - PUBLIC API
	// =====================================================
public:
#ifdef ENABLE_FRUSTUM_CULLING
	// Frustum Culling
	static int GetCulledCount() { return ms_iCulledCount; }
	static int GetRenderedCount() { return ms_iRenderedCount; }
	static int GetTotalCount() { return ms_iTotalCount; }
	void ResetCullingStats();
#endif

#ifdef ENABLE_CHAR_RENDER_LIMIT
	// Character Render Limit
	static void SetRenderLimit(int iLimit) { ms_iRenderLimit = iLimit; }
	static int GetRenderLimit() { return ms_iRenderLimit; }


	static constexpr float kDeadActorMaxDistance = 3000.0f;
	static constexpr size_t kDeadActorMaxRender  = 32;
	static int GetSkippedByLimit() { return ms_iSkippedByLimit; }
#endif

#ifdef ENABLE_RENDER_MODE_GROUPING
	// Render Mode Grouping Stats
	static int GetRenderModeNormalCount() { return ms_iRenderModeNormal; }
	static int GetRenderModeBlendCount() { return ms_iRenderModeBlend; }
	static int GetRenderModeAddCount() { return ms_iRenderModeAdd; }
	static int GetRenderModeModulateCount() { return ms_iRenderModeModulate; }
#endif

#ifdef ENABLE_UPDATE_CULLING
	// Update Culling Stats
	static int GetUpdateSkippedCount() { return ms_iUpdateSkipped; }
	static int GetUpdateFullCount() { return ms_iUpdateFull; }
	static int GetUpdateReducedCount() { return ms_iUpdateReduced; }
	static DWORD GetFrameCounter() { return ms_dwFrameCounter; }
#endif

#ifdef ENABLE_DEFORM_CULLING
	// Deform Culling Stats
	static int GetDeformSkippedCount() { return ms_iDeformSkipped; }
	static int GetDeformRenderedCount() { return ms_iDeformRendered; }
#endif

private:
#ifdef ENABLE_FRUSTUM_CULLING
	static int ms_iCulledCount;
	static int ms_iRenderedCount;
	static int ms_iTotalCount;
#endif

#ifdef ENABLE_CHAR_RENDER_LIMIT
	static int ms_iRenderLimit;
	static int ms_iSkippedByLimit;
#endif

#ifdef ENABLE_RENDER_MODE_GROUPING
	static int ms_iRenderModeNormal;
	static int ms_iRenderModeBlend;
	static int ms_iRenderModeAdd;
	static int ms_iRenderModeModulate;
#endif

#ifdef ENABLE_UPDATE_CULLING
	static int ms_iUpdateSkipped;
	static int ms_iUpdateFull;
	static int ms_iUpdateReduced;
#endif

#if defined(ENABLE_UPDATE_CULLING) || defined(ENABLE_ANIMATION_LOD)
	static DWORD ms_dwFrameCounter;
#endif

#ifdef ENABLE_DEFORM_CULLING
	static int ms_iDeformSkipped;
	static int ms_iDeformRendered;
#endif

public:
		class CharacterIterator
		{
		public:
			CharacterIterator(){}
			CharacterIterator(const TCharacterInstanceMap::iterator & it) : m_it(it) {}

			inline CInstanceBase * operator * () {	return m_it->second; }

			inline CharacterIterator & operator ++()
			{
				++m_it;
				return *this;
			}

			inline CharacterIterator operator ++(int)
			{
				CharacterIterator new_it = *this;
				++(*this);
				return new_it;
			}

			inline CharacterIterator & operator = (const CharacterIterator & rhs)
			{
				m_it = rhs.m_it;
				return (*this);
			}

			inline bool operator == (const CharacterIterator & rhs) const
			{
				return m_it == rhs.m_it;
			}

			inline bool operator != (const CharacterIterator & rhs) const
			{
				return m_it != rhs.m_it;
			}

			private:
				TCharacterInstanceMap::iterator m_it;
		};
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
