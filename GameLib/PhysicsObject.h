#pragma once

#include "MapUtil.h"

class IPhysicsWorld
{
public:
	IPhysicsWorld()
	{
		assert(ms_pWorld == NULL);
		ms_pWorld = this;
	}

	virtual ~IPhysicsWorld()
	{
		if (this == ms_pWorld)
			ms_pWorld = NULL;
	}

	static IPhysicsWorld* GetPhysicsWorld()
	{
		return ms_pWorld;
	}

	virtual bool isPhysicalCollision(const Vector3 & c_rvCheckPosition) = 0;

private:
	static IPhysicsWorld* ms_pWorld;
};

class CActorInstance;
class IObjectManager
{
public:
	IObjectManager()
	{
		assert(ms_ObjManager == NULL);
		ms_ObjManager = this;
	}

	virtual ~IObjectManager()
	{
		if (this == ms_ObjManager)
			ms_ObjManager = NULL;
	}

	static IObjectManager* GetObjectManager()
	{
		return ms_ObjManager;
	}

	virtual void AdjustCollisionWithOtherObjects(CActorInstance* pInst ) = 0;

private:
	static IObjectManager* ms_ObjManager;
};

class CPhysicsObject
{
	public:
		CPhysicsObject();
		virtual ~CPhysicsObject();

		void Initialize();

		void Update(float fElapsedTime);

		bool isBlending();

		void SetDirection(const Vector3 & c_rv3Direction);
		void IncreaseExternalForce(const Vector3 & c_rvBasePosition, float fForce);
		void SetLastPosition(const TPixelPosition & c_rPosition, float fBlendingTime);
		void GetLastPosition(TPixelPosition * pPosition);

		float GetXMovement();
		float GetYMovement();
		void ClearSavingMovement();

		void SetActorInstance( CActorInstance* pInst ) { m_pActorInstance = pInst; }
		CActorInstance* GetActorInstance() { return m_pActorInstance; }

	protected:
		void Accumulate(Vector3 * pv3Position);

	protected:
		float m_fMass;
		float m_fFriction;
		Vector3 m_v3Direction;
		Vector3 m_v3Acceleration;
		Vector3 m_v3Velocity;

		Vector3 m_v3LastPosition;
		CEaseOutInterpolation m_xPushingPosition;
		CEaseOutInterpolation m_yPushingPosition;

		CActorInstance*	m_pActorInstance;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
