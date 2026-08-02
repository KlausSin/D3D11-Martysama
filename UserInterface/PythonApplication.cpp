#include "StdAfx.h"
#include "../eterBase/Error.h"
#include "../eterlib/Camera.h"
#include "../eterlib/AttributeInstance.h"
#include "../eterlib/GrpDevice.h"
#include "../gamelib/AreaTerrain.h"
#include "../EterGrnLib/Material.h"
#include "../CWebBrowser/CWebBrowser.h"

#include "../eterlib/ShaderInit.h"
#include "../eterlib/ShaderManager.h"

#ifdef ENABLE_IMGUI_MANAGER
#include "../eterLib/ImGuiManager.h"
#include "../../Extern/imgui/imgui.h"
#endif

#include "resource.h"
#include "PythonApplication.h"
#include "PythonCharacterManager.h"

#include "ProcessScanner.h"

#include "CheckLatestFiles.h"

extern void GrannyCreateSharedDeformBuffer();
extern void GrannyDestroySharedDeformBuffer();

float MIN_FOG = 12800.0f;
float g_specularSpd = 0.42f;

CPythonApplication * CPythonApplication::ms_pInstance;

float c_fDefaultCameraRotateSpeed = 1.5f;
float c_fDefaultCameraPitchSpeed = 1.5f;
float c_fDefaultCameraZoomSpeed = 0.05f;

CPythonApplication::CPythonApplication() :
m_bCursorVisible(TRUE),
m_bLiarCursorOn(false),
m_iCursorMode(CURSOR_MODE_HARDWARE),
m_isWindowed(false),
m_isFrameSkipDisable(false),
m_poMouseHandler(NULL),
m_dwUpdateFPS(0),
m_dwRenderFPS(0),
m_fAveRenderTime(0.0f),
m_dwFaceCount(0),
m_fGlobalTime(0.0f),
m_fGlobalElapsedTime(0.0f),
m_dwLButtonDownTime(0),
m_dwLastIdleTime(0)
{
#ifndef _DEBUG
	SetEterExceptionHandler();
#endif

	m_dwWidth = 800;
	m_dwHeight = 600;

	ms_pInstance = this;
	m_isWindowFullScreenEnable = FALSE;

	m_v3CenterPosition = Vector3(0.0f, 0.0f, 0.0f);
	m_dwStartLocalTime = ELTimer_GetMSec();
	m_tServerTime = 0;
	m_tLocalStartTime = 0;

	m_iPort = 0;
	m_iFPS = 200;

	m_isActivateWnd = false;
	m_isMinimizedWnd = true;

	m_fRotationSpeed = 0.0f;
	m_fPitchSpeed = 0.0f;
	m_fZoomSpeed = 0.0f;

	m_fFaceSpd=0.0f;

	m_dwFaceAccCount=0;
	m_dwFaceAccTime=0;

	m_dwFaceSpdSum=0;
	m_dwFaceSpdCount=0;

	m_FlyingManager.SetMapManagerPtr(&m_pyBackground);

	m_iCursorNum = CURSOR_SHAPE_NORMAL;
	m_iContinuousCursorNum = CURSOR_SHAPE_NORMAL;

	m_isSpecialCameraMode = FALSE;
	m_fCameraRotateSpeed = c_fDefaultCameraRotateSpeed;
	m_fCameraPitchSpeed = c_fDefaultCameraPitchSpeed;
	m_fCameraZoomSpeed = c_fDefaultCameraZoomSpeed;

	m_iCameraMode = CAMERA_MODE_NORMAL;
	m_fBlendCameraStartTime = 0.0f;
	m_fBlendCameraBlendTime = 0.0f;

	m_iForceSightRange = -1;

	CCameraManager::Instance().AddCamera(EVENT_CAMERA_NUMBER);
	timeBeginPeriod(1);
}

CPythonApplication::~CPythonApplication()
{
}

void CPythonApplication::GetMousePosition(POINT* ppt)
{
	CMSApplication::GetMousePosition(ppt);
}

void CPythonApplication::SetMinFog(float fMinFog)
{
	MIN_FOG = fMinFog;
}

void CPythonApplication::SetFrameSkip(bool isEnable)
{
	if (isEnable)
		m_isFrameSkipDisable=false;
	else
		m_isFrameSkipDisable=true;
}

void CPythonApplication::NotifyHack(const char* c_szFormat, ...)
{
	char szBuf[1024];

	va_list args;
	va_start(args, c_szFormat);
	_vsnprintf(szBuf, sizeof(szBuf), c_szFormat, args);
	va_end(args);
	m_pyNetworkStream.NotifyHack(szBuf);
}

void CPythonApplication::GetInfo(UINT eInfo, std::string* pstInfo)
{
	switch (eInfo)
	{
	case INFO_ACTOR:
		m_kChrMgr.GetInfo(pstInfo);
		break;
	case INFO_EFFECT:
		m_kEftMgr.GetInfo(pstInfo);
		break;
	case INFO_ITEM:
		m_pyItem.GetInfo(pstInfo);
		break;
	case INFO_TEXTTAIL:
		m_pyTextTail.GetInfo(pstInfo);
		break;
	}
}

void CPythonApplication::Abort()
{
	TraceError("============================================================================================================");
	TraceError("Abort!!!!\n\n");

	PostQuitMessage(0);
}

