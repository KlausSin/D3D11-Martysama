#pragma once

//////////////////////////////////////////////////////////////////////////
// GrpMathFunc.h
// Math functions using DirectXMath - Pure DX11
//////////////////////////////////////////////////////////////////////////

#include "GrpMathType.h"

//////////////////////////////////////////////////////////////////////////
// Constants
//////////////////////////////////////////////////////////////////////////
#ifndef MATH_PI
constexpr float MATH_PI = 3.141592654f;
#endif
#ifndef MATH_1BYPI
constexpr float MATH_1BYPI = 0.318309886f;
#endif

inline float ToRadian(float degree) { return degree * (MATH_PI / 180.0f); }
inline float ToDegree(float radian) { return radian * (180.0f / MATH_PI); }

//////////////////////////////////////////////////////////////////////////
// Vector2 Functions
//////////////////////////////////////////////////////////////////////////
inline float Vec2Length(const Vector2* pV)
{
	return pV->Length();
}

inline float Vec2CCW(const Vector2* pV1, const Vector2* pV2)
{
	// 2D cross product (returns scalar z-component)
	return pV1->x * pV2->y - pV1->y * pV2->x;
}

inline Vector2* Vec2Add(Vector2* pOut, const Vector2* pV1, const Vector2* pV2)
{
	pOut->x = pV1->x + pV2->x;
	pOut->y = pV1->y + pV2->y;
	return pOut;
}

inline Vector2* Vec2Subtract(Vector2* pOut, const Vector2* pV1, const Vector2* pV2)
{
	pOut->x = pV1->x - pV2->x;
	pOut->y = pV1->y - pV2->y;
	return pOut;
}

inline Vector2* Vec2Scale(Vector2* pOut, const Vector2* pV, float s)
{
	pOut->x = pV->x * s;
	pOut->y = pV->y * s;
	return pOut;
}

inline float Vec2LengthSq(const Vector2* pV)
{
	return pV->LengthSq();
}

inline float Vec2Dot(const Vector2* pV1, const Vector2* pV2)
{
	return pV1->x * pV2->x + pV1->y * pV2->y;
}

inline Vector2* Vec2Normalize(Vector2* pOut, const Vector2* pV)
{
	*pOut = *pV;
	pOut->Normalize();
	return pOut;
}

inline Vector2* Vec2Lerp(Vector2* pOut, const Vector2* pV1, const Vector2* pV2, float s)
{
	pOut->x = pV1->x + s * (pV2->x - pV1->x);
	pOut->y = pV1->y + s * (pV2->y - pV1->y);
	return pOut;
}

//////////////////////////////////////////////////////////////////////////
// Vector3 Functions
//////////////////////////////////////////////////////////////////////////
inline float Vec3Length(const Vector3* pV)
{
	return pV->Length();
}

inline Vector3* Vec3Add(Vector3* pOut, const Vector3* pV1, const Vector3* pV2)
{
	pOut->x = pV1->x + pV2->x;
	pOut->y = pV1->y + pV2->y;
	pOut->z = pV1->z + pV2->z;
	return pOut;
}

inline Vector3* Vec3Subtract(Vector3* pOut, const Vector3* pV1, const Vector3* pV2)
{
	pOut->x = pV1->x - pV2->x;
	pOut->y = pV1->y - pV2->y;
	pOut->z = pV1->z - pV2->z;
	return pOut;
}

inline Vector3* Vec3Scale(Vector3* pOut, const Vector3* pV, float s)
{
	pOut->x = pV->x * s;
	pOut->y = pV->y * s;
	pOut->z = pV->z * s;
	return pOut;
}

inline Vector3* Vec3Minimize(Vector3* pOut, const Vector3* pV1, const Vector3* pV2)
{
	XMStoreFloat3(pOut, XMVectorMin(XMLoadFloat3(pV1), XMLoadFloat3(pV2)));
	return pOut;
}

inline Vector3* Vec3Maximize(Vector3* pOut, const Vector3* pV1, const Vector3* pV2)
{
	XMStoreFloat3(pOut, XMVectorMax(XMLoadFloat3(pV1), XMLoadFloat3(pV2)));
	return pOut;
}

