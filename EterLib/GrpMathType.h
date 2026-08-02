#pragma once

//////////////////////////////////////////////////////////////////////////
// GrpMathType.h
// Native DirectXMath-based math types - Pure DX11
//////////////////////////////////////////////////////////////////////////

#include <d3d11.h>
#include <DirectXMath.h>
#include <cmath>
#include <cstring>

using namespace DirectX;

//////////////////////////////////////////////////////////////////////////
// Vector2
//////////////////////////////////////////////////////////////////////////
struct Vector2 : public XMFLOAT2
{
	Vector2() : XMFLOAT2(0.f, 0.f) {}
	Vector2(float _x, float _y) : XMFLOAT2(_x, _y) {}
	Vector2(const float* pf) : XMFLOAT2(pf[0], pf[1]) {}
	Vector2(const XMFLOAT2& v) : XMFLOAT2(v) {}
	Vector2(FXMVECTOR v) { XMStoreFloat2(this, v); }

	operator float* () { return reinterpret_cast<float*>(this); }
	operator const float* () const { return reinterpret_cast<const float*>(this); }
	operator XMVECTOR() const { return XMLoadFloat2(this); }

	Vector2& operator += (const Vector2& v) { x += v.x; y += v.y; return *this; }
	Vector2& operator -= (const Vector2& v) { x -= v.x; y -= v.y; return *this; }
	Vector2& operator *= (float f) { x *= f; y *= f; return *this; }
	Vector2& operator /= (float f) { float inv = 1.0f / f; x *= inv; y *= inv; return *this; }

	Vector2 operator + () const { return *this; }
	Vector2 operator - () const { return Vector2(-x, -y); }

	Vector2 operator + (const Vector2& v) const { return Vector2(x + v.x, y + v.y); }
	Vector2 operator - (const Vector2& v) const { return Vector2(x - v.x, y - v.y); }
	Vector2 operator * (float f) const { return Vector2(x * f, y * f); }
	Vector2 operator / (float f) const { float inv = 1.0f / f; return Vector2(x * inv, y * inv); }

	friend Vector2 operator * (float f, const Vector2& v) { return Vector2(f * v.x, f * v.y); }

	bool operator == (const Vector2& v) const { return x == v.x && y == v.y; }
	bool operator != (const Vector2& v) const { return x != v.x || y != v.y; }

	float Length() const { return sqrtf(x * x + y * y); }
	float LengthSq() const { return x * x + y * y; }
	void Normalize() { float len = Length(); if (len > 0) { x /= len; y /= len; } }
};

//////////////////////////////////////////////////////////////////////////
// Vector3
//////////////////////////////////////////////////////////////////////////
struct Vector3 : public XMFLOAT3
{
	Vector3() : XMFLOAT3(0.f, 0.f, 0.f) {}
	Vector3(float _x, float _y, float _z) : XMFLOAT3(_x, _y, _z) {}
	Vector3(const float* pf) : XMFLOAT3(pf[0], pf[1], pf[2]) {}
	Vector3(const XMFLOAT3& v) : XMFLOAT3(v) {}
	Vector3(FXMVECTOR v) { XMStoreFloat3(this, v); }

	operator float* () { return reinterpret_cast<float*>(this); }
	operator const float* () const { return reinterpret_cast<const float*>(this); }
	operator XMVECTOR() const { return XMLoadFloat3(this); }

	Vector3& operator += (const Vector3& v) { x += v.x; y += v.y; z += v.z; return *this; }
	Vector3& operator -= (const Vector3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
	Vector3& operator *= (float f) { x *= f; y *= f; z *= f; return *this; }
	Vector3& operator /= (float f) { float inv = 1.0f / f; x *= inv; y *= inv; z *= inv; return *this; }

	Vector3 operator + () const { return *this; }
	Vector3 operator - () const { return Vector3(-x, -y, -z); }

	Vector3 operator + (const Vector3& v) const { return Vector3(x + v.x, y + v.y, z + v.z); }
	Vector3 operator - (const Vector3& v) const { return Vector3(x - v.x, y - v.y, z - v.z); }
	Vector3 operator * (float f) const { return Vector3(x * f, y * f, z * f); }
	Vector3 operator / (float f) const { float inv = 1.0f / f; return Vector3(x * inv, y * inv, z * inv); }

	friend Vector3 operator * (float f, const Vector3& v) { return Vector3(f * v.x, f * v.y, f * v.z); }

	bool operator == (const Vector3& v) const { return x == v.x && y == v.y && z == v.z; }
	bool operator != (const Vector3& v) const { return x != v.x || y != v.y || z != v.z; }

	float Length() const { return sqrtf(x * x + y * y + z * z); }
	float LengthSq() const { return x * x + y * y + z * z; }
	void Normalize() { float len = Length(); if (len > 0) { x /= len; y /= len; z /= len; } }
	float Dot(const Vector3& v) const { return x * v.x + y * v.y + z * v.z; }
	Vector3 Cross(const Vector3& v) const { return Vector3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x); }
};

