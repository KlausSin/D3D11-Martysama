#include "StdAfx.h"
#include "../eterLib/ResourceManager.h"
#include "../eterLib/ShaderManager.h"

#include "WeaponTrace.h"

#include "../EterBase/StepTimer.h"

CDynamicPool<CWeaponTrace> CWeaponTrace::ms_kPool;

void CWeaponTrace::DestroySystem()
{
	ms_kPool.Destroy();
}

void CWeaponTrace::Delete(CWeaponTrace* pkWTDel)
{
	assert(pkWTDel!=NULL && "CWeaponTrace::Delete");

	pkWTDel->Clear();
	ms_kPool.Free(pkWTDel);
}

CWeaponTrace* CWeaponTrace::New()
{
	return ms_kPool.Alloc();
}

void CWeaponTrace::Update(float fReachScale)
{
	const float now = DX::StepTimer::Instance().GetTotalSeconds();
	float fElapsedTime = now - m_fLastUpdate;
	m_fLastUpdate = now;

	if (!m_pInstance)
		return;
	{
		TTimePointList::iterator it;
		for(it=m_ShortTimePointList.begin();it!=m_ShortTimePointList.end();++it)
		{
			it->first += fElapsedTime;
			if (it->first>m_fLifeTime)
			{
				it++;
				break;
			}
		}
		if (it!=m_ShortTimePointList.end())
		m_ShortTimePointList.erase(it,m_ShortTimePointList.end());
		for(it=m_LongTimePointList.begin();it!=m_LongTimePointList.end();++it)
		{
			it->first += fElapsedTime;
			if (it->first>m_fLifeTime)
			{
				it++;
				break;
			}
		}
		if (it!=m_LongTimePointList.end())
			m_LongTimePointList.erase(it, m_LongTimePointList.end());
	}

	if (m_isPlaying && m_fz>=0.0001f)
	{
		Matrix * pMatrix;
		if (m_pInstance->GetCompositeBoneMatrix(m_dwModelInstanceIndex, m_iBoneIndex, &pMatrix))
		{
			Matrix * pBoneMat;
			m_pInstance->GetBoneMatrix(m_dwModelInstanceIndex, m_iBoneIndex, &pBoneMat);
			Matrix mat = *pMatrix;
			mat._41 = pBoneMat->_41;
			mat._42 = pBoneMat->_42;
			mat._43 = pBoneMat->_43;
			Matrix matPoint;
			Matrix matTranslation;
			Matrix matRotation;

			MatrixTranslation(&matTranslation, 0.0f, 0.0f, m_fLength*fReachScale);
			MatrixRotationZ(&matRotation, ToRadian(m_fRotation));

			matPoint = mat * matRotation;
			m_ShortTimePointList.push_front(
				TTimePoint(
					0.0f,
					Vector3(
						m_fx + matPoint._41,
						m_fy + matPoint._42,
						m_fz + matPoint._43
						)
					)
				);

			matPoint = matTranslation * matPoint;
			m_LongTimePointList.push_front(
				TTimePoint(
					0.0f,
					Vector3(
						m_fx + matPoint._41,
						m_fy + matPoint._42,
						m_fz + matPoint._43
						)
					)
				);
		}
	}
}