inline float Vec3LengthSq(const Vector3* pV)
{
	return pV->LengthSq();
}

inline float Vec3Dot(const Vector3* pV1, const Vector3* pV2)
{
	return pV1->Dot(*pV2);
}

inline Vector3* Vec3Cross(Vector3* pOut, const Vector3* pV1, const Vector3* pV2)
{
	*pOut = pV1->Cross(*pV2);
	return pOut;
}

inline Vector3* Vec3Normalize(Vector3* pOut, const Vector3* pV)
{
	XMStoreFloat3(pOut, XMVector3Normalize(XMLoadFloat3(pV)));
	return pOut;
}

inline Vector3* Vec3Lerp(Vector3* pOut, const Vector3* pV1, const Vector3* pV2, float s)
{
	XMStoreFloat3(pOut, XMVectorLerp(XMLoadFloat3(pV1), XMLoadFloat3(pV2), s));
	return pOut;
}

inline Vector3* Vec3TransformCoord(Vector3* pOut, const Vector3* pV, const Matrix* pM)
{
	XMStoreFloat3(pOut, XMVector3TransformCoord(XMLoadFloat3(pV), XMLoadFloat4x4(pM)));
	return pOut;
}

inline Vector3* Vec3TransformNormal(Vector3* pOut, const Vector3* pV, const Matrix* pM)
{
	XMStoreFloat3(pOut, XMVector3TransformNormal(XMLoadFloat3(pV), XMLoadFloat4x4(pM)));
	return pOut;
}

inline Vector4* Vec3Transform(Vector4* pOut, const Vector3* pV, const Matrix* pM)
{
	XMStoreFloat4(pOut, XMVector3Transform(XMLoadFloat3(pV), XMLoadFloat4x4(pM)));
	return pOut;
}

inline Vector3* Vec3Project(Vector3* pOut, const Vector3* pV, const D3D11_VIEWPORT* pViewport,
	const Matrix* pProjection, const Matrix* pView, const Matrix* pWorld)
{
	XMMATRIX world = pWorld ? XMLoadFloat4x4(pWorld) : XMMatrixIdentity();
	XMMATRIX view = pView ? XMLoadFloat4x4(pView) : XMMatrixIdentity();
	XMMATRIX proj = pProjection ? XMLoadFloat4x4(pProjection) : XMMatrixIdentity();

	XMVECTOR v = XMLoadFloat3(pV);
	v = XMVector3Project(v,
		(float)pViewport->TopLeftX, (float)pViewport->TopLeftY,
		(float)pViewport->Width, (float)pViewport->Height,
		pViewport->MinDepth, pViewport->MaxDepth,
		proj, view, world);
	XMStoreFloat3(pOut, v);
	return pOut;
}

inline Vector3* Vec3Unproject(Vector3* pOut, const Vector3* pV, const D3D11_VIEWPORT* pViewport,
	const Matrix* pProjection, const Matrix* pView, const Matrix* pWorld)
{
	XMMATRIX world = pWorld ? XMLoadFloat4x4(pWorld) : XMMatrixIdentity();
	XMMATRIX view = pView ? XMLoadFloat4x4(pView) : XMMatrixIdentity();
	XMMATRIX proj = pProjection ? XMLoadFloat4x4(pProjection) : XMMatrixIdentity();

	XMVECTOR v = XMLoadFloat3(pV);
	v = XMVector3Unproject(v,
		(float)pViewport->TopLeftX, (float)pViewport->TopLeftY,
		(float)pViewport->Width, (float)pViewport->Height,
		pViewport->MinDepth, pViewport->MaxDepth,
		proj, view, world);
	XMStoreFloat3(pOut, v);
	return pOut;
}

//////////////////////////////////////////////////////////////////////////
// Vector4 Functions
//////////////////////////////////////////////////////////////////////////
inline float Vec4Length(const Vector4* pV)
{
	return XMVectorGetX(XMVector4Length(XMLoadFloat4(pV)));
}

