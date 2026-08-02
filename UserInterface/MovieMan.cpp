#include "stdafx.h"
#include "MovieMan.h"
#include "PythonApplication.h"
#include "../EterLib/ShaderManager.h"
#include "../EterLib/GrpBase.h"

/*
 * MovieMan.cpp
 * DX11 Native Video Playback using Media Foundation Source Reader
 *
 * All rendering uses D3D11: video frames uploaded to DYNAMIC texture,
 * drawn as fullscreen quad via UI shader. No GDI operations.
 */

#define LOGO_PMANG_FILE         "ymir.mpg"
#define LOGO_NW_FILE            "logoNW.mpg"
#define LOGO_EA_FILE            "logoEA.mpg"
#define LOGO_EA_ENGLISH_FILE    "logoEA_english.mpg"
#define LOGO_GAMEON             "gameonbi.mpg"
#define LOGO_IAH_FILE           "logoIAH.mpg"
#define INTRO_FILE              "intro.mpg"
#define LEGAL_FILE_00           "legal00.mpg"
#define LEGAL_FILE_01           "legal01.mpg"
#define TUTORIAL_0              "TutorialMovie\\Tutorial0.mpg"
#define TUTORIAL_1              "TutorialMovie\\Tutorial1.mpg"
#define TUTORIAL_2              "TutorialMovie\\Tutorial2.mpg"

CMovieMan::CMovieMan()
    : m_bMFInitialized(false)
    , m_videoWidth(0)
    , m_videoHeight(0)
    , m_pVideoTexture(nullptr)
    , m_pVideoSRV(nullptr)
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
}

CMovieMan::~CMovieMan()
{
    ShutdownMediaFoundation();
    DestroyVideoTexture();
    CoUninitialize();
}

bool CMovieMan::InitializeMediaFoundation()
{
    if (m_bMFInitialized)
        return true;

    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr))
    {
        TraceError("MovieMan: MFStartup failed (hr=0x%08X)", hr);
        return false;
    }

    m_bMFInitialized = true;
    return true;
}

void CMovieMan::ShutdownMediaFoundation()
{
    if (m_bMFInitialized)
    {
        MFShutdown();
        m_bMFInitialized = false;
    }
}

//////////////////////////////////////////////////////////////////////////
// D3D11 Rendering Helpers
//////////////////////////////////////////////////////////////////////////

bool CMovieMan::CreateVideoTexture()
{
    DestroyVideoTexture();

    auto* pDevice = CGraphicBase::GetDevice();
    if (!pDevice || m_videoWidth == 0 || m_videoHeight == 0)
        return false;

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = m_videoWidth;
    texDesc.Height = m_videoHeight;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DYNAMIC;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = pDevice->CreateTexture2D(&texDesc, nullptr, &m_pVideoTexture);
    if (FAILED(hr))
    {
        TraceError("MovieMan: CreateTexture2D failed (hr=0x%08X)", hr);
        return false;
    }

    hr = pDevice->CreateShaderResourceView(m_pVideoTexture, nullptr, &m_pVideoSRV);
    if (FAILED(hr))
    {
        TraceError("MovieMan: CreateSRV failed (hr=0x%08X)", hr);
        m_pVideoTexture->Release();
        m_pVideoTexture = nullptr;
        return false;
    }

    return true;
}

void CMovieMan::DestroyVideoTexture()
{
    if (m_pVideoSRV) { m_pVideoSRV->Release(); m_pVideoSRV = nullptr; }
    if (m_pVideoTexture) { m_pVideoTexture->Release(); m_pVideoTexture = nullptr; }
}

void CMovieMan::CalcMovieRect(int srcWidth, int srcHeight, RECT& movieRect)
{
    UINT clientWidth = 0, clientHeight = 0;
    CGraphicBase::GetBackBufferSize(&clientWidth, &clientHeight);

    int nMovieWidth, nMovieHeight;
    if (srcWidth >= srcHeight)
    {
        nMovieWidth = (int)clientWidth;
        nMovieHeight = srcHeight * nMovieWidth / srcWidth;
        if (nMovieHeight > (int)clientHeight)
            nMovieHeight = (int)clientHeight;
    }
    else
    {
        nMovieHeight = (int)clientHeight;
        nMovieWidth = srcWidth * nMovieHeight / srcHeight;
        if (nMovieWidth > (int)clientWidth)
            nMovieWidth = (int)clientWidth;
    }

    movieRect.left = ((int)clientWidth - nMovieWidth) / 2;
    movieRect.top = ((int)clientHeight - nMovieHeight) / 2;
    movieRect.right = movieRect.left + nMovieWidth;
    movieRect.bottom = movieRect.top + nMovieHeight;
}

