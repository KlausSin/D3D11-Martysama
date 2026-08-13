#include "StdAfx.h"
#include "../eterLib/ShaderManager.h"
#include "../eterLib/JpegFile.h"
#include "../eterLib/ShaderInit.h"
#include "../eterLib/GrpDevice.h"
#include "PythonGraphic.h"

bool g_isScreenShotKey = false;

void CPythonGraphic::Destroy()
{
}

IDXGIFactory* CPythonGraphic::GetDXGIFactory()
{
	return CGraphicDevice::Instance().GetDXGIFactory();
}

float CPythonGraphic::GetOrthoDepth()
{
	return m_fOrthoDepth;
}

void CPythonGraphic::SetInterfaceRenderState()
{
	SHADERMANAGER.SetMatrix(MATRIX_PROJECTION, &ms_matIdentity);
 	SHADERMANAGER.SetMatrix(MATRIX_VIEW, &ms_matIdentity);
	SHADERMANAGER.SetMatrix(MATRIX_WORLD, &ms_matIdentity);

	SHADERMANAGER.SetSamplerState(0, SAMPLER_MINFILTER, FILTER_POINT);
	SHADERMANAGER.SetSamplerState(0, SAMPLER_MAGFILTER, FILTER_POINT);
	SHADERMANAGER.SetSamplerState(0, SAMPLER_MIPFILTER, FILTER_NONE);

	SHADERMANAGER.SetPipelineState(PSTATE_DEPTHENABLE, FALSE);
	SHADERMANAGER.SetPipelineState(PSTATE_DEPTHWRITEMASK, FALSE);

	SHADERMANAGER.SetPipelineState(PSTATE_CULLMODE, CULL_NONE);

	SHADERMANAGER.SetPipelineState(PSTATE_FILLMODE, FILL_SOLID);

	SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, TRUE);
	SHADERMANAGER.SetPipelineState(PSTATE_SRCBLEND,	BLEND_SRCALPHA);
	SHADERMANAGER.SetPipelineState(PSTATE_DESTBLEND, BLEND_INVSRCALPHA);

	CPythonGraphic::Instance().SetBlendOperation();
	CPythonGraphic::Instance().SetOrtho2D(ms_iWidth, ms_iHeight, GetOrthoDepth());

	SHADERMANAGER.SetLightingEnabled(false);

	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
		SHADERMANAGER.SetParticleColor(0xFFFFFFFF);
		SHADERMANAGER.SetAlphaTest(false, 0.0f);  // Disable alpha test for UI
	}
}

void CPythonGraphic::SetGameRenderState()
{
	SHADERMANAGER.SetSamplerState(0, SAMPLER_MINFILTER, FILTER_LINEAR);
	SHADERMANAGER.SetSamplerState(0, SAMPLER_MAGFILTER, FILTER_LINEAR);
	SHADERMANAGER.SetSamplerState(0, SAMPLER_MIPFILTER, FILTER_LINEAR);

	SHADERMANAGER.SetPipelineState(PSTATE_DEPTHENABLE, TRUE);
	SHADERMANAGER.SetPipelineState(PSTATE_DEPTHWRITEMASK, TRUE);

	SHADERMANAGER.SetPipelineState(PSTATE_CULLMODE, CULL_FRONT);

	SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, FALSE);
	SHADERMANAGER.SetLightingEnabled(true);

	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
		SHADERMANAGER.SetParticleColor(0xFFFFFFFF);
	}
}

void CPythonGraphic::SetCursorPosition(int x, int y)
{
	CScreen::SetCursorPosition(x, y, ms_iWidth, ms_iHeight);
}

