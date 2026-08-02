#include "Stdafx.h"
#include "../eterLib/GrpMath.h"
#include "../effectLib/EffectManager.h"

#include "MapManager.h"

#include "FlyingData.h"
#include "FlyTrace.h"
#include "FlyingInstance.h"
#include "FlyingObjectManager.h"
#include "FlyTarget.h"
#include "FlyHandler.h"

#include "../EterBase/StepTimer.h"

CDynamicPool<CFlyingInstance> CFlyingInstance::ms_kPool;

CFlyingInstance::CFlyingInstance()
{
	__Initialize();
}

CFlyingInstance::~CFlyingInstance()
{
	Destroy();
}

void CFlyingInstance::__Initialize()
{
	m_qAttachRotation=m_qRot=Quaternion(0.0f, 0.0f, 0.0f, 0.0f);
	m_v3Accel=m_v3LocalVelocity=m_v3Velocity=m_v3Position=Vector3(0.0f, 0.0f, 0.0f);

	m_pHandler=NULL;
	m_pData=NULL;
	m_pOwner=NULL;

	m_bAlive=false;
	m_canAttack=false;

	m_dwSkillIndex = 0;

	m_iPierceCount=0;

	m_fStartTime=0.0f;
	m_fLastTime = 0.0f;
	m_fRemainRange=0.0f;

	m_bTargetHitted = FALSE;
	m_HittedObjectSet.clear();
}

void CFlyingInstance::Clear()
{
	Destroy();
}

void CFlyingInstance::Destroy()
{
	m_FlyTarget.Clear();

	ClearAttachInstance();

	__Initialize();
}

void CFlyingInstance::BuildAttachInstance()
{
	for(int i=0;i<m_pData->GetAttachDataCount();i++)
	{
		CFlyingData::TFlyingAttachData & rfad = m_pData->GetAttachDataReference(i);

		switch(rfad.iType)
		{
			case CFlyingData::FLY_ATTACH_OBJECT:
				// NOT Implemented
				assert(!"FlyingInstance.cpp:BuildAttachInstance Not implemented FLY_ATTACH_OBJECT");
				break;
			case CFlyingData::FLY_ATTACH_EFFECT:
				{
					CEffectManager & rem = CEffectManager::Instance();
					TAttachEffectInstance aei;

					StringPath(rfad.strFilename); //@fixme030
					DWORD dwCRC = GetCaseCRC32(rfad.strFilename.c_str(),rfad.strFilename.size());

					aei.iAttachIndex = i;
					aei.dwEffectInstanceIndex = rem.GetEmptyIndex();

					aei.pFlyTrace = NULL;
					if (rfad.bHasTail)
					{
						aei.pFlyTrace = CFlyTrace::New();
						aei.pFlyTrace->Create(rfad);
					}
					rem.CreateEffectInstance(aei.dwEffectInstanceIndex,dwCRC);

					m_vecAttachEffectInstance.push_back(aei);
				}
				break;
		}
	}
}

void CFlyingInstance::Create(CFlyingData* pData, const Vector3& c_rv3StartPos, const CFlyTarget & c_rkTarget, bool canAttack)
{
	m_FlyTarget = c_rkTarget;
	m_canAttack = canAttack;

	__SetDataPointer(pData, c_rv3StartPos);
	__SetTargetDirection(m_FlyTarget);
}

void CFlyingInstance::__SetTargetDirection(const CFlyTarget& c_rkTarget)
{
	Vector3 v3TargetPos=c_rkTarget.GetFlyTargetPosition();

	if (m_pData->m_bMaintainParallel)
	{
		v3TargetPos.z += 50.0f;
	}

	Vector3 v3TargetDir=v3TargetPos-m_v3Position;

	Vec3Normalize(&v3TargetDir, &v3TargetDir);
	__SetTargetNormalizedDirection(v3TargetDir);
}

void CFlyingInstance::__SetTargetNormalizedDirection(const Vector3 & v3NomalizedDirection)
{
	Quaternion q = SafeRotationNormalizedArc(Vector3(0.0f,-1.0f,0.0f),v3NomalizedDirection);
	QuaternionMultiply(&m_qRot,&m_qRot,&q);
	Vec3TransformQuaternion(&m_v3Velocity,&m_v3LocalVelocity,&m_qRot);
	Vec3TransformQuaternion(&m_v3Accel, &m_pData->m_v3Accel, &m_qRot);
}

