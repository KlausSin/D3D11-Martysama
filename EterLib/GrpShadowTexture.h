#pragma once

#include "GrpTexture.h"

class CGraphicShadowTexture : public CGraphicTexture
{
	public:
		CGraphicShadowTexture();
		virtual ~CGraphicShadowTexture();

		void Destroy();

		bool Create(int width, int height);

		void Begin();
		void End();
		void Set(int stage = 0) const;

		const Matrix& GetLightVPMatrixReference() const;
		ID3D11ShaderResourceView* GetD3DTexture() const;

	protected:
		void Initialize();

	protected:
		Matrix					m_d3dLightVPMatrix;
		D3D11_VIEWPORT				m_d3dOldViewport;

		// DX11 render target resources
		ID3D11Texture2D*			m_pShadowTexture;
		ID3D11RenderTargetView*		m_pShadowRTV;
		ID3D11ShaderResourceView*	m_pShadowSRV;

		// Depth buffer for shadow rendering
		ID3D11Texture2D*			m_pDepthTexture;
		ID3D11DepthStencilView*		m_pDepthDSV;

		// Cached old render targets
		ID3D11RenderTargetView*		m_pOldRTV;
		ID3D11DepthStencilView*		m_pOldDSV;
};
