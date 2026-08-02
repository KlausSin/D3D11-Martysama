#include "StdAfx.h"
#include "../eterBase/Random.h"
#include "../eterLib/ShaderManager.h"
#include "../eterLib/ShaderInit.h"
#include "../eterLib/GrpBase.h"
#include "ParticleSystemData.h"
#include "ParticleSystemInstance.h"
#include "ParticleInstance.h"
#include "EffectInstance.h"
#include "GpuParticlePool.h"

#include <algorithm>

extern void BeginParticleShaderRender();
extern void EndParticleShaderRender();

CDynamicPool<CParticleSystemInstance>	CParticleSystemInstance::ms_kPool;
std::vector<CParticleSystemInstance::ParticleBatchEntry> CParticleSystemInstance::ms_BatchBuffer;
std::vector<ParticleGPUInput> CParticleSystemInstance::ms_csInputBuffer;

using namespace NEffectUpdateDecorator;

void CParticleSystemInstance::DestroySystem()
{
	ms_kPool.Destroy();
	ms_BatchBuffer.clear();
	ms_BatchBuffer.shrink_to_fit();
	ms_csInputBuffer.clear();
	ms_csInputBuffer.shrink_to_fit();

	// Shutdown GPU particle pool
	CGpuParticlePool::Instance().Shutdown();

	CParticleInstance::DestroySystem();
}

CParticleSystemInstance* CParticleSystemInstance::New()
{
	return ms_kPool.Alloc();
}

void CParticleSystemInstance::Delete(CParticleSystemInstance* pkPSInst)
{
	pkPSInst->Destroy();
	ms_kPool.Free(pkPSInst);
}

DWORD CParticleSystemInstance::GetEmissionCount()
{
	return m_dwCurrentEmissionCount;
}

