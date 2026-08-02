#pragma once

#include "../EterLib/GrpScreen.h"

class CSnowParticle;

class CSnowEnvironment : public CScreen
{
	public:
		CSnowEnvironment();
		virtual ~CSnowEnvironment();

		bool Create();
		void Destroy();

		void Enable();
		void Disable();

		void Update(const Vector3 & c_rv3Pos);
		void Deform();
		void Render();

	protected:
		void __Initialize();
		bool __CreateBlurTexture();
		void __BeginBlur();
		void __ApplyBlur();

	protected:
		ID3D11RenderTargetView* m_lpOldSurface;
		ID3D11DepthStencilView* m_lpOldDepthStencilSurface;

		ID3D11ShaderResourceView* m_lpSnowTexture;
		ID3D11RenderTargetView* m_lpSnowRenderTargetSurface;
		ID3D11DepthStencilView* m_lpSnowDepthSurface;

		ID3D11ShaderResourceView* m_lpAccumTexture;
		ID3D11RenderTargetView* m_lpAccumRenderTargetSurface;
		ID3D11DepthStencilView* m_lpAccumDepthSurface;

		Vector3 m_v3Center;

		WORD m_wBlurTextureSize;
		CGraphicImageInstance * m_pImageInstance;
		std::vector<CSnowParticle*> m_kVct_pkParticleSnow;

		DWORD m_dwParticleMaxNum;
		BOOL m_bBlurEnable;

		BOOL m_bSnowEnable;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
