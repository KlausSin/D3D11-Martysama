#pragma once

/*
 * GrpTexture.h
 * DX11 Texture Implementation
 */

#include "GrpBase.h"

class CGraphicTexture : public CGraphicBase
{
public:
	virtual bool IsEmpty() const;

	int GetWidth() const;
	int GetHeight() const;
	DXGI_FORMAT GetFormat() const;

	// Bind texture to shader stage
	void SetTextureStage(int stage) const;

	// Bind to specific shader stages
	void BindToVS(UINT slot) const;
	void BindToPS(UINT slot) const;
	void BindToGS(UINT slot) const;

	// Unbind from shader stage
	static void UnbindFromPS(UINT slot);

	// Get DX11 resources
	ID3D11Texture2D* GetTexture() const;
	ID3D11ShaderResourceView* GetSRV() const;

	// Legacy accessor (returns SRV for compatibility)
	ID3D11ShaderResourceView* GetD3DTexture() const { return m_pSRV; }

	void DestroyDeviceObjects();

protected:
	CGraphicTexture();
	virtual ~CGraphicTexture();

	void Destroy();
	void Initialize();

	// Create SRV from texture
	bool CreateShaderResourceView();

protected:
	bool m_bEmpty;

	int m_width;
	int m_height;
	DXGI_FORMAT m_Format;

	ID3D11Texture2D* m_pTexture;
	ID3D11ShaderResourceView* m_pSRV;
};