void CFlyingInstance::SetFlyTarget(const CFlyTarget & cr_Target)
{
	//m_pFlyTarget = pTarget;
	m_FlyTarget = cr_Target;
	//SetStartTargetPosition(m_FlyTarget.GetFlyTargetPosition());

	__SetTargetDirection(m_FlyTarget);
}

void CFlyingInstance::AdjustDirectionForHoming(const Vector3 & v3TargetPosition)
{
	Vector3 vTargetDir(v3TargetPosition);
	vTargetDir -= m_v3Position;
	Vec3Normalize(&vTargetDir,&vTargetDir);
	Vector3 vVel;
	Vec3Normalize(&vVel, &m_v3Velocity);

	auto val = (vVel - vTargetDir);
	if (Vec3LengthSq(&val) < 0.001f)
		return;

	Quaternion q = SafeRotationNormalizedArc(vVel,vTargetDir);

	if (m_pData->m_fHomingMaxAngle > 180)
	{
		Vec3TransformQuaternionSafe(&m_v3Velocity, &m_v3Velocity, &q);
		Vec3TransformQuaternionSafe(&m_v3Accel, &m_v3Accel, &q);
		QuaternionMultiply(&m_qRot, &q, &m_qRot);
		return;
	}

	float c = cosf(ToRadian(m_pData->m_fHomingMaxAngle));
	float s = sinf(ToRadian(m_pData->m_fHomingMaxAngle));

	if (q.w <= -1.0f + 0.0001f)
	{
		q.x = 0;
		q.y = 0;
		q.z = s;
		q.w = c;
	}
	else if (q.w <= c && q.w <= 1.0f - 0.0001f)
	{
		float factor = s / sqrtf(1.0f - q.w * q.w);
		q.x *= factor;
		q.y *= factor;
		q.z *= factor;
		q.w = c;
	}
	/*else
	{
	}*/
	Vec3TransformQuaternionSafe(&m_v3Velocity, &m_v3Velocity, &q);
	Vec3TransformQuaternionSafe(&m_v3Accel, &m_v3Accel, &q);
	QuaternionMultiply(&m_qRot, &m_qRot, &q);
}