void CPythonApplication::Exit()
{
#ifdef ENABLE_SSAO
	CGraphicBase::SetSSAOEnabled(false);
#endif
	HWND hFadeWnd = GetWindowHandle();
	if (hFadeWnd)
	{
		SetWindowLong(hFadeWnd, GWL_EXSTYLE, GetWindowLong(hFadeWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
		for (int nAlpha = 255; nAlpha > 0; nAlpha -= 15)
		{
			SetLayeredWindowAttributes(hFadeWnd, 0, (BYTE)nAlpha, LWA_ALPHA);
			Sleep(6);   // ~100 ms
		}
		SetLayeredWindowAttributes(hFadeWnd, 0, 0, LWA_ALPHA);
	}

	PostQuitMessage(0);
}

bool PERF_CHECKER_RENDER_GAME = false;
void CPythonApplication::RenderGame()
{
	if (!PERF_CHECKER_RENDER_GAME)
	{
		float fAspect=m_kWndMgr.GetAspect();
		float fFarClip=m_pyBackground.GetFarClip();

		m_pyGraphic.SetPerspective(30.0f, fAspect, 100.0, fFarClip);

		CCullingManager::Instance().Process();


		{
			SHADERMANAGER.ResetGlobalDrawCount();

			static LARGE_INTEGER s_qpcFreqST = {};
			if (!s_qpcFreqST.QuadPart) QueryPerformanceFrequency(&s_qpcFreqST);
			auto qpcUsST = [](LARGE_INTEGER a, LARGE_INTEGER b)
			{
				return (double)(b.QuadPart - a.QuadPart) * 1000000.0 / (double)s_qpcFreqST.QuadPart;
			};
			LARGE_INTEGER tFrameST0, tFrameST1, tDeformST0, tDeformST1, tShadowST0, tShadowST1;
			QueryPerformanceCounter(&tFrameST0);

			QueryPerformanceCounter(&tDeformST0);
			m_kChrMgr.Deform();
			QueryPerformanceCounter(&tDeformST1);

			QueryPerformanceCounter(&tShadowST0);
			m_pyBackground.RenderCharacterShadowToTexture();
			QueryPerformanceCounter(&tShadowST1);

			SHADERMANAGER.SnapshotPhase0_AfterShadow();

			m_pyGraphic.SetGameRenderState();
			m_pyGraphic.PushState();

			{
				long lx, ly;
				m_kWndMgr.GetMousePosition(lx, ly);
				m_pyGraphic.SetCursorPosition(lx, ly);
			}

			LARGE_INTEGER tA, tB;
			QueryPerformanceCounter(&tA);
			m_pyBackground.RenderSky();
			m_pyBackground.RenderCelestialBody();
			m_pyBackground.RenderBeforeLensFlare();
			m_pyBackground.RenderCloud();
			QueryPerformanceCounter(&tB);
			SHADERMANAGER.cpuMs_Sky = qpcUsST(tA, tB) / 1000.0;

			m_pyBackground.BeginEnvironment();
			QueryPerformanceCounter(&tA);
			const UINT uDrawsBeforeTerrainST = SHADERMANAGER.GetGlobalDrawCount();
			m_pyBackground.Render();
			SHADERMANAGER.StoreTerrainJobDraws(SHADERMANAGER.GetGlobalDrawCount() - uDrawsBeforeTerrainST);
			QueryPerformanceCounter(&tB);
			SHADERMANAGER.cpuMs_Terrain = qpcUsST(tA, tB) / 1000.0;

			m_pyBackground.SetCharacterDirLight();
			QueryPerformanceCounter(&tA);
			const UINT uDrawsBeforeCharST = SHADERMANAGER.GetGlobalDrawCount();
			m_kChrMgr.Render();
			SHADERMANAGER.StoreCharacterJobDraws(SHADERMANAGER.GetGlobalDrawCount() - uDrawsBeforeCharST);
			QueryPerformanceCounter(&tB);
			SHADERMANAGER.cpuMs_Char = qpcUsST(tA, tB) / 1000.0;

			m_pyBackground.SetBackgroundDirLight();

			SHADERMANAGER.SnapshotPhase1_AfterWorkers();
			QueryPerformanceCounter(&tA);
			m_pyBackground.RenderWater();

			// water= is the reflection pass + the water surface itself
			SHADERMANAGER.SnapshotPhase2_AfterWater();
			QueryPerformanceCounter(&tB);
			SHADERMANAGER.cpuMs_Water = qpcUsST(tA, tB) / 1000.0;

			m_pyBackground.RenderSnow();

			QueryPerformanceCounter(&tA);
			const UINT uDrawsBeforeEffectST = SHADERMANAGER.GetGlobalDrawCount();
			m_pyBackground.RenderEffect();

			m_pyBackground.EndEnvironment();

			m_kEftMgr.Render();
			SHADERMANAGER.StoreEffectJobDraws(SHADERMANAGER.GetGlobalDrawCount() - uDrawsBeforeEffectST);
			QueryPerformanceCounter(&tB);
			SHADERMANAGER.cpuMs_Effects = qpcUsST(tA, tB) / 1000.0;

			QueryPerformanceCounter(&tA);
			m_pyItem.Render();

			m_FlyingManager.Render();

			m_pyBackground.BeginEnvironment();
			m_pyBackground.RenderPCBlocker();
			m_pyBackground.EndEnvironment();

			m_pyBackground.RenderAfterLensFlare();

			SHADERMANAGER.End();
			SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, TRUE);
			SHADERMANAGER.SetPipelineState(PSTATE_SRCBLEND, BLEND_SRCALPHA);
			SHADERMANAGER.SetPipelineState(PSTATE_DESTBLEND, BLEND_INVSRCALPHA);
			SHADERMANAGER.SetPipelineState(PSTATE_DEPTHENABLE, FALSE);
			SHADERMANAGER.SetPipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
			SHADERMANAGER.SetPipelineState(PSTATE_CULLMODE, CULL_NONE);
			SHADERMANAGER.SetPipelineState(PSTATE_FILLMODE, FILL_SOLID);

			SHADERMANAGER.SetAlphaTest(false, 0.0f);
			SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
			SHADERMANAGER.SetMaterialParams(0.0f, 0.0f, 0.0f, 0.0f);
			SHADERMANAGER.SetTextureFactor(0xFFFFFFFF);

			QueryPerformanceCounter(&tB);
			SHADERMANAGER.cpuMs_Misc = qpcUsST(tA, tB) / 1000.0;

			QueryPerformanceCounter(&tFrameST1);
			SHADERMANAGER.cpuMs_Frame      = qpcUsST(tFrameST0,  tFrameST1)  / 1000.0;
			SHADERMANAGER.cpuMs_Deform     = qpcUsST(tDeformST0, tDeformST1) / 1000.0;
			SHADERMANAGER.cpuMs_Shadow     = qpcUsST(tShadowST0, tShadowST1) / 1000.0;
			SHADERMANAGER.cpuMs_WorkerWait = 0.0;
		}
		return;
	}

	DWORD t1=ELTimer_GetMSec();
	m_kChrMgr.Deform();
	DWORD t2=ELTimer_GetMSec();
	DWORD t3=ELTimer_GetMSec();
	m_pyBackground.RenderCharacterShadowToTexture();
	DWORD t4=ELTimer_GetMSec();

	m_pyGraphic.SetGameRenderState();
	m_pyGraphic.PushState();

	float fAspect=m_kWndMgr.GetAspect();
	float fFarClip=m_pyBackground.GetFarClip();

	m_pyGraphic.SetPerspective(30.0f, fAspect, 100.0, fFarClip);

	DWORD t5=ELTimer_GetMSec();

	CCullingManager::Instance().Process();

	DWORD t6=ELTimer_GetMSec();

	{
		long lx, ly;
		m_kWndMgr.GetMousePosition(lx, ly);
		m_pyGraphic.SetCursorPosition(lx, ly);
	}

	m_pyBackground.RenderSky();
	m_pyBackground.RenderCelestialBody();
	DWORD t7=ELTimer_GetMSec();
	m_pyBackground.RenderBeforeLensFlare();
	DWORD t8=ELTimer_GetMSec();
	m_pyBackground.RenderCloud();
	DWORD t9=ELTimer_GetMSec();
	m_pyBackground.BeginEnvironment();
	m_pyBackground.Render();

	m_pyBackground.SetCharacterDirLight();
	DWORD t10=ELTimer_GetMSec();
	m_kChrMgr.Render();
	DWORD t11=ELTimer_GetMSec();

	m_pyBackground.SetBackgroundDirLight();
	m_pyBackground.RenderWater();
	DWORD t12=ELTimer_GetMSec();
	m_pyBackground.RenderEffect();
	DWORD t13=ELTimer_GetMSec();
	m_pyBackground.EndEnvironment();
	m_kEftMgr.Render();
	DWORD t14=ELTimer_GetMSec();
	m_pyItem.Render();
	DWORD t15=ELTimer_GetMSec();
	m_FlyingManager.Render();
	DWORD t16=ELTimer_GetMSec();
	m_pyBackground.BeginEnvironment();
	m_pyBackground.RenderPCBlocker();
	m_pyBackground.EndEnvironment();
	DWORD t17=ELTimer_GetMSec();
	m_pyBackground.RenderAfterLensFlare();
	DWORD t18=ELTimer_GetMSec();
	DWORD tEnd=ELTimer_GetMSec();

	SHADERMANAGER.End();
	SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, TRUE);
	SHADERMANAGER.SetPipelineState(PSTATE_SRCBLEND, BLEND_SRCALPHA);
	SHADERMANAGER.SetPipelineState(PSTATE_DESTBLEND, BLEND_INVSRCALPHA);
	SHADERMANAGER.SetPipelineState(PSTATE_DEPTHENABLE, FALSE);
	SHADERMANAGER.SetPipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
	SHADERMANAGER.SetPipelineState(PSTATE_CULLMODE, CULL_NONE);
	SHADERMANAGER.SetPipelineState(PSTATE_FILLMODE, FILL_SOLID);

	SHADERMANAGER.SetAlphaTest(false, 0.0f);
	SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
	SHADERMANAGER.SetMaterialParams(0.0f, 0.0f, 0.0f, 0.0f);
	SHADERMANAGER.SetTextureFactor(0xFFFFFFFF);

	if (GetAsyncKeyState(VK_Z))
		SHADERMANAGER.SetPipelineState(PSTATE_FILLMODE, FILL_SOLID);

	if (tEnd-t1<3)
		return;

	static FILE* fp=fopen("perf_game_render.txt", "w");

	fprintf(fp, "GR.Total %d (Time %d)\n", tEnd-t1, ELTimer_GetMSec());
	fprintf(fp, "GR.DFM %d\n", t2-t1);
	fprintf(fp, "GR.EFT.UP %d\n", t3-t2);
	fprintf(fp, "GR.SHW %d\n", t4-t3);
	fprintf(fp, "GR.STT %d\n", t5-t4);
	fprintf(fp, "GR.CLL %d\n", t6-t5);
	fprintf(fp, "GR.BG.SKY %d\n", t7-t6);
	fprintf(fp, "GR.BG.LEN %d\n", t8-t7);
	fprintf(fp, "GR.BG.CLD %d\n", t9-t8);
	fprintf(fp, "GR.BG.MAIN %d\n", t10-t9);
	fprintf(fp, "GR.CHR %d\n",	t11-t10);
	fprintf(fp, "GR.BG.WTR %d\n", t12-t11);
	fprintf(fp, "GR.BG.EFT %d\n", t13-t12);
	fprintf(fp, "GR.EFT %d\n", t14-t13);
	fprintf(fp, "GR.ITM %d\n", t15-t14);
	fprintf(fp, "GR.FLY %d\n", t16-t15);
	fprintf(fp, "GR.BG.BLK %d\n", t17-t16);
	fprintf(fp, "GR.BG.LEN %d\n", t18-t17);

	fflush(fp);
}

