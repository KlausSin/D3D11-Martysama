#pragma once

/*
 * GrpVertexBuffer.h
 * DX11 Vertex Buffer Implementation
 */

#include "GrpBase.h"

class CGraphicVertexBuffer : public CGraphicBase
{
public:
	CGraphicVertexBuffer();
	virtual ~CGraphicVertexBuffer();

	void Destroy();

	bool Create(int vtxCount, UINT stride, D3D11_USAGE usage = D3D11_USAGE_DEFAULT, const void* pInitData = nullptr);

	bool Create(int vtxCount, EInputLayoutType layoutType, D3D11_USAGE usage = D3D11_USAGE_DEFAULT, const void* pInitData = nullptr);

	bool CreateDeviceObjects();
	void DestroyDeviceObjects();

	// Copy data to buffer (for DEFAULT usage buffers)
	bool Copy(int bufSize, const void* srcVertices);

	// Map/Unmap for dynamic buffers
	bool Map(void** ppData, D3D11_MAP mapType = D3D11_MAP_WRITE_DISCARD);
	void Unmap();

	// Const versions for compatibility
	bool Map(void** ppData, D3D11_MAP mapType = D3D11_MAP_WRITE_DISCARD) const;
	void Unmap() const;

	// Bind buffer to input assembler
	void Bind(UINT slot = 0, UINT offset = 0) const;

	// Legacy SetStream wrapper
	void SetStream(int stride, int layer = 0) const;

	// Accessors
	int GetVertexCount() const { return m_vtxCount; }
	UINT GetVertexStride() const { return m_uStride; }
	DWORD GetBufferSize() const { return m_dwBufferSize; }
	ID3D11Buffer* GetBuffer() const { return m_pBuffer; }
	ID3D11Buffer* GetD3DVertexBuffer() const { return m_pBuffer; }  // Legacy alias
	bool IsEmpty() const { return m_pBuffer == nullptr; }
	bool IsDynamic() const { return m_Usage == D3D11_USAGE_DYNAMIC; }

	// Get the input layout type this buffer was created for
	EInputLayoutType GetLayoutType() const { return m_LayoutType; }

protected:
	void Initialize();

protected:
	ID3D11Buffer*	m_pBuffer;
	DWORD			m_dwBufferSize;
	UINT			m_uStride;
	D3D11_USAGE		m_Usage;
	int				m_vtxCount;

	// Input layout type for this buffer
	EInputLayoutType m_LayoutType;

	// Mutable for const Map/Unmap
	mutable bool	m_bMapped;
};
