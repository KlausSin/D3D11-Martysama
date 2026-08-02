#pragma once

/*
 * Input.h
 * DX11 Native Input System
 *
 * Uses Windows Raw Input API (WM_INPUT) for keyboard input.
 * Replaces legacy DirectInput8 implementation.
 *
 * Raw Input provides:
 * - Lower latency than DirectInput
 * - No additional DLL dependencies
 * - Better compatibility with modern Windows
 * - Native Windows API (no DirectX dependency)
 */

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=NULL; } }
#endif

class CInputDevice
{
public:
    CInputDevice();
    virtual ~CInputDevice();

    HRESULT CreateDevice(HWND hWnd);

protected:
    static HWND ms_hWnd;
    static bool ms_bRawInputRegistered;
};

class CInputKeyboard : public CInputDevice
{
public:
    CInputKeyboard();
    virtual ~CInputKeyboard();

    bool InitializeKeyboard(HWND hWnd);
    void UpdateKeyboard();
    void ResetKeyboard();

    bool IsPressed(int iIndex);
    void KeyDown(int iIndex);
    void KeyUp(int iIndex);

    // Raw Input message handler - call from WndProc
    static void ProcessRawInput(LPARAM lParam);

protected:
    virtual void OnKeyDown(int iIndex) = 0;
    virtual void OnKeyUp(int iIndex) = 0;

protected:
    static bool ms_bInitialized;
    static bool ms_bPressedKey[256];
    static BYTE ms_keyState[256];
    static CInputKeyboard* ms_pInstance;  // For callback routing
};
