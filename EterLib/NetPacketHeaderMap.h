#pragma once

#include <map>

class CNetworkPacketHeaderMap
{
	public:
		typedef struct SPacketType
		{
			SPacketType(int iSize = 0, bool bFlag = false)
			{
				iPacketSize = iSize;
				isDynamicSizePacket = bFlag;
			}

			int iPacketSize;
			bool isDynamicSizePacket;
		} TPacketType;

	public:
		CNetworkPacketHeaderMap();
		virtual ~CNetworkPacketHeaderMap();

		void Set(int header, const TPacketType & rPacketType);
		bool Get(int header, TPacketType * pPacketType);

	protected:
		std::map<int, TPacketType> m_headerMap;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