void CMovieMan::PresentClear(float r, float g, float b)
{
    auto* pCtx = CGraphicBase::GetContext();
    auto* pRTV = CGraphicBase::GetRenderTargetView();
    auto* pSwapChain = CGraphicBase::GetSwapChain();
    if (!pCtx || !pRTV || !pSwapChain)
        return;

    float clearColor[4] = { r, g, b, 1.0f };
    pCtx->ClearRenderTargetView(pRTV, clearColor);
#ifdef ENABLE_MSAA
    CGraphicBase::ResolveMSAA();
#endif
    pSwapChain->Present(0, 0);
}

void CMovieMan::RenderVideoFrame(BYTE* pFrameData, UINT stride, const RECT& movieRect)
{
    auto* pCtx = CGraphicBase::GetContext();
    auto* pRTV = CGraphicBase::GetRenderTargetView();
    auto* pSwapChain = CGraphicBase::GetSwapChain();
    if (!pCtx || !pRTV || !pSwapChain || !m_pVideoTexture || !m_pVideoSRV)
        return;

    // Upload frame data to DYNAMIC texture
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(pCtx->Map(m_pVideoTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;

    BYTE* pDst = (BYTE*)mapped.pData;
    BYTE* pSrc = pFrameData;
    UINT rowSize = m_videoWidth * 4;

    for (UINT y = 0; y < m_videoHeight; y++)
    {
        memcpy(pDst, pSrc, rowSize);
        pSrc += stride;
        pDst += mapped.RowPitch;
    }

    pCtx->Unmap(m_pVideoTexture, 0);

    // Clear render target to black (handles letterbox bars)
    float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    pCtx->ClearRenderTargetView(pRTV, black);

    Matrix matIdentity;
    MatrixIdentity(&matIdentity);
    SHADERMANAGER.SetMatrix(MATRIX_PROJECTION, &matIdentity);
    SHADERMANAGER.SetMatrix(MATRIX_VIEW, &matIdentity);
    SHADERMANAGER.SetMatrix(MATRIX_WORLD, &matIdentity);
    SHADERMANAGER.SetPipelineState(PSTATE_DEPTHENABLE, FALSE);
    SHADERMANAGER.SetPipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
    SHADERMANAGER.SetPipelineState(PSTATE_CULLMODE, CULL_NONE);
    SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, FALSE);
    SHADERMANAGER.SetMaterialParams(0, 0, 0, 0);

    // Use LINEAR filtering for quality video scaling
    SHADERMANAGER.SetSamplerState(0, SAMPLER_MINFILTER, FILTER_LINEAR);
    SHADERMANAGER.SetSamplerState(0, SAMPLER_MAGFILTER, FILTER_LINEAR);

    // Bind video texture and UI shader
    SHADERMANAGER.SetShaderResource(0, m_pVideoSRV);
    SHADERMANAGER.SetShaderResource(1, NULL);
    SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_PDT);
    SHADERMANAGER.BeginUI();

    // Draw textured quad at movie rect (pixel coordinates)
    struct PDTVertex { float x, y, z; DWORD color; float u, v; };

    float x1 = (float)movieRect.left;
    float y1 = (float)movieRect.top;
    float x2 = (float)movieRect.right;
    float y2 = (float)movieRect.bottom;

    PDTVertex quad[4] = {
        { x1, y1, 0.0f, 0xFFFFFFFF, 0.0f, 0.0f },
        { x2, y1, 0.0f, 0xFFFFFFFF, 1.0f, 0.0f },
        { x1, y2, 0.0f, 0xFFFFFFFF, 0.0f, 1.0f },
        { x2, y2, 0.0f, 0xFFFFFFFF, 1.0f, 1.0f },
    };

    SHADERMANAGER.DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, quad, sizeof(PDTVertex));

#ifdef ENABLE_MSAA
    CGraphicBase::ResolveMSAA();
