#include "StdAfx.h"
#include "../effectLib/EffectManager.h"
#include "../AudioLib/SoundEngine.h"

#include "ActorInstance.h"
#include "RaceData.h"
#include "GameLibDefines.h"

#include "../EterBase/StepTimer.h"

void CActorInstance::SetBattleHitEffect(DWORD dwID)
{
	m_dwBattleHitEffectID = dwID;
}

void CActorInstance::SetBattleAttachEffect(DWORD dwID)
{
	m_dwBattleAttachEffectID = dwID;
}

bool CActorInstance::CanAct()
{
	if (IsDead())
		return false;

	if (IsStun())
		return false;

	if (IsParalysis())
		return false;

	if (IsFaint())
		return false;

	if (IsSleep())
		return false;

	return true;
}

bool CActorInstance::CanUseSkill()
{
	if (!CanAct())
		return false;

	DWORD dwCurMotionIndex=__GetCurrentMotionIndex();

	// Locked during attack
	switch (dwCurMotionIndex)
	{
		case CRaceMotionData::NAME_FISHING_THROW:
		case CRaceMotionData::NAME_FISHING_WAIT:
		case CRaceMotionData::NAME_FISHING_STOP:
		case CRaceMotionData::NAME_FISHING_REACT:
		case CRaceMotionData::NAME_FISHING_CATCH:
		case CRaceMotionData::NAME_FISHING_FAIL:
			return TRUE;
			break;
	}

	// Locked during using skill
	if (IsUsingSkill())
	{
		if (m_pkCurRaceMotionData->IsCancelEnableSkill())
			return TRUE;

		return FALSE;
	}

	return true;
}

bool CActorInstance::CanMove()
{
	if (!CanAct())
		return false;

	if (isLock())
		return false;

	return true;
}

bool CActorInstance::CanAttack()
{
	if (!CanAct())
		return false;

	if (IsUsingSkill())
	{
		if (!CanCancelSkill())
			return false;
	}

	return true;
}

bool CActorInstance::CanFishing()
{
	if (!CanAct())
		return false;

	if (IsUsingSkill())
		return false;

	switch (__GetCurrentMotionIndex())
	{
		case CRaceMotionData::NAME_WAIT:
		case CRaceMotionData::NAME_WALK:
		case CRaceMotionData::NAME_RUN:
			break;
		default:
			return false;
			break;
	}

	return true;
}

BOOL CActorInstance::IsClickableDistanceDestInstance(CActorInstance & rkInstDst, float fDistance)
{
	TPixelPosition kPPosSrc;
	GetPixelPosition(&kPPosSrc);

	Vector3 kD3DVct3Src(kPPosSrc);

	TCollisionPointInstanceList& rkLstkDefPtInst=rkInstDst.m_DefendingPointInstanceList;
	TCollisionPointInstanceList::iterator i;

	for (i=rkLstkDefPtInst.begin(); i!=rkLstkDefPtInst.end(); ++i)
	{
		CDynamicSphereInstanceVector& rkVctkDefSphere = (*i).SphereInstanceVector;

		CDynamicSphereInstanceVector::iterator j;
		for (j=rkVctkDefSphere.begin(); j!=rkVctkDefSphere.end(); ++j)
		{
			CDynamicSphereInstance& rkSphere=(*j);

			auto val = Vector3(rkSphere.v3Position-kD3DVct3Src);
			float fMovDistance=Vec3Length(&val);
			float fAtkDistance=rkSphere.fRadius+fDistance;

			if (fAtkDistance>fMovDistance)
				return TRUE;
		}
	}

	return FALSE;
}

void CActorInstance::InputNormalAttackCommand(float fDirRot)
{
	if (!__CanInputNormalAttackCommand())
		return;

	m_fAtkDirRot=fDirRot;
	NormalAttack(m_fAtkDirRot);
}