void CFlyingInstance::UpdateAttachInstance(float fElapsedTime)
{
	// Update Attach Rotation
	Quaternion q;
	QuaternionRotationYawPitchRoll(&q,
		ToRadian(m_pData->m_v3AngVel.y) * fElapsedTime,
		ToRadian(m_pData->m_v3AngVel.x) * fElapsedTime,
		ToRadian(m_pData->m_v3AngVel.z) * fElapsedTime);

	QuaternionMultiply(&m_qAttachRotation, &m_qAttachRotation, &q);
	QuaternionMultiply(&q, &m_qAttachRotation, &m_qRot);

	CEffectManager & rem = CEffectManager::Instance();
	TAttachEffectInstanceVector::iterator it;
	for(it = m_vecAttachEffectInstance.begin();it!=m_vecAttachEffectInstance.end();++it)
	{
		CFlyingData::TFlyingAttachData & rfad = m_pData->GetAttachDataReference(it->iAttachIndex);
		assert(rfad.iType == CFlyingData::FLY_ATTACH_EFFECT);
		rem.SelectEffectInstance(it->dwEffectInstanceIndex);
		Matrix m;
		switch(rfad.iFlyType)
		{
			case CFlyingData::FLY_ATTACH_TYPE_LINE:
				MatrixRotationQuaternion(&m,&m_qRot);
				//MatrixRotationQuaternion(&m,&q);
				m._41=m_v3Position.x;
				m._42=m_v3Position.y;
				m._43=m_v3Position.z;
				break;
			case CFlyingData::FLY_ATTACH_TYPE_MULTI_LINE:
				{
					Vector3 p(
						-sinf(ToRadian(rfad.fRoll))*rfad.fDistance,
						0.0f,
						-cosf(ToRadian(rfad.fRoll))*rfad.fDistance);
					//Vec3TransformQuaternionSafe(&p,&p,&m_qRot);
					Vec3TransformQuaternionSafe(&p,&p,&q);
					p+=m_v3Position;
					//MatrixRotationQuaternion(&m,&m_qRot);
					MatrixRotationQuaternion(&m,&q);
					m._41=p.x;
					m._42=p.y;
					m._43=p.z;
				}
				break;
			case CFlyingData::FLY_ATTACH_TYPE_SINE:
				{
				float angle = (DX::StepTimer::Instance().GetTotalSeconds() - m_fStartTime) * 2 * 3.1415926535897931f / rfad.fPeriod;
					Vector3 p(
						-sinf(ToRadian(rfad.fRoll))*rfad.fAmplitude*sinf(angle),
						0.0f,
						-cosf(ToRadian(rfad.fRoll))*rfad.fAmplitude*sinf(angle));
					Vec3TransformQuaternionSafe(&p,&p,&q);
					//Vec3TransformQuaternionSafe(&p,&p,&m_qRot);
					p+=m_v3Position;
					MatrixRotationQuaternion(&m,&q);
					//MatrixRotationQuaternion(&m,&m_qRot);
					m._41=p.x;
					m._42=p.y;
					m._43=p.z;
					//assert(!"NOT IMPLEMENTED");
				}
				break;
			case CFlyingData::FLY_ATTACH_TYPE_EXP:
				{
				float dt = DX::StepTimer::Instance().GetTotalSeconds() - m_fStartTime;
					float angle = dt/rfad.fPeriod;
					Vector3 p(
						-sinf(ToRadian(rfad.fRoll))*rfad.fAmplitude*exp(-angle)*angle,
						0.0f,
						-cosf(ToRadian(rfad.fRoll))*rfad.fAmplitude*exp(-angle)*angle);
					//Vec3TransformQuaternionSafe(&p,&p,&m_qRot);
					Vec3TransformQuaternionSafe(&p,&p,&q);
					p+=m_v3Position;
					MatrixRotationQuaternion(&m,&q);
					//MatrixRotationQuaternion(&m,&m_qRot);
					m._41=p.x;
					m._42=p.y;
					m._43=p.z;
					//assert(!"NOT IMPLEMENTED");
				}
				break;
		}
		rem.SetEffectInstanceGlobalMatrix(m);
		if (it->pFlyTrace)
			it->pFlyTrace->UpdateNewPosition(Vector3(m._41,m._42,m._43));
	}
}
struct FCheckBackgroundDuringFlying {
	CDynamicSphereInstance s;
	bool bHit;
	FCheckBackgroundDuringFlying(const Vector3 & v1, const Vector3 & v2)
	{
		s.fRadius = 1.0f;
		s.v3LastPosition = v1;
		s.v3Position = v2;
		bHit = false;
	}
	void operator () (CGraphicObjectInstance * p)
	{
		if (!p)
			return;

		if (!bHit && p->GetType() != ACTOR_OBJECT)
		{
			if (p->CollisionDynamicSphere(s))
			{
				bHit = true;
			}
		}
	}
	bool IsHitted()
	{
		return bHit;
	}
};

struct FCheckAnotherMonsterDuringFlying {
	CDynamicSphereInstance s;
	CGraphicObjectInstance * pInst;
	const IActorInstance * pOwner;
	FCheckAnotherMonsterDuringFlying(const IActorInstance * pOwner, const Vector3 & v1, const Vector3 & v2)
		: pOwner(pOwner)
	{
		s.fRadius = 10.0f;
		s.v3LastPosition = v1;
		s.v3Position = v2;
		pInst = 0;
	}
	void operator () (CGraphicObjectInstance * p)
	{
		if (!p)
			return;

		if (!pInst && p->GetType() == ACTOR_OBJECT)
		{
			IActorInstance * pa = (IActorInstance*) p;
			if (pa != pOwner && pa->TestCollisionWithDynamicSphere(s))
			{
				pInst = p;
			}
		}
	}
	bool IsHitted()
	{
		return pInst!=0;
	}
	CGraphicObjectInstance * GetHittedObject()
	{
		return pInst;
	}
};

