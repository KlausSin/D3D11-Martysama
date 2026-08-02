#pragma once

/*
 * MovieMan.h
 * DX11 Native Video Playback System
 *
 * Uses Media Foundation Source Reader (Windows 7+) for video decoding.
 * Renders video frames directly via D3D11 (DYNAMIC texture + fullscreen quad).
 *
 * Replaces legacy DirectDraw/DirectShow Multimedia Streaming implementation.
 */

#include <d3d11.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include "../eterBase/Singleton.h"

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

#define MOVIEMAN_FADE_DURATION      1300
#define MOVIEMAN_SKIPPABLE_YES      true
#define MOVIEMAN_POSTEFFECT_FADEOUT 1

class CMovieMan : public CSingleton<CMovieMan>
{
public:
    CMovieMan();
    virtual ~CMovieMan();

    // Public interface (unchanged from original)
    void ClearToBlack();
    void PlayLogo(const char* pcszName);
    void PlayIntro();
    BOOL PlayTutorial(LONG nIdx);

private:
    // Initialization
    bool InitializeMediaFoundation();
    void ShutdownMediaFoundation();

    // Playback
    BOOL PlayMovie(const char* cpFileName, bool bSkipAllowed = false, int nPostEffectID = 0, DWORD dwPostEffectData = 0);
    void RenderPostEffectFadeOut(int fadeOutDuration, DWORD fadeOutColor);

    // D3D11 rendering
    bool CreateVideoTexture();
    void DestroyVideoTexture();
    void CalcMovieRect(int srcWidth, int srcHeight, RECT& movieRect);
    void RenderVideoFrame(BYTE* pFrameData, UINT stride, const RECT& movieRect);
    void PresentClear(float r, float g, float b);

    // Media Foundation state
    bool m_bMFInitialized;

    // Video state
    UINT m_videoWidth;
    UINT m_videoHeight;

    // D3D11 video texture
    ID3D11Texture2D* m_pVideoTexture;
    ID3D11ShaderResourceView* m_pVideoSRV;
};