void CPythonGraphic::SetOmniLight()
{
    // Set up a material
    TMaterial Material;
	Material.Ambient = Color(0.3f, 0.3f, 0.3f, 1.0f);
	Material.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
	Material.Emissive = Color(0.1f, 0.1f, 0.1f, 1.0f);
	Material.Specular = Color(1.0f, 1.0f, 1.0f, 1.0f);
	Material.Power = 20.0f;
    SHADERMANAGER.SetMaterial(&Material);

	float fDirX = -0.10f, fDirY = -0.40f, fDirZ = -0.85f;
	float fLR = 1.00f, fLG = 0.89f, fLB = 0.75f, fAmb = 1.22f, fInt = 3.35f;

	TLight Light;
	Light.Type = LIGHT_DIRECTIONAL;
    Light.Position = Vector3(50.0f, 150.0f, 350.0f);
    Light.Direction = Vector3(fDirX, fDirY, fDirZ);
    Light.Theta = ToRadian(30.0f);
    Light.Phi = ToRadian(45.0f);
    Light.Falloff = 1.0f;
    Light.Attenuation0 = 0.0f;
    Light.Attenuation1 = 0.005f;
    Light.Attenuation2 = 0.0f;
    Light.Diffuse.r = fLR * fInt;
    Light.Diffuse.g = fLG * fInt;
    Light.Diffuse.b = fLB * fInt;
	Light.Diffuse.a = 1.0f;
	Light.Ambient.r = 1.0f;
	Light.Ambient.g = 1.0f;
	Light.Ambient.b = 1.0f;
	Light.Ambient.a = 1.0f;
    Light.Range = 500.0f;
	SHADERMANAGER.SetGlobalAmbient(fAmb, fAmb, fAmb, 1.0f);
	SHADERMANAGER.SetLight(0, &Light);
	SHADERMANAGER.LightEnable(0, TRUE);

	Light.Type = LIGHT_POINT;
	Light.Position = Vector3(0.0f, 200.0f, 200.0f);
	Light.Attenuation0 = 0.1f;
	Light.Attenuation1 = 0.01f;
	Light.Attenuation2 = 0.0f;
	SHADERMANAGER.SetLight(1, &Light);
	SHADERMANAGER.LightEnable(1, TRUE);
}

void CPythonGraphic::SetViewport(float fx, float fy, float fWidth, float fHeight)
{
	UINT numViewports = 1;
	ms_pContext->RSGetViewports(&numViewports, &m_backupViewport);

	D3D11_VIEWPORT ViewPort;
	ViewPort.TopLeftX = fx;
	ViewPort.TopLeftY = fy;
	ViewPort.Width = fWidth;
	ViewPort.Height = fHeight;
	ViewPort.MinDepth = 0.0f;
	ViewPort.MaxDepth = 1.0f;
	ms_pContext->RSSetViewports(1, &ViewPort);
}

void CPythonGraphic::RestoreViewport()
{
	ms_pContext->RSSetViewports(1, &m_backupViewport);
}

void CPythonGraphic::SetGamma(float fGammaFactor)
{
	IDXGIOutput* pOutput = nullptr;
	IDXGISwapChain* pSwapChain = CGraphicDevice::Instance().GetSwapChain();
	if (!pSwapChain)
		return;

	if (FAILED(pSwapChain->GetContainingOutput(&pOutput)) || !pOutput)
		return;

	DXGI_GAMMA_CONTROL gammaControl;
	gammaControl.Scale.Red = fGammaFactor;
	gammaControl.Scale.Green = fGammaFactor;
	gammaControl.Scale.Blue = fGammaFactor;
	gammaControl.Offset.Red = 0.0f;
	gammaControl.Offset.Green = 0.0f;
	gammaControl.Offset.Blue = 0.0f;

	// Build gamma curve
	for (int i = 0; i < 1025; ++i)
	{
		float value = static_cast<float>(i) / 1024.0f;
		gammaControl.GammaCurve[i].Red = value;
		gammaControl.GammaCurve[i].Green = value;
		gammaControl.GammaCurve[i].Blue = value;
	}

	pOutput->SetGammaControl(&gammaControl);
	pOutput->Release();
}

