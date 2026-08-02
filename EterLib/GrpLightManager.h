#pragma once

#include "../eterBase/Singleton.h"

#include "GrpBase.h"
#include "Util.h"
#include "Pool.h"

#include <deque>

typedef DWORD TLightID;

enum ELightBehavior
{
	LIGHT_BEHAVIOR_STATIC,		// Continuously turning on light
	LIGHT_BEHAVIOR_DYNAMIC,		// Immediately turning off light
};

class CLightBase
{
	public:
		CLightBase() {};
		virtual ~CLightBase() {};

		void	SetCurrentTime();

	protected:
		static float ms_fCurTime;
};

class CLight : public CGraphicBase, public CLightBase
{
	public:
		CLight();
		virtual ~CLight();

		void		Initialize();
		void		Clear();

		void		Update();

		void		SetParameter(TLightID id, const TLight & c_rLight);

		void		SetDistance(float fDistance);
		float		GetDistance() const { return m_fDistance;	}

		TLightID	GetLightID()	{ return m_LightID;		}

		BOOL		isEdited()		{ return m_isEdited;	}
		void		SetDeviceLight(BOOL bActive);

		void		SetDiffuseColor(float fr, float fg, float fb, float fa = 1.0f);
		void		SetAmbientColor(float fr, float fg, float fb, float fa = 1.0f);
		void		SetRange(float fRange);
		void		SetPosition(float fx, float fy, float fz);

		const Vector3 & GetPosition() const;

		void		BlendDiffuseColor(const Color & c_rColor, float fBlendTime, float fDelayTime = 0.0f);
		void		BlendAmbientColor(const Color & c_rColor, float fBlendTime, float fDelayTime = 0.0f);
		void		BlendRange(float fRange, float fBlendTime, float fDelayTime = 0.0f);

	private:
		TLightID		m_LightID;		// Light ID. equal to D3D light index

		TLight		m_d3dLight;
		BOOL			m_isEdited;
		float			m_fDistance;

		TTransitorColor	m_DiffuseColorTransitor;
		TTransitorColor	m_AmbientColorTransitor;
		TTransitorFloat m_RangeTransitor;
};

class CLightManager : public CGraphicBase, public CLightBase, public CSingleton<CLightManager>
{
	public:
		enum
		{
			LIGHT_LIMIT_DEFAULT = 3,
//			LIGHT_MAX_NUM = 32,
		};

		typedef std::deque<TLightID>			TLightIDDeque;
		typedef std::map<TLightID, CLight *>	TLightMap;
		typedef std::vector<CLight *>			TLightSortVector;

	public:
		CLightManager();
		virtual ~CLightManager();

		void		Destroy();

		void		Initialize();

		void		Update();
		void		FlushLight();
		void		RestoreLight();

		/////
		void		RegisterLight(ELightBehavior LightBehavior, TLightID * poutLightID, TLight & LightData);
		CLight *	GetLight(TLightID LightID);
		void		DeleteLight(TLightID LightID);
		/////

		void		SetCenterPosition(const Vector3 & c_rv3Position);
		void		SetLimitLightCount(DWORD dwLightCount);
		void		SetSkipIndex(DWORD dwSkipIndex);

	protected:
		TLightIDDeque			m_NonUsingLightIDDeque;

		TLightMap				m_LightMap;
		TLightSortVector		m_LightSortVector;

		Vector3				m_v3CenterPosition;
		Vector3				m_v3LastSortPosition;  // Last position when lights were sorted
		DWORD					m_dwLimitLightCount;
		DWORD					m_dwSkipIndex;
		bool					m_bLightsDirty;  // True when lights added/removed

		static constexpr float LIGHT_SORT_DISTANCE_THRESHOLD = 50.0f;  // Only re-sort if moved > this distance

		// Saved lighting state for FlushLight/RestoreLight
		bool					m_bSavedLighting;

	protected:
		TLightID				NewLightID();
		void					ReleaseLightID(TLightID LightID);

		CDynamicPool<CLight>	m_LightPool;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
