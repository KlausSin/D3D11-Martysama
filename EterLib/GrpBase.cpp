#include "StdAfx.h"
#include "../eterBase/Utils.h"
#include "../eterBase/Timer.h"
#include "GrpBase.h"
#include "Camera.h"
#include "ShaderInit.h"
#include "ShaderManager.h"

#include "../EterBase/StepTimer.h"

void PixelPositionToVector3(const Vector3& c_rkPPosSrc, Vector3* pv3Dst)
{
	pv3Dst->x=+c_rkPPosSrc.x;
	pv3Dst->y=-c_rkPPosSrc.y;
	pv3Dst->z=+c_rkPPosSrc.z;
}

void Vector3ToPixelPosition(const Vector3& c_rv3Src, Vector3* pv3Dst)
{
	pv3Dst->x=+c_rv3Src.x;
	pv3Dst->y=-c_rv3Src.y;
	pv3Dst->z=+c_rv3Src.z;
}

UINT					CGraphicBase::ms_iD3DAdapterInfo=0;
UINT					CGraphicBase::ms_iD3DDevInfo=0;
UINT					CGraphicBase::ms_iD3DModeInfo=0;
D3D_CDisplayModeAutoDetector				CGraphicBase::ms_kD3DDetector;

HWND CGraphicBase::ms_hWnd;
HDC CGraphicBase::ms_hDC;

// DX11 Core Objects
ID3D11Device*			CGraphicBase::ms_pDevice = nullptr;
ID3D11DeviceContext*	CGraphicBase::ms_pContext = nullptr;
IDXGISwapChain*			CGraphicBase::ms_pSwapChain = nullptr;
IDXGIFactory*			CGraphicBase::ms_pDXGIFactory = nullptr;
ID3D11RenderTargetView*	CGraphicBase::ms_pRenderTargetView = nullptr;
ID3D11DepthStencilView*	CGraphicBase::ms_pDepthStencilView = nullptr;
ID3D11Texture2D*		CGraphicBase::ms_pDepthStencilBuffer = nullptr;
D3D_FEATURE_LEVEL		CGraphicBase::ms_FeatureLevel = D3D_FEATURE_LEVEL_11_0;

#ifdef ENABLE_MSAA
ID3D11Texture2D*		CGraphicBase::ms_pMSAARenderTarget = nullptr;
ID3D11RenderTargetView*	CGraphicBase::ms_pMSAARenderTargetView = nullptr;
ID3D11Texture2D*		CGraphicBase::ms_pMSAADepthStencilBuffer = nullptr;
ID3D11DepthStencilView*	CGraphicBase::ms_pMSAADepthStencilView = nullptr;
UINT					CGraphicBase::ms_uMSAASampleCount = 1;
#endif

#ifdef ENABLE_SSAO
ID3D11ShaderResourceView*	CGraphicBase::ms_pDepthStencilSRV = nullptr;
ID3D11DepthStencilView*		CGraphicBase::ms_pDepthStencilViewReadOnly = nullptr;
#ifdef ENABLE_MSAA
ID3D11ShaderResourceView*	CGraphicBase::ms_pMSAADepthStencilSRV = nullptr;
#endif
ID3D11Texture2D*			CGraphicBase::ms_pResolvedDepthTex = nullptr;
ID3D11RenderTargetView*		CGraphicBase::ms_pResolvedDepthRTV = nullptr;
ID3D11ShaderResourceView*	CGraphicBase::ms_pResolvedDepthSRV = nullptr;
ID3D11Texture2D*			CGraphicBase::ms_pSSAO_RT = nullptr;
ID3D11RenderTargetView*		CGraphicBase::ms_pSSAO_RTV = nullptr;
ID3D11ShaderResourceView*	CGraphicBase::ms_pSSAO_SRV = nullptr;
ID3D11Texture2D*			CGraphicBase::ms_pSSAOBlur_RT = nullptr;
ID3D11RenderTargetView*		CGraphicBase::ms_pSSAOBlur_RTV = nullptr;
ID3D11ShaderResourceView*	CGraphicBase::ms_pSSAOBlur_SRV = nullptr;
bool						CGraphicBase::ms_bSSAOEnabled = true;
#endif

