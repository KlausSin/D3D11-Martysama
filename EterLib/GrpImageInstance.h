#pragma once

#include "GrpImage.h"
#include "GrpIndexBuffer.h"
#include "GrpVertexBufferDynamic.h"
#include "Pool.h"
#include "../UserInterface/Locale_inc.h"

class CGraphicImageInstance
{
	public:
		static DWORD Type();
		BOOL IsType(DWORD dwType);

	public:
		CGraphicImageInstance();
		virtual ~CGraphicImageInstance();

		void Destroy();

#if defined(__BL_CLIP_MASK__)
		void Render(RECT* pClipRect = NULL);
#else
		void Render();
#endif

		void SetDiffuseColor(float fr, float fg, float fb, float fa);
		void SetPosition(float fx, float fy);

		void SetImagePointer(CGraphicImage* pImage);
		void ReloadImagePointer(CGraphicImage* pImage);
		bool IsEmpty() const;

		int GetWidth();
		int GetHeight();

		CGraphicTexture * GetTexturePointer();
		const CGraphicTexture &	GetTextureReference() const;
		CGraphicImage * GetGraphicImagePointer();

		bool operator == (const CGraphicImageInstance & rhs) const;

	protected:
		void Initialize();

#if defined(__BL_CLIP_MASK__)
		virtual void OnRender(RECT* pClipRect);
#else
		virtual void OnRender();
#endif
		virtual void OnSetImagePointer();

		virtual BOOL OnIsType(DWORD dwType);

	protected:
		Color m_DiffuseColor;
		Vector2 m_v2Position;

		CGraphicImage::TRef m_roImage;

	public:
		static void CreateSystem(UINT uCapacity);
		static void DestroySystem();

		static CGraphicImageInstance* New();
		static void Delete(CGraphicImageInstance* pkImgInst);

		static CDynamicPool<CGraphicImageInstance>		ms_kPool;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