void GenScreenShotTag(const char* src, DWORD crc32, char* leaf, size_t leafLen)
{
	const char* p = src;
	const char* n = p;
	while (n = strchr(p, '\\'))
		p = n + 1;

	_snprintf(leaf, leafLen, "YMIR_METIN2:%s:0x%.8x", p, crc32);
}

bool CPythonGraphic::SaveJPEG(const char * pszFileName, LPBYTE pbyBuffer, UINT uWidth, UINT uHeight)
{
	return jpeg_save(pbyBuffer, uWidth, uHeight, 85, pszFileName) != 0;
}

bool CPythonGraphic::SaveScreenShot(const char * c_pszFileName)
{
	HRESULT hr;

	IDXGISwapChain* pSwapChain = CGraphicDevice::Instance().GetSwapChain();
	if (!pSwapChain)
	{
		TraceError("Failed to get swap chain");
		return false;
	}

	ID3D11Texture2D* pBackBuffer = nullptr;
	if (FAILED(hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer)))
	{
		TraceError("Failed to get back buffer (0x%08x)", hr);
		return false;
	}

	D3D11_TEXTURE2D_DESC backBufferDesc;
	pBackBuffer->GetDesc(&backBufferDesc);

	UINT uWidth = backBufferDesc.Width;
	UINT uHeight = backBufferDesc.Height;

	// Create staging texture for CPU read
	D3D11_TEXTURE2D_DESC stagingDesc = backBufferDesc;
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.BindFlags = 0;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.MiscFlags = 0;

	ID3D11Texture2D* pStagingTexture = nullptr;
	if (FAILED(hr = ms_pDevice->CreateTexture2D(&stagingDesc, nullptr, &pStagingTexture)))
	{
		TraceError("Failed to create staging texture (0x%08x)", hr);
		pBackBuffer->Release();
		return false;
	}

	// Copy back buffer to staging texture
	ms_pContext->CopyResource(pStagingTexture, pBackBuffer);
	pBackBuffer->Release();

	// Map staging texture
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	if (FAILED(hr = ms_pContext->Map(pStagingTexture, 0, D3D11_MAP_READ, 0, &mappedResource)))
	{
		TraceError("Failed to map staging texture (0x%08x)", hr);
		pStagingTexture->Release();
		return false;
	}

	BYTE* pbyBuffer = new BYTE[uWidth * uHeight * 3];
	if (pbyBuffer == NULL)
	{
		ms_pContext->Unmap(pStagingTexture, 0);
		pStagingTexture->Release();
		TraceError("Failed to allocate screenshot buffer");
		return false;
	}

	BYTE* pbySource = (BYTE*)mappedResource.pData;
	BYTE* pbyDestination = pbyBuffer;

	// Convert based on format (most common DX11 formats)
	for (UINT y = 0; y < uHeight; ++y)
	{
		BYTE* pRow = pbySource;

		switch (backBufferDesc.Format)
		{
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			for (UINT x = 0; x < uWidth; ++x)
			{
				*pbyDestination++ = pRow[0];	// Red -> Blue
				*pbyDestination++ = pRow[1];	// Green
				*pbyDestination++ = pRow[2];	// Blue -> Red
				pRow += 4;
			}
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			for (UINT x = 0; x < uWidth; ++x)
			{
				*pbyDestination++ = pRow[2];	// Blue
				*pbyDestination++ = pRow[1];	// Green
				*pbyDestination++ = pRow[0];	// Red
				pRow += 4;
			}
			break;
		default:
			// Unsupported format - fill with black
			for (UINT x = 0; x < uWidth; ++x)
			{
				*pbyDestination++ = 0;
				*pbyDestination++ = 0;
				*pbyDestination++ = 0;
			}
			break;
		}

		pbySource += mappedResource.RowPitch;
	}

	ms_pContext->Unmap(pStagingTexture, 0);
	pStagingTexture->Release();

	bool bSaved = SaveJPEG(c_pszFileName, (LPBYTE)pbyBuffer, uWidth, uHeight);

	if(pbyBuffer) {
		delete [] pbyBuffer;
		pbyBuffer = NULL;
	}

	if(bSaved == false) {
		TraceError("Failed to save JPEG file. (%s, %d, %d)", c_pszFileName, uWidth, uHeight);
		return false;
	}

	if (g_isScreenShotKey)
	{
		FILE* srcFilePtr = fopen(c_pszFileName, "rb");
		if (srcFilePtr)
		{
			fseek(srcFilePtr, 0, SEEK_END);
			size_t fileSize = ftell(srcFilePtr);
			fseek(srcFilePtr, 0, SEEK_SET);

			char head[21];
			size_t tailSize = fileSize - sizeof(head);
			char* tail = (char*)malloc(tailSize);

			fread(head, sizeof(head), 1, srcFilePtr);
			fread(tail, tailSize, 1, srcFilePtr);
			fclose(srcFilePtr);

			char imgDesc[64];
			GenScreenShotTag(c_pszFileName, GetCRC32(tail, tailSize), imgDesc, sizeof(imgDesc));

			int imgDescLen = (int)(strlen(imgDesc) + 1);

			unsigned char exifHeader[] = {
				0xe1,
				0, // blockLen[1],
				0, // blockLen[0],
				0x45,
				0x78,
				0x69,
				0x66,
				0x0,
				0x0,
				0x49,
				0x49,
				0x2a,
				0x0,
				0x8,
				0x0,
				0x0,
				0x0,
				0x1,
				0x0,
				0xe,
				0x1,
				0x2,
				0x0,
				(unsigned char)imgDescLen,
				0, // textLen[1],
				0, // textLen[2],
				0, // textLen[3],
				0x1a,
				0x0,
				0x0,
				0x0,
				0x0,
				0x0,
				0x0,
				0x0,
			};

			exifHeader[2] = (unsigned char)(sizeof(exifHeader) + imgDescLen);

			FILE* dstFilePtr = fopen(c_pszFileName, "wb");
			//FILE* dstFilePtr = fopen("temp.jpg", "wb");
			if (dstFilePtr)
			{
				fwrite(head, sizeof(head), 1, dstFilePtr);
				fwrite(exifHeader, sizeof(exifHeader), 1, dstFilePtr);
				fwrite(imgDesc, imgDescLen, 1, dstFilePtr);
				fputc(0x00, dstFilePtr);
				fputc(0xff, dstFilePtr);
				fwrite(tail, tailSize, 1, dstFilePtr);
				fclose(dstFilePtr);
			}

			free(tail);
		}
	}
	return true;
}

