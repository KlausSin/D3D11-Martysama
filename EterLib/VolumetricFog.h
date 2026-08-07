#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include "../eterBase/Singleton.h"

using namespace DirectX;

static const UINT VFOG_GRID_X = 160;
static const UINT VFOG_GRID_Y = 90;
static const UINT VFOG_GRID_Z = 64;

__declspec(align(16)) struct CBVolumetricFog
{
	XMMATRIX matInvViewProj;
	XMFLOAT4 vCameraPos;       // xyz = world camera
	XMFLOAT4 vGridSize;        // xyz = grid dims, w = unused
	XMFLOAT4 vFogNearFar;      // x = volume near, y = volume far, z = far/near, w = log(far/near)
	XMFLOAT4 vFogMedia;        // x = density, y = height falloff, z = base height, w = phase g
	XMFLOAT4 vFogScatterColor; // rgb = scattering colour, a = unused
};

class CVolumetricFog : public CSingleton<CVolumetricFog>
{
	public:
		CVolumetricFog();
		~CVolumetricFog();

		bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
		void Shutdown();
		bool IsInitialized() const { return m_bInitialized; }

		// Set by the frame loop from the vfog.txt poll. The legacy per-material fog reads this to
		// switch itself off, so the two fogs never composite on top of each other.
		static void SetActive(bool b) { ms_bActive = b; }
		static bool IsActive()        { return ms_bActive; }

		void SetEnabled(bool b) { m_bEnabled = b; }
		bool IsEnabled() const  { return m_bEnabled; }

		// Media parameters. Fed from the env fog so authored maps keep control.
		// Two separable dials, 0..4 multipliers with 1.0 = default (matches the DX12 client):
		//   fNear = overall thickness, fDist = how fast the haze builds with distance.
		void  SetFogNear(float v) { m_cb.vFogMedia.x = (v < 0.0f) ? 0.0f : (v > 4.0f ? 4.0f : v); }
		void  SetFogStart(float v) { m_cb.vFogNearFar.x = (v < 0.0f) ? 0.0f : v; }
		float GetFogNear() const  { return m_cb.vFogMedia.x; }
		float GetFogStart() const { return m_cb.vFogNearFar.x; }
		void  SetDebugMode(float m) { m_cb.vFogMedia.z = m; }

		void SetScatterColor(float r, float g, float b)
		{ m_cb.vFogScatterColor.x = r; m_cb.vFogScatterColor.y = g; m_cb.vFogScatterColor.z = b; }

		void SetMedia(float density, float heightFalloff, float baseHeight,
		              float r, float g, float b);
		void SetRange(float nearDist, float farDist);

		// In-game dials (app.SetVolumetricFog*). Values persist across map changes; the per-frame
		// env feed only supplies the fog COLOUR so tuning here is not overwritten every frame.
		void  SetDensity(float d) { m_cb.vFogMedia.x = (d < 0.0f) ? 0.0f : d; }
		float GetDensity() const  { return m_cb.vFogMedia.x; }
		void  SetHeight(float falloff, float baseHeight)
		{ m_cb.vFogMedia.y = (falloff < 0.0f) ? 0.0f : falloff; m_cb.vFogMedia.z = baseHeight; }
		float GetRangeNear() const { return m_cb.vFogNearFar.x; }
		float GetRangeFar()  const { return m_cb.vFogNearFar.y; }

		// Builds the volume and composites it over the currently bound render target.
		// Call after opaque geometry, before the UI.
		void Render(const XMMATRIX& matView, const XMMATRIX& matProj, const XMFLOAT3& camPos);

	private:
		bool __CreateVolume(ID3D11Texture3D** ppTex, ID3D11ShaderResourceView** ppSRV,
		                    ID3D11UnorderedAccessView** ppUAV);
		void __DestroyVolume(ID3D11Texture3D** ppTex, ID3D11ShaderResourceView** ppSRV,
		                     ID3D11UnorderedAccessView** ppUAV);
		bool __CreateShaders();
		bool __CreateStates();
		void __UpdateConstants(const XMMATRIX& matView, const XMMATRIX& matProj,
		                       const XMFLOAT3& camPos);

		ID3D11Device*        m_pDevice;
		ID3D11DeviceContext* m_pContext;
		bool                 m_bInitialized;
		bool                 m_bEnabled;

		ID3D11Texture3D*           m_pMediaTex;
		ID3D11ShaderResourceView*  m_pMediaSRV;
		ID3D11UnorderedAccessView* m_pMediaUAV;

		ID3D11Texture3D*           m_pIntegratedTex;
		ID3D11ShaderResourceView*  m_pIntegratedSRV;
		ID3D11UnorderedAccessView* m_pIntegratedUAV;

		ID3D11ComputeShader* m_pCSInject;
		ID3D11ComputeShader* m_pCSIntegrate;
		ID3D11VertexShader*  m_pVSComposite;
		ID3D11PixelShader*   m_pPSComposite;

		ID3D11Buffer*            m_pCB;
		ID3D11BlendState*        m_pBlendState;
		ID3D11DepthStencilState* m_pDepthState;
		ID3D11RasterizerState*   m_pRasterState;
		ID3D11SamplerState*      m_pSamplerLinear;

		CBVolumetricFog m_cb;

		static bool ms_bActive;
};
