#include "StdAfx.h"
#include "SnowEnvironment.h"

#include "../EterLib/ShaderManager.h"
#include "../EterLib/Camera.h"
#include "../EterLib/ResourceManager.h"
#include "../EffectLib/GpuParticlePool.h"
#include "SnowParticle.h"

#include "../EterBase/StepTimer.h"

void CSnowEnvironment::Enable()
{
	if (!m_bSnowEnable)
	{
		Create();
	}

	m_bSnowEnable = TRUE;
}

void CSnowEnvironment::Disable()
{
	m_bSnowEnable = FALSE;
}

void CSnowEnvironment::Update(const Vector3 & c_rv3Pos)
{
	if (!m_bSnowEnable)
	{
		if (m_kVct_pkParticleSnow.empty())
			return;
	}

	m_v3Center=c_rv3Pos;
}

void CSnowEnvironment::Deform()
{
	if (!m_bSnowEnable)
	{
		if (m_kVct_pkParticleSnow.empty())
			return;
	}

	const Vector3 & c_rv3Pos=m_v3Center;

	float fElapsedTime = DX::StepTimer::Instance().GetElapsedSeconds();

	CCamera * pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return;

	const Vector3 & c_rv3View = pCamera->GetView();

	Vector3 v3ChangedPos = c_rv3View * 3500.0f + c_rv3Pos;
	v3ChangedPos.z = c_rv3Pos.z;

	std::vector<CSnowParticle*>::iterator itor = m_kVct_pkParticleSnow.begin();
	for (; itor != m_kVct_pkParticleSnow.end();)
	{
		CSnowParticle * pSnow = *itor;
		pSnow->Update(fElapsedTime, v3ChangedPos);

		if (!pSnow->IsActivate())
		{
			CSnowParticle::Delete(pSnow);

			itor = m_kVct_pkParticleSnow.erase(itor);
		}
		else
		{
			++itor;
		}
	}

	if (m_bSnowEnable)
	{
		for (int p = 0; p < min(10, m_dwParticleMaxNum - m_kVct_pkParticleSnow.size()); ++p)
		{
			CSnowParticle * pSnowParticle = CSnowParticle::New();
			pSnowParticle->Init(v3ChangedPos);
			m_kVct_pkParticleSnow.push_back(pSnowParticle);
		}
	}
}

void CSnowEnvironment::__BeginBlur()
{
	if (!m_bBlurEnable)
		return;

	if (!ms_pContext)
		return;

	ms_pContext->OMGetRenderTargets(1, &m_lpOldSurface, &m_lpOldDepthStencilSurface);

	ms_pContext->OMSetRenderTargets(1, &m_lpSnowRenderTargetSurface, m_lpSnowDepthSurface);

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	ms_pContext->ClearRenderTargetView(m_lpSnowRenderTargetSurface, clearColor);
	ms_pContext->ClearDepthStencilView(m_lpSnowDepthSurface, D3D11_CLEAR_DEPTH, 1.0f, 0);

	SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, TRUE);
	SHADERMANAGER.SetPipelineState(PSTATE_SRCBLEND, BLEND_SRCALPHA);
	SHADERMANAGER.SetPipelineState(PSTATE_DESTBLEND, BLEND_DESTALPHA);
}

void CSnowEnvironment::__ApplyBlur()
{
	if (!m_bBlurEnable)
		return;

	if (!ms_pContext)
		return;

	{
		ms_pContext->OMSetRenderTargets(1, &m_lpOldSurface, m_lpOldDepthStencilSurface);

		SHADERMANAGER.SetShaderResource(0, m_lpSnowTexture);
		SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, TRUE);

		D3D11_VIEWPORT viewport;
		UINT numViewports = 1;
		ms_pContext->RSGetViewports(&numViewports, &viewport);
		float sx = viewport.Width;
		float sy = viewport.Height;
		SAFE_RELEASE(m_lpOldSurface);
		SAFE_RELEASE(m_lpOldDepthStencilSurface);

		BlurVertex V[4] = {
			BlurVertex(Vector3(0.0f, 0.0f, 0.0f), 0xFFFFFFFF, 0.0f, 0.0f),
			BlurVertex(Vector3(sx,   0.0f, 0.0f), 0xFFFFFFFF, 1.0f, 0.0f),
			BlurVertex(Vector3(0.0f, sy,   0.0f), 0xFFFFFFFF, 0.0f, 1.0f),
			BlurVertex(Vector3(sx,   sy,   0.0f), 0xFFFFFFFF, 1.0f, 1.0f)
		};

		SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_PDT);
		SHADERMANAGER.BeginUI();
		SHADERMANAGER.DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, V, sizeof(BlurVertex));
	}
}

