#pragma once

#include <windows.h>
#include "Singleton.h"

class CTimer : public CSingleton<CTimer>
{
	public:
		CTimer();
		virtual ~CTimer();

		void	Advance();
		void	Adjust(int iTimeGap);
		void	SetBaseTime();

		float	GetCurrentSecond();
		DWORD	GetCurrentMillisecond();

		float	GetElapsedSecond();
		DWORD	GetElapsedMilliecond();

		void	UseCustomTime();
		void	SetStep(DWORD dwCurrent, DWORD dwElapsed);

	protected:
		bool	m_bUseRealTime;
		DWORD	m_dwBaseTime;
		DWORD	m_dwCurrentTime;
		float	m_fCurrentTime;
		DWORD	m_dwElapsedTime;
		int		m_index;
};

BOOL	ELTimer_Init();

DWORD	ELTimer_GetMSec();

VOID	ELTimer_SetServerMSec(DWORD dwServerTime);
DWORD	ELTimer_GetServerMSec();

VOID	ELTimer_SetFrameMSec();
DWORD	ELTimer_GetFrameMSec();
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
