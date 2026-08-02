#include "StdAfx.h"
#include "../eterBase/Utils.h"
#include "AttributeInstance.h"
#include "GrpMath.h"

CDynamicPool<CAttributeInstance> CAttributeInstance::ms_kPool;

const float c_fStepSize = 50.0f;

bool CAttributeInstance::Picking(const Vector3 & v, const Vector3 & dir, float & out_x, float & out_y)
{
	if (IsEmpty())
		return FALSE;

	bool bPicked = false;
	float nx = 0;
	float ny = 0;

	for (DWORD i = 0; i < m_v3HeightDataVector.size(); ++i)
		for (DWORD j = 0; j < m_v3HeightDataVector[i].size(); j+=3)
		{
			const Vector3 & cv0 = m_v3HeightDataVector[i][j];
			const Vector3 & cv2 = m_v3HeightDataVector[i][j+1];
			const Vector3 & cv1 = m_v3HeightDataVector[i][j+2];

			Vector3 n;
			auto val7 = (cv1-cv0);
			auto val8 = (cv2-cv0);
			Vec3Cross(&n,&val7,&val8);
			Vector3 x;
			float t;
			auto val = (v-cv0);
			t = - Vec3Dot(&val,&n)/Vec3Dot(&dir,&n);

			x = v+t*dir;

			Vector3 temp;
			auto val1 = (cv1-cv0);
			auto val2 = (x-cv0);
			Vec3Cross(&temp,&val1,&val2);
			if (Vec3Dot(&temp,&n)<0) continue;
			auto val3 = (cv2-cv1);
			auto val4 = (x-cv1);
			Vec3Cross(&temp,&val3,&val4);
			if (Vec3Dot(&temp,&n)<0) continue;
			auto val5 = (cv0-cv2);
			auto val6 = (x-cv2);
			Vec3Cross(&temp,&val5,&val6);
			if (Vec3Dot(&temp,&n)<0) continue;

			if (bPicked)
			{
				if ((v.x-x.x)*(v.x-x.x)+(v.y-x.y)*(v.y-x.y)<(v.x-nx)*(v.x-nx)+(v.y-ny)*(v.y-ny))
				{
					nx=x.x;
					ny=x.y;
				}
			}
			else
			{
				nx = x.x;
				ny = x.y;
			}
			bPicked = true;
	}
	if (bPicked)
	{
		out_x = nx;
		out_y = ny;
	}
	return bPicked;
}

BOOL CAttributeInstance::GetHeight(float fx, float fy, float * pfHeight)
{
	if(IsEmpty())
		return FALSE;

	fy *= -1.0f;

	if (!IsInHeight(fx, fy))
		return FALSE;

	BOOL bFlag = FALSE;

	for (DWORD i = 0; i < m_v3HeightDataVector.size(); ++i)
	for (DWORD j = 0; j < m_v3HeightDataVector[i].size(); j+=3)
	{
		const Vector3 & c_rv3Vertex0 = m_v3HeightDataVector[i][j];
		const Vector3 & c_rv3Vertex1 = m_v3HeightDataVector[i][j+1];
		const Vector3 & c_rv3Vertex2 = m_v3HeightDataVector[i][j+2];

		if (
			fx<c_rv3Vertex0.x && fx<c_rv3Vertex1.x && fx<c_rv3Vertex2.x ||
			fx>c_rv3Vertex0.x && fx>c_rv3Vertex1.x && fx>c_rv3Vertex2.x ||
			fy<c_rv3Vertex0.y && fy<c_rv3Vertex1.y && fy<c_rv3Vertex2.y ||
			fy>c_rv3Vertex0.y && fy>c_rv3Vertex1.y && fy>c_rv3Vertex2.y
			)
			continue;

		if (IsInTriangle2D(c_rv3Vertex0.x, c_rv3Vertex0.y,
						   c_rv3Vertex1.x, c_rv3Vertex1.y,
						   c_rv3Vertex2.x, c_rv3Vertex2.y, fx, fy))
		{
			Vector3 v3Line1 = c_rv3Vertex1 - c_rv3Vertex0;
			Vector3 v3Line2 = c_rv3Vertex2 - c_rv3Vertex0;
			Vector3 v3Cross;

			Vec3Cross(&v3Cross, &v3Line1, &v3Line2);
			Vec3Normalize(&v3Cross, &v3Cross);

			if (0.0f != v3Cross.z)
			{
				float fd = (v3Cross.x*c_rv3Vertex0.x + v3Cross.y*c_rv3Vertex0.y + v3Cross.z*c_rv3Vertex0.z);
				float fm = (v3Cross.x*fx + v3Cross.y*fy);
				*pfHeight = fMAX((fd - fm) / v3Cross.z, *pfHeight);

				bFlag = TRUE;
			}
		}
	}

	return bFlag;
}

CAttributeData * CAttributeInstance::GetObjectPointer() const
{
	return m_roAttributeData.GetPointer();
}
BOOL CAttributeInstance::IsInHeight(float fx, float fy)
{
	float fdx = m_matGlobal._41 - fx;
	float fdy = m_matGlobal._42 - fy;
	if (sqrtf(fdx*fdx + fdy*fdy) > m_fHeightRadius)
		return FALSE;

	return TRUE;
}

void CAttributeInstance::SetObjectPointer(CAttributeData * pAttributeData)
{
	Clear();
	m_roAttributeData.SetPointer(pAttributeData);
}

void CAttributeInstance::RefreshObject(const Matrix & c_rmatGlobal)
{
	assert(!m_roAttributeData.IsNull());

	m_matGlobal = c_rmatGlobal;

	// Height
	m_fHeightRadius = m_roAttributeData->GetMaximizeRadius();

	DWORD dwHeightDataCount = m_roAttributeData->GetHeightDataCount();
	m_v3HeightDataVector.clear();
	m_v3HeightDataVector.resize(dwHeightDataCount);
	for (DWORD i = 0; i < dwHeightDataCount; ++i)
	{
		const THeightData * c_pHeightData;
		if (!m_roAttributeData->GetHeightDataPointer(i, &c_pHeightData))
			continue;

		DWORD dwVertexCount = (DWORD)(c_pHeightData->v3VertexVector.size());
		m_v3HeightDataVector[i].clear();
		m_v3HeightDataVector[i].resize(dwVertexCount);
		for (DWORD j = 0; j < dwVertexCount; ++j)
		{
			Vec3TransformCoord(&m_v3HeightDataVector[i][j], &c_pHeightData->v3VertexVector[j], &m_matGlobal);
		}
	}
}

const char * CAttributeInstance::GetDataFileName() const
{
	return m_roAttributeData->GetFileName();
}

void CAttributeInstance::CreateSystem(UINT uCapacity)
{
	ms_kPool.Create(uCapacity);
}

void CAttributeInstance::DestroySystem()
{
	ms_kPool.Destroy();
}

CAttributeInstance* CAttributeInstance::New()
{
	return ms_kPool.Alloc();
}

void CAttributeInstance::Delete(CAttributeInstance* pkInst)
{
	ms_kPool.Free(pkInst);
}

BOOL CAttributeInstance::IsEmpty() const
{
	if (!m_v3HeightDataVector.empty())
		return FALSE;

	return TRUE;
}

void CAttributeInstance::Clear()
{
	m_fHeightRadius = 0.0f;
	m_fCollisionRadius = 0.0f;
	MatrixIdentity(&m_matGlobal);

	m_v3HeightDataVector.clear();

	m_roAttributeData.SetPointer(NULL);
}

CAttributeInstance::CAttributeInstance()
{
}
CAttributeInstance::~CAttributeInstance()
{
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