void CSnowEnvironment::Render()
{
	if (!m_bSnowEnable && m_kVct_pkParticleSnow.empty())
		return;

	if (!ms_pContext)
		return;

	__BeginBlur();

	DWORD dwParticleCount = min(m_dwParticleMaxNum, (DWORD)m_kVct_pkParticleSnow.size());
	if (dwParticleCount == 0)
	{
		__ApplyBlur();
		return;
	}

	CCamera* pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
	{
		__ApplyBlur();
		return;
	}

	SHADERMANAGER.PushState();

	SHADERMANAGER.SetPipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
	SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, TRUE);
	SHADERMANAGER.SetPipelineState(PSTATE_CULLMODE, CULL_NONE);
	SHADERMANAGER.SetPipelineState(PSTATE_SRCBLEND, BLEND_SRCALPHA);
	SHADERMANAGER.SetPipelineState(PSTATE_DESTBLEND, BLEND_INVSRCALPHA);
	SHADERMANAGER.SetShaderResource(1, nullptr);
	SHADERMANAGER.SetShaderResource(0, m_pImageInstance->GetGraphicImagePointer()->GetTextureReference().GetD3DTexture());

	Matrix matIdentity;
	MatrixIdentity(&matIdentity);

	if (SHADERMANAGER.IsComputeParticlesAvailable())
	{
		static std::vector<ParticleGPUInput> s_snowCSInput;
		s_snowCSInput.clear();
		s_snowCSInput.reserve(dwParticleCount);

		for (DWORD i = 0; i < dwParticleCount; ++i)
		{
			CSnowParticle* pSnow = m_kVct_pkParticleSnow[i];
			const Vector3& pos = pSnow->GetPosition();

			ParticleGPUInput input;
			input.posX = pos.x; input.posY = pos.y; input.posZ = pos.z;
			input.lastPosX = pos.x; input.lastPosY = pos.y; input.lastPosZ = pos.z;
			input.halfW = pSnow->GetHalfWidth(); input.halfH = pSnow->GetHalfHeight();
			input.scaleX = 1.0f; input.scaleY = 1.0f; input.rotation = 0.0f;
			input.color = 0xFFFFFFFF; input.flags = 1;
			input._pad[0] = input._pad[1] = input._pad[2] = 0.0f;

			s_snowCSInput.push_back(input);
		}

		SHADERMANAGER.BeginParticlePCT();
		SHADERMANAGER.SetWorldMatrix(&matIdentity);
		SHADERMANAGER.SetParticleColor(0xFFFFFFFF);
		SHADERMANAGER.SetMaterialParams(0, 0, 0, 0);

		float fRots[3] = { 0.0f, 0.0f, 0.0f };
		UINT totalParticles = (UINT)s_snowCSInput.size();
		UINT csOffset = 0;

		while (csOffset < totalParticles)
		{
			UINT chunkSize = min(totalParticles - csOffset, CShaderManager::MAX_CS_PARTICLES);

			if (!SHADERMANAGER.DispatchParticleBillboardCS(
				s_snowCSInput.data() + csOffset, chunkSize, 1, fRots, nullptr))
				break;

			SHADERMANAGER.DrawParticleCSOutput(chunkSize);
			csOffset += chunkSize;
		}
	}
	else if (GPU_PARTICLE_POOL.IsAvailable())
	{
		const Vector3& c_rv3Up = pCamera->GetUp();
		const Vector3& c_rv3Cross = pCamera->GetCross();

		Vector3 v3Up(-c_rv3Cross.x, -c_rv3Cross.y, -c_rv3Cross.z);
		Vector3 v3Cross = c_rv3Up;

		GPU_PARTICLE_POOL.BeginBatch();

		for (DWORD i = 0; i < dwParticleCount; ++i)
		{
			CSnowParticle* pSnow = m_kVct_pkParticleSnow[i];
			const Vector3& pos = pSnow->GetPosition();
			float halfW = pSnow->GetHalfWidth();
			float halfH = pSnow->GetHalfHeight();

			Vector3 sc = -halfW * v3Cross;
			Vector3 su = halfH * v3Up;

			GPUPTVertex verts[4];
			Vector3 p0 = pos - su + sc;
			Vector3 p1 = pos - su - sc;
			Vector3 p2 = pos + su + sc;
			Vector3 p3 = pos + su - sc;

			verts[0] = { p0.x, p0.y, p0.z, 0.0f, 1.0f };
			verts[1] = { p1.x, p1.y, p1.z, 0.0f, 0.0f };
			verts[2] = { p2.x, p2.y, p2.z, 1.0f, 1.0f };
			verts[3] = { p3.x, p3.y, p3.z, 1.0f, 0.0f };

			GPU_PARTICLE_POOL.AddQuad(verts);
		}

		SHADERMANAGER.BeginParticle();
		SHADERMANAGER.SetWorldMatrix(&matIdentity);
		SHADERMANAGER.SetParticleColor(0xFFFFFFFF);
		SHADERMANAGER.SetMaterialParams(0, 0, 0, 0);

		GPU_PARTICLE_POOL.FlushBatch();
	}

	SHADERMANAGER.PopState();

	__ApplyBlur();
}

