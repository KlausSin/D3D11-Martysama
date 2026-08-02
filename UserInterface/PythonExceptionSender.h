#pragma once

class CPythonExceptionSender : public IPythonExceptionSender
{
	public:
		CPythonExceptionSender();
		virtual ~CPythonExceptionSender();

		void Send();

	protected:
		std::set<DWORD> m_kSet_dwSendedExceptionCRC;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
