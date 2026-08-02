#pragma once

#include "GrpTexture.h"
#include "../eterImageLib/DXTCImage.h"

class CGraphicImageTexture : public CGraphicTexture
{
	public:
		CGraphicImageTexture();
		virtual ~CGraphicImageTexture();

		void		Destroy();

		bool		Create(UINT width, UINT height, DXGI_FORMAT dxgiFmt, DWORD dwFilter = 0);
		bool		CreateDeviceObjects();

		void		CreateFromTexturePointer(const CGraphicTexture* c_pSrcTexture);
		bool		CreateFromDiskFile(const char* c_szFileName, DXGI_FORMAT dxgiFmt, DWORD dwFilter = 0);
		bool		CreateFromMemoryFile(UINT bufSize, const void* c_pvBuf, DXGI_FORMAT dxgiFmt, const char* fileName, DWORD dwFilter = 0);
		bool		CreateDDSTexture(CDXTCImage & image, const BYTE * c_pbBuf);

		void		SetFileName(const char * c_szFileName);

		bool		Lock(int* pRetPitch, void** ppRetPixels, int level=0);
		void		Unlock(int level=0);

	protected:
		void		Initialize();

		DXGI_FORMAT	m_dxgiFmt;
		DWORD		m_dwFilter;

		std::string m_stFileName;

		// Staging texture for Lock/Unlock
		ID3D11Texture2D* m_pStagingTexture;
};
