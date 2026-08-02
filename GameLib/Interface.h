#pragma once

class IBackground : public CSingleton<IBackground>
{
	public:
		IBackground() {}
		virtual ~IBackground() {}

		virtual bool IsBlock(int x, int y) = 0;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
