/* Copyright (C) John W. Ratcliff, 2001.
 * All rights reserved worldwide.
 *
 * This software is provided "as is" without express or implied
 * warranties. You may freely copy and compile this source into
 * applications you distribute provided that the copyright text
 * below is included in the resulting source code, for example:
 * "Portions Copyright (C) John W. Ratcliff, 2001"
 */

#include "Stdafx.h"
#include "frustum.h"

//#include "frustum.h"

/*void Frustum::Set(int x1,int y1,int x2,int y2)
{
  mX1 = x1;
  mY1 = y1;
  mX2 = x2;
  mY2 = y2;
}

*/
ViewState Frustum::ViewVolumeTest(const Vector3d& c_v3Center, const float c_fRadius) const
{
	const auto dynamicShadowExtend = 2.0f; // idk i think even 1.5 should do it

	if (m_bUsingSphere)
	{
		Vector3 v(
			c_v3Center.x - m_v3Center.x,
			c_v3Center.y - m_v3Center.y,
			c_v3Center.z - m_v3Center.z);

		if ((c_fRadius * dynamicShadowExtend + m_fRadius) * (c_fRadius * dynamicShadowExtend + m_fRadius) < Vec3LengthSq(&v))
		{
			return VS_OUTSIDE;
		}
	}

	const int count = 6;

	Vector3 center = c_v3Center;

	int i;

	float distance[count];
	for (i = 0; i < count; i++)
	{
		distance[i] = PlaneDotCoord(&m_plane[i], &center);
		if (distance[i] <= -c_fRadius * dynamicShadowExtend)
			return VS_OUTSIDE;
	}

	for (i = 0; i < count; i++)
	{
		if (distance[i] <= c_fRadius)
			return VS_PARTIAL;
	}

	return VS_INSIDE;
}

void Frustum::BuildViewFrustum(Matrix & mat)
{
	m_bUsingSphere = false;
	m_plane[0] = Plane(          mat._13,           mat._23,           mat._33,           mat._43);
	m_plane[1] = Plane(mat._14 - mat._13, mat._24 - mat._23, mat._34 - mat._33, mat._44 - mat._43);
	//m_plane[0] = Plane(mat._14 + mat._13, mat._24 + mat._23, mat._34 + mat._33, mat._44 + mat._43);
	m_plane[2] = Plane(mat._14 + mat._11, mat._24 + mat._21, mat._34 + mat._31, mat._44 + mat._41);
	m_plane[3] = Plane(mat._14 - mat._11, mat._24 - mat._21, mat._34 - mat._31, mat._44 - mat._41);
	m_plane[4] = Plane(mat._14 + mat._12, mat._24 + mat._22, mat._34 + mat._32, mat._44 + mat._42);
	m_plane[5] = Plane(mat._14 - mat._12, mat._24 - mat._22, mat._34 - mat._32, mat._44 - mat._42);

	for(int i=0;i<6;i++)
		PlaneNormalize(&m_plane[i],&m_plane[i]);
}

void Frustum::BuildViewFrustum2(Matrix & mat, float fNear, float fFar, float fFov, float fAspect, const Vector3 & vCamera, const Vector3 & vLook)
{
	float fViewLen = fFar-fNear;
	float fH = fViewLen * tan(fFov*0.5f);
	float fW = fH*fAspect;
	Vector3 P(0.0f, 0.0f, fNear+fViewLen*0.5f);
	Vector3 Q(fW, fH, fViewLen);
	Vector3 PQ = P-Q;
	m_fRadius = Vec3Length(&PQ);
	m_v3Center = vCamera + vLook * (fNear+fViewLen*0.5f);
	BuildViewFrustum(mat);
	m_bUsingSphere = true;
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
