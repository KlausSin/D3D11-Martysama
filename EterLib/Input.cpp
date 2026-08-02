#include "StdAfx.h"
#include "Input.h"

/*
 * Input.cpp
 * DX11 Native Input System using Windows Raw Input API
 *
 * Replaces legacy DirectInput8 implementation with modern Raw Input.
 * Raw Input is processed via WM_INPUT messages in the window procedure.
 */

// Static member initialization
HWND CInputDevice::ms_hWnd = NULL;
bool CInputDevice::ms_bRawInputRegistered = false;

bool CInputKeyboard::ms_bInitialized = false;
bool CInputKeyboard::ms_bPressedKey[256] = { false };
BYTE CInputKeyboard::ms_keyState[256] = { 0 };
CInputKeyboard* CInputKeyboard::ms_pInstance = nullptr;

CInputDevice::CInputDevice()
{
}

CInputDevice::~CInputDevice()
{
    // Unregister Raw Input if this is the last device
    if (ms_bRawInputRegistered)
    {
        RAWINPUTDEVICE rid = {};
        rid.usUsagePage = 0x01;  // HID_USAGE_PAGE_GENERIC
        rid.usUsage = 0x06;      // HID_USAGE_GENERIC_KEYBOARD
        rid.dwFlags = RIDEV_REMOVE;
        rid.hwndTarget = nullptr;
        RegisterRawInputDevices(&rid, 1, sizeof(rid));
        ms_bRawInputRegistered = false;
    }
}

HRESULT CInputDevice::CreateDevice(HWND hWnd)
{
    if (ms_bRawInputRegistered)
        return S_OK;

    ms_hWnd = hWnd;

    // Register for Raw Input keyboard events
    RAWINPUTDEVICE rid = {};
    rid.usUsagePage = 0x01;      // HID_USAGE_PAGE_GENERIC
    rid.usUsage = 0x06;          // HID_USAGE_GENERIC_KEYBOARD
    rid.dwFlags = 0;             // Only receive input when window has focus
    rid.hwndTarget = hWnd;

    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
    {
        DWORD dwError = GetLastError();
        TraceError("Input: RegisterRawInputDevices failed (error=%d)", dwError);
        return E_FAIL;
    }

    ms_bRawInputRegistered = true;
    return S_OK;
}

CInputKeyboard::CInputKeyboard()
{
    ResetKeyboard();
    ms_pInstance = this;
}

CInputKeyboard::~CInputKeyboard()
{
    if (ms_pInstance == this)
        ms_pInstance = nullptr;

    ms_bInitialized = false;
}

void CInputKeyboard::ResetKeyboard()
{
    memset(ms_keyState, 0, sizeof(ms_keyState));
    memset(ms_bPressedKey, 0, sizeof(ms_bPressedKey));
}

bool CInputKeyboard::InitializeKeyboard(HWND hWnd)
{
    if (ms_bInitialized)
        return true;

    if (FAILED(CreateDevice(hWnd)))
        return false;

    ms_bInitialized = true;
    return true;
}

void CInputKeyboard::UpdateKeyboard()
{
    if (!ms_bInitialized)
        return;


    for (int i = 0; i < 256; ++i)
    {
        bool bCurrentlyDown = (ms_keyState[i] & 0x80) != 0;

        if (bCurrentlyDown)
        {
            if (!IsPressed(i))
                KeyDown(i);
        }
        else if (IsPressed(i))
        {
            KeyUp(i);
        }
    }
}

void CInputKeyboard::ProcessRawInput(LPARAM lParam)
{
    if (!ms_bInitialized)
        return;

    UINT dwSize = 0;

    // Get the size of the raw input data
    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));

    if (dwSize == 0)
        return;

    // Allocate buffer for raw input data
    BYTE* lpb = new BYTE[dwSize];
    if (!lpb)
        return;

    // Get the raw input data
    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
    {
        delete[] lpb;
        return;
    }

    RAWINPUT* raw = (RAWINPUT*)lpb;

    // Process keyboard input
    if (raw->header.dwType == RIM_TYPEKEYBOARD)
    {
        RAWKEYBOARD& keyboard = raw->data.keyboard;

        UINT scanCode = keyboard.MakeCode;

        bool bExtended = (keyboard.Flags & RI_KEY_E0) != 0;

        if (bExtended)
        {
            // Map extended keys to their DirectInput equivalents
            // Extended keys have E0 prefix in scan codes
            switch (scanCode)
            {
            case 0x1C: scanCode = 0x9C; break;  // Numpad Enter -> DIK_NUMPADENTER
            case 0x1D: scanCode = 0x9D; break;  // Right Ctrl -> DIK_RCONTROL
            case 0x35: scanCode = 0xB5; break;  // Numpad / -> DIK_DIVIDE
            case 0x38: scanCode = 0xB8; break;  // Right Alt -> DIK_RMENU
            case 0x47: scanCode = 0xC7; break;  // Home -> DIK_HOME
            case 0x48: scanCode = 0xC8; break;  // Up Arrow -> DIK_UP
            case 0x49: scanCode = 0xC9; break;  // Page Up -> DIK_PRIOR
            case 0x4B: scanCode = 0xCB; break;  // Left Arrow -> DIK_LEFT
            case 0x4D: scanCode = 0xCD; break;  // Right Arrow -> DIK_RIGHT
            case 0x4F: scanCode = 0xCF; break;  // End -> DIK_END
            case 0x50: scanCode = 0xD0; break;  // Down Arrow -> DIK_DOWN
            case 0x51: scanCode = 0xD1; break;  // Page Down -> DIK_NEXT
            case 0x52: scanCode = 0xD2; break;  // Insert -> DIK_INSERT
            case 0x53: scanCode = 0xD3; break;  // Delete -> DIK_DELETE
            case 0x5B: scanCode = 0xDB; break;  // Left Windows -> DIK_LWIN
            case 0x5C: scanCode = 0xDC; break;  // Right Windows -> DIK_RWIN
            case 0x5D: scanCode = 0xDD; break;  // App Menu -> DIK_APPS
            case 0x37: scanCode = 0xB7; break;  // Print Screen -> DIK_SYSRQ
            }
        }

        // Store the key state using the scan code (DIK-compatible)
        if (scanCode < 256)
        {
            bool bKeyDown = (keyboard.Flags & RI_KEY_BREAK) == 0;

            if (bKeyDown)
            {
                ms_keyState[scanCode] = 0x80;
            }
            else
            {
                ms_keyState[scanCode] = 0x00;
            }
        }
    }

    delete[] lpb;
}

void CInputKeyboard::KeyDown(int iIndex)
{
    if (iIndex < 0 || iIndex >= 256)
        return;

    ms_bPressedKey[iIndex] = true;
    OnKeyDown(iIndex);
}

void CInputKeyboard::KeyUp(int iIndex)
{
    if (iIndex < 0 || iIndex >= 256)
        return;

    ms_bPressedKey[iIndex] = false;
    OnKeyUp(iIndex);
}

bool CInputKeyboard::IsPressed(int iIndex)
{
    if (iIndex < 0 || iIndex >= 256)
        return false;

    return ms_bPressedKey[iIndex];
}
