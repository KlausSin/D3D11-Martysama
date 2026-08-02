#pragma once

class CNetworkDevice
{
	public:
		CNetworkDevice();
		virtual ~CNetworkDevice();

		void Destroy();
		bool Create();

	protected:
		void Initialize();

	protected:
		bool m_isWSA;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
