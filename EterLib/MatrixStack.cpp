#include "StdAfx.h"
#include "MatrixStack.h"

CMatrixStack::CMatrixStack()
{
	LoadIdentity();
	m_Stack.reserve(16);
}

CMatrixStack::~CMatrixStack()
{
}

void CMatrixStack::Push()
{
	m_Stack.push_back(m_CurrentMatrix);
}

void CMatrixStack::Pop()
{
	if (!m_Stack.empty())
	{
		m_CurrentMatrix = m_Stack.back();
		m_Stack.pop_back();
	}
}

void CMatrixStack::Reset()
{
	m_Stack.clear();
	LoadIdentity();
}

void CMatrixStack::LoadIdentity()
{
	XMStoreFloat4x4(&m_CurrentMatrix, XMMatrixIdentity());
}

void CMatrixStack::LoadMatrix(const Matrix* pMat)
{
	m_CurrentMatrix = *pMat;
}

void CMatrixStack::LoadMatrix(const XMFLOAT4X4* pMat)
{
	m_CurrentMatrix = *static_cast<const Matrix*>(static_cast<const void*>(pMat));
}

void CMatrixStack::GetTop(Matrix* pMat) const
{
	*pMat = m_CurrentMatrix;
}

void CMatrixStack::GetTop(XMFLOAT4X4* pMat) const
{
	*pMat = m_CurrentMatrix;
}

const Matrix* CMatrixStack::GetTop() const
{
	return &m_CurrentMatrix;
}

void CMatrixStack::MultMatrix(const Matrix* pMat)
{
	XMMATRIX current = XMLoadFloat4x4(&m_CurrentMatrix);
	XMMATRIX mult = XMLoadFloat4x4(pMat);
	XMStoreFloat4x4(&m_CurrentMatrix, XMMatrixMultiply(mult, current));
}

void CMatrixStack::MultMatrix(const XMFLOAT4X4* pMat)
{
	XMMATRIX current = XMLoadFloat4x4(&m_CurrentMatrix);
	XMMATRIX mult = XMLoadFloat4x4(pMat);
	XMStoreFloat4x4(&m_CurrentMatrix, XMMatrixMultiply(mult, current));
}

void CMatrixStack::MultMatrixLocal(const Matrix* pMat)
{
	XMMATRIX current = XMLoadFloat4x4(&m_CurrentMatrix);
	XMMATRIX mult = XMLoadFloat4x4(pMat);
	XMStoreFloat4x4(&m_CurrentMatrix, XMMatrixMultiply(current, mult));
}

void CMatrixStack::MultMatrixLocal(const XMFLOAT4X4* pMat)
{
	XMMATRIX current = XMLoadFloat4x4(&m_CurrentMatrix);
	XMMATRIX mult = XMLoadFloat4x4(pMat);
	XMStoreFloat4x4(&m_CurrentMatrix, XMMatrixMultiply(current, mult));
}

void CMatrixStack::RotateAxis(const XMFLOAT3* pV, float angle)
{
	XMMATRIX current = XMLoadFloat4x4(&m_CurrentMatrix);
	XMVECTOR axis = XMLoadFloat3(pV);
	XMMATRIX rotation = XMMatrixRotationAxis(axis, angle);
	XMStoreFloat4x4(&m_CurrentMatrix, XMMatrixMultiply(rotation, current));
}

void CMatrixStack::RotateAxisLocal(const XMFLOAT3* pV, float angle)
{
	XMMATRIX current = XMLoadFloat4x4(&m_CurrentMatrix);
	XMVECTOR axis = XMLoadFloat3(pV);
	XMMATRIX rotation = XMMatrixRotationAxis(axis, angle);
	XMStoreFloat4x4(&m_CurrentMatrix, XMMatrixMultiply(current, rotation));
}

void CMatrixStack::RotateYawPitchRoll(float yaw, float pitch, float roll)
{
	XMMATRIX current = XMLoadFloat4x4(&m_CurrentMatrix);
	XMMATRIX rotation = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
	XMStoreFloat4x4(&m_CurrentMatrix, XMMatrixMultiply(rotation, current));
}

void CMatrixStack::RotateYawPitchRollLocal(float yaw, float pitch, float roll)
{
	XMMATRIX current = XMLoadFloat4x4(&m_CurrentMatrix);
	XMMATRIX rotation = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
	XMStoreFloat4x4(&m_CurrentMatrix, XMMatrixMultiply(current, rotation));
}

void CMatrixStack::Scale(float x, float y, float z)
{
	XMMATRIX current = XMLoadFloat4x4(&m_CurrentMatrix);
	XMMATRIX scale = XMMatrixScaling(x, y, z);
	XMStoreFloat4x4(&m_CurrentMatrix, XMMatrixMultiply(scale, current));
}

void CMatrixStack::ScaleLocal(float x, float y, float z)
{
	XMMATRIX current = XMLoadFloat4x4(&m_CurrentMatrix);
	XMMATRIX scale = XMMatrixScaling(x, y, z);
	XMStoreFloat4x4(&m_CurrentMatrix, XMMatrixMultiply(current, scale));
}

void CMatrixStack::Translate(float x, float y, float z)
{
	XMMATRIX current = XMLoadFloat4x4(&m_CurrentMatrix);
	XMMATRIX translation = XMMatrixTranslation(x, y, z);
	XMStoreFloat4x4(&m_CurrentMatrix, XMMatrixMultiply(translation, current));
}

void CMatrixStack::TranslateLocal(float x, float y, float z)
{
	XMMATRIX current = XMLoadFloat4x4(&m_CurrentMatrix);
	XMMATRIX translation = XMMatrixTranslation(x, y, z);
	XMStoreFloat4x4(&m_CurrentMatrix, XMMatrixMultiply(current, translation));
}

CMatrixStack* CreateMatrixStack()
{
	return new CMatrixStack();
}