void CPythonApplication::UpdateGame()
{
	DWORD t1=ELTimer_GetMSec();
	POINT ptMouse;
	GetMousePosition(&ptMouse);

	CGraphicTextInstance::Hyperlink_UpdateMousePos(ptMouse.x, ptMouse.y);

	DWORD t2=ELTimer_GetMSec();

	//if (m_isActivateWnd)
	{
		CScreen s;
		float fAspect = UI::CWindowManager::Instance().GetAspect();
		float fFarClip = CPythonBackground::Instance().GetFarClip();

		s.SetPerspective(30.0f,fAspect, 100.0f, fFarClip);
		s.BuildViewFrustum();
	}

	DWORD t3=ELTimer_GetMSec();
	TPixelPosition kPPosMainActor;
	m_pyPlayer.NEW_GetMainActorPosition(&kPPosMainActor);

	DWORD t4=ELTimer_GetMSec();
	m_pyBackground.Update(kPPosMainActor.x, kPPosMainActor.y, kPPosMainActor.z);

	DWORD t5=ELTimer_GetMSec();
	m_GameEventManager.SetCenterPosition(kPPosMainActor.x, kPPosMainActor.y, kPPosMainActor.z);
	m_GameEventManager.Update();

	DWORD t6=ELTimer_GetMSec();
	m_kChrMgr.Update();
	DWORD t7=ELTimer_GetMSec();
#ifdef ENABLE_EFFECT_LIMIT
	CEffectManager::SetMainPlayerPosition(Vector3(kPPosMainActor.x, -kPPosMainActor.y, kPPosMainActor.z));
#endif
	m_kEftMgr.Update(); //@fixme029
	m_kEftMgr.UpdateSound();
	DWORD t8=ELTimer_GetMSec();
	m_FlyingManager.Update();
	DWORD t9=ELTimer_GetMSec();
	m_pyItem.Update(ptMouse);
	DWORD t10=ELTimer_GetMSec();
	m_pyPlayer.Update();
	DWORD t11=ELTimer_GetMSec();

	m_pyPlayer.NEW_GetMainActorPosition(&kPPosMainActor);
	SetCenterPosition(kPPosMainActor.x, kPPosMainActor.y, kPPosMainActor.z);
	DWORD t12=ELTimer_GetMSec();

	if (PERF_CHECKER_RENDER_GAME)
	{
		if (t12-t1>5)
		{
			static FILE* fp=fopen("perf_game_update.txt", "w");

			fprintf(fp, "GU.Total %d (Time %d)\n", t12-t1, ELTimer_GetMSec());
			fprintf(fp, "GU.GMP %d\n", t2-t1);
			fprintf(fp, "GU.SCR %d\n", t3-t2);
			fprintf(fp, "GU.MPS %d\n", t4-t3);
			fprintf(fp, "GU.BG %d\n", t5-t4);
			fprintf(fp, "GU.GEM %d\n", t6-t5);
			fprintf(fp, "GU.CHR %d\n", t7-t6);
			fprintf(fp, "GU.EFT %d\n", t8-t7);
			fprintf(fp, "GU.FLY %d\n", t9-t8);
			fprintf(fp, "GU.ITM %d\n", t10-t9);
			fprintf(fp, "GU.PLR %d\n", t11-t10);
			fprintf(fp, "GU.POS %d\n", t12-t11);
			fflush(fp);
		}
	}

}

