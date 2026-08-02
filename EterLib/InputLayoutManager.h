#pragma once

/*
 * InputLayoutManager.h
 * DX11 Input Layout Management System
 *
 * Manages creation and binding of ID3D11InputLayout objects.
 * Input layouts define how vertex data is fed to the input-assembler stage.
 */

#include <d3d11.h>
#include <unordered_map>
#include <string>
#include "GrpBase.h"
#include "../eterBase/Singleton.h"

//////////////////////////////////////////////////////////////////////////
// Input Layout Manager
//////////////////////////////////////////////////////////////////////////

class CInputLayoutManager : public CSingleton<CInputLayoutManager>
{
public:
	CInputLayoutManager();
	~CInputLayoutManager();

	// Initialize with device pointer
	bool Initialize(ID3D11Device* pDevice);
	void Destroy();

	bool CreateInputLayout(EInputLayoutType eType, const void* pShaderBytecode, SIZE_T bytecodeLength);

	bool CreateAllLayouts(const void* pShaderBytecode, SIZE_T bytecodeLength);

	// Bind an input layout to the input-assembler stage
	void BindInputLayout(EInputLayoutType eType);

	// Get raw input layout pointer (for custom binding)
	ID3D11InputLayout* GetInputLayout(EInputLayoutType eType);

	// Check if a layout exists
	bool HasInputLayout(EInputLayoutType eType) const;

	// Get vertex stride for a layout type
	UINT GetVertexStride(EInputLayoutType eType) const;

	static const D3D11_INPUT_ELEMENT_DESC* GetInputElements(EInputLayoutType eType, UINT* pElementCount);

private:
	// Create a single input layout
	bool CreateLayoutInternal(EInputLayoutType eType,
		const D3D11_INPUT_ELEMENT_DESC* pElements,
		UINT numElements,
		const void* pShaderBytecode,
		SIZE_T bytecodeLength);

	// Release all input layouts
	void ReleaseAllLayouts();

private:
	ID3D11Device*		m_pDevice;
	ID3D11InputLayout*	m_pInputLayouts[INPUT_LAYOUT_COUNT];
	bool				m_bInitialized;
};

#define INPUTLAYOUTMANAGER CInputLayoutManager::Instance()

//////////////////////////////////////////////////////////////////////////
// Implementation
//////////////////////////////////////////////////////////////////////////

inline CInputLayoutManager::CInputLayoutManager()
	: m_pDevice(nullptr)
	, m_bInitialized(false)
{
	for (int i = 0; i < INPUT_LAYOUT_COUNT; ++i)
		m_pInputLayouts[i] = nullptr;
}

inline CInputLayoutManager::~CInputLayoutManager()
{
	Destroy();
}

inline bool CInputLayoutManager::Initialize(ID3D11Device* pDevice)
{
	if (!pDevice)
		return false;

	m_pDevice = pDevice;
	m_bInitialized = true;
	return true;
}

inline void CInputLayoutManager::Destroy()
{
	ReleaseAllLayouts();
	m_pDevice = nullptr;
	m_bInitialized = false;
}

inline void CInputLayoutManager::ReleaseAllLayouts()
{
	for (int i = 0; i < INPUT_LAYOUT_COUNT; ++i)
	{
		if (m_pInputLayouts[i])
		{
			m_pInputLayouts[i]->Release();
			m_pInputLayouts[i] = nullptr;
		}
	}
}

inline const D3D11_INPUT_ELEMENT_DESC* CInputLayoutManager::GetInputElements(EInputLayoutType eType, UINT* pElementCount)
{
	switch (eType)
	{
	case INPUT_LAYOUT_PDT:
		*pElementCount = InputLayouts::PDT_ELEMENT_COUNT;
		return InputLayouts::PDT_ELEMENTS;

	case INPUT_LAYOUT_PNT:
		*pElementCount = InputLayouts::PNT_ELEMENT_COUNT;
		return InputLayouts::PNT_ELEMENTS;

	case INPUT_LAYOUT_PNT2:
		*pElementCount = InputLayouts::PNT2_ELEMENT_COUNT;
		return InputLayouts::PNT2_ELEMENTS;

	case INPUT_LAYOUT_PT:
		*pElementCount = InputLayouts::PT_ELEMENT_COUNT;
		return InputLayouts::PT_ELEMENTS;

	case INPUT_LAYOUT_PD:
		*pElementCount = InputLayouts::PD_ELEMENT_COUNT;
		return InputLayouts::PD_ELEMENTS;

	case INPUT_LAYOUT_PDT2:
		*pElementCount = InputLayouts::PDT2_ELEMENT_COUNT;
		return InputLayouts::PDT2_ELEMENTS;

	case INPUT_LAYOUT_SKINNED:
		*pElementCount = InputLayouts::SKINNED_ELEMENT_COUNT;
		return InputLayouts::SKINNED_ELEMENTS;

	case INPUT_LAYOUT_TRANSFORMED:
		*pElementCount = InputLayouts::TRANSFORMED_ELEMENT_COUNT;
		return InputLayouts::TRANSFORMED_ELEMENTS;

	case INPUT_LAYOUT_PN:
		*pElementCount = InputLayouts::PN_ELEMENT_COUNT;
		return InputLayouts::PN_ELEMENTS;

	default:
		*pElementCount = 0;
		return nullptr;
	}
}

