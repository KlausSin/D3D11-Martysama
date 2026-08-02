#pragma once

#include "ActorInstance.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//	CGameEventManager
//
//
//

class CGameEventManager : public CSingleton<CGameEventManager>, public CScreen
{
	public:
		CGameEventManager();
		virtual ~CGameEventManager();

		void SetCenterPosition(float fx, float fy, float fz);
		void Update();

		void ProcessEventScreenWaving(CActorInstance * pActorInstance, const CRaceMotionData::TScreenWavingEventData * c_pData);

	protected:
		TPixelPosition m_CenterPosition;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
