#pragma once

#include "GrpColorInstance.h"
#include "GrpScreen.h"
#include "CullingManager.h"
#include "CollisionData.h"
#include "AttributeInstance.h"

enum
{
	THING_OBJECT   = 0xadf21f13,
	TREE_OBJECT    = 0x8ac9f7a6,
	ACTOR_OBJECT   = 0x29a76c24,
	EFFECT_OBJECT  = 0x1cfa97c6,
	DUNGEON_OBJECT = 0x18326035,
};

enum
{
	PORTAL_ID_MAX_NUM = 8,
};

class CGraphicObjectInstance : public CGraphicCollisionObject
{
	public:
		CGraphicObjectInstance();
		virtual ~CGraphicObjectInstance();
		virtual int GetType() const = 0;

	public:
		const Vector3 &		GetPosition() const;
		const Vector3 &		GetScale() const;
		float					GetRotation();
		float					GetYaw();
		float					GetPitch();
		float					GetRoll();

		void					SetPosition(float x, float y, float z);
		void					SetPosition(const Vector3& rv3Pos);
		void					SetScale(float x, float y, float z);
		void					SetRotation(float fRotation);
		void					SetRotation(float fYaw, float fPitch, float fRoll);
		void					SetRotationQuaternion(const Quaternion &q);
		void					SetRotationMatrix(const Matrix & m);

		void					Clear();
		void					Update();
		bool					Render();
		void					BlendRender();
		void					RenderToShadowMap();
		void					RenderShadow();
		void					RenderPCBlocker();
		void					Deform();
		void					Transform();

		void					Show();
		void					Hide();
		bool					isShow();

#ifdef __ENABLE_STEALTH_FIX__
		void					ApplyAlwaysHidden();
		void					ReleaseAlwaysHidden();
#endif

		// Camera Block
		void					BlockCamera(bool bBlock) {m_BlockCamera = bBlock;}
		bool					BlockCamera() { return m_BlockCamera; }

		// Ray Test
		bool					isIntersect(const CRay & c_rRay, float * pu, float * pv, float * pt);

		// Bounding Box
		Vector4 &			GetWTBBoxVertex(const unsigned char & c_rucNumTBBoxVertex);
		Vector3 &			GetTBBoxMin() { return m_v3TBBoxMin; }
		Vector3 &			GetTBBoxMax() { return m_v3TBBoxMax; }
		Vector3 &			GetBBoxMin() { return m_v3BBoxMin; }
		Vector3 &			GetBBoxMax() { return m_v3BBoxMax; }

		// Matrix
 		Matrix &			GetMatrix();
		const Matrix&		GetWorldMatrix() { return m_worldMatrix; }

		// Portal
		void					SetPortal(DWORD dwIndex, int iID);
		int						GetPortal(DWORD dwIndex);

		// Initialize
		void					Initialize();
		virtual void			OnInitialize();

	// Bounding Sphere
	public:
		void					UpdateBoundingSphere();
		void					RegisterBoundingSphere();
		virtual bool			GetBoundingSphere(Vector3 & v3Center, float & fRadius) = 0;

#ifdef ENABLE_FRUSTUM_CULLING
		float					GetBoundingSphereRadius() const { return m_fBoundingSphereRadius; }
		void					GetBoundingSpherePosition(Vector3* pv3Position) const { *pv3Position = m_v3Position; }
		void					SetBoundingSphereRadius(float fRadius) { m_fBoundingSphereRadius = fRadius; }
#endif

		virtual void			OnRender() = 0;
		virtual void			OnBlendRender() = 0;
		virtual void			OnRenderToShadowMap() = 0;
		virtual void			OnRenderShadow() = 0;
		virtual void			OnRenderPCBlocker() = 0;
		virtual void			OnClear(){}
		virtual void			OnUpdate(){}
		virtual void			OnDeform(){}

	protected:
		Vector3				m_v3Position;
		Vector3				m_v3Scale;

		float					m_fYaw;
		float					m_fPitch;
		float					m_fRoll;

		Matrix				m_mRotation;

		bool					m_isVisible;
#ifdef __ENABLE_STEALTH_FIX__
		bool					m_isAlwaysHidden;
#endif

		// Matrix caching - only recalculate when dirty
		bool					m_bMatrixDirty;
		Matrix				m_worldMatrix;

		// Camera Block
		bool					m_BlockCamera;

		// Bounding Box
		Vector4				m_v4TBBox[8];
		Vector3				m_v3TBBoxMin, m_v3TBBoxMax;
		Vector3				m_v3BBoxMin, m_v3BBoxMax;

		// Portal
		BYTE					m_abyPortalID[PORTAL_ID_MAX_NUM];

		// Culling
		CCullingManager::CullingHandle	m_CullingHandle;

#ifdef ENABLE_FRUSTUM_CULLING
		float					m_fBoundingSphereRadius;
#endif

	// Static Collision Data
	public:
		void					AddCollision(const CStaticCollisionData * pscd, const Matrix * pMat);
		void					ClearCollision();
		bool					CollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		bool					MovementCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		Vector3				GetCollisionMovementAdjust(const CDynamicSphereInstance & s) const;

		void					UpdateCollisionData(const CStaticCollisionDataVector * pscdVector = 0);

	protected:
		CCollisionInstanceVector	m_StaticCollisionInstanceVector;
		virtual void				OnUpdateCollisionData(const CStaticCollisionDataVector * pscdVector) = 0;

	// using in WorldEditor
	public:
		DWORD						GetCollisionInstanceCount();
		CBaseCollisionInstance *	GetCollisionInstanceData(DWORD dwIndex);

	// Height Data
	public:
		void					SetHeightInstance(CAttributeInstance * pAttributeInstance);
		void					ClearHeightInstance();

		void					UpdateHeightInstance(CAttributeInstance * pAttributeInstance = 0);

		bool					IsObjectHeight();
		bool					GetObjectHeight(float fX, float fY, float * pfHeight);

	protected:
		CAttributeInstance *		m_pHeightAttributeInstance;
		virtual void				OnUpdateHeighInstance(CAttributeInstance * pAttributeInstance) = 0;
		virtual bool				OnGetObjectHeight(float fX, float fY, float * pfHeight) = 0;

#ifdef ENABLE_ACCE_COSTUME_SYSTEM
	public:
		void					SetScaleWorld(float x, float y, float z);
		void					SetAcceScale(float x, float y, float z);
		void					SetAcceScale(const Vector3& rv3Scale, BYTE bRace);

	protected:
		BYTE					m_bAttachedAcceRace{};
		Vector3				m_v3ScaleAcce{};
		Matrix				m_matAbsoluteTrans{};
		Matrix				m_matScale{};
		Matrix				m_matScaleWorld{};
#endif
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
