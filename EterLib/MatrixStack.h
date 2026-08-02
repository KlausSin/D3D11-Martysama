#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include "GrpMathType.h"

using namespace DirectX;

class CMatrixStack
{
public:
	CMatrixStack();
	~CMatrixStack();

	// Stack operations
	void Push();
	void Pop();
	void Reset();  // Clear stack and load identity

	// Load/Get current matrix
	void LoadIdentity();
	void LoadMatrix(const Matrix* pMat);
	void LoadMatrix(const XMFLOAT4X4* pMat);  // Legacy overload
	void GetTop(Matrix* pMat) const;
	void GetTop(XMFLOAT4X4* pMat) const;  // Legacy overload
	const Matrix* GetTop() const;

	// Matrix multiplication
	void MultMatrix(const Matrix* pMat);
	void MultMatrix(const XMFLOAT4X4* pMat);  // Legacy overload
	void MultMatrixLocal(const Matrix* pMat);
	void MultMatrixLocal(const XMFLOAT4X4* pMat);  // Legacy overload

	// Transformations (applied in world space)
	void RotateAxis(const XMFLOAT3* pV, float angle);
	void RotateAxisLocal(const XMFLOAT3* pV, float angle);
	void RotateYawPitchRoll(float yaw, float pitch, float roll);
	void RotateYawPitchRollLocal(float yaw, float pitch, float roll);
	void Scale(float x, float y, float z);
	void ScaleLocal(float x, float y, float z);
	void Translate(float x, float y, float z);
	void TranslateLocal(float x, float y, float z);

private:
	std::vector<Matrix> m_Stack;
	Matrix m_CurrentMatrix;
};

// Global creation function
CMatrixStack* CreateMatrixStack();
