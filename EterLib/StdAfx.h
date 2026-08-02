#pragma once

#define WIN32_LEAN_AND_MEAN
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "../UserInterface/Locale_inc.h"

#define _WIN32_DCOM

#pragma warning(disable:4710)	// not inlined
#pragma warning(disable:4786)
#pragma warning(disable:4244)	// type conversion possible lose of data

#pragma warning(disable:4018)
#pragma warning(disable:4245)
#pragma warning(disable:4512)
#pragma warning(disable:4201)

#if _MSC_VER >= 1400
#pragma warning(disable:4201 4512 4238 4239)
#endif

// DirectX 11 Headers
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <DirectXColors.h>

using namespace DirectX;

// Math types and functions (native DirectXMath-based)
#include "GrpMathType.h"
#include "GrpMathFunc.h"
#include "GrpStateEnum.h"
#include "GrpLightType.h"

// ComPtr for safe COM pointer management
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#include "DIKCompat.h"

#pragma warning ( disable : 4201 )
#include <mmsystem.h>
#pragma warning ( default : 4201 )
#include <process.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <direct.h>
#include <malloc.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include "../eterBase/StdAfx.h"
#include "../eterBase/Debug.h"
#include "../eterLocale/CodePageId.h"

#ifndef VC_EXTRALEAN
#include <winsock.h>
#endif