inline float Vec4LengthSq(const Vector4* pV)
{
	return XMVectorGetX(XMVector4LengthSq(XMLoadFloat4(pV)));
}

inline float Vec4Dot(const Vector4* pV1, const Vector4* pV2)
{
	return XMVectorGetX(XMVector4Dot(XMLoadFloat4(pV1), XMLoadFloat4(pV2)));
}

inline Vector4* Vec4Normalize(Vector4* pOut, const Vector4* pV)
{
	XMStoreFloat4(pOut, XMVector4Normalize(XMLoadFloat4(pV)));
	return pOut;
}

inline Vector4* Vec4Lerp(Vector4* pOut, const Vector4* pV1, const Vector4* pV2, float s)
{
	XMStoreFloat4(pOut, XMVectorLerp(XMLoadFloat4(pV1), XMLoadFloat4(pV2), s));
	return pOut;
}

inline Vector4* Vec4Transform(Vector4* pOut, const Vector4* pV, const Matrix* pM)
{
	XMStoreFloat4(pOut, XMVector4Transform(XMLoadFloat4(pV), XMLoadFloat4x4(pM)));
	return pOut;
}

//////////////////////////////////////////////////////////////////////////
// Matrix Functions
//////////////////////////////////////////////////////////////////////////
inline Matrix* MatrixIdentity(Matrix* pOut)
{
	XMStoreFloat4x4(pOut, XMMatrixIdentity());
	return pOut;
}

inline bool MatrixIsIdentity(const Matrix* pM)
{
	return XMMatrixIsIdentity(XMLoadFloat4x4(pM));
}

inline float MatrixDeterminant(const Matrix* pM)
{
	return XMVectorGetX(XMMatrixDeterminant(XMLoadFloat4x4(pM)));
}

inline Matrix* MatrixTranspose(Matrix* pOut, const Matrix* pM)
{
	XMStoreFloat4x4(pOut, XMMatrixTranspose(XMLoadFloat4x4(pM)));
	return pOut;
}

inline Matrix* MatrixInverse(Matrix* pOut, float* pDeterminant, const Matrix* pM)
{
	XMVECTOR det;
	XMStoreFloat4x4(pOut, XMMatrixInverse(&det, XMLoadFloat4x4(pM)));
	if (pDeterminant)
		*pDeterminant = XMVectorGetX(det);
	return pOut;
}

inline Matrix* MatrixMultiply(Matrix* pOut, const Matrix* pM1, const Matrix* pM2)
{
	XMStoreFloat4x4(pOut, XMMatrixMultiply(XMLoadFloat4x4(pM1), XMLoadFloat4x4(pM2)));
	return pOut;
}

inline Matrix* MatrixScaling(Matrix* pOut, float sx, float sy, float sz)
{
	XMStoreFloat4x4(pOut, XMMatrixScaling(sx, sy, sz));
	return pOut;
}

inline Matrix* MatrixTranslation(Matrix* pOut, float x, float y, float z)
{
	XMStoreFloat4x4(pOut, XMMatrixTranslation(x, y, z));
	return pOut;
}

inline Matrix* MatrixRotationX(Matrix* pOut, float angle)
{
	XMStoreFloat4x4(pOut, XMMatrixRotationX(angle));
	return pOut;
}

inline Matrix* MatrixRotationY(Matrix* pOut, float angle)
{
	XMStoreFloat4x4(pOut, XMMatrixRotationY(angle));
	return pOut;
}

inline Matrix* MatrixRotationZ(Matrix* pOut, float angle)
{
	XMStoreFloat4x4(pOut, XMMatrixRotationZ(angle));
	return pOut;
}

inline Matrix* MatrixRotationAxis(Matrix* pOut, const Vector3* pV, float angle)
{
	XMStoreFloat4x4(pOut, XMMatrixRotationAxis(XMLoadFloat3(pV), angle));
	return pOut;
}

inline Matrix* MatrixRotationYawPitchRoll(Matrix* pOut, float yaw, float pitch, float roll)
{
	XMStoreFloat4x4(pOut, XMMatrixRotationRollPitchYaw(pitch, yaw, roll));
	return pOut;
}