void CPythonApplication::SkipRenderBuffering(DWORD dwSleepMSec)
{
	m_dwBufSleepSkipTime=ELTimer_GetMSec()+dwSleepMSec;
}

bool CPythonApplication::Process()
{
#if defined(CHECK_LATEST_DATA_FILES)
	if (CheckLatestFiles_PollEvent())
		return false;
#endif
	ELTimer_SetFrameMSec();

	// 	m_Profiler.Clear();
	DWORD dwStart = ELTimer_GetMSec();

	///////////////////////////////////////////////////////////////////////////////////////////////////
	static DWORD	s_dwUpdateFrameCount = 0;
	static DWORD	s_dwRenderFrameCount = 0;
	static DWORD	s_dwFaceCount = 0;
	static UINT		s_uiLoad = 0;
	static DWORD	s_dwCheckTime = ELTimer_GetMSec();

	if (ELTimer_GetMSec() - s_dwCheckTime > 1000)
	{
		m_dwUpdateFPS		= s_dwUpdateFrameCount;
		m_dwRenderFPS		= s_dwRenderFrameCount;
		m_dwLoad			= s_uiLoad;

		m_dwFaceCount		= s_dwFaceCount / max(1, s_dwRenderFrameCount);

		s_dwCheckTime		= ELTimer_GetMSec();

		s_uiLoad = s_dwFaceCount = s_dwUpdateFrameCount = s_dwRenderFrameCount = 0;
	}

	// Update Time
	static BOOL s_bFrameSkip = false;

#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime1=ELTimer_GetMSec();
#endif
	DX::StepTimer& rkTimer = DX::StepTimer::instance();
	rkTimer.Tick([]() {});

	m_fGlobalElapsedTime = rkTimer.GetElapsedSeconds();
	m_fGlobalTime = rkTimer.GetTotalSeconds();

	SHADERMANAGER.SetTime(m_fGlobalTime, m_fGlobalElapsedTime);

	DWORD updatestart = ELTimer_GetMSec();
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime2=ELTimer_GetMSec();
#endif
	// Network I/O
	m_pyNetworkStream.Process();
	//m_pyNetworkDatagram.Process();

	m_kGuildMarkUploader.Process();

	m_kGuildMarkDownloader.Process();
	m_kAccountConnector.Process();

#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime3=ELTimer_GetMSec();
#endif
	//////////////////////
	// Input Process
	// Keyboard
	UpdateKeyboard();
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime4=ELTimer_GetMSec();
#endif
	// Mouse
	POINT Point;
	if (GetCursorPos(&Point))
	{
		ScreenToClient(m_hWnd, &Point);
		OnMouseMove(Point.x, Point.y);
	}
	//////////////////////
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime5=ELTimer_GetMSec();
#endif
	//if (m_isActivateWnd)
	__UpdateCamera();
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime6=ELTimer_GetMSec();
#endif
	// Update Game Playing
	CResourceManager::Instance().Update();
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime7=ELTimer_GetMSec();
#endif
	OnCameraUpdate();

	UpdateShaderFrameConstants();

#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime8=ELTimer_GetMSec();
#endif
	OnMouseUpdate();
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime9=ELTimer_GetMSec();
#endif
	OnUIUpdate();

#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime10=ELTimer_GetMSec();

	if (dwUpdateTime10-dwUpdateTime1>10)
	{
		static FILE* fp=fopen("perf_app_update.txt", "w");

		fprintf(fp, "AU.Total %d (Time %d)\n", dwUpdateTime9-dwUpdateTime1, ELTimer_GetMSec());
		fprintf(fp, "AU.TU %d\n", dwUpdateTime2-dwUpdateTime1);
		fprintf(fp, "AU.NU %d\n", dwUpdateTime3-dwUpdateTime2);
		fprintf(fp, "AU.KU %d\n", dwUpdateTime4-dwUpdateTime3);
		fprintf(fp, "AU.MP %d\n", dwUpdateTime5-dwUpdateTime4);
		fprintf(fp, "AU.CP %d\n", dwUpdateTime6-dwUpdateTime5);
		fprintf(fp, "AU.RU %d\n", dwUpdateTime7-dwUpdateTime6);
		fprintf(fp, "AU.CU %d\n", dwUpdateTime8-dwUpdateTime7);
		fprintf(fp, "AU.MU %d\n", dwUpdateTime9-dwUpdateTime8);
		fprintf(fp, "AU.UU %d\n", dwUpdateTime10-dwUpdateTime9);
		fprintf(fp, "----------------------------------\n");
		fflush(fp);
	}
#endif

	m_dwCurUpdateTime = ELTimer_GetMSec() - updatestart;


	if (!s_bFrameSkip)
	{
		float fSpecularMove = g_specularSpd * m_fGlobalElapsedTime;
		CGrannyMaterial::TranslateSpecularMatrix(fSpecularMove, fSpecularMove, 0.0f);

		DWORD dwRenderStartTime = ELTimer_GetMSec();

		bool canRender = true;

		if (m_isMinimizedWnd)
		{
			canRender = false;
		}
		else
		{
#ifdef ENABLE_FIX_MOBS_LAG
			if (DEVICE_STATE_OK != CheckDeviceState())
			{
				canRender = false;
			}
#else
			if (m_pyGraphic.IsLostDevice())
			{
				CPythonBackground& rkBG = CPythonBackground::Instance();
				rkBG.ReleaseCharacterShadowTexture();

				if (m_pyGraphic.RestoreDevice())
					rkBG.CreateCharacterShadowTexture();
				else
					canRender = false;
			}
#endif
		}

		if (!IsActive())
		{
			SkipRenderBuffering(3000);
		}

		if (!canRender)
		{
			SkipRenderBuffering(3000);
		}
		else
		{
			// RestoreLostDevice
			CCullingManager::Instance().Update();
			if (m_pyGraphic.Begin())
			{
				m_pyGraphic.SetClearColor(0.0f, 0.0f, 0.0f);
				m_pyGraphic.Clear();

				{
					/////////////////////
					// Interface
					m_pyGraphic.SetInterfaceRenderState();

#ifdef ENABLE_IMGUI_MANAGER
					CImGuiManager::Instance().BeginFrame();

					// Perf debug overlay — top-left, always-on-top, shows FPS +
					// particle-batcher counters so we can see if batching is
					// actually collapsing per-PSI draws into group draws.
					{
						const uint32_t uRealFPS = DX::StepTimer::instance().GetFramesPerSecond();
						float dt = (uRealFPS > 0) ? (1.0f / (float)uRealFPS) : 0.0f;
						float s_fpsSmoothed = (float)uRealFPS;

						ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
						ImGui::SetNextWindowBgAlpha(0.5f);
						if (ImGui::Begin("PerfDebug", nullptr,
							ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
							ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
							ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove))
						{
							ImGui::Text("FPS: %.1f  (%.2f ms)", s_fpsSmoothed, dt * 1000.0f);
							const auto& s = SHADERMANAGER.GetParticleBatchStats();
							ImGui::Separator();
							ImGui::Text("Particle batcher (last frame):");
							ImGui::Text("  PSIs contributed: %u", s.psisContributed);
							ImGui::Text("  Particles batched: %u", s.particlesBatched);
							ImGui::Text("  Batch groups: %u", s.batchGroups);
							ImGui::Text("  Dispatches + Draws: %u + %u", s.dispatches, s.drawsIssued);
							if (s.psisContributed > 0 && s.drawsIssued > 0)
								ImGui::Text("  Batching ratio: %u PSIs -> %u draws (%.1fx)",
									s.psisContributed, s.drawsIssued,
									(float)s.psisContributed / (float)s.drawsIssued);
						}
						ImGui::End();
					}
#endif

					OnUIRender();

					OnMouseRender();

#ifdef ENABLE_IMGUI_MANAGER
					CImGuiManager::Instance().EndFrame();
					CImGuiManager::Instance().Render();
#endif
					/////////////////////
				}

				m_pyGraphic.End();

				m_pyGraphic.Show();

				DWORD dwRenderEndTime = ELTimer_GetMSec();

				static DWORD s_dwRenderCheckTime = dwRenderEndTime;
				static DWORD s_dwRenderRangeTime = 0;
				static DWORD s_dwRenderRangeFrame = 0;

				m_dwCurRenderTime = dwRenderEndTime - dwRenderStartTime;
				s_dwRenderRangeTime += m_dwCurRenderTime;
				++s_dwRenderRangeFrame;

				if (dwRenderEndTime-s_dwRenderCheckTime>1000)
				{
					m_fAveRenderTime=float(double(s_dwRenderRangeTime)/double(s_dwRenderRangeFrame));

					s_dwRenderCheckTime=ELTimer_GetMSec();
					s_dwRenderRangeTime=0;
					s_dwRenderRangeFrame=0;
				}

				DWORD dwCurFaceCount=m_pyGraphic.GetFaceCount();
				m_pyGraphic.ResetFaceCount();
				s_dwFaceCount += dwCurFaceCount;

				if (dwCurFaceCount > 5000)
				{
					if (dwRenderEndTime > m_dwBufSleepSkipTime)
					{
						static float s_fBufRenderTime = 0.0f;

						float fCurRenderTime = m_dwCurRenderTime;

						if (fCurRenderTime > s_fBufRenderTime)
						{
							float fRatio = fMAX(0.5f, (fCurRenderTime - s_fBufRenderTime) / 30.0f);
							s_fBufRenderTime = (s_fBufRenderTime * (100.0f - fRatio) + (fCurRenderTime + 5) * fRatio) / 100.0f;
						}
						else
						{
							float fRatio = 0.5f;
							s_fBufRenderTime = (s_fBufRenderTime * (100.0f - fRatio) + fCurRenderTime * fRatio) / 100.0f;
						}

						if (s_fBufRenderTime > 100.0f)
							s_fBufRenderTime = 100.0f;

						DWORD dwBufRenderTime = s_fBufRenderTime;

						if (m_isWindowed)
						{
							if (dwBufRenderTime>58)
								dwBufRenderTime=64;
							else if (dwBufRenderTime>42)
								dwBufRenderTime=48;
							else if (dwBufRenderTime>26)
								dwBufRenderTime=32;
							else if (dwBufRenderTime>10)
								dwBufRenderTime=16;
							else
								dwBufRenderTime=8;
						}

						m_fAveRenderTime=s_fBufRenderTime;
					}

					m_dwFaceAccCount += dwCurFaceCount;
					m_dwFaceAccTime += m_dwCurRenderTime;

					if (m_dwFaceAccTime > 0)
						m_fFaceSpd=(m_dwFaceAccCount/m_dwFaceAccTime);
					else
						m_fFaceSpd = 0.0f;

					if (-1 == m_iForceSightRange)
					{
						static float s_fAveRenderTime = 16.0f;
						float fRatio=0.3f;
						s_fAveRenderTime=(s_fAveRenderTime*(100.0f-fRatio)+max(16.0f, m_dwCurRenderTime)*fRatio)/100.0f;

						float fFar=25600.0f;
						float fNear=MIN_FOG;
						double dbAvePow=double(1000.0f/s_fAveRenderTime);
						double dbMaxPow=60.0;
						float fDistance=max(fNear+(fFar-fNear)*(dbAvePow)/dbMaxPow, fNear);
						m_pyBackground.SetViewDistanceSet(0, fDistance);
					}
					else
					{
						m_pyBackground.SetViewDistanceSet(0, float(m_iForceSightRange));
					}
				}
				else
				{
					m_pyBackground.SetViewDistanceSet(0, 25600.0f);
				}

				++s_dwRenderFrameCount;
			}
		}
	}

	// Use StepTimer's precise frame limiter (200 FPS target)
	rkTimer.WaitForNextFrame();

	++s_dwUpdateFrameCount;

	s_uiLoad += ELTimer_GetMSec() - dwStart;
	//m_Profiler.ProfileByScreen();

	return true;
}