#endif
    pSwapChain->Present(1, 0);
}

//////////////////////////////////////////////////////////////////////////
// Public Interface
//////////////////////////////////////////////////////////////////////////

void CMovieMan::ClearToBlack()
{
    PresentClear(0.0f, 0.0f, 0.0f);
}

void CMovieMan::PlayLogo(const char* pcszName)
{
    PlayMovie(pcszName);
}

void CMovieMan::PlayIntro()
{
    PlayMovie(INTRO_FILE, MOVIEMAN_SKIPPABLE_YES, MOVIEMAN_POSTEFFECT_FADEOUT, 0xFFFFFF);
}

BOOL CMovieMan::PlayTutorial(LONG nIdx)
{
    ClearToBlack();

    switch (nIdx)
    {
    case 0:
        return PlayMovie(TUTORIAL_0, MOVIEMAN_SKIPPABLE_YES, MOVIEMAN_POSTEFFECT_FADEOUT, 0xFFFFFF);
    case 1:
        return PlayMovie(TUTORIAL_1, MOVIEMAN_SKIPPABLE_YES, MOVIEMAN_POSTEFFECT_FADEOUT, 0xFFFFFF);
    case 2:
        return PlayMovie(TUTORIAL_2, MOVIEMAN_SKIPPABLE_YES, MOVIEMAN_POSTEFFECT_FADEOUT, 0xFFFFFF);
    }

    return FALSE;
}

BOOL CMovieMan::PlayMovie(const char* cpFileName, bool bSkipAllowed, int nPostEffectID, DWORD dwPostEffectData)
{
    HWND hWnd = CPythonApplication::Instance().GetWindowHandle();

    // Initialize Media Foundation
    if (!InitializeMediaFoundation())
    {
        TraceError("MovieMan: Failed to initialize Media Foundation");
        return FALSE;
    }

    // Convert filename to full path
    WCHAR wPath[MAX_PATH + 1];
    MultiByteToWideChar(CP_ACP, 0, cpFileName, -1, wPath, MAX_PATH + 1);

    WCHAR wsFullPath[MAX_PATH + 1] = {};
    GetCurrentDirectoryW(MAX_PATH, wsFullPath);
    wcscat_s(wsFullPath, L"\\");
    wcscat_s(wsFullPath, wPath);

    // Check if file exists (optional intro videos)
    if (GetFileAttributesW(wsFullPath) == INVALID_FILE_ATTRIBUTES)
        return FALSE;

    // Create source reader attributes
    IMFAttributes* pAttributes = nullptr;
    HRESULT hr = MFCreateAttributes(&pAttributes, 2);
    if (FAILED(hr))
    {
        TraceError("MovieMan: MFCreateAttributes failed (hr=0x%08X)", hr);
        return FALSE;
    }

    // Enable video processing for format conversion
    pAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

    // Create source reader
    IMFSourceReader* pSourceReader = nullptr;
    hr = MFCreateSourceReaderFromURL(wsFullPath, pAttributes, &pSourceReader);
    pAttributes->Release();

    if (FAILED(hr))
    {
        TraceError("MovieMan: MFCreateSourceReaderFromURL failed (hr=0x%08X) for %s", hr, cpFileName);
        return FALSE;
    }

    // Configure output format to RGB32
    IMFMediaType* pMediaType = nullptr;
    hr = MFCreateMediaType(&pMediaType);
    if (FAILED(hr))
    {
        TraceError("MovieMan: MFCreateMediaType failed (hr=0x%08X)", hr);
        pSourceReader->Release();
        return FALSE;
    }

    pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    pMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);

    hr = pSourceReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pMediaType);
    pMediaType->Release();

    if (FAILED(hr))
    {
        TraceError("MovieMan: SetCurrentMediaType failed (hr=0x%08X)", hr);
        pSourceReader->Release();
        return FALSE;
    }

    // Get video dimensions
    IMFMediaType* pOutputType = nullptr;
    hr = pSourceReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pOutputType);
    if (SUCCEEDED(hr))
    {
        MFGetAttributeSize(pOutputType, MF_MT_FRAME_SIZE, &m_videoWidth, &m_videoHeight);
        pOutputType->Release();
    }

    if (m_videoWidth == 0 || m_videoHeight == 0)
    {
        m_videoWidth = 640;
        m_videoHeight = 480;
    }

    // Create D3D11 video texture
    if (!CreateVideoTexture())
    {
        TraceError("MovieMan: Failed to create video texture (%dx%d)", m_videoWidth, m_videoHeight);
        pSourceReader->Release();
        return FALSE;
    }

    RECT movieRect;
    CalcMovieRect(m_videoWidth, m_videoHeight, movieRect);

    // Clear to black before starting
    PresentClear(0.0f, 0.0f, 0.0f);

    // Main playback loop
    #define KEY_DOWN(vk) (GetAsyncKeyState(vk) & 0x8000)

    bool bPlaying = true;
    while (bPlaying)
    {
        // Process Windows messages
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Check for skip
        if (bSkipAllowed && (KEY_DOWN(VK_LBUTTON) || KEY_DOWN(VK_ESCAPE) || KEY_DOWN(VK_SPACE)))
        {
            break;
        }

        // Read next frame
        DWORD dwStreamIndex, dwFlags;
        LONGLONG llTimestamp;
        IMFSample* pSample = nullptr;

        hr = pSourceReader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0,
            &dwStreamIndex,
            &dwFlags,
            &llTimestamp,
            &pSample);

        if (FAILED(hr))
        {
            TraceError("MovieMan: ReadSample failed (hr=0x%08X)", hr);
            break;
        }

        // Check for end of stream
        if (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM)
        {
            if (pSample) pSample->Release();
            bPlaying = false;
            break;
        }

        // Process frame
        if (pSample)
        {
            IMFMediaBuffer* pBuffer = nullptr;
            hr = pSample->ConvertToContiguousBuffer(&pBuffer);
            if (SUCCEEDED(hr))
            {
                BYTE* pData = nullptr;
                DWORD dwMaxLength, dwCurrentLength;

                hr = pBuffer->Lock(&pData, &dwMaxLength, &dwCurrentLength);
                if (SUCCEEDED(hr))
                {
                    // Upload to GPU texture, draw quad, present
                    RenderVideoFrame(pData, m_videoWidth * 4, movieRect);
                    pBuffer->Unlock();
                }
                pBuffer->Release();
            }
            pSample->Release();
        }

        // Small delay to control frame rate
        Sleep(1);
    }

    // Cleanup
    pSourceReader->Release();
    DestroyVideoTexture();

    // Post effect
    switch (nPostEffectID)
    {
    case MOVIEMAN_POSTEFFECT_FADEOUT:
        RenderPostEffectFadeOut(MOVIEMAN_FADE_DURATION, dwPostEffectData);
        break;
    }

    // Clear pending input
    MSG msg;
    while (PeekMessage(&msg, hWnd, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE));
    while (PeekMessage(&msg, hWnd, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE));

    return TRUE;
}