inline Matrix* MatrixRotationQuaternion(Matrix* pOut, const Quaternion* pQ)
{
	XMStoreFloat4x4(pOut, XMMatrixRotationQuaternion(XMLoadFloat4(pQ)));
	return pOut;
}

inline Matrix* MatrixLookAtLH(Matrix* pOut, const Vector3* pEye, const Vector3* pAt, const Vector3* pUp)
{
	XMStoreFloat4x4(pOut, XMMatrixLookAtLH(XMLoadFloat3(pEye), XMLoadFloat3(pAt), XMLoadFloat3(pUp)));
	return pOut;
}

inline Matrix* MatrixLookAtRH(Matrix* pOut, const Vector3* pEye, const Vector3* pAt, const Vector3* pUp)
{
	XMStoreFloat4x4(pOut, XMMatrixLookAtRH(XMLoadFloat3(pEye), XMLoadFloat3(pAt), XMLoadFloat3(pUp)));
	return pOut;
}

inline Matrix* MatrixPerspectiveFovLH(Matrix* pOut, float fovy, float aspect, float zn, float zf)
{
	XMStoreFloat4x4(pOut, XMMatrixPerspectiveFovLH(fovy, aspect, zn, zf));
	return pOut;
}

inline Matrix* MatrixPerspectiveFovRH(Matrix* pOut, float fovy, float aspect, float zn, float zf)
{
	XMStoreFloat4x4(pOut, XMMatrixPerspectiveFovRH(fovy, aspect, zn, zf));
	return pOut;
}

inline Matrix* MatrixOrthoLH(Matrix* pOut, float w, float h, float zn, float zf)
{
	XMStoreFloat4x4(pOut, XMMatrixOrthographicLH(w, h, zn, zf));
	return pOut;
}

inline Matrix* MatrixOrthoRH(Matrix* pOut, float w, float h, float zn, float zf)
{
	XMStoreFloat4x4(pOut, XMMatrixOrthographicRH(w, h, zn, zf));
	return pOut;
}

inline Matrix* MatrixOrthoOffCenterLH(Matrix* pOut, float l, float r, float b, float t, float zn, float zf)
{
	XMStoreFloat4x4(pOut, XMMatrixOrthographicOffCenterLH(l, r, b, t, zn, zf));
	return pOut;
}

inline Matrix* MatrixOrthoOffCenterRH(Matrix* pOut, float l, float r, float b, float t, float zn, float zf)
{
	XMStoreFloat4x4(pOut, XMMatrixOrthographicOffCenterRH(l, r, b, t, zn, zf));
	return pOut;
}

inline Matrix* MatrixAffineTransformation(Matrix* pOut, float scaling, const Vector3* pRotationCenter,
	const Quaternion* pRotation, const Vector3* pTranslation)
{
	XMVECTOR rotCenter = pRotationCenter ? XMLoadFloat3(pRotationCenter) : XMVectorZero();
	XMVECTOR rotation = pRotation ? XMLoadFloat4(pRotation) : XMQuaternionIdentity();
	XMVECTOR translation = pTranslation ? XMLoadFloat3(pTranslation) : XMVectorZero();
	XMStoreFloat4x4(pOut, XMMatrixAffineTransformation(XMVectorReplicate(scaling), rotCenter, rotation, translation));
	return pOut;
}

inline Matrix* MatrixTransformation(Matrix* pOut, const Vector3* pScalingCenter, const Quaternion* pScalingRotation,
	const Vector3* pScaling, const Vector3* pRotationCenter, const Quaternion* pRotation, const Vector3* pTranslation)
{
	XMVECTOR scaleCenter = pScalingCenter ? XMLoadFloat3(pScalingCenter) : XMVectorZero();
	XMVECTOR scaleRot = pScalingRotation ? XMLoadFloat4(pScalingRotation) : XMQuaternionIdentity();
	XMVECTOR scale = pScaling ? XMLoadFloat3(pScaling) : XMVectorSet(1, 1, 1, 0);
	XMVECTOR rotCenter = pRotationCenter ? XMLoadFloat3(pRotationCenter) : XMVectorZero();
	XMVECTOR rotation = pRotation ? XMLoadFloat4(pRotation) : XMQuaternionIdentity();
	XMVECTOR translation = pTranslation ? XMLoadFloat3(pTranslation) : XMVectorZero();
	XMStoreFloat4x4(pOut, XMMatrixTransformation(scaleCenter, scaleRot, scale, rotCenter, rotation, translation));
	return pOut;
}