bool CFlyingInstance::Update()
{

	float fElapsedTime = float(DX::StepTimer::Instance().GetTotalSeconds() - m_fLastTime);
	m_fLastTime = DX::StepTimer::Instance().GetTotalSeconds();

	if (!m_bAlive)
		return false;

	if (m_pData->m_bIsHoming &&
		m_pData->m_fHomingStartTime + m_fStartTime < DX::StepTimer::Instance().GetTotalSeconds())
	{
		if (m_FlyTarget.IsObject())
			AdjustDirectionForHoming(m_FlyTarget.GetFlyTargetPosition());
	}

	Vector3 v3LastPosition = m_v3Position;

	m_v3Velocity += m_v3Accel * fElapsedTime;
	m_v3Velocity.z += m_pData->m_fGravity * fElapsedTime;
	Vector3 v3Movement = m_v3Velocity * fElapsedTime;
	float _fMoveDistance = Vec3Length(&v3Movement);
	float fCollisionSphereRadius = max(_fMoveDistance*2, m_pData->m_fCollisionSphereRadius);
	m_fRemainRange -= _fMoveDistance;
	m_v3Position += v3Movement;

	UpdateAttachInstance(fElapsedTime);

	if (m_fRemainRange<0)
	{
		if (m_pHandler)
			m_pHandler->OnExplodingOutOfRange();

		__Explode(false);
		return false;
	}

	if (m_FlyTarget.IsObject())
	{
		if (!m_bTargetHitted)
		{
			if (square_distance_between_linesegment_and_point(m_v3Position,v3LastPosition,m_FlyTarget.GetFlyTargetPosition())<m_pData->m_fBombRange*m_pData->m_fBombRange)
			{
				m_bTargetHitted = TRUE;

				if (m_canAttack)
				{
					IFlyTargetableObject* pVictim=m_FlyTarget.GetFlyTarget();
					if (pVictim)
					{
						pVictim->OnShootDamage();
					}
				}

				if (m_pHandler)
				{
					m_pHandler->OnExplodingAtTarget(m_dwSkillIndex);
				}

				if (m_iPierceCount)
				{
					m_iPierceCount--;
					__Bomb();
				}
				else
				{
					__Explode();
					return false;
				}

				return true;
			}
		}
	}
	else if (m_FlyTarget.IsPosition())
	{
		if (square_distance_between_linesegment_and_point(m_v3Position,v3LastPosition,m_FlyTarget.GetFlyTargetPosition())<m_pData->m_fBombRange*m_pData->m_fBombRange)
		{
			__Explode();
			return false;
		}
	}

	Vector3d vecStart, vecDir;
	vecStart.Set(v3LastPosition.x,v3LastPosition.y,v3LastPosition.z);
	vecDir.Set(v3Movement.x,v3Movement.y,v3Movement.z);

	CCullingManager & rkCullingMgr = CCullingManager::Instance();

	if (m_pData->m_bHitOnAnotherMonster)
	{
		FCheckAnotherMonsterDuringFlying kCheckAnotherMonsterDuringFlying(m_pOwner, v3LastPosition,m_v3Position);
		rkCullingMgr.ForInRange(vecStart,fCollisionSphereRadius, &kCheckAnotherMonsterDuringFlying);
		if (kCheckAnotherMonsterDuringFlying.IsHitted())
		{
			IActorInstance * pHittedInstance = (IActorInstance*)kCheckAnotherMonsterDuringFlying.GetHittedObject();
			if (m_HittedObjectSet.end() == m_HittedObjectSet.find(pHittedInstance))
			{
				m_HittedObjectSet.insert(pHittedInstance);

				if (m_pHandler)
				{
					m_pHandler->OnExplodingAtAnotherTarget(m_dwSkillIndex, pHittedInstance->GetVirtualID());
				}

				if (m_iPierceCount)
				{
					m_iPierceCount--;
					__Bomb();
				}
				else
				{
					__Explode();
					return false;
				}

				return true;
			}
		}
	}

	if (m_pData->m_bHitOnBackground)
	{
		if (CFlyingManager::Instance().GetMapManagerPtr())
		{
			float fGroundHeight = CFlyingManager::Instance().GetMapManagerPtr()->GetTerrainHeight(m_v3Position.x,-m_v3Position.y);
			if (fGroundHeight>m_v3Position.z)
			{
				if (m_pHandler)
					m_pHandler->OnExplodingAtBackground();

				__Explode();
				return false;
			}
		}

		FCheckBackgroundDuringFlying kCheckBackgroundDuringFlying(v3LastPosition,m_v3Position);
		rkCullingMgr.ForInRange(vecStart,fCollisionSphereRadius, &kCheckBackgroundDuringFlying);

		if (kCheckBackgroundDuringFlying.IsHitted())
		{
			if (m_pHandler)
				m_pHandler->OnExplodingAtBackground();

			__Explode();
			return false;
		}
	}

	return true;
}

