#include "StdAfx.h"
#include "PythonApplication.h"
#include "../eterLib/ShaderManager.h"

/*
 * PythonApplicationLogo.cpp
 * DX11 Native Logo/Video Playback using Media Foundation Source Reader
 *
 * Replaces legacy DirectShow SampleGrabber implementation with modern
 * Media Foundation IMFSourceReader for frame-by-frame video capture.
 */

static bool bInitializedLogo = false;

int CPythonApplication::OnLogoOpen(char* szName)
{
    m_pLogoTex = nullptr;
    m_pLogoTex = new CGraphicImageTexture();
    m_pCaptureBuffer = nullptr;
    m_lBufferSize = 0;
    m_bLogoError = true;
    m_bLogoPlay = false;
    m_pSourceReader = nullptr;
    m_nVideoWidth = 0;
    m_nVideoHeight = 0;

    m_nLeft = 0; m_nRight = 0; m_nTop = 0; m_nBottom = 0;

    // Create initial 1x1 texture
    if (!m_pLogoTex->Create(1, 1, DXGI_FORMAT_B8G8R8A8_UNORM))
    {
        return 0;
    }

    // Initialize Media Foundation (if not already done)
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr))
    {
        TraceError("OnLogoOpen: MFStartup failed (hr=0x%08X)", hr);
        return 0;
    }

    // Convert filename to wide string
    WCHAR wFileName[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, szName, -1, wFileName, MAX_PATH);

    // Create source reader attributes
    IMFAttributes* pAttributes = nullptr;
    hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr))
    {
        TraceError("OnLogoOpen: MFCreateAttributes failed (hr=0x%08X)", hr);
        return 0;
    }

    // Enable video processing for format conversion
    pAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);

    // Create source reader
    hr = MFCreateSourceReaderFromURL(wFileName, pAttributes, &m_pSourceReader);
    pAttributes->Release();

    if (FAILED(hr))
    {
        TraceError("OnLogoOpen: MFCreateSourceReaderFromURL failed (hr=0x%08X)", hr);
        return 0;
    }

    // Configure output format to RGB32
    IMFMediaType* pMediaType = nullptr;
    hr = MFCreateMediaType(&pMediaType);
    if (FAILED(hr))
    {
        TraceError("OnLogoOpen: MFCreateMediaType failed (hr=0x%08X)", hr);
        return 0;
    }

    pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    pMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);

    hr = m_pSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pMediaType);
    pMediaType->Release();

    if (FAILED(hr))
    {
        TraceError("OnLogoOpen: SetCurrentMediaType failed (hr=0x%08X)", hr);
        return 0;
    }

    // Get actual output format to retrieve video dimensions
    IMFMediaType* pOutputType = nullptr;
    hr = m_pSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pOutputType);
    if (SUCCEEDED(hr))
    {
        MFGetAttributeSize(pOutputType, MF_MT_FRAME_SIZE, &m_nVideoWidth, &m_nVideoHeight);
        pOutputType->Release();
    }

    if (m_nVideoWidth == 0 || m_nVideoHeight == 0)
    {
        m_nVideoWidth = 640;
        m_nVideoHeight = 480;
    }

    bInitializedLogo = true;
    return 1;
}