#ifdef ENABLE_BLOOM
ID3D11Texture2D*			CGraphicBase::ms_pSceneTexture = nullptr;
ID3D11RenderTargetView*		CGraphicBase::ms_pSceneTextureRTV = nullptr;
ID3D11ShaderResourceView*	CGraphicBase::ms_pSceneTextureSRV = nullptr;
ID3D11Texture2D*			CGraphicBase::ms_pBloomRTA = nullptr;
ID3D11RenderTargetView*		CGraphicBase::ms_pBloomRTA_RTV = nullptr;
ID3D11ShaderResourceView*	CGraphicBase::ms_pBloomRTA_SRV = nullptr;
ID3D11Texture2D*			CGraphicBase::ms_pBloomRTB = nullptr;
ID3D11RenderTargetView*		CGraphicBase::ms_pBloomRTB_RTV = nullptr;
ID3D11ShaderResourceView*	CGraphicBase::ms_pBloomRTB_SRV = nullptr;
bool						CGraphicBase::ms_bBloomEnabled = true;
#ifdef ENABLE_GODRAYS
ID3D11Texture2D*			CGraphicBase::ms_pGodRaysRT = nullptr;
ID3D11RenderTargetView*		CGraphicBase::ms_pGodRaysRTV = nullptr;
ID3D11ShaderResourceView*	CGraphicBase::ms_pGodRaysSRV = nullptr;
#endif

#endif

CMatrixStack*			CGraphicBase::ms_pMatrixStack = nullptr;
D3D11_VIEWPORT			CGraphicBase::ms_Viewport = {};

HRESULT					CGraphicBase::ms_hLastResult = S_OK;

int						CGraphicBase::ms_iWidth;
int						CGraphicBase::ms_iHeight;

Matrix					CGraphicBase::ms_matIdentity;

Matrix					CGraphicBase::ms_matView;
Matrix					CGraphicBase::ms_matProj;
Matrix					CGraphicBase::ms_matInverseView;
Matrix					CGraphicBase::ms_matInverseViewYAxis;

Matrix					CGraphicBase::ms_matWorld;
Matrix					CGraphicBase::ms_matWorldView;

Matrix					CGraphicBase::ms_matScreen0;
Matrix					CGraphicBase::ms_matScreen1;
Matrix					CGraphicBase::ms_matScreen2;

Vector3					CGraphicBase::ms_vtPickRayOrig;
Vector3					CGraphicBase::ms_vtPickRayDir;

float					CGraphicBase::ms_fFieldOfView;
float					CGraphicBase::ms_fNearY;
float					CGraphicBase::ms_fFarY;
float					CGraphicBase::ms_fAspect;

DWORD					CGraphicBase::ms_dwWavingEndTime;
int						CGraphicBase::ms_iWavingPower;
DWORD					CGraphicBase::ms_dwFlashingEndTime;
Color					CGraphicBase::ms_FlashingColor;

CRay					CGraphicBase::ms_Ray;
bool					CGraphicBase::ms_bSupportDXT = true;
bool					CGraphicBase::ms_isLowTextureMemory = false;
bool					CGraphicBase::ms_isHighTextureMemory = false;
DWORD					CGraphicBase::ms_faceCount = 0;

/*
std::vector<TIndex>		CGraphicBase::ms_lineIdxVector;
std::vector<TIndex>		CGraphicBase::ms_lineTriIdxVector;
std::vector<TIndex>		CGraphicBase::ms_lineRectIdxVector;
std::vector<TIndex>		CGraphicBase::ms_lineCubeIdxVector;

std::vector<TIndex>		CGraphicBase::ms_fillTriIdxVector;
std::vector<TIndex>		CGraphicBase::ms_fillRectIdxVector;
std::vector<TIndex>		CGraphicBase::ms_fillCubeIdxVector;
*/


#ifdef ENABLE_FIX_MOBS_LAG
ID3D11Buffer* CGraphicBase::m_tinyPdtVertexBuffer = nullptr;
ID3D11Buffer* CGraphicBase::m_smallPdtVertexBuffer = nullptr;
ID3D11Buffer* CGraphicBase::m_mediumPdtVertexBuffer = nullptr;
ID3D11Buffer* CGraphicBase::m_largePdtVertexBuffer = nullptr;
ID3D11Buffer* CGraphicBase::m_xlargePdtVertexBuffer = nullptr;
#else
ID3D11Buffer*	CGraphicBase::ms_alpd3dPDTVB[PDT_VERTEXBUFFER_NUM] = {};
#endif

ID3D11Buffer*	CGraphicBase::ms_alpd3dDefIB[DEFAULT_IB_NUM] = {};

ID3D11DeviceContext* CGraphicBase::GetActiveContext()
{
	return ms_pContext;
}