bool CActorInstance::InputComboAttackCommand(float fDirRot)
{
	m_fAtkDirRot=fDirRot;

	if (m_isPreInput)
		return false;

	/////////////////////////////////////////////////////////////////////////////////

	// Process Input
  	if (0 == m_dwcurComboIndex)
	{
 		__RunNextCombo();
		return true;
	}
	else if (m_pkCurRaceMotionData->IsComboInputTimeData())
	{
 		float fElapsedTime = GetAttackingElapsedTime();

		if (fElapsedTime > m_pkCurRaceMotionData->GetComboInputEndTime())
		{
			if (IsBowMode())
				m_isNextPreInput = TRUE;
			return false;
		}

		if (fElapsedTime > m_pkCurRaceMotionData->GetNextComboTime())
		{
			// args : BlendingTime
			__RunNextCombo();
			return true;
		}
		else if (fElapsedTime > m_pkCurRaceMotionData->GetComboInputStartTime())
		{
			m_isPreInput = TRUE;
			return false;
		}
	}
	else
	{
		float fElapsedTime = GetAttackingElapsedTime();
		if (fElapsedTime > m_pkCurRaceMotionData->GetMotionDuration()*0.9f)
		{
			// args : BlendingTime
			__RunNextCombo();
			return true;
		}
	}
	// Process Input

	return false;
}

void CActorInstance::ComboProcess()
{
	// If combo is on action
	if (0 != m_dwcurComboIndex)
	{
		if (!m_pkCurRaceMotionData)
		{
			Tracef("Attacking motion data is NULL! : %d\n", m_dwcurComboIndex);
			__ClearCombo();
			return;
		}

		float fElapsedTime = GetAttackingElapsedTime();

		// Process PreInput
		if (m_isPreInput)
		{
			if (fElapsedTime > m_pkCurRaceMotionData->GetNextComboTime())
			{
  				__RunNextCombo();
				m_isPreInput = FALSE;

				return;
			}
		}
	}
	else
	{
		m_isPreInput = FALSE;

		if (!IsUsingSkill())
		if (m_isNextPreInput)
		{
			__RunNextCombo();
			m_isNextPreInput = FALSE;
		}
	}
}

void CActorInstance::__RunNextCombo()
{
 	++m_dwcurComboIndex;
	///////////////////////////

	WORD wComboIndex = m_dwcurComboIndex;
	WORD wComboType = __GetCurrentComboType();

	if (wComboIndex==0)
	{
		Tracenf("CActorInstance::__RunNextCombo(wComboType=%d, wComboIndex=%d)", wComboType, wComboIndex); // @warme668
		return;
	}

	DWORD dwComboArrayIndex = wComboIndex - 1;

	CRaceData::TComboData * pComboData;

	if (!m_pkCurRaceData->GetComboDataPointer(m_wcurMotionMode, wComboType, &pComboData))
	{
		Tracenf("CActorInstance::__RunNextCombo(wComboType=%d, wComboIndex=%d) - m_pkCurRaceData->GetComboDataPointer(m_wcurMotionMode=%d, &pComboData) == NULL",
			wComboType, wComboIndex, m_wcurMotionMode); // @warme668
		return;
	}

	if (dwComboArrayIndex >= pComboData->ComboIndexVector.size())
	{
		Tracenf("CActorInstance::__RunNextCombo(wComboType=%d, wComboIndex=%d) - (dwComboArrayIndex=%d) >= (pComboData->ComboIndexVector.size()=%d)",
			wComboType, wComboIndex, dwComboArrayIndex, pComboData->ComboIndexVector.size()); // @warme668
		return;
	}

	WORD wcurComboMotionIndex = pComboData->ComboIndexVector[dwComboArrayIndex];
	ComboAttack(wcurComboMotionIndex, m_fAtkDirRot, 0.1f);

	////////////////////////////////
	if (m_dwcurComboIndex == pComboData->ComboIndexVector.size())
	{
		__OnEndCombo();
	}
}

void CActorInstance::__OnEndCombo()
{
	if (__IsMountingHorse())
	{
		m_dwcurComboIndex = 1;
	}
}

