#pragma once

#include <vector>
#include "AttributeData.h"
#include "Pool.h"

class CAttributeInstance
{
	public:
		CAttributeInstance();
		virtual ~CAttributeInstance();

		void Clear();
		BOOL IsEmpty() const;

		const char * GetDataFileName() const;

		void SetObjectPointer(CAttributeData * pAttributeData);
		void RefreshObject(const Matrix & c_rmatGlobal);
		CAttributeData * GetObjectPointer() const;

		bool Picking(const Vector3 & v, const Vector3 & dir, float & out_x, float & out_y);

		BOOL IsInHeight(float fx, float fy);
		BOOL GetHeight(float fx, float fy, float * pfHeight);

		BOOL IsHeightData() const;

	protected:
		void SetGlobalMatrix(const Matrix & c_rmatGlobal);
		void SetGlobalPosition(const Vector3 & c_rv3Position);

	protected:
		float m_fCollisionRadius;
		float m_fHeightRadius;

		Matrix m_matGlobal;

		std::vector< std::vector<Vector3> > m_v3HeightDataVector;

		CAttributeData::TRef					m_roAttributeData;

		/*
		BOOL m_isHeightCached;
		struct SHeightCacheData
		{
			float fxMin;
			float fyMin;
			float fxMax;
			float fyMax;
			DWORD dwxStep;
			DWORD dwyStep;
			std::vector<float> kVec_fHeight;
		} m_kHeightCacheData;
		*/

	public:
		static void CreateSystem(UINT uCapacity);
		static void DestroySystem();

		static CAttributeInstance* New();
		static void Delete(CAttributeInstance* pkInst);

		static CDynamicPool<CAttributeInstance> ms_kPool;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