CMatrixStack* CGraphicBase::GetActiveMatrixStack()
{
	return ms_pMatrixStack;
}

bool CGraphicBase::IsLowTextureMemory()
{
	return ms_isLowTextureMemory;
}

bool CGraphicBase::IsHighTextureMemory()
{
	return ms_isHighTextureMemory;
}

bool CGraphicBase::IsTLVertexClipping()
{
	// DX11 always clips transformed vertices
	return true;
}

void CGraphicBase::GetBackBufferSize(UINT* puWidth, UINT* puHeight)
{
	*puWidth = static_cast<UINT>(ms_iWidth);
	*puHeight = static_cast<UINT>(ms_iHeight);
}

void CGraphicBase::SetDefaultIndexBuffer(UINT eDefIB)
{
	if (eDefIB>=DEFAULT_IB_NUM)
		return;

	SHADERMANAGER.SetIndexBuffer(ms_alpd3dDefIB[eDefIB]);
}

bool CGraphicBase::SetPDTStream(SPDTVertex* pVertices, UINT uVtxCount)
{
	return SetPDTStream((SPDTVertexRaw*)pVertices, uVtxCount);
}

#ifdef ENABLE_FIX_MOBS_LAG
ID3D11Buffer* CGraphicBase::GetPdtVertexBufferForSize(UINT uVtxCount)
{
	// Select optimal buffer tier based on vertex count
	if (uVtxCount <= TINY_PDT_VERTEX_BUFFER_SIZE)
		return m_tinyPdtVertexBuffer;
	else if (uVtxCount <= SMALL_PDT_VERTEX_BUFFER_SIZE)
		return m_smallPdtVertexBuffer;
	else if (uVtxCount <= MEDIUM_PDT_VERTEX_BUFFER_SIZE)
		return m_mediumPdtVertexBuffer;
	else if (uVtxCount <= LARGE_PDT_VERTEX_BUFFER_SIZE)
		return m_largePdtVertexBuffer;
	else if (uVtxCount <= XLARGE_PDT_VERTEX_BUFFER_SIZE)
		return m_xlargePdtVertexBuffer;

	return nullptr; // Exceeds all buffer sizes
}