int CPythonApplication::OnLogoUpdate()
{
    if (m_pSourceReader == nullptr || !bInitializedLogo)
    {
        return 0;
    }

    m_bLogoPlay = true;

    // Read next video frame
    DWORD dwStreamIndex, dwFlags;
    LONGLONG llTimestamp;
    IMFSample* pSample = nullptr;

    HRESULT hr = m_pSourceReader->ReadSample(
        MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        0,
        &dwStreamIndex,
        &dwFlags,
        &llTimestamp,
        &pSample);

    if (FAILED(hr))
    {
        m_bLogoError = true;
        return 1;
    }

    // Check for end of stream
    if (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM)
    {
        if (pSample) pSample->Release();
        return 0;
    }

    // Check for format change
    if (dwFlags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
    {
        IMFMediaType* pOutputType = nullptr;
        hr = m_pSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pOutputType);
        if (SUCCEEDED(hr))
        {
            MFGetAttributeSize(pOutputType, MF_MT_FRAME_SIZE, &m_nVideoWidth, &m_nVideoHeight);
            pOutputType->Release();
        }
    }

    if (!pSample)
    {
        // No sample available yet
        return 1;
    }

    m_bLogoError = false;

    // Calculate display rect
    long lWidth = m_nVideoWidth;
    long lHeight = m_nVideoHeight;

    if (lWidth >= lHeight)
    {
        m_nLeft = 0;
        m_nRight = this->GetWidth();
        m_nTop = (this->GetHeight() >> 1) - ((this->GetWidth() * lHeight / lWidth) >> 1);
        m_nBottom = (this->GetHeight() >> 1) + ((this->GetWidth() * lHeight / lWidth) >> 1);
    }
    else
    {
        m_nTop = 0;
        m_nBottom = this->GetHeight();
        m_nLeft = (this->GetWidth() >> 1) - ((this->GetHeight() * lWidth / lHeight) >> 1);
        m_nRight = (this->GetWidth() >> 1) + ((this->GetHeight() * lWidth / lHeight) >> 1);
    }

    // Resize texture if needed
    if (m_pLogoTex->GetWidth() != lWidth || m_pLogoTex->GetHeight() != lHeight)
    {
        m_pLogoTex->Destroy();
        m_pLogoTex->Create(lWidth, lHeight, DXGI_FORMAT_B8G8R8A8_UNORM);
    }

    // Get buffer from sample
    IMFMediaBuffer* pBuffer = nullptr;
    hr = pSample->ConvertToContiguousBuffer(&pBuffer);
    if (SUCCEEDED(hr))
    {
        BYTE* pData = nullptr;
        DWORD dwMaxLength, dwCurrentLength;

        hr = pBuffer->Lock(&pData, &dwMaxLength, &dwCurrentLength);
        if (SUCCEEDED(hr))
        {
            // Copy frame to texture
            int pitch = 0;
            void* pPixels = nullptr;
            if (m_pLogoTex->Lock(&pitch, &pPixels))
            {
                BYTE* destb = static_cast<BYTE*>(pPixels);
                LONG copySize = min((LONG)dwCurrentLength, lWidth * lHeight * 4);

                // RGB32 data - copy directly, set alpha to opaque
                for (LONG a = 0; a < copySize; a += 4)
                {
                    BYTE* src = &pData[a];
                    BYTE* dest = &destb[a];
                    dest[0] = src[0];  // B
                    dest[1] = src[1];  // G
                    dest[2] = src[2];  // R
                    dest[3] = 0xff;    // A
                }

                m_pLogoTex->Unlock();
            }

            pBuffer->Unlock();
        }
        pBuffer->Release();
    }

    pSample->Release();

    // Check for skip
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
    {
        return 0;
    }

    return 1;
}

void CPythonApplication::OnLogoRender()
{
    if (!m_pLogoTex->IsEmpty() && !m_bLogoError && bInitializedLogo)
    {
        SHADERMANAGER.SetSamplerState(0, SAMPLER_MINFILTER, FILTER_LINEAR);
        SHADERMANAGER.SetSamplerState(0, SAMPLER_MAGFILTER, FILTER_LINEAR);
        m_pLogoTex->SetTextureStage(0);
        CPythonGraphic::instance().RenderTextureBox(m_nLeft, m_nTop, m_nRight, m_nBottom, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
    }
}

void CPythonApplication::OnLogoClose()
{
    if (!bInitializedLogo)
        return;

    SAFE_DELETE_ARRAY(m_pCaptureBuffer);

    if (m_pLogoTex != nullptr)
    {
        m_pLogoTex->Destroy();
        delete m_pLogoTex;
        m_pLogoTex = nullptr;
    }

    if (m_pSourceReader != nullptr)
    {
        m_pSourceReader->Release();
        m_pSourceReader = nullptr;
    }

    MFShutdown();

    SHADERMANAGER.SetSamplerState(0, SAMPLER_MINFILTER, FILTER_POINT);
    SHADERMANAGER.SetSamplerState(0, SAMPLER_MAGFILTER, FILTER_POINT);

    bInitializedLogo = false;
}