void CFlyingInstance::ClearAttachInstance()
{
	CEffectManager & rkEftMgr = CEffectManager::Instance();

	TAttachEffectInstanceVector::iterator i;
	for(i = m_vecAttachEffectInstance.begin();i!=m_vecAttachEffectInstance.end();++i)
	{
		rkEftMgr.DestroyEffectInstance(i->dwEffectInstanceIndex);

		if (i->pFlyTrace)
			CFlyTrace::Delete(i->pFlyTrace);

		i->iAttachIndex=0;
		i->dwEffectInstanceIndex=0;
		i->pFlyTrace=NULL;
	}
	m_vecAttachEffectInstance.clear();
}

void CFlyingInstance::__Explode(bool bBomb)
{
	if (!m_bAlive)
		return;

	m_bAlive = false;

	if (bBomb)
		__Bomb();
}

void CFlyingInstance::__Bomb()
{
	CEffectManager & rkEftMgr = CEffectManager::Instance();
	if (!m_pData->m_dwBombEffectID)
		return;

	DWORD dwEmptyIndex = rkEftMgr.GetEmptyIndex();
	rkEftMgr.CreateEffectInstance(dwEmptyIndex,m_pData->m_dwBombEffectID);

	Matrix m;
//	MatrixRotationQuaternion(&m,&m_qRot);
	MatrixIdentity(&m);
	m._41 = m_v3Position.x;
	m._42 = m_v3Position.y;
	m._43 = m_v3Position.z;
	rkEftMgr.SelectEffectInstance(dwEmptyIndex);
	rkEftMgr.SetEffectInstanceGlobalMatrix(m);
}

void CFlyingInstance::Render()
{
	if (!m_bAlive)
		return;
	RenderAttachInstance();
}

void CFlyingInstance::RenderAttachInstance()
{
	TAttachEffectInstanceVector::iterator it;
	for(it = m_vecAttachEffectInstance.begin();it!=m_vecAttachEffectInstance.end();++it)
	{
		if (it->pFlyTrace)
			it->pFlyTrace->Render();
	}
}

void CFlyingInstance::SetDataPointer(CFlyingData * pData, const Vector3 & v3StartPosition)
{
	__SetDataPointer(pData, v3StartPosition);
}

void CFlyingInstance::__SetDataPointer(CFlyingData * pData, const Vector3 & v3StartPosition)
{
	m_pData = pData;
	m_qRot = Quaternion(0.0f,0.0f,0.0f,1.0f),
	m_v3Position = (v3StartPosition);
	m_bAlive = (true);

	m_fStartTime = DX::StepTimer::Instance().GetTotalSeconds();
	m_fLastTime = DX::StepTimer::Instance().GetTotalSeconds();

	QuaternionRotationYawPitchRoll(&m_qRot,ToRadian(pData->m_fRollAngle-90.0f),0.0f,ToRadian(pData->m_fConeAngle));
	if (pData->m_bSpreading)
	{
		Quaternion q1, q2;
		auto val1 = Vector3(0.0f,0.0f,1.0f);
		QuaternionRotationAxis(&q2, &val1,(frandom(-3.141592f/3,+3.141592f/3)+frandom(-3.141592f/3,+3.141592f/3))/2);
		auto val2 = Vector3(0.0f,-1.0f,0.0f);
		QuaternionRotationAxis(&q1, &val2, frandom(0,2*3.1415926535897931f));
		QuaternionMultiply(&q1,&q2,&q1);
		QuaternionMultiply(&m_qRot,&q1,&m_qRot);
	}
	m_v3Velocity = m_v3LocalVelocity = Vector3(0.0f,-pData->m_fInitVel,0.0f);
	m_v3Accel = pData->m_v3Accel;
	m_fRemainRange = pData->m_fRange;
	m_qAttachRotation = Quaternion(0.0f,0.0f,0.0f,1.0f);

	BuildAttachInstance();
	UpdateAttachInstance(0.0f);

	m_iPierceCount = pData->m_iPierceCount;
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