bool CGraphicBase::SetPDTStream(SPDTVertexRaw* pSrcVertices, UINT uVtxCount)
{
	if (!uVtxCount || !GetActiveContext())
		return false;

	ID3D11Buffer* pVertexBuffer = GetPdtVertexBufferForSize(uVtxCount);

	if (!pVertexBuffer)
	{
		// Vertex count exceeds all buffers - fallback warning
		assert(uVtxCount <= XLARGE_PDT_VERTEX_BUFFER_SIZE && "Vertex count exceeds maximum buffer size");
		return false;
	}

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	if (FAILED(GetActiveContext()->Map(pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
	{
		return false;
	}

	memcpy(mappedResource.pData, pSrcVertices, sizeof(TPDTVertex) * uVtxCount);
	GetActiveContext()->Unmap(pVertexBuffer, 0);
	SHADERMANAGER.SetVertexBuffer(0, pVertexBuffer, sizeof(TPDTVertex));

	EShaderType currentShader = SHADERMANAGER.GetCurrentShader();
	if (currentShader != SHADER_SKY && currentShader != SHADER_WATER &&
		currentShader != SHADER_TERRAIN && currentShader != SHADER_MESH)
	{
		SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_PDT);
	}

	return true;
}
#else
bool CGraphicBase::SetPDTStream(SPDTVertexRaw* pSrcVertices, UINT uVtxCount)
{
	if (!uVtxCount || !GetActiveContext())
		return false;

	static DWORD s_dwVBPos = 0;

	if (s_dwVBPos >= PDT_VERTEXBUFFER_NUM)
		s_dwVBPos = 0;

	ID3D11Buffer* pVertexBuffer = ms_alpd3dPDTVB[s_dwVBPos];
	++s_dwVBPos;

	if (!pVertexBuffer)
		return false;

	assert(PDT_VERTEX_NUM >= uVtxCount);
	if (uVtxCount >= PDT_VERTEX_NUM)
		return false;

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	if (FAILED(GetActiveContext()->Map(pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
	{
		return false;
	}

	memcpy(mappedResource.pData, pSrcVertices, sizeof(TPDTVertex) * uVtxCount);
	GetActiveContext()->Unmap(pVertexBuffer, 0);

	SHADERMANAGER.SetVertexBuffer(0, pVertexBuffer, sizeof(TPDTVertex));

	EShaderType currentShader = SHADERMANAGER.GetCurrentShader();
	if (currentShader != SHADER_SKY && currentShader != SHADER_WATER &&
		currentShader != SHADER_TERRAIN && currentShader != SHADER_MESH)
	{
		SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_PDT);
	}

	return true;
}
#endif

DWORD CGraphicBase::GetAvailableTextureMemory()
{
	if (!ms_pDevice)
		return 0;

	static DWORD s_dwNextUpdateTime = 0;
	static DWORD s_dwTexMemSize = 0;

	DWORD dwCurTime = ELTimer_GetMSec();
	if (s_dwNextUpdateTime < dwCurTime)
	{
		s_dwNextUpdateTime = dwCurTime + 5000;

		// Get DXGI device and adapter
		IDXGIDevice* pDXGIDevice = nullptr;
		if (SUCCEEDED(ms_pDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDXGIDevice)))
		{
			IDXGIAdapter* pAdapter = nullptr;
			if (SUCCEEDED(pDXGIDevice->GetAdapter(&pAdapter)))
			{
				DXGI_ADAPTER_DESC adapterDesc;
				if (SUCCEEDED(pAdapter->GetDesc(&adapterDesc)))
				{
					SIZE_T totalMem = adapterDesc.DedicatedVideoMemory;
					s_dwTexMemSize = static_cast<DWORD>(min(totalMem, (SIZE_T)MAXDWORD));
				}
				pAdapter->Release();
			}
			pDXGIDevice->Release();
		}
	}

	return s_dwTexMemSize;
}

const Matrix& CGraphicBase::GetViewMatrix()
{
	return ms_matView;
}

const Matrix& CGraphicBase::GetIdentityMatrix()
{
	return ms_matIdentity;
}

void CGraphicBase::SetEyeCamera(float xEye, float yEye, float zEye,
								float xCenter, float yCenter, float zCenter,
								float xUp, float yUp, float zUp)
{
	CCamera* pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return;

	Vector3 vectorEye(xEye, yEye, zEye);
	Vector3 vectorCenter(xCenter, yCenter, zCenter);
	Vector3 vectorUp(xUp, yUp, zUp);

//	CCameraManager::Instance().SetCurrentCamera(CCameraManager::DEFAULT_PERSPECTIVE_CAMERA);
	pCamera->SetViewParams(vectorEye, vectorCenter, vectorUp);
	UpdateViewMatrix();
}

void CGraphicBase::SetSimpleCamera(float x, float y, float z, float pitch, float roll)
{
	CCamera * pCamera = CCameraManager::Instance().GetCurrentCamera();
	Vector3 vectorEye(x, y, z);

	pCamera->SetViewParams(Vector3(0.0f, y, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f));
	pCamera->RotateEyeAroundTarget(pitch, roll);
	pCamera->Move(vectorEye);

	UpdateViewMatrix();

	SHADERMANAGER.GetMatrix(MATRIX_WORLD, &ms_matWorld);
	MatrixMultiply(&ms_matWorldView, &ms_matWorld, &ms_matView);
}

void CGraphicBase::SetAroundCamera(float distance, float pitch, float roll, float lookAtZ)
{
	CCamera * pCamera = CCameraManager::Instance().GetCurrentCamera();
	pCamera->SetViewParams(Vector3(0.0f, -distance, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f));
	pCamera->RotateEyeAroundTarget(pitch, roll);
	Vector3 v3Target = pCamera->GetTarget();
	v3Target.z = lookAtZ;
	pCamera->SetTarget(v3Target);

	UpdateViewMatrix();

	SHADERMANAGER.GetMatrix(MATRIX_WORLD, &ms_matWorld);
	MatrixMultiply(&ms_matWorldView, &ms_matWorld, &ms_matView);
}

void CGraphicBase::SetPositionCamera(float fx, float fy, float fz, float distance, float pitch, float roll)
{
	if (ms_dwWavingEndTime > DX::StepTimer::instance().GetTotalMillieSeconds())
	{
		if (ms_iWavingPower>0)
		{
			fx += float(rand() % ms_iWavingPower) / 10.0f;
			fy += float(rand() % ms_iWavingPower) / 10.0f;
			fz += float(rand() % ms_iWavingPower) / 10.0f;
		}
	}

	CCamera * pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return;

	pCamera->SetViewParams(Vector3(0.0f, -distance, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f));
	pitch = fMIN(80.0f, fMAX(-80.0f, pitch) );
//	Tracef("SetPosition Camera : %f, %f\n", pitch, roll);
	pCamera->RotateEyeAroundTarget(pitch, roll);
	pCamera->Move(Vector3(fx, fy, fz));

	UpdateViewMatrix();

	SHADERMANAGER.GetMatrix(MATRIX_WORLD, &ms_matWorld);
	MatrixMultiply(&ms_matWorldView, &ms_matWorld, &ms_matView);
}

void CGraphicBase::SetOrtho2D(float hres, float vres, float zres)
{
	//CCameraManager::Instance().SetCurrentCamera(CCameraManager::DEFAULT_ORTHO_CAMERA);
	MatrixOrthoOffCenterRH(&ms_matProj, 0, hres, vres, 0, 0, zres);
	//UpdatePipeLineMatrix();
	UpdateProjMatrix();
}

void CGraphicBase::SetOrtho3D(float hres, float vres, float zmin, float zmax)
{
	//CCameraManager::Instance().SetCurrentCamera(CCameraManager::DEFAULT_PERSPECTIVE_CAMERA);
	MatrixOrthoRH(&ms_matProj, hres, vres, zmin, zmax);
	//UpdatePipeLineMatrix();
	UpdateProjMatrix();
}

void CGraphicBase::SetPerspective(float fov, float aspect, float nearz, float farz)
{
	ms_fFieldOfView = fov;

	//if (ms_d3dPresentParameter.BackBufferWidth>0 && ms_d3dPresentParameter.BackBufferHeight>0)
	//	ms_fAspect = float(ms_d3dPresentParameter.BackBufferWidth)/float(ms_d3dPresentParameter.BackBufferHeight);
	//else
	ms_fAspect = aspect;

	ms_fNearY = nearz;
	ms_fFarY = farz;

	//CCameraManager::Instance().SetCurrentCamera(CCameraManager::DEFAULT_PERSPECTIVE_CAMERA);
	MatrixPerspectiveFovRH(&ms_matProj, ToRadian(fov), ms_fAspect, nearz, farz);
	//UpdatePipeLineMatrix();
	UpdateProjMatrix();
}

void CGraphicBase::UpdateProjMatrix()
{
	SHADERMANAGER.SetMatrix(MATRIX_PROJECTION, &ms_matProj);
}

void CGraphicBase::UpdateViewMatrix()
{
	CCamera* pkCamera=CCameraManager::Instance().GetCurrentCamera();
	if (!pkCamera)
		return;

	ms_matView = pkCamera->GetViewMatrix();
	SHADERMANAGER.SetMatrix(MATRIX_VIEW, &ms_matView);

	MatrixInverse(&ms_matInverseView, NULL, &ms_matView);
	ms_matInverseViewYAxis._11 = ms_matInverseView._11;
	ms_matInverseViewYAxis._12 = ms_matInverseView._12;
	ms_matInverseViewYAxis._21 = ms_matInverseView._21;
	ms_matInverseViewYAxis._22 = ms_matInverseView._22;
}

void CGraphicBase::UpdatePipeLineMatrix()
{
	UpdateProjMatrix();
	UpdateViewMatrix();
}

void CGraphicBase::SetViewport(DWORD dwX, DWORD dwY, DWORD dwWidth, DWORD dwHeight, float fMinZ, float fMaxZ)
{
	ms_Viewport.TopLeftX = static_cast<FLOAT>(dwX);
	ms_Viewport.TopLeftY = static_cast<FLOAT>(dwY);
	ms_Viewport.Width = static_cast<FLOAT>(dwWidth);
	ms_Viewport.Height = static_cast<FLOAT>(dwHeight);
	ms_Viewport.MinDepth = fMinZ;
	ms_Viewport.MaxDepth = fMaxZ;
}

void CGraphicBase::GetTargetPosition(float * px, float * py, float * pz)
{
	CCamera* pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
	{
		*px = *py = *pz = 0.0f;
		return;
	}
	*px = pCamera->GetTarget().x;
	*py = pCamera->GetTarget().y;
	*pz = pCamera->GetTarget().z;
}

void CGraphicBase::GetCameraPosition(float * px, float * py, float * pz)
{
	CCamera* pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
	{
		*px = *py = *pz = 0.0f;
		return;
	}
	*px = pCamera->GetEye().x;
	*py = pCamera->GetEye().y;
	*pz = pCamera->GetEye().z;
}

void CGraphicBase::GetMatrix(Matrix* pRetMatrix) const
{
	CMatrixStack* pStack = GetActiveMatrixStack();
	assert(pStack != NULL);
	*pRetMatrix = *pStack->GetTop();
}

const Matrix* CGraphicBase::GetMatrixPointer() const
{
	CMatrixStack* pStack = GetActiveMatrixStack();
	assert(pStack != NULL);
	return pStack->GetTop();
}

void CGraphicBase::GetSphereMatrix(Matrix* pMatrix, float fValue)
{
	MatrixIdentity(pMatrix);
	pMatrix->_11 = fValue * ms_matWorldView._11;
	pMatrix->_21 = fValue * ms_matWorldView._21;
	pMatrix->_31 = fValue * ms_matWorldView._31;
	pMatrix->_41 = fValue;
	pMatrix->_12 = -fValue * ms_matWorldView._12;
	pMatrix->_22 = -fValue * ms_matWorldView._22;
	pMatrix->_32 = -fValue * ms_matWorldView._32;
	pMatrix->_42 = -fValue;
}

float CGraphicBase::GetFOV()
{
	return ms_fFieldOfView;
}

void CGraphicBase::PushMatrix()
{
	GetActiveMatrixStack()->Push();
}

void CGraphicBase::Scale(float x, float y, float z)
{
	GetActiveMatrixStack()->Scale(x, y, z);
}

void CGraphicBase::Rotate(float degree, float x, float y, float z)
{
	Vector3 vec(x, y, z);
	GetActiveMatrixStack()->RotateAxis(&vec, ToRadian(degree));
}

void CGraphicBase::RotateLocal(float degree, float x, float y, float z)
{
	Vector3 vec(x, y, z);
	GetActiveMatrixStack()->RotateAxisLocal(&vec, ToRadian(degree));
}

void CGraphicBase::MultMatrix(const Matrix* pMat)
{
	GetActiveMatrixStack()->MultMatrix(pMat);
}

void CGraphicBase::MultMatrixLocal(const Matrix* pMat)
{
	GetActiveMatrixStack()->MultMatrixLocal(pMat);
}

void CGraphicBase::RotateYawPitchRollLocal(float fYaw, float fPitch, float fRoll)
{
	GetActiveMatrixStack()->RotateYawPitchRollLocal(ToRadian(fYaw), ToRadian(fPitch), ToRadian(fRoll));
}

void CGraphicBase::Translate(float x, float y, float z)
{
	GetActiveMatrixStack()->Translate(x, y, z);
}

void CGraphicBase::LoadMatrix(const Matrix& c_rSrcMatrix)
{
	GetActiveMatrixStack()->LoadMatrix(&c_rSrcMatrix);
}

void CGraphicBase::PopMatrix()
{
	GetActiveMatrixStack()->Pop();
}

DWORD CGraphicBase::GetColor(float r, float g, float b, float a)
{
	BYTE argb[4] =
	{
		(BYTE) (255.0f * b),
		(BYTE) (255.0f * g),
		(BYTE) (255.0f * r),
		(BYTE) (255.0f * a)
	};

	return *((DWORD *) argb);
}

void CGraphicBase::InitScreenEffect()
{
	ms_dwWavingEndTime = 0;
	ms_dwFlashingEndTime = 0;
	ms_iWavingPower = 0;
	ms_FlashingColor = Color(0.0f, 0.0f, 0.0f, 0.0f);
}

void CGraphicBase::SetScreenEffectWaving(float fDuringTime, int iPower)
{
	ms_dwWavingEndTime = DX::StepTimer::instance().GetTotalMillieSeconds() + long(fDuringTime * 1000.0f);
	ms_iWavingPower = iPower;
}

void CGraphicBase::SetScreenEffectFlashing(float fDuringTime, const Color& c_rColor)
{
	ms_dwFlashingEndTime = DX::StepTimer::instance().GetTotalMillieSeconds() + long(fDuringTime * 1000.0f);
	ms_FlashingColor = c_rColor;
}

DWORD CGraphicBase::GetFaceCount()
{
	return ms_faceCount;
}

void CGraphicBase::ResetFaceCount()
{
	ms_faceCount = 0;
}

HRESULT CGraphicBase::GetLastResult()
{
	return ms_hLastResult;
}

CGraphicBase::CGraphicBase()
{
}

CGraphicBase::~CGraphicBase()
{
}


//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
