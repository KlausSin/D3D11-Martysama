#pragma once

#include "../eterlib/GrpBase.h"
#include "../eterLib/Pool.h"
#include "EffectUpdateDecorator.h"
class CParticleProperty;
class CEmitterProperty;

class CParticleInstance
{
	friend class CParticleSystemData;
	friend class CParticleSystemInstance;

	friend class NEffectUpdateDecorator::CBaseDecorator;
	friend class NEffectUpdateDecorator::CAirResistanceDecorator;
	friend class NEffectUpdateDecorator::CGravityDecorator;
	friend class NEffectUpdateDecorator::CRotationDecorator;

	public:
		CParticleInstance();
		~CParticleInstance();

		float GetRadiusApproximation();

		BOOL Update(float fElapsedTime, float fAngle);

	protected:
		Vector3			m_v3StartPosition;

		Vector3			m_v3Position;
		Vector3			m_v3LastPosition;
		Vector3			m_v3Velocity;

		Vector2			m_v2HalfSize;
		Vector2			m_v2Scale;

		float				m_fRotation;
#ifdef WORLD_EDITOR
		Color			m_Color;
#else
		DWORDCOLOR			m_dcColor;
#endif

		BYTE				m_byTextureAnimationType;
		float				m_fLastFrameTime;
		BYTE				m_byFrameIndex;

		float				m_fLifeTime;
		float				m_fLastLifeTime;

		CParticleProperty *	m_pParticleProperty;
		CEmitterProperty *	m_pEmitterProperty;

		float m_fAirResistance;
		float m_fRotationSpeed;
		float m_fGravity;

		NEffectUpdateDecorator::CBaseDecorator * m_pDecorator;
	public:
		static CParticleInstance* New();
		static void DestroySystem();

		void Transform(const Matrix * c_matLocal=NULL);
		void Transform(const Matrix * c_matLocal, const float c_fZRotation);

		TPTVertex * GetParticleMeshPointer();
		DWORD GetColor() const;

		void DeleteThis();

		void Destroy();

	protected:
		void __Initialize();
		TPTVertex		m_ParticleMesh[4];
	public:
		static CDynamicPool<CParticleInstance> ms_kPool;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