void CParticleSystemInstance::CreateParticles(float fElapsedTime)
{
	float fEmissionCount;
	m_pEmitterProperty->GetEmissionCountPerSecond(m_fLocalTime, &fEmissionCount);

	float fCreatingValue = fEmissionCount * (fElapsedTime / 1.0f) + m_fEmissionResidue;
	int iCreatingCount = int(fCreatingValue);
	m_fEmissionResidue = fCreatingValue - iCreatingCount;

	int icurEmissionCount = GetEmissionCount();
	int iMaxEmissionCount = int(m_pEmitterProperty->GetMaxEmissionCount());
	int iNextEmissionCount = int(icurEmissionCount + iCreatingCount);
	iCreatingCount -= max(0, iNextEmissionCount - iMaxEmissionCount);

	float fLifeTime = 0.0f;
	float fEmittingSize = 0.0f;
	Vector3 _v3TimePosition;
	Vector3 _v3Velocity;
	float fVelocity = 0.0f;
	Vector2 v2HalfSize;
	float fLieRotation = 0;
	if (iCreatingCount)
	{
		m_pEmitterProperty->GetParticleLifeTime(m_fLocalTime, &fLifeTime);
		if (fLifeTime==0.0f)
		{
			return;
		}

		m_pEmitterProperty->GetEmittingSize(m_fLocalTime, &fEmittingSize);

		m_pData->GetPosition(m_fLocalTime, _v3TimePosition);

		m_pEmitterProperty->GetEmittingDirectionX(m_fLocalTime, &_v3Velocity.x);
		m_pEmitterProperty->GetEmittingDirectionY(m_fLocalTime, &_v3Velocity.y);
		m_pEmitterProperty->GetEmittingDirectionZ(m_fLocalTime, &_v3Velocity.z);

		m_pEmitterProperty->GetEmittingVelocity(m_fLocalTime, &fVelocity);

		m_pEmitterProperty->GetParticleSizeX(m_fLocalTime, &v2HalfSize.x);
		m_pEmitterProperty->GetParticleSizeY(m_fLocalTime, &v2HalfSize.y);

		if (BILLBOARD_TYPE_LIE == m_pParticleProperty->m_byBillboardType && mc_pmatLocal)
		{
			float fsx = mc_pmatLocal->_32;
			float fcx = sqrtf(1.0f - fsx * fsx);

			if (fcx >= 0.00001f)
				fLieRotation = ToDegree(atan2f(-mc_pmatLocal->_12, mc_pmatLocal->_22));
		}

	}

	CParticleInstance * pFirstInstance = 0;

	for (int i = 0; i < iCreatingCount; ++i)
	{
		CParticleInstance * pInstance;

		pInstance = CParticleInstance::New();
		pInstance->m_pParticleProperty = m_pParticleProperty;
		pInstance->m_pEmitterProperty = m_pEmitterProperty;

		// LifeTime
		pInstance->m_fLifeTime = fLifeTime;
		pInstance->m_fLastLifeTime = fLifeTime;

		// Position
		switch (m_pEmitterProperty->GetEmitterShape())
		{
			case CEmitterProperty::EMITTER_SHAPE_POINT:
				pInstance->m_v3Position.x = 0.0f;
				pInstance->m_v3Position.y = 0.0f;
				pInstance->m_v3Position.z = 0.0f;
				break;

			case CEmitterProperty::EMITTER_SHAPE_ELLIPSE:
				pInstance->m_v3Position.x = frandom(-500.0f, 500.0f);
				pInstance->m_v3Position.y = frandom(-500.0f, 500.0f);
				pInstance->m_v3Position.z = 0.0f;
				Vec3Normalize(&pInstance->m_v3Position, &pInstance->m_v3Position);

				if (m_pEmitterProperty->isEmitFromEdge())
				{
					pInstance->m_v3Position *= (m_pEmitterProperty->m_fEmittingRadius + fEmittingSize);
				}
				else
				{
					pInstance->m_v3Position *= (frandom(0.0f, m_pEmitterProperty->m_fEmittingRadius) + fEmittingSize);
				}
				break;

			case CEmitterProperty::EMITTER_SHAPE_SQUARE:
				pInstance->m_v3Position.x = (frandom(-m_pEmitterProperty->m_v3EmittingSize.x/2.0f, m_pEmitterProperty->m_v3EmittingSize.x/2.0f) + fEmittingSize);
				pInstance->m_v3Position.y = (frandom(-m_pEmitterProperty->m_v3EmittingSize.y/2.0f, m_pEmitterProperty->m_v3EmittingSize.y/2.0f) + fEmittingSize);
				pInstance->m_v3Position.z = (frandom(-m_pEmitterProperty->m_v3EmittingSize.z/2.0f, m_pEmitterProperty->m_v3EmittingSize.z/2.0f) + fEmittingSize);
				break;

			case CEmitterProperty::EMITTER_SHAPE_SPHERE:
				pInstance->m_v3Position.x = frandom(-500.0f, 500.0f);
				pInstance->m_v3Position.y = frandom(-500.0f, 500.0f);
				pInstance->m_v3Position.z = frandom(-500.0f, 500.0f);
				Vec3Normalize(&pInstance->m_v3Position, &pInstance->m_v3Position);

				if (m_pEmitterProperty->isEmitFromEdge())
				{
					pInstance->m_v3Position *= (m_pEmitterProperty->m_fEmittingRadius + fEmittingSize);
				}
				else
				{
					pInstance->m_v3Position *= (frandom(0.0f, m_pEmitterProperty->m_fEmittingRadius) + fEmittingSize);
				}
				break;
		}

		// Position
		Vector3 v3TimePosition=_v3TimePosition;

		pInstance->m_v3Position += v3TimePosition;

		if (mc_pmatLocal && !m_pParticleProperty->m_bAttachFlag)
		{
			Vec3TransformCoord(&pInstance->m_v3Position,&pInstance->m_v3Position,mc_pmatLocal);
			Vec3TransformCoord(&v3TimePosition, &v3TimePosition, mc_pmatLocal);
		}
		pInstance->m_v3StartPosition = v3TimePosition;

		// Direction & Velocity
		pInstance->m_v3Velocity.x = 0.0f;
		pInstance->m_v3Velocity.y = 0.0f;
		pInstance->m_v3Velocity.z = 0.0f;

		if (CEmitterProperty::EMITTER_ADVANCED_TYPE_INNER == m_pEmitterProperty->GetEmitterAdvancedType())
		{
			auto val = (pInstance->m_v3Position-v3TimePosition);
			Vec3Normalize(&pInstance->m_v3Velocity, &val);
			pInstance->m_v3Velocity *= -100.0f;
		}
		else if (CEmitterProperty::EMITTER_ADVANCED_TYPE_OUTER == m_pEmitterProperty->GetEmitterAdvancedType())
		{
			if (m_pEmitterProperty->GetEmitterShape() == CEmitterProperty::EMITTER_SHAPE_POINT)
			{
				pInstance->m_v3Velocity.x = frandom(-100.0f, 100.0f);
				pInstance->m_v3Velocity.y = frandom(-100.0f, 100.0f);
				pInstance->m_v3Velocity.z = frandom(-100.0f, 100.0f);
			}
			else
			{
				auto val = (pInstance->m_v3Position-v3TimePosition);
				Vec3Normalize(&pInstance->m_v3Velocity, &val);
				pInstance->m_v3Velocity *= 100.0f;
			}
		}

		Vector3 v3Velocity = _v3Velocity;
		if (mc_pmatLocal && !m_pParticleProperty->m_bAttachFlag)
		{
			Vec3TransformNormal(&v3Velocity, &v3Velocity, mc_pmatLocal);
		}

		pInstance->m_v3Velocity += v3Velocity;
		if (m_pEmitterProperty->m_v3EmittingDirection.x > 0.0f)
			pInstance->m_v3Velocity.x += frandom(-m_pEmitterProperty->m_v3EmittingDirection.x/2.0f, m_pEmitterProperty->m_v3EmittingDirection.x/2.0f) * 1000.0f;
		if (m_pEmitterProperty->m_v3EmittingDirection.y > 0.0f)
			pInstance->m_v3Velocity.y += frandom(-m_pEmitterProperty->m_v3EmittingDirection.y/2.0f, m_pEmitterProperty->m_v3EmittingDirection.y/2.0f) * 1000.0f;
		if (m_pEmitterProperty->m_v3EmittingDirection.z > 0.0f)
			pInstance->m_v3Velocity.z += frandom(-m_pEmitterProperty->m_v3EmittingDirection.z/2.0f, m_pEmitterProperty->m_v3EmittingDirection.z/2.0f) * 1000.0f;

		pInstance->m_v3Velocity *= fVelocity;

		// Size
		pInstance->m_v2HalfSize = v2HalfSize;

		// Rotation
		pInstance->m_fRotation = m_pParticleProperty->m_wRotationRandomStartingBegin;
		pInstance->m_fRotation = frandom(m_pParticleProperty->m_wRotationRandomStartingBegin,m_pParticleProperty->m_wRotationRandomStartingEnd);
		if (BILLBOARD_TYPE_LIE == m_pParticleProperty->m_byBillboardType && mc_pmatLocal)
		{
			pInstance->m_fRotation += fLieRotation;
		}

		// Texture Animation
		pInstance->m_byFrameIndex = 0;
		pInstance->m_byTextureAnimationType = m_pParticleProperty->GetTextureAnimationType();

		if (m_pParticleProperty->GetTextureAnimationFrameCount() > 1)
		{
			if (CParticleProperty::TEXTURE_ANIMATION_TYPE_RANDOM_DIRECTION == m_pParticleProperty->GetTextureAnimationType())
			{
				if (random() & 1)
				{
					pInstance->m_byFrameIndex = 0;
					pInstance->m_byTextureAnimationType = CParticleProperty::TEXTURE_ANIMATION_TYPE_CW;
				}
				else
				{
					pInstance->m_byFrameIndex = m_pParticleProperty->GetTextureAnimationFrameCount() - 1;
					pInstance->m_byTextureAnimationType = CParticleProperty::TEXTURE_ANIMATION_TYPE_CCW;
				}
			}
			if (m_pParticleProperty->m_bTexAniRandomStartFrameFlag)
			{
				pInstance->m_byFrameIndex = random_range(0,m_pParticleProperty->GetTextureAnimationFrameCount()-1);
			}
		}

		// Simple Update
		{
			pInstance->m_v3LastPosition = pInstance->m_v3Position - (pInstance->m_v3Velocity * fElapsedTime);
			pInstance->m_v2Scale.x = m_pParticleProperty->m_TimeEventScaleX.front().m_Value;
			pInstance->m_v2Scale.y= m_pParticleProperty->m_TimeEventScaleY.front().m_Value;
#ifdef WORLD_EDITOR
			pInstance->m_Color.r = m_pParticleProperty->m_TimeEventColorRed.front().m_Value;
			pInstance->m_Color.g = m_pParticleProperty->m_TimeEventColorGreen.front().m_Value;
			pInstance->m_Color.b = m_pParticleProperty->m_TimeEventColorBlue.front().m_Value;
			pInstance->m_Color.a = m_pParticleProperty->m_TimeEventAlpha.front().m_Value;
#else
			pInstance->m_dcColor = m_pParticleProperty->m_TimeEventColor.front().m_Value;
#endif
		}

		if (!pFirstInstance)
		{
			m_pData->BuildDecorator(pInstance);
			pFirstInstance = pInstance;
		}
		else
		{
			pInstance->m_pDecorator = pFirstInstance->m_pDecorator->Clone(pFirstInstance,pInstance);
		}

		m_ParticleInstanceListVector[pInstance->m_byFrameIndex].push_back(pInstance);
		m_dwCurrentEmissionCount++;
	}
}