//////////////////////////////////////////////////////////////////////////
// Vector4
//////////////////////////////////////////////////////////////////////////
struct Vector4 : public XMFLOAT4
{
	Vector4() : XMFLOAT4(0.f, 0.f, 0.f, 0.f) {}
	Vector4(float _x, float _y, float _z, float _w) : XMFLOAT4(_x, _y, _z, _w) {}
	Vector4(const float* pf) : XMFLOAT4(pf[0], pf[1], pf[2], pf[3]) {}
	Vector4(const XMFLOAT4& v) : XMFLOAT4(v) {}
	Vector4(const Vector3& v, float _w) : XMFLOAT4(v.x, v.y, v.z, _w) {}
	Vector4(FXMVECTOR v) { XMStoreFloat4(this, v); }

	operator float* () { return reinterpret_cast<float*>(this); }
	operator const float* () const { return reinterpret_cast<const float*>(this); }
	operator XMVECTOR() const { return XMLoadFloat4(this); }

	Vector4& operator += (const Vector4& v) { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
	Vector4& operator -= (const Vector4& v) { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
	Vector4& operator *= (float f) { x *= f; y *= f; z *= f; w *= f; return *this; }
	Vector4& operator /= (float f) { float inv = 1.0f / f; x *= inv; y *= inv; z *= inv; w *= inv; return *this; }

	Vector4 operator + () const { return *this; }
	Vector4 operator - () const { return Vector4(-x, -y, -z, -w); }

	Vector4 operator + (const Vector4& v) const { return Vector4(x + v.x, y + v.y, z + v.z, w + v.w); }
	Vector4 operator - (const Vector4& v) const { return Vector4(x - v.x, y - v.y, z - v.z, w - v.w); }
	Vector4 operator * (float f) const { return Vector4(x * f, y * f, z * f, w * f); }
	Vector4 operator / (float f) const { float inv = 1.0f / f; return Vector4(x * inv, y * inv, z * inv, w * inv); }

	friend Vector4 operator * (float f, const Vector4& v) { return Vector4(f * v.x, f * v.y, f * v.z, f * v.w); }

	bool operator == (const Vector4& v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }
	bool operator != (const Vector4& v) const { return x != v.x || y != v.y || z != v.z || w != v.w; }
};

//////////////////////////////////////////////////////////////////////////
// Color (RGBA float)
//////////////////////////////////////////////////////////////////////////
struct Color
{
	float r, g, b, a;

	Color() : r(0.f), g(0.f), b(0.f), a(1.f) {}
	Color(float _r, float _g, float _b, float _a = 1.0f) : r(_r), g(_g), b(_b), a(_a) {}
	Color(DWORD argb)
	{
		const float f = 1.0f / 255.0f;
		r = f * (float)(unsigned char)(argb >> 16);
		g = f * (float)(unsigned char)(argb >> 8);
		b = f * (float)(unsigned char)(argb);
		a = f * (float)(unsigned char)(argb >> 24);
	}
	Color(const XMFLOAT4& v) : r(v.x), g(v.y), b(v.z), a(v.w) {}
	Color(FXMVECTOR v) { XMFLOAT4 f; XMStoreFloat4(&f, v); r = f.x; g = f.y; b = f.z; a = f.w; }

	operator float* () { return reinterpret_cast<float*>(this); }
	operator const float* () const { return reinterpret_cast<const float*>(this); }
	operator XMVECTOR() const { return XMVectorSet(r, g, b, a); }
	operator DWORD() const
	{
		DWORD _r = r >= 1.0f ? 0xff : r <= 0.0f ? 0x00 : (DWORD)(r * 255.0f + 0.5f);
		DWORD _g = g >= 1.0f ? 0xff : g <= 0.0f ? 0x00 : (DWORD)(g * 255.0f + 0.5f);
		DWORD _b = b >= 1.0f ? 0xff : b <= 0.0f ? 0x00 : (DWORD)(b * 255.0f + 0.5f);
		DWORD _a = a >= 1.0f ? 0xff : a <= 0.0f ? 0x00 : (DWORD)(a * 255.0f + 0.5f);
		return (_a << 24) | (_r << 16) | (_g << 8) | _b;
	}
	operator XMFLOAT4() const { return XMFLOAT4(r, g, b, a); }

	Color& operator += (const Color& c) { r += c.r; g += c.g; b += c.b; a += c.a; return *this; }
	Color& operator -= (const Color& c) { r -= c.r; g -= c.g; b -= c.b; a -= c.a; return *this; }
	Color& operator *= (float f) { r *= f; g *= f; b *= f; a *= f; return *this; }
	Color& operator /= (float f) { float inv = 1.0f / f; r *= inv; g *= inv; b *= inv; a *= inv; return *this; }

	Color operator + (const Color& c) const { return Color(r + c.r, g + c.g, b + c.b, a + c.a); }
	Color operator - (const Color& c) const { return Color(r - c.r, g - c.g, b - c.b, a - c.a); }
	Color operator * (float f) const { return Color(r * f, g * f, b * f, a * f); }
	Color operator / (float f) const { float inv = 1.0f / f; return Color(r * inv, g * inv, b * inv, a * inv); }

	friend Color operator * (float f, const Color& c) { return Color(f * c.r, f * c.g, f * c.b, f * c.a); }

	bool operator == (const Color& c) const { return r == c.r && g == c.g && b == c.b && a == c.a; }
	bool operator != (const Color& c) const { return r != c.r || g != c.g || b != c.b || a != c.a; }
};

// Packed 32-bit color helper
inline DWORD PackColor(BYTE a, BYTE r, BYTE g, BYTE b)
{
	return ((DWORD)(a) << 24) | ((DWORD)(r) << 16) | ((DWORD)(g) << 8) | (DWORD)(b);
}

inline DWORD PackColorFromFloat(float r, float g, float b, float a)
{
	DWORD _r = r >= 1.0f ? 0xff : r <= 0.0f ? 0x00 : (DWORD)(r * 255.0f + 0.5f);
	DWORD _g = g >= 1.0f ? 0xff : g <= 0.0f ? 0x00 : (DWORD)(g * 255.0f + 0.5f);
	DWORD _b = b >= 1.0f ? 0xff : b <= 0.0f ? 0x00 : (DWORD)(b * 255.0f + 0.5f);
	DWORD _a = a >= 1.0f ? 0xff : a <= 0.0f ? 0x00 : (DWORD)(a * 255.0f + 0.5f);
	return (_a << 24) | (_r << 16) | (_g << 8) | _b;
}

//////////////////////////////////////////////////////////////////////////
// Matrix (4x4)
//////////////////////////////////////////////////////////////////////////
struct Matrix : public XMFLOAT4X4
{
	Matrix()
	{
		_11 = 1.f; _12 = 0.f; _13 = 0.f; _14 = 0.f;
		_21 = 0.f; _22 = 1.f; _23 = 0.f; _24 = 0.f;
		_31 = 0.f; _32 = 0.f; _33 = 1.f; _34 = 0.f;
		_41 = 0.f; _42 = 0.f; _43 = 0.f; _44 = 1.f;
	}
	Matrix(float m11, float m12, float m13, float m14,
		   float m21, float m22, float m23, float m24,
		   float m31, float m32, float m33, float m34,
		   float m41, float m42, float m43, float m44)
		: XMFLOAT4X4(m11, m12, m13, m14, m21, m22, m23, m24, m31, m32, m33, m34, m41, m42, m43, m44) {}
	Matrix(const float* pf) : XMFLOAT4X4(pf) {}
	Matrix(const XMFLOAT4X4& m) : XMFLOAT4X4(m) {}
	Matrix(CXMMATRIX m) { XMStoreFloat4x4(this, m); }

	float& operator () (UINT row, UINT col) { return m[row][col]; }
	float operator () (UINT row, UINT col) const { return m[row][col]; }

	operator float* () { return reinterpret_cast<float*>(this); }
	operator const float* () const { return reinterpret_cast<const float*>(this); }
	operator XMMATRIX() const { return XMLoadFloat4x4(this); }

	Matrix operator * (const Matrix& mat) const
	{
		Matrix result;
		XMStoreFloat4x4(&result, XMMatrixMultiply(XMLoadFloat4x4(this), XMLoadFloat4x4(&mat)));
		return result;
	}
	Matrix& operator *= (const Matrix& mat)
	{
		XMStoreFloat4x4(this, XMMatrixMultiply(XMLoadFloat4x4(this), XMLoadFloat4x4(&mat)));
		return *this;
	}

	bool operator == (const Matrix& mat) const { return memcmp(this, &mat, sizeof(Matrix)) == 0; }
	bool operator != (const Matrix& mat) const { return memcmp(this, &mat, sizeof(Matrix)) != 0; }

	static Matrix Identity()
	{
		Matrix m;
		XMStoreFloat4x4(&m, XMMatrixIdentity());
		return m;
	}

	void SetIdentity() { XMStoreFloat4x4(this, XMMatrixIdentity()); }
	Matrix Transpose() const { Matrix r; XMStoreFloat4x4(&r, XMMatrixTranspose(XMLoadFloat4x4(this))); return r; }
	Matrix Inverse() const { Matrix r; XMStoreFloat4x4(&r, XMMatrixInverse(nullptr, XMLoadFloat4x4(this))); return r; }
};

//////////////////////////////////////////////////////////////////////////
// Quaternion
//////////////////////////////////////////////////////////////////////////
struct Quaternion : public XMFLOAT4
{
	Quaternion() : XMFLOAT4(0.f, 0.f, 0.f, 1.f) {}
	Quaternion(float _x, float _y, float _z, float _w) : XMFLOAT4(_x, _y, _z, _w) {}
	Quaternion(const float* pf) : XMFLOAT4(pf[0], pf[1], pf[2], pf[3]) {}
	Quaternion(const XMFLOAT4& q) : XMFLOAT4(q) {}
	Quaternion(FXMVECTOR v) { XMStoreFloat4(this, v); }

	operator float* () { return reinterpret_cast<float*>(this); }
	operator const float* () const { return reinterpret_cast<const float*>(this); }
	operator XMVECTOR() const { return XMLoadFloat4(this); }

	Quaternion operator * (const Quaternion& q) const
	{
		Quaternion result;
		XMStoreFloat4(&result, XMQuaternionMultiply(XMLoadFloat4(this), XMLoadFloat4(&q)));
		return result;
	}
	Quaternion& operator *= (const Quaternion& q)
	{
		XMStoreFloat4(this, XMQuaternionMultiply(XMLoadFloat4(this), XMLoadFloat4(&q)));
		return *this;
	}

	bool operator == (const Quaternion& q) const { return x == q.x && y == q.y && z == q.z && w == q.w; }
	bool operator != (const Quaternion& q) const { return x != q.x || y != q.y || z != q.z || w != q.w; }

	static Quaternion Identity() { return Quaternion(0.f, 0.f, 0.f, 1.f); }

	void SetIdentity() { x = y = z = 0.f; w = 1.f; }
	void Normalize() { XMStoreFloat4(this, XMQuaternionNormalize(XMLoadFloat4(this))); }
	Quaternion Conjugate() const { return Quaternion(-x, -y, -z, w); }
	Quaternion Inverse() const { Quaternion q; XMStoreFloat4(&q, XMQuaternionInverse(XMLoadFloat4(this))); return q; }
};

//////////////////////////////////////////////////////////////////////////
// Plane
//////////////////////////////////////////////////////////////////////////
struct Plane
{
	float a, b, c, d;

	Plane() : a(0.f), b(1.f), c(0.f), d(0.f) {}
	Plane(float _a, float _b, float _c, float _d) : a(_a), b(_b), c(_c), d(_d) {}
	Plane(const float* pf) : a(pf[0]), b(pf[1]), c(pf[2]), d(pf[3]) {}
	Plane(const XMFLOAT4& p) : a(p.x), b(p.y), c(p.z), d(p.w) {}
	Plane(FXMVECTOR v) { XMFLOAT4 f; XMStoreFloat4(&f, v); a = f.x; b = f.y; c = f.z; d = f.w; }

	operator float* () { return reinterpret_cast<float*>(this); }
	operator const float* () const { return reinterpret_cast<const float*>(this); }
	operator XMVECTOR() const { return XMVectorSet(a, b, c, d); }
	operator XMFLOAT4() const { return XMFLOAT4(a, b, c, d); }

	bool operator == (const Plane& p) const { return a == p.a && b == p.b && c == p.c && d == p.d; }
	bool operator != (const Plane& p) const { return a != p.a || b != p.b || c != p.c || d != p.d; }

	void Normalize()
	{
		XMFLOAT4 f(a, b, c, d);
		XMVECTOR v = XMPlaneNormalize(XMLoadFloat4(&f));
		XMStoreFloat4(&f, v);
		a = f.x; b = f.y; c = f.z; d = f.w;
	}
	float DotCoord(const Vector3& v) const
	{
		return XMVectorGetX(XMPlaneDotCoord(XMVectorSet(a, b, c, d), XMLoadFloat3(&v)));
	}
	float DotNormal(const Vector3& v) const
	{
		return XMVectorGetX(XMPlaneDotNormal(XMVectorSet(a, b, c, d), XMLoadFloat3(&v)));
	}
};
