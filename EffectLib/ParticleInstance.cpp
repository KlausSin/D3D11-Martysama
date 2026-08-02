#include "StdAfx.h"
#include "ParticleInstance.h"
#include "ParticleProperty.h"

#include "../eterBase/Random.h"
#include "../eterLib/Camera.h"
#include "../eterLib/ShaderManager.h"
#include "../eterLib/ShaderInit.h"

CDynamicPool<CParticleInstance> CParticleInstance::ms_kPool;

using namespace NEffectUpdateDecorator;

void CParticleInstance::DestroySystem()
{
	ms_kPool.Destroy();
}

CParticleInstance* CParticleInstance::New()
{
	return ms_kPool.Alloc();
}

void CParticleInstance::DeleteThis()
{
	Destroy();

	ms_kPool.Free(this);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float CParticleInstance::GetRadiusApproximation()
{
	return m_v2HalfSize.y*m_v2Scale.y + m_v2HalfSize.x*m_v2Scale.x;
}

BOOL CParticleInstance::Update(float fElapsedTime, float fAngle)
{
	m_fLastLifeTime -= fElapsedTime;
	if (m_fLastLifeTime < 0.0f)
		return FALSE;

	float fLifePercentage = (m_fLifeTime - m_fLastLifeTime) / m_fLifeTime;

	m_pDecorator->Excute(CDecoratorData(fLifePercentage,fElapsedTime,this));

	m_v3LastPosition = m_v3Position;
	m_v3Position += m_v3Velocity * fElapsedTime;

	if (fAngle)
	{
		if (m_pParticleProperty->m_bAttachFlag)
		{
			float fCos, fSin;
			fAngle = ToRadian(fAngle);
			fCos = cos(fAngle);
			fSin = sin(fAngle);

			float rx = m_v3Position.x - m_v3StartPosition.x;
			float ry = m_v3Position.y - m_v3StartPosition.y;

			m_v3Position.x =   fCos * rx + fSin * ry + m_v3StartPosition.x;
			m_v3Position.y = - fSin * rx + fCos * ry + m_v3StartPosition.y;
		}
		else
		{
			float fRad = ToRadian(fAngle);
			float fCosAngle = cosf(fRad);
			float fSinAngle = sinf(fRad);
			float fOneMinusCos = 1.0f - fCosAngle;

			// Vector to rotate: v = position - start
			Vector3 v(
				m_v3Position.x - m_v3StartPosition.x,
				m_v3Position.y - m_v3StartPosition.y,
				m_v3Position.z - m_v3StartPosition.z);

			const Vector3& k = m_pParticleProperty->m_v3ZAxis;

			// k × v (cross product)
			Vector3 kCrossV;
			Vec3Cross(&kCrossV, &k, &v);

			// k · v (dot product)
			float kDotV = Vec3Dot(&k, &v);

			// Apply Rodrigues' formula
			m_v3Position.x = v.x * fCosAngle + kCrossV.x * fSinAngle + k.x * kDotV * fOneMinusCos + m_v3StartPosition.x;
			m_v3Position.y = v.y * fCosAngle + kCrossV.y * fSinAngle + k.y * kDotV * fOneMinusCos + m_v3StartPosition.y;
			m_v3Position.z = v.z * fCosAngle + kCrossV.z * fSinAngle + k.z * kDotV * fOneMinusCos + m_v3StartPosition.z;
		}
	}

	return TRUE;
}

void CParticleInstance::Transform(const Matrix * c_matLocal)
{
	/////

	Vector3 v3Up;
	Vector3 v3Cross;

	if (!m_pParticleProperty->m_bStretchFlag)
	{
		CCamera * pCurrentCamera = CCameraManager::Instance().GetCurrentCamera();
		const Vector3 & c_rv3Up = pCurrentCamera->GetUp();
		const Vector3 & c_rv3Cross = pCurrentCamera->GetCross();

		Vector3 v3Rotation;

		switch(m_pParticleProperty->m_byBillboardType) {
		case BILLBOARD_TYPE_LIE:
			{
				float fCos = cosf(ToRadian(m_fRotation)), fSin = sinf(ToRadian(m_fRotation));
				v3Up.x = fCos;
				v3Up.y = -fSin;
				v3Up.z = 0;
				v3Cross.x = fSin;
				v3Cross.y = fCos;
				v3Cross.z = 0;
			}
			break;
		case BILLBOARD_TYPE_2FACE:
		case BILLBOARD_TYPE_3FACE:
			// using setting with y, and local rotation at render
		case BILLBOARD_TYPE_Y:
			{
				v3Up = Vector3(0.0f,0.0f,1.0f);
				const Vector3 & c_rv3View = pCurrentCamera->GetView();
				if (v3Up.x * c_rv3View.y - v3Up.y * c_rv3View.x<0)
					v3Up*=-1;
				auto val = Vector3(c_rv3View.x,c_rv3View.y,0);
				Vec3Cross(&v3Cross, &v3Up, &val);
				Vec3Normalize(&v3Cross, &v3Cross);

				if (m_fRotation)
				{
					float fCos = -sinf(ToRadian(m_fRotation)); // + 90
					float fSin = cosf(ToRadian(m_fRotation));

					Vector3 v3Temp = v3Up * fCos - v3Cross * fSin;
					v3Cross = v3Cross * fCos + v3Up * fSin;
					v3Up = v3Temp;
				}
			}
			break;
		case BILLBOARD_TYPE_ALL:
		default:
			{
				if (m_fRotation==0.0f)
				{
					v3Up = -c_rv3Cross;
					v3Cross = c_rv3Up;
				}
				else
				{
					const Vector3 & c_rv3View = pCurrentCamera->GetView();
					float fRad = ToRadian(m_fRotation);
					float fCosAngle = cosf(fRad);
					float fSinAngle = sinf(fRad);
					float fOneMinusCos = 1.0f - fCosAngle;

					// Rotate -c_rv3Cross to get v3Up
					{
						Vector3 v(-c_rv3Cross.x, -c_rv3Cross.y, -c_rv3Cross.z);
						Vector3 kCrossV;
						Vec3Cross(&kCrossV, &c_rv3View, &v);
						float kDotV = Vec3Dot(&c_rv3View, &v);

						v3Up.x = v.x * fCosAngle + kCrossV.x * fSinAngle + c_rv3View.x * kDotV * fOneMinusCos;
						v3Up.y = v.y * fCosAngle + kCrossV.y * fSinAngle + c_rv3View.y * kDotV * fOneMinusCos;
						v3Up.z = v.z * fCosAngle + kCrossV.z * fSinAngle + c_rv3View.z * kDotV * fOneMinusCos;
					}

					// Rotate c_rv3Up to get v3Cross
					{
						Vector3 kCrossV;
						Vec3Cross(&kCrossV, &c_rv3View, &c_rv3Up);
						float kDotV = Vec3Dot(&c_rv3View, &c_rv3Up);

						v3Cross.x = c_rv3Up.x * fCosAngle + kCrossV.x * fSinAngle + c_rv3View.x * kDotV * fOneMinusCos;
						v3Cross.y = c_rv3Up.y * fCosAngle + kCrossV.y * fSinAngle + c_rv3View.y * kDotV * fOneMinusCos;
						v3Cross.z = c_rv3Up.z * fCosAngle + kCrossV.z * fSinAngle + c_rv3View.z * kDotV * fOneMinusCos;
					}
				}
			}
			break;
		}

	}
	else
	{
		v3Up = m_v3Position - m_v3LastPosition;

		if (c_matLocal)
		{
			Vec3TransformNormal(&v3Up, &v3Up, c_matLocal);
		}

		float length = Vec3Length(&v3Up);
		if (length == 0.0f)
		{
			v3Up = Vector3(0.0f,0.0f,1.0f);
		}
		else
			v3Up *=(1+log(1+length))/length;

		CCamera * pCurrentCamera = CCameraManager::Instance().GetCurrentCamera();
		const Vector3 & c_rv3View = pCurrentCamera->GetView();
		Vec3Cross(&v3Cross, &v3Up, &c_rv3View);
		Vec3Normalize(&v3Cross, &v3Cross);

	}

	v3Cross = -(m_v2HalfSize.x*m_v2Scale.x) * v3Cross;
	v3Up = (m_v2HalfSize.y*m_v2Scale.y) * v3Up;

	if (c_matLocal && m_pParticleProperty->m_bAttachFlag)
	{
		Vector3 v3Position;
		Vec3TransformCoord(&v3Position, &m_v3Position, c_matLocal);
		m_ParticleMesh[0].position = v3Position - v3Up + v3Cross;
		m_ParticleMesh[1].position = v3Position - v3Up - v3Cross;
		m_ParticleMesh[2].position = v3Position + v3Up + v3Cross;
		m_ParticleMesh[3].position = v3Position + v3Up - v3Cross;
	}
	else
	{
		m_ParticleMesh[0].position = m_v3Position - v3Up + v3Cross;
		m_ParticleMesh[1].position = m_v3Position - v3Up - v3Cross;
		m_ParticleMesh[2].position = m_v3Position + v3Up + v3Cross;
		m_ParticleMesh[3].position = m_v3Position + v3Up - v3Cross;
	}
}

void CParticleInstance::Transform(const Matrix * c_matLocal, const float c_fZRotation)
{
	/////

	Vector3 v3Up;
	Vector3 v3Cross;

	if (!m_pParticleProperty->m_bStretchFlag)
	{
		CCamera * pCurrentCamera = CCameraManager::Instance().GetCurrentCamera();
		const Vector3 & c_rv3Up = pCurrentCamera->GetUp();
		const Vector3 & c_rv3Cross = pCurrentCamera->GetCross();

		Vector3 v3Rotation;

		switch(m_pParticleProperty->m_byBillboardType) {
		case BILLBOARD_TYPE_LIE:
			{
				float fCos = cosf(ToRadian(m_fRotation)), fSin = sinf(ToRadian(m_fRotation));
				v3Up.x = fCos;
				v3Up.y = -fSin;
				v3Up.z = 0;

				v3Cross.x = fSin;
				v3Cross.y = fCos;
				v3Cross.z = 0;
			}
			break;
		case BILLBOARD_TYPE_2FACE:
		case BILLBOARD_TYPE_3FACE:
			// using setting with y, and local rotation at render
		case BILLBOARD_TYPE_Y:
			{
				v3Up = Vector3(0.0f,0.0f,1.0f);
				const Vector3 & c_rv3View = pCurrentCamera->GetView();
				if (v3Up.x * c_rv3View.y - v3Up.y * c_rv3View.x<0)
					v3Up*=-1;
				auto val = Vector3(c_rv3View.x,c_rv3View.y,0);
				Vec3Cross(&v3Cross, &v3Up, &val);
				Vec3Normalize(&v3Cross, &v3Cross);

				if (m_fRotation)
				{
					float fCos = -sinf(ToRadian(m_fRotation)); // + 90
					float fSin = cosf(ToRadian(m_fRotation));

					Vector3 v3Temp = v3Up * fCos - v3Cross * fSin;
					v3Cross = v3Cross * fCos + v3Up * fSin;
					v3Up = v3Temp;
				}
			}
			break;
		case BILLBOARD_TYPE_ALL:
		default:
			{
				if (m_fRotation==0.0f)
				{
					v3Up = -c_rv3Cross;
					v3Cross = c_rv3Up;
				}
				else
				{
					const Vector3 & c_rv3View = pCurrentCamera->GetView();
					Matrix matRotation;

					MatrixRotationAxis(&matRotation, &c_rv3View, ToRadian(m_fRotation));
					auto val = (-c_rv3Cross);
					Vec3TransformCoord(&v3Up, &val, &matRotation);
					Vec3TransformCoord(&v3Cross, &c_rv3Up, &matRotation);
				}
			}
			break;
		}
	}
	else
	{
		v3Up = m_v3Position - m_v3LastPosition;

		if (c_matLocal)
		{
			Vec3TransformNormal(&v3Up, &v3Up, c_matLocal);
		}

		float length = Vec3Length(&v3Up);
		if (length == 0.0f)
		{
			v3Up = Vector3(0.0f,0.0f,1.0f);
		}
		else
			v3Up *=(1+log(1+length))/length;

		CCamera * pCurrentCamera = CCameraManager::Instance().GetCurrentCamera();
		const Vector3 & c_rv3View = pCurrentCamera->GetView();
		Vec3Cross(&v3Cross, &v3Up, &c_rv3View);
		Vec3Normalize(&v3Cross, &v3Cross);

	}

	if (c_fZRotation)
	{
		float x, y;
		float fCos = cosf(c_fZRotation);
		float fSin = sinf(c_fZRotation);

		x = v3Up.x;
		y = v3Up.y;
		v3Up.x = x * fCos - y * fSin;
		v3Up.y = y * fCos + x * fSin;

		x = v3Cross.x;
		y = v3Cross.y;
		v3Cross.x = x * fCos - y * fSin;
		v3Cross.y = y * fCos + x * fSin;
	}

	v3Cross = -(m_v2HalfSize.x*m_v2Scale.x) * v3Cross;
	v3Up = (m_v2HalfSize.y*m_v2Scale.y) * v3Up;

	if (c_matLocal && m_pParticleProperty->m_bAttachFlag)
	{
		Vector3 v3Position;
		Vec3TransformCoord(&v3Position, &m_v3Position, c_matLocal);
		m_ParticleMesh[0].position = v3Position - v3Up + v3Cross;
		m_ParticleMesh[1].position = v3Position - v3Up - v3Cross;
		m_ParticleMesh[2].position = v3Position + v3Up + v3Cross;
		m_ParticleMesh[3].position = v3Position + v3Up - v3Cross;
	}
	else
	{
		m_ParticleMesh[0].position = m_v3Position - v3Up + v3Cross;
		m_ParticleMesh[1].position = m_v3Position - v3Up - v3Cross;
		m_ParticleMesh[2].position = m_v3Position + v3Up + v3Cross;
		m_ParticleMesh[3].position = m_v3Position + v3Up - v3Cross;
	}
}

void CParticleInstance::Destroy()
{
	if (m_pDecorator)
		m_pDecorator->DeleteThis();

	__Initialize();
}

void CParticleInstance::__Initialize()
{
	m_pDecorator=NULL;

	m_v3Position = Vector3(0.0f, 0.0f, 0.0f);
	m_v3LastPosition = m_v3Position;
	m_v3Velocity = Vector3(0.0f, 0.0f, 0.0f);

	m_v2Scale = Vector2(1.0f, 1.0f);
#ifdef WORLD_EDITOR
	m_Color = Color(1.0f, 1.0f, 1.0f, 1.0f);
#else
	m_dcColor.m_dwColor = 0xffffffff;
#endif

	m_byFrameIndex = 0;
	m_ParticleMesh[0].texCoord = Vector2(0.0f, 1.0f);
	m_ParticleMesh[1].texCoord = Vector2(0.0f, 0.0f);
	m_ParticleMesh[2].texCoord = Vector2(1.0f, 1.0f);
	m_ParticleMesh[3].texCoord = Vector2(1.0f, 0.0f);
}

CParticleInstance::CParticleInstance()
{
	__Initialize();
}

CParticleInstance::~CParticleInstance()
{
	Destroy();
}

DWORD CParticleInstance::GetColor() const
{
#ifdef WORLD_EDITOR
	return m_Color;
#else
	return m_dcColor.m_dwColor;
#endif
}

TPTVertex * CParticleInstance::GetParticleMeshPointer()
{
	return m_ParticleMesh;
}

//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
