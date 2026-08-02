#pragma once

#include "EffectElementBaseInstance.h"
#include "ParticleInstance.h"
#include "ParticleProperty.h"

#include "../eterlib/GrpScreen.h"
#include "../eterlib/ShaderManager.h"
#include "../eterLib/GrpImageInstance.h"
#include "EmitterProperty.h"

class CParticleSystemInstance : public CEffectElementBaseInstance
{
	public:
		static void DestroySystem();

		static CParticleSystemInstance* New();
		static void Delete(CParticleSystemInstance* pkData);

		static CDynamicPool<CParticleSystemInstance>	ms_kPool;

		// Batch rendering entry: pre-transformed quad + color
		struct ParticleBatchEntry
		{
			TPTVertex verts[4];  // 80 bytes
			DWORD     dwColor;   // 4 bytes
		};

		static std::vector<ParticleBatchEntry> ms_BatchBuffer;

	public:
		CParticleSystemInstance();
		virtual ~CParticleSystemInstance();

		void OnSetDataPointer(CEffectElementBase * pElement);

		void CreateParticles(float fElapsedTime);

		inline bool InFrustum(CParticleInstance * pInstance)
		{
			if (m_pParticleProperty->m_bAttachFlag)
				return CScreen::GetFrustum().ViewVolumeTest(Vector3d(
					pInstance->m_v3Position.x + mc_pmatLocal->_41,
					pInstance->m_v3Position.y + mc_pmatLocal->_42,
					pInstance->m_v3Position.z + mc_pmatLocal->_43
					),pInstance->GetRadiusApproximation())!=VS_OUTSIDE;
			else
				return CScreen::GetFrustum().ViewVolumeTest(Vector3d(pInstance->m_v3Position.x,pInstance->m_v3Position.y,pInstance->m_v3Position.z),pInstance->GetRadiusApproximation())!=VS_OUTSIDE;
		}

		DWORD GetEmissionCount();

	protected:
		void OnInitialize();
		void OnDestroy();

		bool OnUpdate(float fElapsedTime);
		void OnRender();

		// Batched particle rendering (CPU fallback)
		void RenderBatched(const Matrix* pAttachMatrix, int facesPerParticle, const float fRotations[]);
		static void FlushParticleBatch();

		void RenderBatchedCS(const Matrix* pAttachMatrix, int facesPerParticle, const float fRotations[]);
		static std::vector<ParticleGPUInput> ms_csInputBuffer;

		void ContributeToBatch();

	protected:
		float m_fEmissionResidue;

		DWORD m_dwCurrentEmissionCount;
		int	m_iLoopCount;

		typedef std::list<CParticleInstance*> TParticleInstanceList;
		typedef std::vector<TParticleInstanceList> TParticleInstanceListVector;
		TParticleInstanceListVector m_ParticleInstanceListVector;

		typedef std::vector<CGraphicImageInstance*> TImageInstanceVector;
		TImageInstanceVector m_kVct_pkImgInst;

		CParticleSystemData * m_pData;

		CParticleProperty * m_pParticleProperty;
		CEmitterProperty * m_pEmitterProperty;
};
