#include "StdAfx.h"
#include "ActorInstance.h"
#include "RaceData.h"

void CActorInstance::__Push(int x, int y)
{
	const Vector3& c_rv3Src=GetPosition();
	const Vector3 c_v3Dst=Vector3(x, -y, c_rv3Src.z);
	const Vector3 c_v3Delta=c_v3Dst-c_rv3Src;

	constexpr auto LoopValue = 100;
	const Vector3 inc = c_v3Delta / LoopValue;

	Vector3 v3Movement(0.0f, 0.0f, 0.0f);

	IPhysicsWorld* pWorld = IPhysicsWorld::GetPhysicsWorld();
	if (!pWorld)
		return;

	for (int i = 0; i < LoopValue; ++i)
	{
		if (pWorld->isPhysicalCollision(c_rv3Src + v3Movement))
		{
			ResetBlendingPosition();
			return;
		}
		v3Movement += inc;
	}

	SetBlendingPosition(c_v3Dst);

	if (IsResistFallen()) // @fixme033 check moved here
		return;

	if (!IsUsingSkill())
	{
		const int len = sqrt(c_v3Delta.x*c_v3Delta.x+c_v3Delta.y*c_v3Delta.y);
		if (len > 150.0f)
		{
			InterceptOnceMotion(CRaceMotionData::NAME_DAMAGE_FLYING);
			PushOnceMotion(CRaceMotionData::NAME_STAND_UP);
		}
	}
}

void CActorInstance::TEMP_Push(int x, int y)
{
	__Push(x, y);
}

bool CActorInstance::__IsSyncing()
{
	if (IsDead())
		return TRUE;

	if (IsStun())
		return TRUE;

	if (IsPushing())
		return TRUE;

	return FALSE;
}

bool CActorInstance::IsPushing()
{
	return m_PhysicsObject.isBlending();
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