inline UINT CInputLayoutManager::GetVertexStride(EInputLayoutType eType) const
{
	switch (eType)
	{
	case INPUT_LAYOUT_PDT:
		return sizeof(TPDTVertex);  // 24 bytes: float3 + DWORD + float2

	case INPUT_LAYOUT_PNT:
		return sizeof(TPNTVertex);  // 32 bytes: float3 + float3 + float2

	case INPUT_LAYOUT_PNT2:
		return sizeof(TPNT2Vertex); // 40 bytes: float3 + float3 + float2 + float2

	case INPUT_LAYOUT_PT:
		return sizeof(TPTVertex);   // 20 bytes: float3 + float2

	case INPUT_LAYOUT_PD:
		return sizeof(TPDVertex);   // 16 bytes: float3 + DWORD

	case INPUT_LAYOUT_PDT2:
		return sizeof(TPDT2Vertex); // 32 bytes: float3 + DWORD + float2 + float2

	case INPUT_LAYOUT_SKINNED:
		return sizeof(TSkinnedVertex);  // 52 bytes: Position + Normal + TexCoord + BlendWeights + BlendIndices

	case INPUT_LAYOUT_TRANSFORMED:
		return VertexStride::TRANSFORMED;  // 24 bytes: float3 pos + DWORD diffuse + float2 texcoord

	case INPUT_LAYOUT_PN:
		return 24;  // 24 bytes: float3 + float3 (Position + Normal, no texcoords)

	default:
		return 0;
	}
}

inline bool CInputLayoutManager::CreateLayoutInternal(
	EInputLayoutType eType,
	const D3D11_INPUT_ELEMENT_DESC* pElements,
	UINT numElements,
	const void* pShaderBytecode,
	SIZE_T bytecodeLength)
{
	if (!m_pDevice || !pElements || numElements == 0 || !pShaderBytecode || bytecodeLength == 0)
		return false;

	// Release existing layout if any
	if (m_pInputLayouts[eType])
	{
		m_pInputLayouts[eType]->Release();
		m_pInputLayouts[eType] = nullptr;
	}

	HRESULT hr = m_pDevice->CreateInputLayout(
		pElements,
		numElements,
		pShaderBytecode,
		bytecodeLength,
		&m_pInputLayouts[eType]
	);

	return SUCCEEDED(hr);
}

inline bool CInputLayoutManager::CreateInputLayout(EInputLayoutType eType, const void* pShaderBytecode, SIZE_T bytecodeLength)
{
	UINT elementCount = 0;
	const D3D11_INPUT_ELEMENT_DESC* pElements = GetInputElements(eType, &elementCount);

	if (!pElements)
		return false;

	return CreateLayoutInternal(eType, pElements, elementCount, pShaderBytecode, bytecodeLength);
}

inline bool CInputLayoutManager::CreateAllLayouts(const void* pShaderBytecode, SIZE_T bytecodeLength)
{
	bool bAllSucceeded = true;

	for (int i = 0; i < INPUT_LAYOUT_COUNT; ++i)
	{
		if (!CreateInputLayout(static_cast<EInputLayoutType>(i), pShaderBytecode, bytecodeLength))
		{
			bAllSucceeded = false;
		}
	}

	return bAllSucceeded;
}

inline void CInputLayoutManager::BindInputLayout(EInputLayoutType eType)
{
	if (eType >= 0 && eType < INPUT_LAYOUT_COUNT && m_pInputLayouts[eType])
	{
		ID3D11DeviceContext* pContext = CGraphicBase::GetContext();
		if (pContext)
			pContext->IASetInputLayout(m_pInputLayouts[eType]);
	}
}

inline ID3D11InputLayout* CInputLayoutManager::GetInputLayout(EInputLayoutType eType)
{
	if (eType >= 0 && eType < INPUT_LAYOUT_COUNT)
		return m_pInputLayouts[eType];
	return nullptr;
}

inline bool CInputLayoutManager::HasInputLayout(EInputLayoutType eType) const
{
	if (eType >= 0 && eType < INPUT_LAYOUT_COUNT)
		return m_pInputLayouts[eType] != nullptr;
	return false;
}