void CPythonApplication::UpdateClientRect()
{
	RECT rcApp;
	GetClientRect(&rcApp);
	OnSizeChange(rcApp.right - rcApp.left, rcApp.bottom - rcApp.top);
}

void CPythonApplication::SetMouseHandler(PyObject* poMouseHandler)
{
	m_poMouseHandler = poMouseHandler;
}

int CPythonApplication::CheckDeviceState()
{
	CGraphicDevice::EDeviceState e_deviceState = m_grpDevice.GetDeviceState();

	switch (e_deviceState)
	{
	case CGraphicDevice::DEVICESTATE_NULL:
		return DEVICE_STATE_FALSE;

	case CGraphicDevice::DEVICESTATE_BROKEN:
		return DEVICE_STATE_SKIP;

	case CGraphicDevice::DEVICESTATE_OK:
		break;
	}

	return DEVICE_STATE_OK;
}

bool CPythonApplication::CreateDevice(int width, int height, int Windowed, int bit /* = 32*/, int frequency /* = 0*/)
{
	int iRet;

	m_grpDevice.InitBackBufferCount(2);
	m_grpDevice.RegisterWarningString(CGraphicDevice::CREATE_BAD_DRIVER, ApplicationStringTable_GetStringz(IDS_WARN_BAD_DRIVER, "WARN_BAD_DRIVER"));

	iRet = m_grpDevice.Create(GetWindowHandle(), width, height, Windowed ? true : false, bit,frequency);

	switch (iRet)
	{
	case CGraphicDevice::CREATE_OK:
		return true;

	case CGraphicDevice::CREATE_REFRESHRATE:
		return true;

	case CGraphicDevice::CREATE_ENUM:
	case CGraphicDevice::CREATE_DETECT:
		SET_EXCEPTION(CREATE_NO_APPROPRIATE_DEVICE);
		TraceError("CreateDevice: Enum & Detect failed");
		return false;

	case CGraphicDevice::CREATE_NO_DIRECTX:
		SET_EXCEPTION(CREATE_NO_DIRECTX);
		TraceError("CreateDevice: DirectX 11 required to run game");
		return false;

	case CGraphicDevice::CREATE_DEVICE:
		SET_EXCEPTION(CREATE_DEVICE);
		TraceError("CreateDevice: GraphicDevice create failed");
		return false;

	case CGraphicDevice::CREATE_FORMAT:
		SET_EXCEPTION(CREATE_FORMAT);
		TraceError("CreateDevice: Change the screen format");
		return false;

	case CGraphicDevice::CREATE_GET_DEVICE_CAPS:
		PyErr_SetString(PyExc_RuntimeError, "GetDevCaps failed");
		TraceError("CreateDevice: GetDevCaps failed");
		return false;

	case CGraphicDevice::CREATE_GET_DEVICE_CAPS2:
		PyErr_SetString(PyExc_RuntimeError, "GetDevCaps2 failed");
		TraceError("CreateDevice: GetDevCaps2 failed");
		return false;

	default:
		SET_EXCEPTION(UNKNOWN_ERROR);
		TraceError("CreateDevice: Unknown error code: %d", iRet);
		return false;
	}
}

