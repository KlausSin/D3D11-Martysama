#include "StdAfx.h"
#include "Timer.h"

#include "StepTimer.h"

static LARGE_INTEGER gs_liTickCountPerSec;
static DWORD gs_dwBaseTime=0;
static DWORD gs_dwServerTime=0;
static DWORD gs_dwClientTime=0;
static DWORD gs_dwFrameTime=0;

#pragma comment(lib, "winmm.lib")

BOOL ELTimer_Init()
{
	gs_dwBaseTime = timeGetTime();
	return 1;
}

DWORD ELTimer_GetMSec()
{
	return timeGetTime() - gs_dwBaseTime;
}

VOID	ELTimer_SetServerMSec(DWORD dwServerTime)
{
	if (0 != dwServerTime)
	{
		gs_dwServerTime = dwServerTime;
		gs_dwClientTime = DX::StepTimer::instance().GetTotalMillieSeconds();
	}
}

DWORD	ELTimer_GetServerMSec()
{
	return DX::StepTimer::instance().GetTotalMillieSeconds() - gs_dwClientTime + gs_dwServerTime;
}

DWORD	ELTimer_GetFrameMSec()
{
	return gs_dwFrameTime;
}

DWORD	ELTimer_GetServerFrameMSec()
{
	return ELTimer_GetFrameMSec() - gs_dwClientTime + gs_dwServerTime;
}

VOID	ELTimer_SetFrameMSec()
{
	gs_dwFrameTime = DX::StepTimer::instance().GetTotalMillieSeconds();
}

CTimer::CTimer()
{
	ELTimer_Init();

	if (this)
	{
		m_dwCurrentTime = 0;
		m_bUseRealTime = true;
		m_index = 0;

		m_dwElapsedTime = 0;

		m_fCurrentTime = 0.0f;
	}
}

CTimer::~CTimer()
{
}

void CTimer::SetBaseTime()
{
	m_dwCurrentTime = 0;
}

void CTimer::Advance()
{
	if (!m_bUseRealTime)
	{
		++m_index;

		if (m_index == 1)
			m_index = -1;

		m_dwCurrentTime += 16 + (m_index & 1);
		m_fCurrentTime = m_dwCurrentTime / 1000.0f;
	}
	else
	{
		DWORD currentTime = ELTimer_GetMSec();

		if (m_dwCurrentTime == 0)
			m_dwCurrentTime = currentTime;

		m_dwElapsedTime = currentTime - m_dwCurrentTime;
		m_dwCurrentTime = currentTime;
	}
}

void CTimer::Adjust(int iTimeGap)
{
	m_dwCurrentTime += iTimeGap;
}

float CTimer::GetCurrentSecond()
{
	return m_fCurrentTime;
}

DWORD CTimer::GetCurrentMillisecond()
{
	return m_dwCurrentTime;
}

float CTimer::GetElapsedSecond()
{
	return GetElapsedMilliecond() / 1000.0f;
}

DWORD CTimer::GetElapsedMilliecond()
{
	if (!m_bUseRealTime)

		return 16 + (m_index & 1);

	return m_dwElapsedTime;
}

void CTimer::UseCustomTime()
{
	m_bUseRealTime = false;
}

void CTimer::SetStep(DWORD dwCurrent, DWORD dwElapsed)
{
	m_dwCurrentTime = dwCurrent;
	m_dwElapsedTime = dwElapsed;
	m_fCurrentTime = dwCurrent / 1000.0f;
	m_bUseRealTime = true;
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
