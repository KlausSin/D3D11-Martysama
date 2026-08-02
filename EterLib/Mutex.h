#ifndef __INC_ETERLIB_MUTEX_H__
#define __INC_ETERLIB_MUTEX_H__

class Mutex
{
	public:
		Mutex();
		~Mutex();

		void Lock();
		void Unlock();
		bool Trylock();

	private:
		CRITICAL_SECTION lock;
};

#endif
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