void CPythonGraphic::PushState()
{
	TState curState;

	curState.matProj = ms_matProj;
	curState.matView = ms_matView;

	m_stateStack.push(curState);
}

void CPythonGraphic::PopState()
{
	if (m_stateStack.empty())
	{
		assert(!"PythonGraphic::PopState StateStack is EMPTY");
		return;
	}

	TState & rState = m_stateStack.top();

	ms_matProj = rState.matProj;
	ms_matView = rState.matView;

	UpdatePipeLineMatrix();

	m_stateStack.pop();
}

void CPythonGraphic::RenderImage(CGraphicImageInstance* pImageInstance, float x, float y)
{
	assert(pImageInstance != NULL);

	//SetColorRenderState();
	const CGraphicTexture * c_pTexture = pImageInstance->GetTexturePointer();

	float width = (float) pImageInstance->GetWidth();
	float height = (float) pImageInstance->GetHeight();

	c_pTexture->SetTextureStage(0);

	RenderTextureBox(x,
					 y,
					 x + width,
					 y + height,
					 0.0f,
					 0.5f / width,
					 0.5f / height,
					 (width + 0.5f) / width,
					 (height + 0.5f) / height);
}

void CPythonGraphic::RenderAlphaImage(CGraphicImageInstance* pImageInstance, float x, float y, float aLeft, float aRight)
{
	assert(pImageInstance != NULL);

	Color DiffuseColor1 = Color(1.0f, 1.0f, 1.0f, aLeft);
	Color DiffuseColor2 = Color(1.0f, 1.0f, 1.0f, aRight);

	const CGraphicTexture * c_pTexture = pImageInstance->GetTexturePointer();

	float width = (float) pImageInstance->GetWidth();
	float height = (float) pImageInstance->GetHeight();

	c_pTexture->SetTextureStage(0);

	float sx = x;
	float sy = y;
	float ex = x + width;
	float ey = y + height;
	float z = 0.0f;

	float su = 0.0f;
	float sv = 0.0f;
	float eu = 1.0f;
	float ev = 1.0f;

	TPDTVertex vertices[4];
	vertices[0].position = TPosition(sx, sy, z);
	vertices[0].diffuse = DiffuseColor1;
	vertices[0].texCoord = TTextureCoordinate(su, sv);

	vertices[1].position = TPosition(ex, sy, z);
	vertices[1].diffuse = DiffuseColor2;
	vertices[1].texCoord = TTextureCoordinate(eu, sv);

	vertices[2].position = TPosition(sx, ey, z);
	vertices[2].diffuse = DiffuseColor1;
	vertices[2].texCoord = TTextureCoordinate(su, ev);

	vertices[3].position = TPosition(ex, ey, z);
	vertices[3].diffuse = DiffuseColor2;
	vertices[3].texCoord = TTextureCoordinate(eu, ev);

	SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_PDT);
	CGraphicBase::SetDefaultIndexBuffer(DEFAULT_IB_FILL_RECT);
	if (CGraphicBase::SetPDTStream(vertices, 4))
		SHADERMANAGER.DrawIndexed(TOPOLOGY_TRIANGLELIST, 0, 4, 0, 2);
}