bool CParticleSystemInstance::OnUpdate(float fElapsedTime)
{
	bool bMakeParticle = true;

	/////

	if (m_fLocalTime >= m_pEmitterProperty->GetCycleLength())
	{
		if (m_pEmitterProperty->isCycleLoop() && --m_iLoopCount!=0)
		{
			if (m_iLoopCount<0)
				m_iLoopCount = 0;
			m_fLocalTime = m_fLocalTime - m_pEmitterProperty->GetCycleLength();
		}
		else
		{
			bMakeParticle = false;
			m_iLoopCount=1;
			if (GetEmissionCount()==0)
				return false;
		}
	}

	/////

	int dwFrameIndex;
	int dwFrameCount = m_pParticleProperty->GetTextureAnimationFrameCount();

	float fAngularVelocity;
	m_pEmitterProperty->GetEmittingAngularVelocity(m_fLocalTime,&fAngularVelocity);

	if (fAngularVelocity && !m_pParticleProperty->m_bAttachFlag)
	{
		auto val = Vector3(0.0f,0.0f,1.0f);
		Vec3TransformNormal(&m_pParticleProperty->m_v3ZAxis,&val,mc_pmatLocal);
	}

	for (dwFrameIndex = 0; dwFrameIndex < dwFrameCount; dwFrameIndex++)
	{
		TParticleInstanceList::iterator itor = m_ParticleInstanceListVector[dwFrameIndex].begin();
		for (; itor != m_ParticleInstanceListVector[dwFrameIndex].end();)
		{
			CParticleInstance * pInstance = *itor;

			if (!pInstance->Update(fElapsedTime,fAngularVelocity))
			{
				pInstance->DeleteThis();

				itor = m_ParticleInstanceListVector[dwFrameIndex].erase(itor);
				m_dwCurrentEmissionCount--;
			}
			else
			{
				if (pInstance->m_byFrameIndex != dwFrameIndex)
				{
					m_ParticleInstanceListVector[dwFrameCount+pInstance->m_byFrameIndex].push_back(*itor);
					itor = m_ParticleInstanceListVector[dwFrameIndex].erase(itor);
				}
				else
					++itor;
			}
		}
	}
	if (isActive() && bMakeParticle)
		CreateParticles(fElapsedTime);

	for (dwFrameIndex = 0; dwFrameIndex < dwFrameCount; ++dwFrameIndex)
	{
		m_ParticleInstanceListVector[dwFrameIndex].splice(m_ParticleInstanceListVector[dwFrameIndex].end(),m_ParticleInstanceListVector[dwFrameIndex+dwFrameCount]);
		m_ParticleInstanceListVector[dwFrameIndex+dwFrameCount].clear();
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Batched Particle Rendering
//////////////////////////////////////////////////////////////////////////

void CParticleSystemInstance::RenderBatched(const Matrix* pAttachMatrix, int facesPerParticle, const float fRotations[])
{
	for (DWORD dwFrameIndex = 0; dwFrameIndex < m_kVct_pkImgInst.size(); dwFrameIndex++)
	{
		SHADERMANAGER.SetShaderResource(0, m_kVct_pkImgInst[dwFrameIndex]->GetTextureReference().GetD3DTexture());

		ms_BatchBuffer.clear();

		TParticleInstanceList::iterator itor = m_ParticleInstanceListVector[dwFrameIndex].begin();
		for (; itor != m_ParticleInstanceListVector[dwFrameIndex].end(); ++itor)
		{
			CParticleInstance* pInstance = *itor;

			if (!InFrustum(pInstance))
				continue;

			DWORD dwColor = pInstance->GetColor();

			for (int face = 0; face < facesPerParticle; face++)
			{
				if (fRotations[face] == 0.0f)
					pInstance->Transform(pAttachMatrix);
				else
					pInstance->Transform(pAttachMatrix, fRotations[face]);

				ParticleBatchEntry entry;
				memcpy(entry.verts, pInstance->GetParticleMeshPointer(), sizeof(TPTVertex) * 4);
				entry.dwColor = dwColor;
				ms_BatchBuffer.push_back(entry);
			}
		}

		FlushParticleBatch();
	}
}

void CParticleSystemInstance::FlushParticleBatch()
{
	if (ms_BatchBuffer.empty())
		return;

	static const UINT MAX_BATCH_QUADS = 512;

	std::stable_sort(ms_BatchBuffer.begin(), ms_BatchBuffer.end(),
		[](const ParticleBatchEntry& a, const ParticleBatchEntry& b) {
			return a.dwColor < b.dwColor;
		});

	size_t totalQuads = ms_BatchBuffer.size();
	size_t offset = 0;

	while (offset < totalQuads)
	{
		size_t chunkSize = totalQuads - offset;
		if (chunkSize > MAX_BATCH_QUADS)
			chunkSize = MAX_BATCH_QUADS;

		UINT dataBytes = (UINT)(chunkSize * 4 * sizeof(TPTVertex));

		// Map dynamic VB and copy all quad vertices contiguously
		CShaderManager::MappedDynamicVB mapped;
		if (!SHADERMANAGER.MapDynamicVB(dataBytes, mapped))
		{
			offset += chunkSize;
			continue;
		}

		TPTVertex* pDest = (TPTVertex*)mapped.pData;
		for (size_t i = 0; i < chunkSize; i++)
		{
			memcpy(pDest, ms_BatchBuffer[offset + i].verts, sizeof(TPTVertex) * 4);
			pDest += 4;
		}

		SHADERMANAGER.UnmapDynamicVB();
		SHADERMANAGER.AdvanceDynamicVBOffset(dataBytes);

		// Bind multi-rect index buffer
		CGraphicBase::SetDefaultIndexBuffer(CGraphicBase::DEFAULT_IB_FILL_RECT_MULTI);

		// Draw per color group
		UINT stride = sizeof(TPTVertex);
		UINT vbByteOffset = mapped.byteOffset;
		size_t groupStart = 0;
		DWORD currentColor = ms_BatchBuffer[offset].dwColor;

		for (size_t i = 1; i <= chunkSize; i++)
		{
			bool isEnd = (i == chunkSize);
			bool colorChanged = !isEnd && (ms_BatchBuffer[offset + i].dwColor != currentColor);

			if (isEnd || colorChanged)
			{
				UINT groupSize = (UINT)(i - groupStart);
				SHADERMANAGER.SetParticleColor(currentColor);
				SHADERMANAGER.DrawBatchedQuads(stride, vbByteOffset, (UINT)groupStart, groupSize);

				if (!isEnd)
				{
					groupStart = i;
					currentColor = ms_BatchBuffer[offset + i].dwColor;
				}
			}
		}

		offset += chunkSize;
	}
}

void CParticleSystemInstance::ContributeToBatch()
{
	SHADERMANAGER.StatsNotePSIContribution();

	// Determine face count + rotations like OnRender would
	int facesPerParticle = 1;
	float rotations[3] = { 0.0f, 0.0f, 0.0f };
	if (m_pParticleProperty->m_byBillboardType == BILLBOARD_TYPE_2FACE)
	{
		facesPerParticle = 2;
		rotations[0] = ToRadian(-30.0f);
		rotations[1] = ToRadian(30.0f);
	}
	else if (m_pParticleProperty->m_byBillboardType == BILLBOARD_TYPE_3FACE)
	{
		facesPerParticle = 3;
		rotations[0] = 0.0f;
		rotations[1] = ToRadian(-60.0f);
		rotations[2] = ToRadian(60.0f);
	}

	const UINT psiFlags = (UINT)m_pParticleProperty->m_byBillboardType
		| (m_pParticleProperty->m_bStretchFlag ? 0x10 : 0);
	// (no 0x20 attach flag here — attach PSIs take the per-PSI path)

	for (DWORD dwFrameIndex = 0; dwFrameIndex < m_kVct_pkImgInst.size(); ++dwFrameIndex)
	{
		ID3D11ShaderResourceView* pTex = m_kVct_pkImgInst[dwFrameIndex]
			->GetTextureReference().GetD3DTexture();
		if (!pTex)
			continue;

		CShaderManager::ParticleBatchKey key = {};
		key.pTexture = pTex;
		key.srcBlend = m_pParticleProperty->m_bySrcBlendType;
		key.destBlend = m_pParticleProperty->m_byDestBlendType;
		key.colorOp = m_pParticleProperty->m_byColorOperationType;
		key.facesPerParticle = (BYTE)facesPerParticle;
		key.stretchFlag = m_pParticleProperty->m_bStretchFlag ? 1 : 0;
		key.rot0 = rotations[0];
		key.rot1 = rotations[1];
		key.rot2 = rotations[2];

		TParticleInstanceList::iterator itor = m_ParticleInstanceListVector[dwFrameIndex].begin();
		for (; itor != m_ParticleInstanceListVector[dwFrameIndex].end(); ++itor)
		{
			CParticleInstance* pInstance = *itor;
			if (!InFrustum(pInstance))
				continue;

			ParticleGPUInput input;
			input.posX = pInstance->m_v3Position.x;
			input.posY = pInstance->m_v3Position.y;
			input.posZ = pInstance->m_v3Position.z;
			input.lastPosX = pInstance->m_v3LastPosition.x;
			input.lastPosY = pInstance->m_v3LastPosition.y;
			input.lastPosZ = pInstance->m_v3LastPosition.z;
			input.halfW = pInstance->m_v2HalfSize.x;
			input.halfH = pInstance->m_v2HalfSize.y;
			input.scaleX = pInstance->m_v2Scale.x;
			input.scaleY = pInstance->m_v2Scale.y;
			input.rotation = ToRadian(pInstance->m_fRotation);
			input.color = pInstance->GetColor();
			input.flags = psiFlags;
			input._pad[0] = 0.0f;
			input._pad[1] = 0.0f;
			input._pad[2] = 0.0f;

			SHADERMANAGER.AddParticleToBatch(key, input);
		}
	}
}

void CParticleSystemInstance::RenderBatchedCS(const Matrix* pAttachMatrix, int facesPerParticle, const float fRotations[])
{
	for (DWORD dwFrameIndex = 0; dwFrameIndex < m_kVct_pkImgInst.size(); dwFrameIndex++)
	{
		SHADERMANAGER.SetShaderResource(0, m_kVct_pkImgInst[dwFrameIndex]->GetTextureReference().GetD3DTexture());

		ms_csInputBuffer.clear();

		TParticleInstanceList::iterator itor = m_ParticleInstanceListVector[dwFrameIndex].begin();
		for (; itor != m_ParticleInstanceListVector[dwFrameIndex].end(); ++itor)
		{
			CParticleInstance* pInstance = *itor;

			if (!InFrustum(pInstance))
				continue;

			ParticleGPUInput input;
			input.posX = pInstance->m_v3Position.x;
			input.posY = pInstance->m_v3Position.y;
			input.posZ = pInstance->m_v3Position.z;
			input.lastPosX = pInstance->m_v3LastPosition.x;
			input.lastPosY = pInstance->m_v3LastPosition.y;
			input.lastPosZ = pInstance->m_v3LastPosition.z;
			input.halfW = pInstance->m_v2HalfSize.x;
			input.halfH = pInstance->m_v2HalfSize.y;
			input.scaleX = pInstance->m_v2Scale.x;
			input.scaleY = pInstance->m_v2Scale.y;
			input.rotation = ToRadian(pInstance->m_fRotation);
			input.color = pInstance->GetColor();
			input.flags = (UINT)m_pParticleProperty->m_byBillboardType
				| (m_pParticleProperty->m_bStretchFlag ? 0x10 : 0)
				| (m_pParticleProperty->m_bAttachFlag ? 0x20 : 0);
			input._pad[0] = 0.0f;
			input._pad[1] = 0.0f;
			input._pad[2] = 0.0f;
			ms_csInputBuffer.push_back(input);
		}

		if (ms_csInputBuffer.empty())
			continue;

		UINT totalParticles = (UINT)ms_csInputBuffer.size();
		float fRots[3] = { fRotations[0],
			facesPerParticle > 1 ? fRotations[1] : 0.0f,
			facesPerParticle > 2 ? fRotations[2] : 0.0f };

		UINT offset = 0;
		while (offset < totalParticles)
		{
			UINT chunkSize = min(totalParticles - offset, CShaderManager::MAX_CS_PARTICLES);

			if (!SHADERMANAGER.DispatchParticleBillboardCS(
					ms_csInputBuffer.data() + offset, chunkSize,
					(UINT)facesPerParticle, fRots, pAttachMatrix))
				break;

			UINT quadCount = chunkSize * (UINT)facesPerParticle;
			SHADERMANAGER.DrawParticleCSOutput(quadCount);
			offset += chunkSize;
		}
	}
}

void CParticleSystemInstance::OnRender()
{
	CScreen::Identity();

	if (CEffectInstance::ms_bBatchRenderState
		&& SHADERMANAGER.IsParticleBatchingActive()
		&& SHADERMANAGER.IsComputeParticlesAvailable()
		&& !m_pParticleProperty->m_bAttachFlag)
	{
		ContributeToBatch();
		return;
	}

	if (!CEffectInstance::ms_bBatchRenderState)
		BeginParticleShaderRender();
	else
		SHADERMANAGER.BeginParticle();

	SHADERMANAGER.SetPipelineState(PSTATE_SRCBLEND, m_pParticleProperty->m_bySrcBlendType);
	SHADERMANAGER.SetPipelineState(PSTATE_DESTBLEND, m_pParticleProperty->m_byDestBlendType);
	SHADERMANAGER.SetParticleColorOp(m_pParticleProperty->m_byColorOperationType);

	const Matrix* pAttachMatrix = m_pParticleProperty->m_bAttachFlag ? mc_pmatLocal : NULL;

	bool bUseCS = SHADERMANAGER.IsComputeParticlesAvailable();

	if (bUseCS)
		SHADERMANAGER.BeginParticlePCT();

	if (m_pParticleProperty->m_byBillboardType < BILLBOARD_TYPE_2FACE)
	{
		float rotations[] = { 0.0f };
		if (bUseCS)
			RenderBatchedCS(pAttachMatrix, 1, rotations);
		else
			RenderBatched(pAttachMatrix, 1, rotations);
	}
	else if (m_pParticleProperty->m_byBillboardType == BILLBOARD_TYPE_2FACE)
	{
		float rotations[] = { ToRadian(-30.0f), ToRadian(30.0f) };
		if (bUseCS)
			RenderBatchedCS(pAttachMatrix, 2, rotations);
		else
			RenderBatched(pAttachMatrix, 2, rotations);
	}
	else if (m_pParticleProperty->m_byBillboardType == BILLBOARD_TYPE_3FACE)
	{
		float rotations[] = { 0.0f, ToRadian(-60.0f), ToRadian(60.0f) };
		if (bUseCS)
			RenderBatchedCS(pAttachMatrix, 3, rotations);
		else
			RenderBatched(pAttachMatrix, 3, rotations);
	}

	if (!CEffectInstance::ms_bBatchRenderState)
		EndParticleShaderRender();
}

void CParticleSystemInstance::OnSetDataPointer(CEffectElementBase * pElement)
{
	m_pData = (CParticleSystemData *)pElement;

	m_dwCurrentEmissionCount = 0;
	m_pParticleProperty = m_pData->GetParticlePropertyPointer();
	m_pEmitterProperty = m_pData->GetEmitterPropertyPointer();
	m_iLoopCount = m_pEmitterProperty->GetLoopCount();
	m_ParticleInstanceListVector.resize(m_pParticleProperty->GetTextureAnimationFrameCount()*2+2);

	/////

	assert(m_kVct_pkImgInst.empty());
	m_kVct_pkImgInst.reserve(m_pParticleProperty->m_ImageVector.size());
	for (DWORD i = 0; i < m_pParticleProperty->m_ImageVector.size(); ++i)
	{
		CGraphicImage * pImage = m_pParticleProperty->m_ImageVector[i];

		CGraphicImageInstance* pkImgInstNew = CGraphicImageInstance::New();
		pkImgInstNew->SetImagePointer(pImage);
		m_kVct_pkImgInst.push_back(pkImgInstNew);
	}
}

void CParticleSystemInstance::OnInitialize()
{
	m_dwCurrentEmissionCount = 0;
	m_iLoopCount = 0;
	m_fEmissionResidue = 0.0f;
}

void CParticleSystemInstance::OnDestroy()
{
	TParticleInstanceListVector::iterator i;
	for(i = m_ParticleInstanceListVector.begin(); i!=m_ParticleInstanceListVector.end(); ++i)
	{
		TParticleInstanceList& rkLst_kParticleInst=*i;

		TParticleInstanceList::iterator j;
		for(j = rkLst_kParticleInst.begin(); j!=rkLst_kParticleInst.end(); ++j)
		{
			CParticleInstance* pkParticleInst=*j;
			pkParticleInst->DeleteThis();
		}

		rkLst_kParticleInst.clear();
	}
	m_ParticleInstanceListVector.clear();

	std::for_each(m_kVct_pkImgInst.begin(), m_kVct_pkImgInst.end(), CGraphicImageInstance::Delete);
	m_kVct_pkImgInst.clear();
}

CParticleSystemInstance::CParticleSystemInstance()
{
	Initialize();
}

CParticleSystemInstance::~CParticleSystemInstance()
{
	assert(m_ParticleInstanceListVector.empty());
	assert(m_kVct_pkImgInst.empty());
}
