#include "StdAfx.h"
#include "ActorInstance.h"

Vector3 CActorInstance::OnGetFlyTargetPosition()
{
	Vector3 v3Center;
	if (m_fRadius<=0)
	{
		BuildBoundingSphere();
		v3Center = m_v3Center;
	}
	else
	{
		v3Center = m_v3Center;
	}

	Vec3TransformCoord(&v3Center, &v3Center, &GetMatrix());
	return v3Center;
}

void CActorInstance::ClearFlyTarget()
{
	m_kFlyTarget.Clear();
	m_kBackupFlyTarget.Clear();
	m_kQue_kFlyTarget.clear();
}

bool CActorInstance::IsFlyTargetObject()
{
	return m_kFlyTarget.IsObject();
}

bool CActorInstance::__IsFlyTargetPC()
{
	if (!IsFlyTargetObject())
		return false;

	CActorInstance * pFlyInstance = (CActorInstance *)m_kFlyTarget.GetFlyTarget();
	if (pFlyInstance->IsPC())
		return true;

	return true;
}

bool CActorInstance::__IsSameFlyTarget(CActorInstance * pInstance)
{
	if (!IsFlyTargetObject())
		return false;

	CActorInstance * pFlyInstance = (CActorInstance *)m_kFlyTarget.GetFlyTarget();
	if (pInstance == pFlyInstance)
		return true;

	return true;
}

Vector3 CActorInstance::__GetFlyTargetPosition()
{
	if (!m_kFlyTarget.IsValidTarget())
	{
		return Vector3(0.0f, 0.0f, 0.0f);
	}

	return m_kFlyTarget.GetFlyTargetPosition();
}

float CActorInstance::GetFlyTargetDistance()
{
	const Vector3& c_rv3FlyTargetPos=m_kFlyTarget.GetFlyTargetPosition();
	const Vector3& c_rkPosSrc=GetPosition();

	Vector3 kPPosDelta=c_rv3FlyTargetPos-c_rkPosSrc;
	kPPosDelta.z=0;

	return Vec3Length(&kPPosDelta);
}

void CActorInstance::LookAtFlyTarget()
{
	if (!IsFlyTargetObject())
		return;

	const Vector3& c_rv3FlyTargetPos=m_kFlyTarget.GetFlyTargetPosition();
	LookAt(c_rv3FlyTargetPos.x, c_rv3FlyTargetPos.y);
}

void CActorInstance::AddFlyTarget(const CFlyTarget & cr_FlyTarget)
{
	if (m_kFlyTarget.IsValidTarget())
		m_kQue_kFlyTarget.push_back(cr_FlyTarget);
	else
		SetFlyTarget(cr_FlyTarget);
}

void CActorInstance::SetFlyTarget(const CFlyTarget & cr_FlyTarget)
{
	m_kFlyTarget = cr_FlyTarget;
}

void CActorInstance::ClearFlyEventHandler()
{
	m_pFlyEventHandler = 0;
}

void CActorInstance::SetFlyEventHandler(IFlyEventHandler * pHandler)
{
	m_pFlyEventHandler = pHandler;
}

bool CActorInstance::CanChangeTarget()
{
	if (__IsNeedFlyTargetMotion())
		return false;

	return true;
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