bool CWeaponTrace::BuildSplineCoefficients(std::vector<WeaponTraceSplineSegment>& outSegments, int& outNumSamples)
{
	const int max_size = 300;
	float h[max_size];
	float stk[max_size];
	int sp = 0;
	Vector3 r[max_size];

	if (m_LongTimePointList.size() <= 1)
		return false;

	int n = (int)m_LongTimePointList.size() - 1;
	assert(n < max_size - 1);

	float length = min(m_fLifeTime, m_LongTimePointList.back().first);
	outNumSamples = (int)(length / m_fSamplingTime) + 1;
	if (outNumSamples < 2)
		return false;

	outSegments.resize(n * 2);

	for (int loop = 0; loop <= 1; ++loop)
	{
		TTimePointList& Input = (loop) ? m_LongTimePointList : m_ShortTimePointList;
		WeaponTraceSplineSegment* pOut = &outSegments[loop * n]; // short at [0], long at [n]
		int i;
		sp = 0;

		for (i = 0; i < n; ++i)
		{
			h[i] = Input[i + 1].first - Input[i].first;
			r[i] = (Input[i + 1].second - Input[i].second) * (3 / h[i]);
		}
		r[n] = Vector3(0.0f, 0.0f, 0.0f);
		for (i = n; i > 0; i--)
		{
			r[i] += r[i - 1];
		}

		float rate = 0.5f;
		r[0] *= 0.5f;
		stk[sp++] = rate;
		for (i = 1; i < n; i++)
		{
			r[i] -= r[i - 1];
			rate = 1 / (4 - rate);
			r[i] *= rate;
			stk[sp++] = rate;
		}
		r[n] -= r[n - 1];
		rate = 1 / (2 - rate);
		r[n] *= rate;

		for (i = n - 1; i >= 0; i--)
		{
			r[i] -= stk[--sp] * r[i + 1];
		}

		float timebase = 0.0f;
		for (i = 0; i < n; ++i)
		{
			Vector3 v3Tmp = Input[i + 1].second - Input[i].second;
			Vector3 a = Input[i].second;
			Vector3 b = r[i];
			Vector3 c = (3 * v3Tmp - r[i + 1] * h[i] - (2 * h[i]) * r[i])
				* (1 / (h[i] * h[i]));
			Vector3 d = (-2 * v3Tmp + (r[i + 1] + r[i]) * h[i])
				* (1 / (h[i] * h[i] * h[i]));

			pOut[i].a = XMFLOAT3(a.x, a.y, a.z);
			pOut[i].timeStart = timebase;
			pOut[i].b = XMFLOAT3(b.x, b.y, b.z);
			pOut[i].timeEnd = timebase + h[i];
			pOut[i].c = XMFLOAT3(c.x, c.y, c.z);
			pOut[i]._pad0 = 0.0f;
			pOut[i].d = XMFLOAT3(d.x, d.y, d.z);
			pOut[i]._pad1 = 0.0f;

			timebase += h[i];
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// BuildVertex — CPU fallback: full spline solve + vertex generation
//////////////////////////////////////////////////////////////////////////
bool CWeaponTrace::BuildVertex()
{
	const int max_size = 300;
	float h[max_size];
	float stk[max_size];
	int sp=0;
	Vector3 r[max_size];

	if (m_LongTimePointList.size()<=1)
		return false;

	m_ShortVertexVector.clear();
	m_LongVertexVector.clear();

	float length = min(m_fLifeTime, m_LongTimePointList.back().first);

	int n = (int)m_LongTimePointList.size()-1;
	assert(n<max_size-1);

	for(int loop = 0; loop<=1; ++loop)
	{
		TTimePointList & Input = (loop) ? m_LongTimePointList : m_ShortTimePointList;
		std::vector<TPDTVertex> & Output = (loop) ? m_LongVertexVector : m_ShortVertexVector;
		int i;
		sp = 0;

		for(i=0;i<n;++i)
		{
			h[i] = Input[i+1].first - Input[i].first;
			r[i] = (Input[i+1].second - Input[i].second)*(3/h[i]);
		}
		r[n] = Vector3(0.0f,0.0f,0.0f);
		for(i=n;i>0;i--)
		{
			r[i]+=r[i-1];
		}

		float rate = 0.5f;
		r[0] *= 0.5f;
		stk[sp++] = rate;
		for(i=1;i<n;i++)
		{
			r[i]-=r[i-1];
			rate = 1/(4-rate);
			r[i] *= rate;
			stk[sp++]=rate;
		}
		r[n]-=r[n-1];
		rate = 1/(2-rate);
		r[n]*=rate;

		for(i=n-1;i>=0;i--)
		{
			r[i] -= stk[--sp] * r[i+1];
		}

		int base = 0;
		Vector3 a,b,c,d;
		Vector3 v3Tmp = Input[base+1].second-Input[base].second;
		float timebase=0,timenext=h[base], dt=m_fSamplingTime;
		a = Input[base].second;
		b = r[base];
		c = ( 3*v3Tmp - r[base+1]*h[base] - (2*h[base])*r[base] )
			* (1/(h[base]*h[base]));
		d = ( -2*v3Tmp + (r[base+1]+r[base])*h[base])
			* (1/(h[base]*h[base]*h[base]));

		for(float t = 0; t<=length; t+=dt)
		{
			while (t>timenext)
			{
				timebase = timenext;
				base++;
				if (base>=n) break;
				Vector3 v3Tmp = Input[base+1].second-Input[base].second;
				a = Input[base].second;
				b = r[base];
				c = ( 3*v3Tmp - r[base+1]*h[base] - (2*h[base])*r[base] )
					* (1/(h[base]*h[base]));
				d = ( -2*v3Tmp + (r[base+1]+r[base])*h[base])
					* (1/(h[base]*h[base]*h[base]));

				timenext+=h[base];
			}
			if (base>n) break;
			float cc = t - timebase;

			TPDTVertex v;
			float ttt = min(max((t+Input[0].first)/m_fLifeTime,0.0f),1.0f);
			v.diffuse = Color(0.3f,0.8f,1.0f, (loop)?min(max((1.0f-ttt)*(1.0f-ttt)/2.5f-0.1f,0.0f),1.0f):0.0f );
			v.position = a+cc*(b+cc*(c+cc*d));
			v.texCoord.x = t/m_fLifeTime;
			v.texCoord.y = loop ? 0.0f : 1.0f;
			Output.push_back(v);
		}
	}

	// Interleave long+short as triangle strip
	m_PDTVertexVector.clear();

	std::vector<TPDTVertex>::iterator lit,sit;
	for(lit = m_LongVertexVector.begin(), sit = m_ShortVertexVector.begin();
		lit != m_LongVertexVector.end();
		++lit,++sit)
		{
			m_PDTVertexVector.push_back(*lit);
			m_PDTVertexVector.push_back(*sit);
		}

	return true;
}

void CWeaponTrace::Render()
{
	if (m_LongTimePointList.size() <= 1)
		return;

	Matrix matWorld;
	MatrixIdentity(&matWorld);

	SHADERMANAGER.SaveTransform(MATRIX_WORLD, &matWorld);
	SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_PDT);
	SHADERMANAGER.SavePipelineState(PSTATE_CULLMODE, CULL_NONE);

	SHADERMANAGER.SavePipelineState(PSTATE_BLENDENABLE, TRUE);
	SHADERMANAGER.SavePipelineState(PSTATE_SRCBLEND, BLEND_SRCALPHA);
	SHADERMANAGER.SavePipelineState(PSTATE_DESTBLEND, BLEND_INVSRCALPHA);

	bool bSavedAlphaTest = SHADERMANAGER.GetAlphaTestEnabled();
	SHADERMANAGER.SetAlphaTestEnabled(false);

	SHADERMANAGER.SavePipelineState(PSTATE_DEPTHENABLE, TRUE);
	SHADERMANAGER.SavePipelineState(PSTATE_DEPTHFUNC, COMPARISON_LESSEQUAL);
	SHADERMANAGER.SavePipelineState(PSTATE_DEPTHWRITEMASK, FALSE);

	SHADERMANAGER.SetLightingEnabled(false);
	SHADERMANAGER.SetShaderResource(0, (ID3D11ShaderResourceView*)NULL);
	SHADERMANAGER.SetShaderResource(1, NULL);

	if (SHADERMANAGER.IsWeaponTraceCSAvailable())
	{
		// ---- CS PATH: CPU tridiagonal solve → GPU spline sampling + vertex generation ----
		static std::vector<WeaponTraceSplineSegment> s_csSegments;
		int numSamples = 0;

		if (BuildSplineCoefficients(s_csSegments, numSamples) && numSamples >= 2)
		{
			int n = (int)(s_csSegments.size() / 2);

			CBWeaponTraceCS params;
			params.numSegments = (UINT)n;
			params.numSamples = (UINT)numSamples;
			params.lifetime = m_fLifeTime;
			params.samplingTime = m_fSamplingTime;
			params.firstPointTime = m_LongTimePointList[0].first;
			params.totalLength = min(m_fLifeTime, m_LongTimePointList.back().first);
			params._pad[0] = 0;
			params._pad[1] = 0;

			SHADERMANAGER.SetParticleColor(0xFFFFFFFF);
			SHADERMANAGER.BeginParticlePCT();

			if (SHADERMANAGER.DispatchWeaponTraceCS(s_csSegments.data(), (UINT)n, params))
			{
				SHADERMANAGER.DrawWeaponTraceCSOutput((UINT)numSamples);
			}
		}
	}
	else
	{
		if (!BuildVertex())
			goto cleanup;

		if (m_PDTVertexVector.size() < 4)
			goto cleanup;

		SHADERMANAGER.BeginUI();
		SHADERMANAGER.DrawDynamic(TOPOLOGY_TRIANGLESTRIP,
									 int(m_PDTVertexVector.size() - 2),
									 &m_PDTVertexVector[0],
									 sizeof(TPDTVertex));
	}

cleanup:
	SHADERMANAGER.SetLightingEnabled(true);

	SHADERMANAGER.RestorePipelineState(PSTATE_DEPTHENABLE);
	SHADERMANAGER.RestorePipelineState(PSTATE_DEPTHFUNC);
	SHADERMANAGER.RestorePipelineState(PSTATE_DEPTHWRITEMASK);

	SHADERMANAGER.SetAlphaTestEnabled(bSavedAlphaTest);

	SHADERMANAGER.RestorePipelineState(PSTATE_BLENDENABLE);
	SHADERMANAGER.RestorePipelineState(PSTATE_SRCBLEND);
	SHADERMANAGER.RestorePipelineState(PSTATE_DESTBLEND);

	SHADERMANAGER.RestoreTransform(MATRIX_WORLD);
	SHADERMANAGER.RestorePipelineState(PSTATE_CULLMODE);
}

void CWeaponTrace::UseAlpha()
{
	m_bUseTexture = false;
}

void CWeaponTrace::UseTexture()
{
	m_bUseTexture = true;
}

void CWeaponTrace::SetTexture(const char * c_szFileName)
{
	CGraphicImage * pImage = (CGraphicImage *)CResourceManager::Instance().GetResourcePointer("lot_ade10-2.tga");
	m_ImageInstance.SetImagePointer(pImage);
}

bool CWeaponTrace::SetWeaponInstance(CGraphicThingInstance * pInstance, DWORD dwModelIndex, const char * c_szBoneName)
{
	pInstance->Update();
	pInstance->DeformNoSkin();

	Vector3 v3Min;
	Vector3 v3Max;
	if (!pInstance->GetBoundBox(dwModelIndex, &v3Min, &v3Max))
		return false;

	m_iBoneIndex = 0;
	m_dwModelInstanceIndex = dwModelIndex;

	m_pInstance = pInstance;
	Matrix * pmat;
	pInstance->GetBoneMatrix(dwModelIndex, 0, &pmat);
	Vector3 v3Bone(pmat->_41,pmat->_42,pmat->_43);

	auto val1 = (v3Bone-v3Min);
	auto val2 = (v3Bone-v3Max);
	m_fLength =
		sqrtf(
			fMAX(
				Vec3LengthSq(&val1),
				Vec3LengthSq(&val2)
				)
			);

	return true;
}

void CWeaponTrace::SetPosition(float fx, float fy, float fz)
{
	m_fx = fx;
	m_fy = fy;
	m_fz = fz;
}

void CWeaponTrace::SetRotation(float fRotation)
{
	m_fRotation = fRotation;
}

void CWeaponTrace::SetLifeTime(float fLifeTime)
{
	m_fLifeTime = fLifeTime;
}

void CWeaponTrace::SetSamplingTime(float fSamplingTime)
{
	m_fSamplingTime = fSamplingTime;
}

void CWeaponTrace::TurnOn()
{
	m_isPlaying = TRUE;
}
void CWeaponTrace::TurnOff()
{
	m_isPlaying = FALSE;
}

void CWeaponTrace::Clear()
{
	m_ShortTimePointList.clear();
	m_LongTimePointList.clear();
	Initialize();
}

void CWeaponTrace::Initialize()
{
	m_pInstance = NULL;
	m_dwModelInstanceIndex = 0;

	m_fx = 0.0f;
	m_fy = 0.0f;
	m_fz = 0.0f;
	m_fRotation = 0.0f;

	m_fLifeTime = 0.18f;
	m_fSamplingTime = 0.003f;

	m_isPlaying = FALSE;

	m_bUseTexture = false;

	m_iBoneIndex = 0;

	m_fLastUpdate = DX::StepTimer::Instance().GetTotalSeconds();
}

CWeaponTrace::CWeaponTrace()
{
	Initialize();
}
CWeaponTrace::~CWeaponTrace()
{
}
