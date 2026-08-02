#pragma once

#include "GrpBase.h"

class CGraphicCollisionObject : public CGraphicBase
{
	public:
		CGraphicCollisionObject();
		virtual ~CGraphicCollisionObject();

	protected:
		bool IntersectTriangle(const Vector3& c_orig, const Vector3& c_dir, const Vector3& c_v0, const Vector3& c_v1, const Vector3& c_v2, float* pu, float* pv, float* pt);
		bool IntersectBoundBox(const Matrix* c_pmatWorld, const TBoundBox& c_rboundBox, float* pu, float* pv, float* pt);
		bool IntersectCube(const Matrix* c_pmatWorld, float sx, float sy, float sz, float ex, float ey, float ez, Vector3 & RayOriginal, Vector3 & RayDirection, float* pu, float* pv, float* pt);
		bool IntersectIndexedMesh(const Matrix* c_pmatWorld, const void* vertices, int step, int vtxCount, const void* indices, int idxCount, Vector3 & RayOriginal, Vector3 & RayDirection, float* pu, float* pv, float* pt);
		bool IntersectMesh(const Matrix * c_pmatWorld, const void * vertices, DWORD dwStep, DWORD dwvtxCount, Vector3 & RayOriginal, Vector3 & RayDirection, float* pu, float* pv, float* pt);

		bool IntersectSphere(const Vector3 & c_rv3Position, float fRadius, const Vector3 & c_rv3RayOriginal, const Vector3 & c_rv3RayDirection);
		bool IntersectCylinder(const Vector3 & c_rv3Position, float fRadius, float fHeight, const Vector3 & c_rv3RayOriginal, const Vector3 & c_rv3RayDirection);

		bool IntersectSphere(const Vector3 & c_rv3Position, float fRadius);
		bool IntersectCylinder(const Vector3 & c_rv3Position, float fRadius, float fHeight);
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