void CPythonApplication::Loop()
{
	while (1)
	{
		if (IsMessage())
		{
			if (!MessageProcess())
				break;
		}
		else
		{
			if (!Process())
				break;

			m_dwLastIdleTime=ELTimer_GetMSec();
		}
	}
}

#define ENABLE_LOAD_ITEM_LIST_FROM_ROOT
#define ENABLE_LOAD_ITEM_SCALE_FROM_ROOT
#define ENABLE_LOAD_SKILL_TABLE_FROM_ROOT
bool LoadLocaleData(const char* localePath)
{
	CPythonNonPlayer&	rkNPCMgr	= CPythonNonPlayer::Instance();
	CItemManager&		rkItemMgr	= CItemManager::Instance();
	CPythonSkill&		rkSkillMgr	= CPythonSkill::Instance();
	CPythonNetworkStream& rkNetStream = CPythonNetworkStream::Instance();

	char szItemList[256];
	char szItemProto[256];
	char szItemDesc[256];
	char szMobProto[256];
	char szSkillDescFileName[256];
	char szSkillTableFileName[256];
	char szInsultList[256];
#ifdef ENABLE_LOAD_ITEM_LIST_FROM_ROOT
	snprintf(szItemList,	sizeof(szItemList) ,	"item_list.txt");
#else
	snprintf(szItemList,	sizeof(szItemList) ,	"%s/item_list.txt",	localePath);
#endif
	snprintf(szItemProto,	sizeof(szItemProto),	"%s/item_proto",	localePath);
	snprintf(szItemDesc,	sizeof(szItemDesc),	"%s/itemdesc.txt",	localePath);
	snprintf(szMobProto,	sizeof(szMobProto),	"%s/mob_proto",		localePath);
	snprintf(szSkillDescFileName, sizeof(szSkillDescFileName),	"%s/SkillDesc.txt", localePath);
	#ifdef ENABLE_LOAD_SKILL_TABLE_FROM_ROOT
	snprintf(szSkillTableFileName, sizeof(szSkillTableFileName),	"SkillTable.txt");
	#else
	snprintf(szSkillTableFileName, sizeof(szSkillTableFileName),	"%s/SkillTable.txt", localePath);
	#endif
	snprintf(szInsultList,	sizeof(szInsultList),	"%s/insult.txt", localePath);

	rkNPCMgr.Destroy();
	rkItemMgr.Destroy();
	rkSkillMgr.Destroy();

	if (!rkItemMgr.LoadItemList(szItemList))
	{
		TraceError("LoadLocaleData - LoadItemList(%s) Error", szItemList);
	}

	if (!rkItemMgr.LoadItemTable(szItemProto))
	{
		TraceError("LoadLocaleData - LoadItemProto(%s) Error", szItemProto);
		return false;
	}

	if (!rkItemMgr.LoadItemDesc(szItemDesc))
	{
		Tracenf("LoadLocaleData - LoadItemDesc(%s) Error", szItemDesc);
	}

	if (!rkNPCMgr.LoadNonPlayerData(szMobProto))
	{
		TraceError("LoadLocaleData - LoadMobProto(%s) Error", szMobProto);
		return false;
	}

	if (!rkSkillMgr.RegisterSkillDesc(szSkillDescFileName))
	{
		TraceError("LoadLocaleData - RegisterSkillDesc(%s) Error", szSkillDescFileName); //@warme670
		return false;
	}

	if (!rkSkillMgr.RegisterSkillTable(szSkillTableFileName))
	{
		TraceError("LoadLocaleData - RegisterSkillTable(%s) Error", szSkillTableFileName); //@warme670
		return false;
	}

	if (!rkNetStream.LoadInsultList(szInsultList))
	{
		Tracenf("CPythonApplication - CPythonNetworkStream::LoadInsultList(%s)", szInsultList);
	}

#ifdef ENABLE_ACCE_COSTUME_SYSTEM
	char szItemScale[256]{};
	#ifdef ENABLE_LOAD_ITEM_SCALE_FROM_ROOT
	snprintf(szItemScale, sizeof(szItemScale), "item_scale.txt");
	#else
	snprintf(szItemScale, sizeof(szItemScale), "%s/item_scale.txt", localePath);
	#endif

	if (!rkItemMgr.LoadItemScale(szItemScale))
	{
		Tracenf("LoadLocaleData: error while loading %s.", szItemScale);
		return false;
	}
#endif

	return true;
}

