#pragma once

#include "AbstractSingleton.h"

class CInstanceBase;

class IAbstractCharacterManager : public TAbstractSingleton<IAbstractCharacterManager>
{
	public:
		IAbstractCharacterManager() {}
		virtual ~IAbstractCharacterManager() {}

		virtual void Destroy() = 0;
		virtual CInstanceBase *						GetInstancePtr(DWORD dwVID) = 0;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
