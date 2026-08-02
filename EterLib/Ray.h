#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include "GrpMathType.h"
#include "GrpMathFunc.h"

class CRay
{
	public:
		CRay(const Vector3& v3Start, const Vector3& v3Dir, float fRayRange) : m_v3Start(v3Start), m_v3Direction(v3Dir)
		{
			assert(fRayRange >= 0);
			m_fRayRange = fRayRange;
			Vec3Normalize(&m_v3Direction, &m_v3Direction);
			m_v3End = m_v3Start + fRayRange * m_v3Direction;
		}

		CRay()
		{
		}

		void SetStartPoint(const Vector3& v3Start)
		{
			m_v3Start = v3Start;
		}

		void SetDirection(const Vector3& v3Dir, float fRayRange)
		{
			assert(fRayRange >= 0);
			m_v3Direction = v3Dir;
			Vec3Normalize(&m_v3Direction, &m_v3Direction);
			m_fRayRange = fRayRange;
			m_v3End = m_v3Start + m_fRayRange * m_v3Direction;
		}

		void GetStartPoint(Vector3* pv3Start) const
		{
			*pv3Start = m_v3Start;
		}

		void GetDirection(Vector3* pv3Dir, float* pfRayRange) const
		{
			*pv3Dir = m_v3Direction;
			*pfRayRange = m_fRayRange;
		}

		void GetEndPoint(Vector3* pv3End) const
		{
			*pv3End = m_v3End;
		}

		const CRay& operator = (const CRay& rhs)
		{
			assert(rhs.m_fRayRange >= 0);
			m_v3Start = rhs.m_v3Start;
			m_v3Direction = rhs.m_v3Direction;
			m_fRayRange = rhs.m_fRayRange;
			Vec3Normalize(&m_v3Direction, &m_v3Direction);
			m_v3End = m_v3Start + m_fRayRange * m_v3Direction;
			return *this;
		}

	private:
		Vector3		m_v3Start;
		Vector3		m_v3End;
		Vector3		m_v3Direction;
		float		m_fRayRange;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