//////////////////////////////////////////////////////////////////////////
// Quaternion Functions
//////////////////////////////////////////////////////////////////////////
inline Quaternion* QuaternionIdentity(Quaternion* pOut)
{
	pOut->SetIdentity();
	return pOut;
}

inline bool QuaternionIsIdentity(const Quaternion* pQ)
{
	return XMQuaternionIsIdentity(XMLoadFloat4(pQ));
}

inline float QuaternionLength(const Quaternion* pQ)
{
	return XMVectorGetX(XMQuaternionLength(XMLoadFloat4(pQ)));
}

inline float QuaternionLengthSq(const Quaternion* pQ)
{
	return XMVectorGetX(XMQuaternionLengthSq(XMLoadFloat4(pQ)));
}

inline float QuaternionDot(const Quaternion* pQ1, const Quaternion* pQ2)
{
	return XMVectorGetX(XMQuaternionDot(XMLoadFloat4(pQ1), XMLoadFloat4(pQ2)));
}

inline Quaternion* QuaternionConjugate(Quaternion* pOut, const Quaternion* pQ)
{
	XMStoreFloat4(pOut, XMQuaternionConjugate(XMLoadFloat4(pQ)));
	return pOut;
}

inline Quaternion* QuaternionInverse(Quaternion* pOut, const Quaternion* pQ)
{
	XMStoreFloat4(pOut, XMQuaternionInverse(XMLoadFloat4(pQ)));
	return pOut;
}

inline Quaternion* QuaternionNormalize(Quaternion* pOut, const Quaternion* pQ)
{
	XMStoreFloat4(pOut, XMQuaternionNormalize(XMLoadFloat4(pQ)));
	return pOut;
}

inline Quaternion* QuaternionMultiply(Quaternion* pOut, const Quaternion* pQ1, const Quaternion* pQ2)
{
	XMStoreFloat4(pOut, XMQuaternionMultiply(XMLoadFloat4(pQ1), XMLoadFloat4(pQ2)));
	return pOut;
}

inline Quaternion* QuaternionRotationAxis(Quaternion* pOut, const Vector3* pV, float angle)
{
	XMStoreFloat4(pOut, XMQuaternionRotationAxis(XMLoadFloat3(pV), angle));
	return pOut;
}

inline Quaternion* QuaternionRotationMatrix(Quaternion* pOut, const Matrix* pM)
{
	XMStoreFloat4(pOut, XMQuaternionRotationMatrix(XMLoadFloat4x4(pM)));
	return pOut;
}

inline Quaternion* QuaternionRotationYawPitchRoll(Quaternion* pOut, float yaw, float pitch, float roll)
{
	XMStoreFloat4(pOut, XMQuaternionRotationRollPitchYaw(pitch, yaw, roll));
	return pOut;
}

inline Quaternion* QuaternionSlerp(Quaternion* pOut, const Quaternion* pQ1, const Quaternion* pQ2, float t)
{
	XMStoreFloat4(pOut, XMQuaternionSlerp(XMLoadFloat4(pQ1), XMLoadFloat4(pQ2), t));
	return pOut;
}

inline void QuaternionToAxisAngle(const Quaternion* pQ, Vector3* pAxis, float* pAngle)
{
	XMVECTOR axis;
	XMQuaternionToAxisAngle(&axis, pAngle, XMLoadFloat4(pQ));
	XMStoreFloat3(pAxis, axis);
}

