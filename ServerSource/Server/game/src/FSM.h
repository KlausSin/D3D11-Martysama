#ifndef _fsm_fsm_h
#define _fsm_fsm_h

// Local Includes
#include "state.h"

// FSM Class
class CFSM
{
	protected:
		CState *		m_pCurrentState;	// Current State
		CState *		m_pNewState;		// New State
		CStateTemplate<CFSM>	m_stateInitial;		// Initial State

	public:
		// Constructor
		CFSM();

		// Destructor
		virtual ~CFSM() {}

		// Global Functions
		virtual void Update();

		// State Functions
		bool IsState(CState &State) const;
		bool GotoState(CState &NewState);

		virtual void BeginStateInitial() {}
		virtual void StateInitial() {}
		virtual void EndStateInitial() {}
};

#endif
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