void CPythonGraphic::RenderCoolTimeBox(float fxCenter, float fyCenter, float fRadius, float fTime)
{
	if (fTime >= 1.0f)
		return;

	fTime = max(0.0f, fTime);

	static Color color = Color(0.0f, 0.0f, 0.0f, 0.5f);
	static Vector2 s_v2BoxPos[8] =
	{
		Vector2( -1.0f, -1.0f ),
		Vector2( -1.0f,  0.0f ),
		Vector2( -1.0f, +1.0f ),
		Vector2(  0.0f, +1.0f ),
		Vector2( +1.0f, +1.0f ),
		Vector2( +1.0f,  0.0f ),
		Vector2( +1.0f, -1.0f ),
		Vector2(  0.0f, -1.0f ),
	};

	int iTriCount = int(8 - 8.0f * fTime);
	float fLastPercentage = (8 - 8.0f * fTime) - iTriCount;


	// First, collect edge vertices (not including center)
	std::vector<TPDTVertex> edgeVerts;
	TPDTVertex vertex;
	vertex.position.z = 0.0f;
	vertex.diffuse = color;
	vertex.texCoord.x = 0.0f;
	vertex.texCoord.y = 0.0f;

	// First edge vertex (top)
	vertex.position.x = fxCenter;
	vertex.position.y = fyCenter - fRadius;
	edgeVerts.push_back(vertex);

	// Box corner vertices
	for (int j = 0; j < iTriCount; ++j)
	{
		vertex.position.x = fxCenter + s_v2BoxPos[j].x * fRadius;
		vertex.position.y = fyCenter + s_v2BoxPos[j].y * fRadius;
		edgeVerts.push_back(vertex);
	}

	// Partial last vertex for smooth animation
	if (fLastPercentage > 0.0f)
	{
		Vector2 * pv2Pos;
		Vector2 * pv2LastPos;

		assert((iTriCount-1+8)%8 >= 0 && (iTriCount-1+8)%8 < 8);
		assert((iTriCount+8)%8 >= 0 && (iTriCount+8)%8 < 8);
		pv2LastPos = &s_v2BoxPos[(iTriCount-1+8)%8];
		pv2Pos = &s_v2BoxPos[(iTriCount+8)%8];

		vertex.position.x = fxCenter + ((pv2Pos->x-pv2LastPos->x) * fLastPercentage + pv2LastPos->x) * fRadius;
		vertex.position.y = fyCenter + ((pv2Pos->y-pv2LastPos->y) * fLastPercentage + pv2LastPos->y) * fRadius;
		edgeVerts.push_back(vertex);
		++iTriCount;
	}

	if (edgeVerts.size() < 2 || iTriCount < 1)
		return;

	std::vector<TPDTVertex> triListVerts;
	TPDTVertex centerVert;
	centerVert.position.x = fxCenter;
	centerVert.position.y = fyCenter;
	centerVert.position.z = 0.0f;
	centerVert.diffuse = color;
	centerVert.texCoord.x = 0.0f;
	centerVert.texCoord.y = 0.0f;

	for (int i = 0; i < iTriCount && i + 1 < (int)edgeVerts.size(); ++i)
	{
		triListVerts.push_back(centerVert);
		triListVerts.push_back(edgeVerts[i]);
		triListVerts.push_back(edgeVerts[i + 1]);
	}

	if (triListVerts.empty())
		return;

	if (SetPDTStream(&triListVerts[0], (UINT)(triListVerts.size())))
	{
		SHADERMANAGER.SetDefaultTexture(0);
		SHADERMANAGER.SetShaderResource(1, NULL);
			SHADERMANAGER.SetInputLayout(INPUT_LAYOUT_PDT);
		SHADERMANAGER.Draw(TOPOLOGY_TRIANGLELIST, 0, iTriCount);
	}
}

