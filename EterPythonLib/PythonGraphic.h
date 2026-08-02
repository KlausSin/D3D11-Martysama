#pragma once

#include "../eterlib/GrpTextInstance.h"
#include "../eterlib/GrpMarkInstance.h"
#include "../eterlib/GrpImageInstance.h"
#include "../eterlib/GrpExpandedImageInstance.h"

#include "../eterGrnLib/ThingInstance.h"

class CPythonGraphic : public CScreen, public CSingleton<CPythonGraphic>
{
	public:
		CPythonGraphic();
		virtual ~CPythonGraphic();

		void Destroy();

		void PushState();
		void PopState();

		IDXGIFactory* GetDXGIFactory();

		float GetOrthoDepth();
		void SetInterfaceRenderState();
		void SetGameRenderState();

		void SetCursorPosition(int x, int y);

		void SetOmniLight();

		void SetViewport(float fx, float fy, float fWidth, float fHeight);
		void RestoreViewport();

		long GenerateColor(float r, float g, float b, float a);
		void RenderDownButton(float sx, float sy, float ex, float ey);
		void RenderUpButton(float sx, float sy, float ex, float ey);

		void RenderImage(CGraphicImageInstance* pImageInstance, float x, float y);
		void RenderAlphaImage(CGraphicImageInstance* pImageInstance, float x, float y, float aLeft, float aRight);
		void RenderCoolTimeBox(float fxCenter, float fyCenter, float fRadius, float fTime);

		bool SaveJPEG(const char * pszFileName, LPBYTE pbyBuffer, UINT uWidth, UINT uHeight);
		bool SaveScreenShot(const char *szFileName);

		DWORD GetAvailableMemory();
		void SetGamma(float fGammaFactor = 1.0f);

	protected:
		typedef struct SState
		{
			Matrix matView;
			Matrix matProj;
		} TState;

		DWORD		m_lightColor;
		DWORD		m_darkColor;

	protected:
		std::stack<TState>						m_stateStack;

		Matrix								m_SaveWorldMatrix;

		CCullingManager							m_CullingManager;

		D3D11_VIEWPORT							m_backupViewport;

		float									m_fOrthoDepth;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