//////////////////////////////////////////////////////////////////////////
// Plane Functions
//////////////////////////////////////////////////////////////////////////
inline Plane* PlaneNormalize(Plane* pOut, const Plane* pP)
{
	XMFLOAT4 temp(pP->a, pP->b, pP->c, pP->d);
	XMFLOAT4 result;
	XMStoreFloat4(&result, XMPlaneNormalize(XMLoadFloat4(&temp)));
	pOut->a = result.x;
	pOut->b = result.y;
	pOut->c = result.z;
	pOut->d = result.w;
	return pOut;
}

inline float PlaneDotCoord(const Plane* pP, const Vector3* pV)
{
	return XMVectorGetX(XMPlaneDotCoord(XMVectorSet(pP->a, pP->b, pP->c, pP->d), XMLoadFloat3(pV)));
}

inline float PlaneDotNormal(const Plane* pP, const Vector3* pV)
{
	return XMVectorGetX(XMPlaneDotNormal(XMVectorSet(pP->a, pP->b, pP->c, pP->d), XMLoadFloat3(pV)));
}

inline Plane* PlaneFromPointNormal(Plane* pOut, const Vector3* pPoint, const Vector3* pNormal)
{
	XMFLOAT4 result;
	XMStoreFloat4(&result, XMPlaneFromPointNormal(XMLoadFloat3(pPoint), XMLoadFloat3(pNormal)));
	pOut->a = result.x;
	pOut->b = result.y;
	pOut->c = result.z;
	pOut->d = result.w;
	return pOut;
}

inline Plane* PlaneFromPoints(Plane* pOut, const Vector3* pV1, const Vector3* pV2, const Vector3* pV3)
{
	XMFLOAT4 result;
	XMStoreFloat4(&result, XMPlaneFromPoints(XMLoadFloat3(pV1), XMLoadFloat3(pV2), XMLoadFloat3(pV3)));
	pOut->a = result.x;
	pOut->b = result.y;
	pOut->c = result.z;
	pOut->d = result.w;
	return pOut;
}

inline Vector3* PlaneIntersectLine(Vector3* pOut, const Plane* pP, const Vector3* pV1, const Vector3* pV2)
{
	XMStoreFloat3(pOut, XMPlaneIntersectLine(XMVectorSet(pP->a, pP->b, pP->c, pP->d), XMLoadFloat3(pV1), XMLoadFloat3(pV2)));
	return pOut;
}

inline Plane* PlaneTransform(Plane* pOut, const Plane* pP, const Matrix* pM)
{
	XMFLOAT4 result;
	XMStoreFloat4(&result, XMPlaneTransform(XMVectorSet(pP->a, pP->b, pP->c, pP->d), XMLoadFloat4x4(pM)));
	pOut->a = result.x;
	pOut->b = result.y;
	pOut->c = result.z;
	pOut->d = result.w;
	return pOut;
}

//////////////////////////////////////////////////////////////////////////
// Color Functions
//////////////////////////////////////////////////////////////////////////
inline Color* ColorLerp(Color* pOut, const Color* pC1, const Color* pC2, float s)
{
	pOut->r = pC1->r + s * (pC2->r - pC1->r);
	pOut->g = pC1->g + s * (pC2->g - pC1->g);
	pOut->b = pC1->b + s * (pC2->b - pC1->b);
	pOut->a = pC1->a + s * (pC2->a - pC1->a);
	return pOut;
}

inline Color* ColorModulate(Color* pOut, const Color* pC1, const Color* pC2)
{
	pOut->r = pC1->r * pC2->r;
	pOut->g = pC1->g * pC2->g;
	pOut->b = pC1->b * pC2->b;
	pOut->a = pC1->a * pC2->a;
	return pOut;
}

inline Color* ColorNegative(Color* pOut, const Color* pC)
{
	pOut->r = 1.0f - pC->r;
	pOut->g = 1.0f - pC->g;
	pOut->b = 1.0f - pC->b;
	pOut->a = pC->a;
	return pOut;
}

inline Color* ColorScale(Color* pOut, const Color* pC, float s)
{
	pOut->r = pC->r * s;
	pOut->g = pC->g * s;
	pOut->b = pC->b * s;
	pOut->a = pC->a * s;
	return pOut;
}