bool CSnowEnvironment::__CreateBlurTexture()
{
	if (!m_bBlurEnable)
		return true;

	if (!ms_pDevice)
		return false;

	ID3D11Texture2D* pSnowTexture = nullptr;
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = m_wBlurTextureSize;
	texDesc.Height = m_wBlurTextureSize;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;

	if (FAILED(ms_pDevice->CreateTexture2D(&texDesc, nullptr, &pSnowTexture)))
		return false;

	if (FAILED(ms_pDevice->CreateShaderResourceView(pSnowTexture, nullptr, &m_lpSnowTexture)))
	{
		pSnowTexture->Release();
		return false;
	}

	if (FAILED(ms_pDevice->CreateRenderTargetView(pSnowTexture, nullptr, &m_lpSnowRenderTargetSurface)))
	{
		pSnowTexture->Release();
		return false;
	}
	pSnowTexture->Release();

	ID3D11Texture2D* pSnowDepth = nullptr;
	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = m_wBlurTextureSize;
	depthDesc.Height = m_wBlurTextureSize;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D16_UNORM;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	if (FAILED(ms_pDevice->CreateTexture2D(&depthDesc, nullptr, &pSnowDepth)))
		return false;

	if (FAILED(ms_pDevice->CreateDepthStencilView(pSnowDepth, nullptr, &m_lpSnowDepthSurface)))
	{
		pSnowDepth->Release();
		return false;
	}
	pSnowDepth->Release();

	ID3D11Texture2D* pAccumTexture = nullptr;
	if (FAILED(ms_pDevice->CreateTexture2D(&texDesc, nullptr, &pAccumTexture)))
		return false;

	if (FAILED(ms_pDevice->CreateShaderResourceView(pAccumTexture, nullptr, &m_lpAccumTexture)))
	{
		pAccumTexture->Release();
		return false;
	}

	if (FAILED(ms_pDevice->CreateRenderTargetView(pAccumTexture, nullptr, &m_lpAccumRenderTargetSurface)))
	{
		pAccumTexture->Release();
		return false;
	}
	pAccumTexture->Release();

	ID3D11Texture2D* pAccumDepth = nullptr;
	if (FAILED(ms_pDevice->CreateTexture2D(&depthDesc, nullptr, &pAccumDepth)))
		return false;

	if (FAILED(ms_pDevice->CreateDepthStencilView(pAccumDepth, nullptr, &m_lpAccumDepthSurface)))
	{
		pAccumDepth->Release();
		return false;
	}
	pAccumDepth->Release();

	return true;
}

bool CSnowEnvironment::Create()
{
	Destroy();

	if (!__CreateBlurTexture())
		return false;

	CGraphicImage * pImage = (CGraphicImage *)CResourceManager::Instance().GetResourcePointer("d:/ymir work/special/snow.dds");
	m_pImageInstance = CGraphicImageInstance::New();
	m_pImageInstance->SetImagePointer(pImage);

	return true;
}

void CSnowEnvironment::Destroy()
{
	SAFE_RELEASE(m_lpSnowTexture);
	SAFE_RELEASE(m_lpSnowRenderTargetSurface);
	SAFE_RELEASE(m_lpSnowDepthSurface);
	SAFE_RELEASE(m_lpAccumTexture);
	SAFE_RELEASE(m_lpAccumRenderTargetSurface);
	SAFE_RELEASE(m_lpAccumDepthSurface);

	stl_wipe(m_kVct_pkParticleSnow);
	CSnowParticle::DestroyPool();

	if (m_pImageInstance)
	{
		CGraphicImageInstance::Delete(m_pImageInstance);
		m_pImageInstance = NULL;
	}

	__Initialize();
}

void CSnowEnvironment::__Initialize()
{
	m_bSnowEnable = FALSE;
	m_lpSnowTexture = NULL;
	m_lpSnowRenderTargetSurface = NULL;
	m_lpSnowDepthSurface = NULL;
	m_lpAccumTexture = NULL;
	m_lpAccumRenderTargetSurface = NULL;
	m_lpAccumDepthSurface = NULL;
	m_pImageInstance = NULL;

	m_kVct_pkParticleSnow.reserve(m_dwParticleMaxNum);
}

CSnowEnvironment::CSnowEnvironment()
{
	m_bBlurEnable = FALSE;
	m_dwParticleMaxNum = 3000;
	m_wBlurTextureSize = 512;

	__Initialize();
}
CSnowEnvironment::~CSnowEnvironment()
{
	Destroy();
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