unsigned __GetWindowMode(bool windowed)
{
	if (windowed)
		return WS_OVERLAPPED | WS_CAPTION |   WS_SYSMENU | WS_MINIMIZEBOX;

	return WS_POPUP;
}

bool CPythonApplication::Create(PyObject * poSelf, const char * c_szName, int width, int height, int Windowed)
{
	Windowed = CPythonSystem::Instance().IsWindowed() ? 1 : 0;

	bool bAnotherWindow = false;

	if (FindWindow(NULL, c_szName))
		bAnotherWindow = true;

	m_dwWidth = width;
	m_dwHeight = height;

	// Window
	UINT WindowMode = __GetWindowMode(Windowed ? true : false);

	if (!CMSWindow::Create(c_szName, 4, 0, WindowMode, ::LoadIcon( GetInstance(), MAKEINTRESOURCE( IDI_METIN2 ) ), IDC_CURSOR_NORMAL))
	{
		TraceError("CMSWindow::Create failed");
		SET_EXCEPTION(CREATE_WINDOW);
		return false;
	}

	if (m_pySystem.IsUseDefaultIME())
		CPythonIME::Instance().UseDefaultIME();

	#ifdef ENABLE_DISCORD_RPC
	m_pyNetworkStream.Discord_Start();
	#endif

	if (!m_pySystem.IsWindowed())
	{
		m_isWindowed = false;
		m_isWindowFullScreenEnable = TRUE;
		__SetFullScreenWindow(GetWindowHandle(), width, height, m_pySystem.GetBPP());

		Windowed = true;
	}
	else
	{
		AdjustSize(m_pySystem.GetWidth(), m_pySystem.GetHeight());

		if (Windowed)
		{
			m_isWindowed = true;
			// @fixme031 BEGIN
			RECT rc{};
			GetClientRect(&rc);
			const auto windowWidth = rc.right - rc.left;
			const auto windowHeight = (rc.bottom - rc.top);
			RECT rc2{};
			GetWindowRect(&rc2);
			const auto windowWidth2 = rc2.right - rc2.left;
			const auto windowHeight2 = (rc2.bottom - rc2.top);
			const auto windowWidthDiff = windowWidth2 - windowWidth;
			const auto windowHeightDiff = windowHeight2 - windowHeight;
			const auto dropshadowSize = (windowWidthDiff / 2 != 0) ? (windowWidthDiff / 2 - 1) : 0;
			constexpr auto taskbarSize = 73;
			constexpr auto titlebarSize = 10;
			if (bAnotherWindow)
				CMSApplication::SetPosition(GetScreenWidth() - windowWidth - dropshadowSize, GetScreenHeight() - windowHeight - taskbarSize);
			else
				SetPosition(-dropshadowSize, (m_pySystem.GetHeight() >= 1000) ? -titlebarSize : 0);
			// @fixme031 END
		}
		else
		{
			m_isWindowed = false;
			SetPosition(0, 0);
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// Cursor
	if (!CreateCursors())
	{
		TraceError("CMSWindow::Cursors Create Error");
		SET_EXCEPTION("CREATE_CURSOR");
		return false;
	}

	if (!m_pySystem.IsNoSoundCard())
	{
		// Sound
		if (!m_SoundEngine.Initialize())
		{
			TraceError("SoundEngine initialization failed");
		}
	}

	extern bool GRAPHICS_CAPS_SOFTWARE_TILING;

	if (!m_pySystem.IsAutoTiling())
		GRAPHICS_CAPS_SOFTWARE_TILING = m_pySystem.IsSoftwareTiling();

	// Device
	if (!CreateDevice(m_pySystem.GetWidth(), m_pySystem.GetHeight(), Windowed, m_pySystem.GetBPP(), m_pySystem.GetFrequency()))
		return false;

	GrannyCreateSharedDeformBuffer();

	if (m_pySystem.IsAutoTiling())
		m_pyBackground.ReserveSoftwareTilingEnable(false);
	else
		m_pyBackground.ReserveSoftwareTilingEnable(m_pySystem.IsSoftwareTiling());

	SetVisibleMode(true);

	if (m_isWindowFullScreenEnable)
		SetWindowPos(GetWindowHandle(), HWND_TOP, 0, 0, width, height, SWP_SHOWWINDOW);

	if (!InitializeKeyboard(GetWindowHandle()))
		return false;

	m_pySystem.GetDisplaySettings();

	// Mouse
	if (m_pySystem.IsSoftwareCursor())
		SetCursorMode(CURSOR_MODE_SOFTWARE);
	else
		SetCursorMode(CURSOR_MODE_HARDWARE);

	// Network
	if (!m_netDevice.Create())
	{
		//PyErr_SetString(PyExc_RuntimeError, "NetDevice::Create failed");
		TraceError("NetDevice::Create failed");
		SET_EXCEPTION("CREATE_NETWORK");
		return false;
	}

	m_pyItem.Create();

	// Other Modules
#ifdef ENABLE_IMGUI_MANAGER
	if (!CImGuiManager::Instance().Initialize(GetWindowHandle(), CGraphicDevice::GetDevice(), CGraphicDevice::GetContext()))
	{
		TraceError("Failed to initialize ImGui Manager!");
		return false;
	}
#else
	DefaultFont_Startup();
#endif

	CPythonIME::Instance().Create(GetWindowHandle());
	CPythonIME::Instance().SetText("", 0);
	CPythonTextTail::Instance().Initialize();

	// Light Manager
	m_LightManager.Initialize();

	CGraphicImageInstance::CreateSystem(32);

	STICKYKEYS sStickKeys;
	memset(&sStickKeys, 0, sizeof(sStickKeys));
	sStickKeys.cbSize = sizeof(sStickKeys);
	SystemParametersInfo( SPI_GETSTICKYKEYS, sizeof(sStickKeys), &sStickKeys, 0 );
	m_dwStickyKeysFlag = sStickKeys.dwFlags;

	sStickKeys.dwFlags &= ~(SKF_AVAILABLE|SKF_HOTKEYACTIVE);
	SystemParametersInfo( SPI_SETSTICKYKEYS, sizeof(sStickKeys), &sStickKeys, 0 );

	// SphereMap
	CGrannyMaterial::CreateSphereMap(0, "d:/ymir work/special/spheremap.jpg");
	CGrannyMaterial::CreateSphereMap(1, "d:/ymir work/special/spheremap01.jpg");
	return true;
}

void CPythonApplication::SetGlobalCenterPosition(LONG x, LONG y)
{
	CPythonBackground& rkBG=CPythonBackground::Instance();
	rkBG.GlobalPositionToLocalPosition(x, y);

	float z = CPythonBackground::Instance().GetHeight(x, y);

	CPythonApplication::Instance().SetCenterPosition(x, y, z);
}

void CPythonApplication::SetCenterPosition(float fx, float fy, float fz)
{
	m_v3CenterPosition.x = +fx;
	m_v3CenterPosition.y = -fy;
	m_v3CenterPosition.z = +fz;
}

void CPythonApplication::GetCenterPosition(TPixelPosition * pPixelPosition)
{
	pPixelPosition->x = +m_v3CenterPosition.x;
	pPixelPosition->y = -m_v3CenterPosition.y;
	pPixelPosition->z = +m_v3CenterPosition.z;
}

void CPythonApplication::SetServerTime(time_t tTime)
{
	m_dwStartLocalTime	= ELTimer_GetMSec();
	m_tServerTime		= tTime;
	m_tLocalStartTime	= time(0);
}

time_t CPythonApplication::GetServerTime()
{
	return (ELTimer_GetMSec() - m_dwStartLocalTime) + m_tServerTime;
}

time_t CPythonApplication::GetServerTimeStamp()
{
	return (time(0) - m_tLocalStartTime) + m_tServerTime;
}

float CPythonApplication::GetGlobalTime()
{
	return m_fGlobalTime;
}

float CPythonApplication::GetGlobalElapsedTime()
{
	return m_fGlobalElapsedTime;
}

void CPythonApplication::SetFPS(int iFPS)
{
	m_iFPS = iFPS;
	DX::StepTimer::instance().SetTargetFPS(static_cast<uint32_t>(iFPS));
}

int CPythonApplication::GetWidth()
{
	return m_dwWidth;
}

int CPythonApplication::GetHeight()
{
	return m_dwHeight;
}

void CPythonApplication::SetConnectData(const char * c_szIP, int iPort)
{
	m_strIP = c_szIP;
	m_iPort = iPort;
}

void CPythonApplication::GetConnectData(std::string & rstIP, int & riPort)
{
	rstIP	= m_strIP;
	riPort	= m_iPort;
}

void CPythonApplication::EnableSpecialCameraMode()
{
	m_isSpecialCameraMode = TRUE;
}

void CPythonApplication::SetCameraSpeed(int iPercentage)
{
	m_fCameraRotateSpeed = c_fDefaultCameraRotateSpeed * float(iPercentage) / 100.0f;
	m_fCameraPitchSpeed = c_fDefaultCameraPitchSpeed * float(iPercentage) / 100.0f;
	m_fCameraZoomSpeed = c_fDefaultCameraZoomSpeed * float(iPercentage) / 100.0f;
}

void CPythonApplication::SetForceSightRange(int iRange)
{
	m_iForceSightRange = iRange;
}

void CPythonApplication::Clear()
{
	m_pySystem.Clear();
}

void CPythonApplication::Destroy()
{
	WebBrowser_Destroy();

	// SphereMap
	CGrannyMaterial::DestroySphereMap();

	m_kWndMgr.Destroy();

	CPythonSystem::Instance().SaveConfig();

	DestroyCollisionInstanceSystem();

	m_pySystem.SaveInterfaceStatus();

	m_pyEventManager.Destroy();
	m_FlyingManager.Destroy();

	m_pyMiniMap.Destroy();

	m_pyTextTail.Destroy();
	m_pyChat.Destroy();
	m_kChrMgr.Destroy();
	m_RaceManager.Destroy();

	m_pyItem.Destroy();
	m_kItemMgr.Destroy();

	m_pyBackground.Destroy();

	m_kEftMgr.Destroy();
	m_LightManager.Destroy();

	// DEFAULT_FONT
#ifdef ENABLE_IMGUI_MANAGER
	CImGuiManager::Instance().Shutdown();
#else
	DefaultFont_Cleanup();
#endif
	// END_OF_DEFAULT_FONT

	GrannyDestroySharedDeformBuffer();

	m_pyGraphic.Destroy();
	//m_pyNetworkDatagram.Destroy();

	#ifdef ENABLE_DISCORD_RPC
	m_pyNetworkStream.Discord_Close();
	#endif

	m_pyRes.Destroy();

	m_kGuildMarkDownloader.Disconnect();

	CGrannyModelInstance::DestroySystem();
	CGraphicImageInstance::DestroySystem();

	// SoundEngine cleanup is handled by destructor
	m_grpDevice.Destroy();

	//CSpeedTreeForestDX11::Instance().Clear();

	CAttributeInstance::DestroySystem();
	CTextFileLoader::DestroySystem();
	DestroyCursors();

	CMSApplication::Destroy();

	STICKYKEYS sStickKeys;
	memset(&sStickKeys, 0, sizeof(sStickKeys));
	sStickKeys.cbSize = sizeof(sStickKeys);
	sStickKeys.dwFlags = m_dwStickyKeysFlag;
	SystemParametersInfo( SPI_SETSTICKYKEYS, sizeof(sStickKeys), &sStickKeys, 0 );
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
