#pragma once

class CReferenceObject
{
	public:
		CReferenceObject();
		virtual ~CReferenceObject();

		void AddReference();
		void AddReferenceOnly();
		void Release();

		int GetReferenceCount();

		bool canDestroy();

	protected:
		virtual void OnConstruct();
		virtual void OnSelfDestruct();

	private:
		int m_refCount;
		bool m_destructed;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
