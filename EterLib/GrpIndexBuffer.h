#pragma once

/*
 * GrpIndexBuffer.h
 * DX11 Index Buffer Implementation
 */

#include "GrpBase.h"

class CGraphicIndexBuffer : public CGraphicBase
{
public:
	CGraphicIndexBuffer();
	virtual ~CGraphicIndexBuffer();

	void Destroy();

	bool Create(int idxCount, DXGI_FORMAT format = DXGI_FORMAT_R16_UINT, D3D11_USAGE usage = D3D11_USAGE_DEFAULT, const void* pInitData = nullptr);

	// Create from face data (16-bit indices)
	bool Create(int faceCount, TFace* faces);

	bool CreateDeviceObjects();
	void DestroyDeviceObjects();

	// Copy data to buffer (for DEFAULT usage buffers)
	bool Copy(int bufSize, const void* srcIndices);

	// Map/Unmap for dynamic buffers
	bool Map(void** ppData, D3D11_MAP mapType = D3D11_MAP_WRITE_DISCARD);
	void Unmap();

	// Const versions for compatibility
	bool Map(void** ppData, D3D11_MAP mapType = D3D11_MAP_WRITE_DISCARD) const;
	void Unmap() const;

	// Bind buffer to input assembler
	void Bind(UINT offset = 0) const;

	// Legacy SetIndexBuffer wrapper
	void SetIndexBuffer(int startIndex = 0) const;

	// Accessors
	int GetIndexCount() const { return m_idxCount; }
	DWORD GetBufferSize() const { return m_dwBufferSize; }
	ID3D11Buffer* GetBuffer() const { return m_pBuffer; }
	ID3D11Buffer* GetD3DIndexBuffer() const { return m_pBuffer; }  // Legacy alias
	DXGI_FORMAT GetFormat() const { return m_Format; }
	bool Is32Bit() const { return m_Format == DXGI_FORMAT_R32_UINT; }
	bool IsEmpty() const { return m_pBuffer == nullptr; }
	bool IsDynamic() const { return m_Usage == D3D11_USAGE_DYNAMIC; }

protected:
	void Initialize();

protected:
	ID3D11Buffer*	m_pBuffer;
	DWORD			m_dwBufferSize;
	DXGI_FORMAT		m_Format;
	D3D11_USAGE		m_Usage;
	int				m_idxCount;

	// Mutable for const Map/Unmap
	mutable bool	m_bMapped;
};