long CPythonGraphic::GenerateColor(float r, float g, float b, float a)
{
	return GetColor(r, g, b, a);
}

void CPythonGraphic::RenderDownButton(float sx, float sy, float ex, float ey)
{
	RenderBox2d(sx, sy, ex, ey);

	SetDiffuseColor(m_darkColor);
	RenderLine2d(sx, sy, ex, sy);
	RenderLine2d(sx, sy, sx, ey);

	SetDiffuseColor(m_lightColor);
	RenderLine2d(sx, ey, ex, ey);
	RenderLine2d(ex, sy, ex, ey);
}

void CPythonGraphic::RenderUpButton(float sx, float sy, float ex, float ey)
{
	RenderBox2d(sx, sy, ex, ey);

	SetDiffuseColor(m_lightColor);
	RenderLine2d(sx, sy, ex, sy);
	RenderLine2d(sx, sy, sx, ey);

	SetDiffuseColor(m_darkColor);
	RenderLine2d(sx, ey, ex, ey);
	RenderLine2d(ex, sy, ex, ey);
}

DWORD CPythonGraphic::GetAvailableMemory()
{
	IDXGIDevice* pDXGIDevice = nullptr;
	if (FAILED(ms_pDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDXGIDevice)))
		return 0;

	IDXGIAdapter* pAdapter = nullptr;
	if (FAILED(pDXGIDevice->GetAdapter(&pAdapter)))
	{
		pDXGIDevice->Release();
		return 0;
	}

	DXGI_ADAPTER_DESC adapterDesc;
	if (FAILED(pAdapter->GetDesc(&adapterDesc)))
	{
		pAdapter->Release();
		pDXGIDevice->Release();
		return 0;
	}

	pAdapter->Release();
	pDXGIDevice->Release();

	// Return dedicated video memory in bytes
	return static_cast<DWORD>(adapterDesc.DedicatedVideoMemory);
}

CPythonGraphic::CPythonGraphic()
{
	m_lightColor = GetColor(1.0f, 1.0f, 1.0f);
	m_darkColor = GetColor(0.0f, 0.0f, 0.0f);

	memset(&m_backupViewport, 0, sizeof(D3D11_VIEWPORT));

	m_fOrthoDepth = 1000.0f;
}

CPythonGraphic::~CPythonGraphic()
{
	Tracef("Python Graphic Clear\n");
}
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