void CActorInstance::__ClearCombo()
{
	m_dwcurComboIndex = 0;
	m_isPreInput = FALSE;
	m_pkCurRaceMotionData = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CActorInstance::isAttacking()
{
	if (isNormalAttacking())
		return TRUE;

	if (isComboAttacking())
		return TRUE;

	if (IsSplashAttacking())
		return TRUE;

	return FALSE;
}

BOOL CActorInstance::isValidAttacking()
{
	if (!m_pkCurRaceMotionData)
		return FALSE;

	if (!m_pkCurRaceMotionData->isAttackingMotion())
		return FALSE;

	const NRaceData::TMotionAttackData * c_pData = m_pkCurRaceMotionData->GetMotionAttackDataPointer();
	float fElapsedTime = GetAttackingElapsedTime();
	NRaceData::THitDataContainer::const_iterator itor = c_pData->HitDataContainer.begin();
	for (; itor != c_pData->HitDataContainer.end(); ++itor)
	{
		const NRaceData::THitData & c_rHitData = *itor;
		if (fElapsedTime > c_rHitData.fAttackStartTime &&
			fElapsedTime < c_rHitData.fAttackEndTime)
			return TRUE;
	}

	return TRUE;
}

BOOL CActorInstance::CanCheckAttacking()
{
	if (isAttacking())
		return true;

	return false;
}

bool CActorInstance::__IsInSplashTime()
{
	if (m_kSplashArea.fDisappearingTime>GetLocalTime())
		return true;

	return false;
}

BOOL CActorInstance::isNormalAttacking()
{
	if (!m_pkCurRaceMotionData)
		return FALSE;

	if (!m_pkCurRaceMotionData->isAttackingMotion())
		return FALSE;

	const NRaceData::TMotionAttackData * c_pData = m_pkCurRaceMotionData->GetMotionAttackDataPointer();
	if (NRaceData::MOTION_TYPE_NORMAL != c_pData->iMotionType)
		return FALSE;

	return TRUE;
}

BOOL CActorInstance::isComboAttacking()
{
	if (!m_pkCurRaceMotionData)
		return FALSE;

	if (!m_pkCurRaceMotionData->isAttackingMotion())
		return FALSE;

	const NRaceData::TMotionAttackData * c_pData = m_pkCurRaceMotionData->GetMotionAttackDataPointer();
	if (NRaceData::MOTION_TYPE_COMBO != c_pData->iMotionType)
		return FALSE;

	return TRUE;
}

BOOL CActorInstance::IsSplashAttacking()
{
	if (!m_pkCurRaceMotionData)
		return FALSE;

	if (m_pkCurRaceMotionData->HasSplashMotionEvent())
		return TRUE;

	return FALSE;
}

BOOL CActorInstance::__IsMovingSkill(WORD wSkillNumber)
{
	enum
	{
		HORSE_DASH_SKILL_NUMBER = 137,
	};

	return HORSE_DASH_SKILL_NUMBER == wSkillNumber;
}

BOOL CActorInstance::IsActEmotion()
{
	DWORD dwCurMotionIndex=__GetCurrentMotionIndex();
	switch (dwCurMotionIndex)
	{
		case CRaceMotionData::NAME_FRENCH_KISS_START+0:
		case CRaceMotionData::NAME_FRENCH_KISS_START+1:
		case CRaceMotionData::NAME_FRENCH_KISS_START+2:
		case CRaceMotionData::NAME_FRENCH_KISS_START+3:
#ifdef ENABLE_WOLFMAN_CHARACTER
		case CRaceMotionData::NAME_FRENCH_KISS_START+4:
#endif
		case CRaceMotionData::NAME_KISS_START+0:
		case CRaceMotionData::NAME_KISS_START+1:
		case CRaceMotionData::NAME_KISS_START+2:
		case CRaceMotionData::NAME_KISS_START+3:
#ifdef ENABLE_WOLFMAN_CHARACTER
		case CRaceMotionData::NAME_KISS_START+4:
#endif
			return TRUE;
			break;
	}

	return FALSE;
}

BOOL CActorInstance::IsUsingMovingSkill()
{
	return __IsMovingSkill(m_kCurMotNode.uSkill);
}

DWORD CActorInstance::GetComboIndex()
{
	return m_dwcurComboIndex;
}

float CActorInstance::GetAttackingElapsedTime()
{
	return (GetLocalTime() - m_kCurMotNode.fStartTime) * m_kCurMotNode.fSpeedRatio;
//	return (GetLocalTime() - m_kCurMotNode.fStartTime) * __GetAttackSpeed();
}

bool CActorInstance::__CanInputNormalAttackCommand()
{
	if (IsWaiting())
		return true;

	if (isNormalAttacking())
	{
		float fElapsedTime = GetAttackingElapsedTime();

		if (fElapsedTime > m_pkCurRaceMotionData->GetMotionDuration()*0.9f)
			return true;
	}

	return false;
}

BOOL CActorInstance::NormalAttack(float fDirRot, float fBlendTime)
{
	WORD wMotionIndex;
	if (!m_pkCurRaceData->GetNormalAttackIndex(m_wcurMotionMode, &wMotionIndex))
		return FALSE;

	BlendRotation(fDirRot, fBlendTime);
	SetAdvancingRotation(fDirRot);
	InterceptOnceMotion(wMotionIndex, 0.1f, 0, __GetAttackSpeed());

	__OnAttack(wMotionIndex);

	NEW_SetAtkPixelPosition(NEW_GetCurPixelPositionRef());

	return TRUE;
}

BOOL CActorInstance::ComboAttack(DWORD dwMotionIndex, float fDirRot, float fBlendTime)
{
	BlendRotation(fDirRot, fBlendTime);
	SetAdvancingRotation(fDirRot);

	InterceptOnceMotion(dwMotionIndex, fBlendTime, 0, __GetAttackSpeed());

	__OnAttack(dwMotionIndex);

	NEW_SetAtkPixelPosition(NEW_GetCurPixelPositionRef());

	return TRUE;
}

void CActorInstance::__ProcessMotionEventAttackSuccess(DWORD dwMotionKey, BYTE byEventIndex, CActorInstance & rVictim)
{
	CRaceMotionData * pMotionData;

	if (!m_pkCurRaceData->GetMotionDataPointer(dwMotionKey, &pMotionData))
		return;

	if (byEventIndex >= pMotionData->GetMotionEventDataCount())
		return;

	const CRaceMotionData::TMotionAttackingEventData * pMotionEventData;
	if (!pMotionData->GetMotionAttackingEventDataPointer(byEventIndex, &pMotionEventData))
		return;

	const Vector3& c_rv3VictimPos=rVictim.GetPositionVectorRef();
	__ProcessDataAttackSuccess(pMotionEventData->AttackData, rVictim, c_rv3VictimPos);
}

void CActorInstance::__ProcessMotionAttackSuccess(DWORD dwMotionKey, CActorInstance & rVictim)
{
	CRaceMotionData * c_pMotionData;

	if (!m_pkCurRaceData->GetMotionDataPointer(dwMotionKey, &c_pMotionData))
		return;

	const Vector3& c_rv3VictimPos=rVictim.GetPositionVectorRef();
	__ProcessDataAttackSuccess(c_pMotionData->GetMotionAttackDataReference(), rVictim, c_rv3VictimPos);
}

DWORD CActorInstance::__GetOwnerVID()
{
	return m_dwOwnerVID;
}

float CActorInstance::__GetOwnerTime()
{
	return GetLocalTime()-m_fOwnerBaseTime;
}

bool IS_HUGE_RACE(unsigned int vnum)
{
	switch (vnum)
	{
	case 2493:
		return true;
	}
	return false;
}

bool CActorInstance::__CanPushDestActor(CActorInstance& rkActorDst)
{
	if (rkActorDst.IsBuilding())
		return false;

	if (rkActorDst.IsDoor())
		return false;

	if (rkActorDst.IsStone())
		return false;

	if (rkActorDst.IsNPC())
		return false;

	extern bool IS_HUGE_RACE(unsigned int vnum);
	if (IS_HUGE_RACE(rkActorDst.GetRace()))
		return false;

	if (rkActorDst.IsStun())
		return true;

	if (rkActorDst.__GetOwnerVID()!=GetVirtualID())
		return false;

	if (rkActorDst.__GetOwnerTime()>3.0f)
		return false;

	return true;
}

bool IS_PARTY_HUNTING_RACE(unsigned int vnum)
{
	return true;
}

void CActorInstance::__ProcessDataAttackSuccess(const NRaceData::TAttackData & c_rAttackData, CActorInstance & rVictim, const Vector3 & c_rv3Position, UINT uiSkill, BOOL isSendPacket)
{
	if (NRaceData::HIT_TYPE_NONE == c_rAttackData.iHittingType)
		return;

	InsertDelay(c_rAttackData.fStiffenTime);

	if (__CanPushDestActor(rVictim) && c_rAttackData.fExternalForce > 0.0f)
	{
		__PushCircle(rVictim);

		// VICTIM_COLLISION_TEST
		const Vector3& kVictimPos = rVictim.GetPosition();
		auto externalForceRatio = 1.0f;
		if (rVictim.IsResistFallen()) // @fixme033 reduced force with mental strong body almost like before
			externalForceRatio = 0.8f;
		rVictim.m_PhysicsObject.IncreaseExternalForce(kVictimPos, c_rAttackData.fExternalForce * externalForceRatio);
		// VICTIM_COLLISION_TEST_END
	}

	// Invisible Time
	if (IS_PARTY_HUNTING_RACE(rVictim.GetRace()))
	{
		if (uiSkill)
			rVictim.m_fInvisibleTime = DX::StepTimer::Instance().GetTotalSeconds() + c_rAttackData.fInvisibleTime;

		if (m_isMain)
			rVictim.m_fInvisibleTime = DX::StepTimer::Instance().GetTotalSeconds() + c_rAttackData.fInvisibleTime;
	}
	else
	{
		rVictim.m_fInvisibleTime = DX::StepTimer::Instance().GetTotalSeconds() + c_rAttackData.fInvisibleTime;
	}

	// Stiffen Time
	rVictim.InsertDelay(c_rAttackData.fStiffenTime);

	// Hit Effect
	Vector3 vec3Effect(rVictim.m_x, rVictim.m_y, rVictim.m_z);

	extern bool IS_HUGE_RACE(unsigned int vnum);
	if (IS_HUGE_RACE(rVictim.GetRace()))
	{
		vec3Effect = c_rv3Position;
	}

	const Vector3 & v3Pos = GetPosition();

	float fHeight = ToDegree(atan2(-vec3Effect.x + v3Pos.x,+vec3Effect.y - v3Pos.y));

	if (rVictim.IsBuilding()||rVictim.IsDoor())
	{
		Vector3 vec3Delta=vec3Effect-v3Pos;
		Vec3Normalize(&vec3Delta, &vec3Delta);
		vec3Delta*=30.0f;

		CEffectManager& rkEftMgr=CEffectManager::Instance();
		if (m_dwBattleHitEffectID)
			rkEftMgr.CreateEffect(m_dwBattleHitEffectID, v3Pos+vec3Delta, Vector3(0.0f, 0.0f, 0.0f));
	}
	else
	{
		CEffectManager& rkEftMgr=CEffectManager::Instance();
		if (m_dwBattleHitEffectID)
			rkEftMgr.CreateEffect(m_dwBattleHitEffectID, vec3Effect, Vector3(0.0f, 0.0f, fHeight));
		if (m_dwBattleAttachEffectID)
			rVictim.AttachEffectByID(0, NULL, m_dwBattleAttachEffectID);
	}

	if (rVictim.IsBuilding())
	{
	}
	else if (rVictim.IsStone() || rVictim.IsDoor())
	{
		__HitStone(rVictim);
	}
	else
	{
		///////////
		// Motion
		if (NRaceData::HIT_TYPE_GOOD == c_rAttackData.iHittingType || rVictim.IsResistFallen())
		{
			__HitGood(rVictim);
		}
		else if (NRaceData::HIT_TYPE_GREAT == c_rAttackData.iHittingType)
		{
			__HitGreate(rVictim);
		}
		else
		{
			TraceError("ProcessSucceedingAttacking: Unknown AttackingData.iHittingType %d", c_rAttackData.iHittingType);
		}
	}

	__OnHit(uiSkill, rVictim, isSendPacket);
}

void CActorInstance::OnShootDamage()
{
	if (IsStun())
	{
		Die();
	}
	else
	{
		__Shake(100);

		if (!isLock() && !__IsKnockDownMotion() && !__IsStandUpMotion())
		{
			if (InterceptOnceMotion(CRaceMotionData::NAME_DAMAGE))
				PushLoopMotion(CRaceMotionData::NAME_WAIT);
		}
	}
}

void CActorInstance::__Shake(DWORD dwDuration)
{
	DWORD dwCurTime=ELTimer_GetMSec();
	m_dwShakeTime=dwCurTime+dwDuration;
}

void CActorInstance::ShakeProcess()
{
	if (m_dwShakeTime)
	{
		Vector3 v3Pos(0.0f, 0.0f, 0.0f);

		DWORD dwCurTime=ELTimer_GetMSec();

		if (m_dwShakeTime<dwCurTime)
		{
			m_dwShakeTime=0;
		}
		else
		{
			int nShakeSize=10;

			switch (rand()%2)
			{
				case 0:v3Pos.x+=rand()%nShakeSize;break;
				case 1:v3Pos.x-=rand()%nShakeSize;break;
			}

			switch (rand()%2)
			{
				case 0:v3Pos.y+=rand()%nShakeSize;break;
				case 1:v3Pos.y-=rand()%nShakeSize;break;
			}

			switch (rand()%2)
			{
				case 0:v3Pos.z+=rand()%nShakeSize;break;
				case 1:v3Pos.z-=rand()%nShakeSize;break;
			}
		}

		m_worldMatrix._41	+= v3Pos.x;
		m_worldMatrix._42	+= v3Pos.y;
		m_worldMatrix._43	+= v3Pos.z;
	}
}

void CActorInstance::__HitStone(CActorInstance& rVictim)
{
	if (rVictim.IsStun())
	{
		rVictim.Die();
	}
	else
	{
		rVictim.__Shake(100);
	}
}

void CActorInstance::__HitGood(CActorInstance& rVictim)
{
	if (rVictim.IsKnockDown())
		return;

	if (rVictim.IsStun())
	{
		rVictim.Die();
	}
	else
	{
		rVictim.__Shake(100);

		if (!rVictim.isLock())
		{
			float fRotRad = ToRadian(GetRotation());
			float fVictimRotRad = ToRadian(rVictim.GetRotation());

			Vector2 v2Normal(sin(fRotRad), cos(fRotRad));
			Vector2 v2VictimNormal(sin(fVictimRotRad), cos(fVictimRotRad));

			Vec2Normalize(&v2Normal, &v2Normal);
			Vec2Normalize(&v2VictimNormal, &v2VictimNormal);

			float fScalar = Vec2Dot(&v2Normal, &v2VictimNormal);

			if (fScalar < 0.0f)
			{
				if (rVictim.InterceptOnceMotion(CRaceMotionData::NAME_DAMAGE))
					rVictim.PushLoopMotion(CRaceMotionData::NAME_WAIT);
			}
			else
			{
				if (rVictim.InterceptOnceMotion(CRaceMotionData::NAME_DAMAGE_BACK))
					rVictim.PushLoopMotion(CRaceMotionData::NAME_WAIT);
				else if (rVictim.InterceptOnceMotion(CRaceMotionData::NAME_DAMAGE))
					rVictim.PushLoopMotion(CRaceMotionData::NAME_WAIT);
			}
		}
	}
}

void CActorInstance::__HitGreate(CActorInstance& rVictim)
{
	// DISABLE_KNOCKDOWN_ATTACK
	if (rVictim.IsKnockDown())
		return;
	if (rVictim.__IsStandUpMotion())
		return;
	// END_OF_DISABLE_KNOCKDOWN_ATTACK

	float fRotRad = ToRadian(GetRotation());
	float fVictimRotRad = ToRadian(rVictim.GetRotation());

	Vector2 v2Normal(sin(fRotRad), cos(fRotRad));
	Vector2 v2VictimNormal(sin(fVictimRotRad), cos(fVictimRotRad));

	Vec2Normalize(&v2Normal, &v2Normal);
	Vec2Normalize(&v2VictimNormal, &v2VictimNormal);

	float fScalar = Vec2Dot(&v2Normal, &v2VictimNormal);

	rVictim.__Shake(100);

	if (rVictim.IsUsingSkill())
		return;

	if (rVictim.IsStun())
	{
		if (fScalar < 0.0f)
			rVictim.InterceptOnceMotion(CRaceMotionData::NAME_DAMAGE_FLYING);
		else
		{
			if (!rVictim.InterceptOnceMotion(CRaceMotionData::NAME_DAMAGE_FLYING_BACK))
				rVictim.InterceptOnceMotion(CRaceMotionData::NAME_DAMAGE_FLYING);
		}

		rVictim.m_isRealDead=true;
	}
	else
	{
		if (fScalar < 0.0f)
		{
			if (rVictim.InterceptOnceMotion(CRaceMotionData::NAME_DAMAGE_FLYING))
			{
				rVictim.PushOnceMotion(CRaceMotionData::NAME_STAND_UP);
				rVictim.PushLoopMotion(CRaceMotionData::NAME_WAIT);
			}
		}
		else
		{
			if (!rVictim.InterceptOnceMotion(CRaceMotionData::NAME_DAMAGE_FLYING_BACK))
			{
				if (rVictim.InterceptOnceMotion(CRaceMotionData::NAME_DAMAGE_FLYING))
				{
					rVictim.PushOnceMotion(CRaceMotionData::NAME_STAND_UP);
					rVictim.PushLoopMotion(CRaceMotionData::NAME_WAIT);
				}
			}
			else
			{
				rVictim.PushOnceMotion(CRaceMotionData::NAME_STAND_UP_BACK);
				rVictim.PushLoopMotion(CRaceMotionData::NAME_WAIT);
			}
		}
	}
}

void CActorInstance::SetBlendingPosition(const TPixelPosition & c_rPosition, float fBlendingTime)
{
	//return;
	TPixelPosition Position;

	Position.x = c_rPosition.x - m_x;
	Position.y = c_rPosition.y - m_y;
	Position.z = 0;

	m_PhysicsObject.SetLastPosition(Position, fBlendingTime);
}

void CActorInstance::ResetBlendingPosition()
{
	m_PhysicsObject.Initialize();
}

void CActorInstance::GetBlendingPosition(TPixelPosition * pPosition)
{
	if (m_PhysicsObject.isBlending())
	{
		m_PhysicsObject.GetLastPosition(pPosition);
		pPosition->x += m_x;
		pPosition->y += m_y;
		pPosition->z += m_z;
	}
	else
	{
		pPosition->x = m_x;
		pPosition->y = m_y;
		pPosition->z = m_z;
	}
}

void CActorInstance::__PushCircle(CActorInstance & rVictim)
{
	const TPixelPosition& c_rkPPosAtk=NEW_GetAtkPixelPositionRef();

	Vector3 v3SrcPos(c_rkPPosAtk.x, -c_rkPPosAtk.y, c_rkPPosAtk.z);

	const Vector3& c_rv3SrcPos = v3SrcPos;
	const Vector3& c_rv3DstPos = rVictim.GetPosition();

	Vector3 v3Direction;
	v3Direction.x = c_rv3DstPos.x - c_rv3SrcPos.x;
	v3Direction.y = c_rv3DstPos.y - c_rv3SrcPos.y;
	v3Direction.z = 0.0f;
	Vec3Normalize(&v3Direction, &v3Direction);

	rVictim.__SetFallingDirection(v3Direction.x, v3Direction.y);
}

void CActorInstance::__PushDirect(CActorInstance & rVictim)
{
	Vector3 v3Direction;
	v3Direction.x = cosf(ToRadian(m_fcurRotation + 270.0f));
	v3Direction.y = sinf(ToRadian(m_fcurRotation + 270.0f));
	v3Direction.z = 0.0f;

	rVictim.__SetFallingDirection(v3Direction.x, v3Direction.y);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CActorInstance::__isInvisible()
{
	if (IsDead())
		return true;

	if (DX::StepTimer::Instance().GetTotalSeconds() >= m_fInvisibleTime)
		return false;

	return true;
}

void CActorInstance::__SetFallingDirection(float fx, float fy)
{
	m_PhysicsObject.SetDirection(Vector3(fx, fy, 0.0f));
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
