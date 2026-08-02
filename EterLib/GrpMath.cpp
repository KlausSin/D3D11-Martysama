#include "StdAfx.h"
#include "GrpMath.h"

float CrossProduct2D(float x1, float y1, float x2, float y2)
{
	return x1*y2-y1*x2;
}

bool IsInTriangle2D(float ax, float ay, float bx, float by, float cx, float cy, float tx, float ty)
{
	float c1 = CrossProduct2D(bx-ax, by-ay, tx-ax, ty-ay);
	float c2 = CrossProduct2D(cx-bx, cy-by, tx-bx, ty-by);
	float c3 = CrossProduct2D(ax-cx, ay-cy, tx-cx, ty-cy);

	if (c1 * c2 > 0.0f && c1 * c3 > 0.0f)
		return true;

	if (c1 * c2 * c3 == 0.0f)
	{
		if (tx < ax)
			if (tx < bx)
				if (tx < cx)
					return false;

		if (tx > ax)
			if (tx > bx)
				if (tx > cx)
					return false;

		if (ty < ay)
			if (ty < by)
				if (ty < cy)
					return false;

		if (ty > ay)
			if (ty > by)
				if (ty > cy)
					return false;

		return true;
	}

	return false;
}

Vector3* Vec3Rotation(Vector3* pvtOut, const Vector3* c_pvtSrc, const Quaternion* c_pqtRot)
{
	Quaternion qtSrc(c_pvtSrc->x, c_pvtSrc->y, c_pvtSrc->z, 0);
	Quaternion qtRet;
	QuaternionConjugate(&qtRet, c_pqtRot);
	QuaternionMultiply(&qtRet, &qtSrc, &qtRet);
	QuaternionMultiply(&qtRet, c_pqtRot, &qtRet);

	pvtOut->x=qtRet.x;
	pvtOut->y=qtRet.y;
	pvtOut->z=qtRet.z;

	return pvtOut;
}

void GetRotationFromMatrix(Vector3 * pRotation, const Matrix * c_pMatrix)
{
	float sx = c_pMatrix->_32;
	float cx = sqrtf(1.0f - sx * sx);

	if (cx < 0.00001f)
	{
		if (sx > 0)
			pRotation->x = MATH_PI / 2;
		else
			pRotation->x = -MATH_PI / 2;

		pRotation->y = atan2f(c_pMatrix->_31, c_pMatrix->_11);
		pRotation->z = 0.0f;
	}
	else
	{
		pRotation->x = atan2f(sx, cx);
		pRotation->y = atan2f(-c_pMatrix->_31, c_pMatrix->_33);
		pRotation->z = atan2f(-c_pMatrix->_12, c_pMatrix->_22);
	}
}

void GetPivotAndRotationFromMatrix(Matrix * pMatrix, Vector3 * pPivot, Vector3 * pRotation)
{
	float sx = pMatrix->_32;
	float cx = sqrtf(1.0f - sx * sx);
	float x, y, z;

	if (cx < 0.00001f)
	{
		if (sx > 0)
			x = MATH_PI / 2;
		else
			x = -MATH_PI / 2;

		y = atan2f(pMatrix->_31, pMatrix->_11);
		z = 0.0f;
	}
	else
	{
		x = atan2f(sx, cx);
		y = atan2f(-pMatrix->_31, pMatrix->_33);
		z = atan2f(-pMatrix->_12, pMatrix->_22);
	}

	pRotation->x = x;
	pRotation->y = y;
	pRotation->z = z;

	pPivot->x = pMatrix->_41;
	pPivot->y = pMatrix->_42;
	pPivot->z = pMatrix->_43;
}

// NOTE : must be optimized!
void ExtractMovement(Matrix * pTargetMatrix, Matrix * pSourceMatrix)
{
	Vector3 v3Pivot;
	Vector3 v3Rotation;
	GetPivotAndRotationFromMatrix(pSourceMatrix, &v3Pivot, &v3Rotation);

	Matrix matRotationX;
	MatrixRotationX(&matRotationX, v3Rotation.x);
	Matrix matRotationY;
	MatrixRotationY(&matRotationY, v3Rotation.y);
	Matrix matRotationZ;
	MatrixRotationZ(&matRotationZ, v3Rotation.z);

	Matrix matTranslation;
	MatrixTranslation(&matTranslation, v3Pivot.x, v3Pivot.y, v3Pivot.z);

	*pTargetMatrix = matRotationX * matRotationY * matRotationZ * matTranslation;
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
