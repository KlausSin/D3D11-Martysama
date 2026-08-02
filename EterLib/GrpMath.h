#pragma once

float CrossProduct2D(float x1, float y1, float x2, float y2);

bool IsInTriangle2D(float ax, float ay, float bx, float by, float cx, float cy, float tx, float ty);

Vector3* Vec3Rotation(Vector3* pvtOut, const Vector3* c_pvtSrc, const Quaternion* c_pqtRot);
Vector3* Vec3Translation(Vector3* pvtOut, const Vector3* c_pvtSrc, const Vector3* c_pvtTrans);

void GetRotationFromMatrix(Vector3 * pRotation, const Matrix * c_pMatrix);
void GetPivotAndRotationFromMatrix(Matrix * pMatrix, Vector3 * pPivot, Vector3 * pRotation);
void ExtractMovement(Matrix * pTargetMatrix, Matrix * pSourceMatrix);

inline Vector3* Vec3Blend(Vector3* pvtOut, const Vector3* c_pvtSrc1, const Vector3* c_pvtSrc2, float d)
{
	pvtOut->x=c_pvtSrc1->x+d*(c_pvtSrc2->x-c_pvtSrc1->x);
	pvtOut->y=c_pvtSrc1->y+d*(c_pvtSrc2->y-c_pvtSrc1->y);
	pvtOut->z=c_pvtSrc1->z+d*(c_pvtSrc2->z-c_pvtSrc1->z);

	return pvtOut;
}

inline Quaternion* QuaternionBlend(Quaternion* pqtOut, const Quaternion* c_pqtSrc1, const Quaternion* c_pqtSrc2, float d)
{
	pqtOut->x=c_pqtSrc1->x+d*(c_pqtSrc2->x-c_pqtSrc1->x);
	pqtOut->y=c_pqtSrc1->y+d*(c_pqtSrc2->y-c_pqtSrc1->y);
	pqtOut->z=c_pqtSrc1->z+d*(c_pqtSrc2->z-c_pqtSrc1->z);
	pqtOut->w=c_pqtSrc1->w+d*(c_pqtSrc2->w-c_pqtSrc1->w);
	return pqtOut;
}

inline float ClampDegree(float fDegree)
{
	if (fDegree >= 360.0f)
		fDegree -= 360.0f;
	if (fDegree < 0.0f)
		fDegree += 360.0f;

	return fDegree;
}

inline float GetVector3Distance(const Vector3 & c_rv3Source, const Vector3 & c_rv3Target)
{
	return (c_rv3Source.x-c_rv3Target.x)*(c_rv3Source.x-c_rv3Target.x) + (c_rv3Source.y-c_rv3Target.y)*(c_rv3Source.y-c_rv3Target.y);
}

inline Quaternion SafeRotationNormalizedArc(const Vector3 & vFrom , const Vector3 & vTo)
{
	if (vFrom == vTo)
		return Quaternion(0.0f,0.0f,0.0f,1.0f);
	if (vFrom == -vTo)
		return Quaternion(0.0f,0.0f,1.0f,0.0f);
	Vector3 c;
	Vec3Cross(&c, &vFrom, &vTo);
	float d = Vec3Dot(&vFrom, &vTo);
	float s = sqrtf((1+d)*2);

	return Quaternion(c.x/s,c.y/s,c.z/s,s*0.5f);
}

inline Quaternion RotationNormalizedArc(const Vector3 & vFrom , const Vector3 & vTo)

{
	Vector3 c;
	Vec3Cross(&c, &vFrom, &vTo);
	float d = Vec3Dot(&vFrom, &vTo);
	float s = sqrtf((1+d)*2);

	return Quaternion(c.x/s,c.y/s,c.z/s,s*0.5f);
}

inline Quaternion RotationArc(const Vector3 & vFrom , const Vector3 & vTo)
{
	Vector3 vnFrom, vnTo;
	Vec3Normalize(&vnFrom, &vFrom);
	Vec3Normalize(&vnTo, &vTo);
	return RotationNormalizedArc(vnFrom, vnTo);
}

inline float square_distance_between_linesegment_and_point(const Vector3& p1,const Vector3& p2,const Vector3& x)
{
	auto val1 = (p2-p1);
	auto val2 = (x-p1);
	auto val3 = (p2-p1);
	float l = Vec3LengthSq(&val1);
	float d = Vec3Dot(&val2,&val3);
	if (d<=0.0f)
	{
		auto val = (x-p1);
		return Vec3LengthSq(&val);
	}
	else if (d>=l)
	{
		auto val = (x-p2);
		return Vec3LengthSq(&val);
	}
	else
	{
		Vector3 c;
		auto val1 = (x-p1);
		auto val2 = (p2-p1);
		return Vec3LengthSq(Vec3Cross(&c,&val1,&val2))/l;
	}
}

inline Vector3 * Vec3TransformQuaternionSafe(Vector3* pvout, const Vector3* pv, const Quaternion* pq)
{
	Vector3 v;
	Vec3Cross(&v,pv,(Vector3*)pq);
	v *= -2*pq->w;
	v += (pq->w*pq->w - Vec3LengthSq((Vector3*)pq))*(*pv);
	v += 2*Vec3Dot((Vector3*)pq,pv)*(*(Vector3*)pq);
	*pvout = v;
	return pvout;
}

inline Vector3 * Vec3TransformQuaternion(Vector3* pvout, const Vector3* pv, const Quaternion* pq)
{
	Vec3Cross(pvout,pv,(Vector3*)pq);
	*pvout *= -2*pq->w;
	*pvout += (pq->w*pq->w - Vec3LengthSq((Vector3*)pq))*(*pv);
	*pvout += 2*Vec3Dot((Vector3*)pq,pv)*(*(Vector3*)pq);

	return pvout;
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
