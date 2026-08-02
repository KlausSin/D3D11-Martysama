#pragma once

#include "GrpMathType.h"
#include "GrpStateEnum.h"

// Collision Detection
typedef struct SSphereData
{
	Vector3		v3Position;
	float		fRadius;
} TSphereData;

typedef struct SPlaneData
{
	Vector3 v3Position;
	Vector3 v3Normal;

	Vector3 v3QuadPosition[4];
	Vector3 v3InsideVector[4];
} TPlaneData;

typedef struct SAABBData
{
	Vector3 v3Min;
	Vector3 v3Max;
} TAABBData;

typedef struct SOBBData
{
	Vector3 v3Min;
	Vector3 v3Max;
	Matrix matRot;
} TOBBData;

typedef struct SCylinderData
{
	Vector3 v3Position;
	float fRadius;
	float fHeight;
} TCylinderData;

enum ECollisionType
{
	COLLISION_TYPE_PLANE,
	COLLISION_TYPE_BOX,
	COLLISION_TYPE_SPHERE,
	COLLISION_TYPE_CYLINDER,
	COLLISION_TYPE_AABB,
	COLLISION_TYPE_OBB,
};

struct CDynamicSphereInstance
{
	Vector3 v3Position;
	Vector3 v3LastPosition;

	float fRadius;
};

class CStaticCollisionData
{
public:
	DWORD dwType;
	char szName[32+1];

	Vector3 v3Position;
	float fDimensions[3];
	Quaternion quatRotation;
};

void DestroyCollisionInstanceSystem();

typedef std::vector<CStaticCollisionData> CStaticCollisionDataVector;

/////////////////////////////////////////////
// Base
class CBaseCollisionInstance
{
	public:
		virtual void Render(EFillMode d3dFillMode = FILL_SOLID) = 0;

		bool MovementCollisionDynamicSphere(const CDynamicSphereInstance & s) const
		{
			return OnMovementCollisionDynamicSphere(s);
		}
		bool CollisionDynamicSphere(const CDynamicSphereInstance & s) const
		{
			return OnCollisionDynamicSphere(s);
		}

		Vector3 GetCollisionMovementAdjust(const CDynamicSphereInstance & s) const
		{
			return OnGetCollisionMovementAdjust(s);
		}

		void Destroy();

		static CBaseCollisionInstance * BuildCollisionInstance(const CStaticCollisionData * c_pCollisionData, const Matrix * pMat);

	protected:
		virtual Vector3 OnGetCollisionMovementAdjust(const CDynamicSphereInstance & s) const = 0;
		virtual bool OnMovementCollisionDynamicSphere(const CDynamicSphereInstance & s) const  = 0;
		virtual bool OnCollisionDynamicSphere(const CDynamicSphereInstance & s) const  = 0;
		virtual void OnDestroy() = 0;
};

/////////////////////////////////////////////
// Sphere
class CSphereCollisionInstance : public CBaseCollisionInstance
{
	public:
		TSphereData & GetAttribute();
		const TSphereData & GetAttribute() const;
		virtual void Render(EFillMode d3dFillMode = FILL_SOLID);

	protected:
		void OnDestroy();
		bool OnMovementCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual bool OnCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual Vector3 OnGetCollisionMovementAdjust(const CDynamicSphereInstance & s) const;

	protected:
		TSphereData m_attribute;
};

/////////////////////////////////////////////
// Plane
class CPlaneCollisionInstance : public CBaseCollisionInstance
{
	public:
		TPlaneData & GetAttribute();
		const TPlaneData & GetAttribute() const;
		virtual void Render(EFillMode d3dFillMode = FILL_SOLID);

	protected:
		void OnDestroy();
		bool OnMovementCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual bool OnCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual Vector3 OnGetCollisionMovementAdjust(const CDynamicSphereInstance & s) const;

	protected:
		TPlaneData m_attribute;
};

/////////////////////////////////////////////
// AABB (Aligned Axis Bounding Box)
class CAABBCollisionInstance : public CBaseCollisionInstance
{
	public:
		TAABBData & GetAttribute();
		const TAABBData & GetAttribute() const;
		virtual void Render(EFillMode d3dFillMode = FILL_SOLID);

	protected:
		void OnDestroy();
		bool OnMovementCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual bool OnCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual Vector3 OnGetCollisionMovementAdjust(const CDynamicSphereInstance & s) const;

	protected:
		TAABBData m_attribute;
};

/////////////////////////////////////////////
// OBB
class COBBCollisionInstance : public CBaseCollisionInstance
{
	public:
		TOBBData & GetAttribute();
		const TOBBData & GetAttribute() const;
		virtual void Render(EFillMode d3dFillMode = FILL_SOLID);

	protected:
		void OnDestroy();
		bool OnMovementCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual bool OnCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual Vector3 OnGetCollisionMovementAdjust(const CDynamicSphereInstance & s) const;

	protected:
		TOBBData m_attribute;
};

/////////////////////////////////////////////
// Cylinder
class CCylinderCollisionInstance : public CBaseCollisionInstance
{
	public:
		TCylinderData & GetAttribute();
		const TCylinderData & GetAttribute() const;
		virtual void Render(EFillMode d3dFillMode = FILL_SOLID);

	protected:
		void OnDestroy();
		bool OnMovementCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual bool OnCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual Vector3 OnGetCollisionMovementAdjust(const CDynamicSphereInstance & s) const;

		bool CollideCylinderVSDynamicSphere(const TCylinderData & c_rattribute, const CDynamicSphereInstance & s) const;

	protected:
		TCylinderData m_attribute;
};

typedef std::vector<CSphereCollisionInstance> CSphereCollisionInstanceVector;
typedef std::vector<CDynamicSphereInstance> CDynamicSphereInstanceVector;
typedef std::vector<CBaseCollisionInstance*> CCollisionInstanceVector;
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