void CMovieMan::RenderPostEffectFadeOut(int fadeOutDuration, DWORD fadeOutColor)
{
    auto* pCtx = CGraphicBase::GetContext();
    auto* pRTV = CGraphicBase::GetRenderTargetView();
    auto* pSwapChain = CGraphicBase::GetSwapChain();
    if (!pCtx || !pRTV || !pSwapChain)
        return;

    float r = ((fadeOutColor >> 16) & 255) / 255.0f;
    float g = ((fadeOutColor >> 8) & 255) / 255.0f;
    float b = (fadeOutColor & 255) / 255.0f;

    LONG fadeBegin = GetTickCount();
    float fadeProgress = 0.0f;

    while ((fadeProgress = ((float)((LONG)GetTickCount() - fadeBegin) / fadeOutDuration)) < 1.0f)
    {
        // Process messages during fade
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        float clearColor[4] = { r * fadeProgress, g * fadeProgress, b * fadeProgress, 1.0f };
        pCtx->ClearRenderTargetView(pRTV, clearColor);
#ifdef ENABLE_MSAA
        CGraphicBase::ResolveMSAA();
#endif
        pSwapChain->Present(1, 0);
    }

    // Final fill with target color
    float finalColor[4] = { r, g, b, 1.0f };
    pCtx->ClearRenderTargetView(pRTV, finalColor);
#ifdef ENABLE_MSAA
    CGraphicBase::ResolveMSAA();
#endif
    pSwapChain->Present(0, 0);
}
