#pragma once

#ifndef VC_EXTRALEAN

class CNetDatagramSender
{
	public:
		CNetDatagramSender();
		virtual ~CNetDatagramSender();

		BOOL isSocket();

		BOOL SetSocket(const char * c_szIP, WORD wPortIndex);
		BOOL SetSocket(DWORD dwAddress, WORD wPortIndex);
		BOOL Send(const void * pBuffer, int iSize);

	protected:
		BOOL m_isSocket;

		WORD m_dwAddress;
		WORD m_wPortIndex;

		SOCKET m_Socket;
		SOCKADDR_IN m_SockAddr;
};

#endif
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
