#pragma once

/*
 * ShaderInit.h
 * Shader system initialization and helper functions
 *
 * This file provides helper functions that integrate the ShaderManager
 * with the rest of the rendering code.
 */

#include "GrpMathType.h"
#include "GrpLightType.h"
#include "ShaderManager.h"
#include "../eterBase/Debug.h"

//////////////////////////////////////////////////////////////////////////
// Helper Functions
//////////////////////////////////////////////////////////////////////////

// Check if shader system is ready
inline bool AreRuntimeShadersEnabled()
{
	return SHADERMANAGER.IsInitialized();
}

// Initialization (called from GrpDevice)
inline bool InitializeShaderSystem()
{
	return true; // ShaderManager is initialized separately in GrpDevice
}

inline void ShutdownShaderSystem()
{
	SHADERMANAGER.Shutdown();
}

// Per-frame updates
inline void UpdateShaderFrameConstants()
{
	if (!SHADERMANAGER.IsInitialized())
		return;

	// Get current viewport size using public getter
	const D3D11_VIEWPORT& viewport = CGraphicBase::GetViewport();
	SHADERMANAGER.SetViewportSize(viewport.Width, viewport.Height);
}

// Set world matrix (called before each draw)
inline void SetShaderWorldMatrix(const Matrix* pWorld)
{
	// Always update StateManager for compatibility
	SHADERMANAGER.SetMatrix(MATRIX_WORLD, pWorld);

	// Also update ShaderManager if active
	if (SHADERMANAGER.IsInitialized() && SHADERMANAGER.GetCurrentShader() != SHADER_NONE)
	{
		SHADERMANAGER.SetWorldMatrix(pWorld);
		SHADERMANAGER.CommitChanges();
	}
}

// Lighting and fog
inline void UpdateShaderLightingConstants(const TLight& light, const Color& ambient)
{
	if (!SHADERMANAGER.IsInitialized())
		return;

	Vector3 lightDir(light.Direction.x, light.Direction.y, light.Direction.z);
	Color lightColor(light.Diffuse.r, light.Diffuse.g, light.Diffuse.b, 1.0f);
	SHADERMANAGER.SetLight(&lightDir, &lightColor, 1.0f);
	SHADERMANAGER.SetAmbient(&ambient);
}

inline void UpdateShaderFogConstants(bool bEnabled, float fStart, float fEnd, float fDensity, const Color& color, bool bLinear)
{
	if (!SHADERMANAGER.IsInitialized())
		return;

	// Convert Color to DWORD (ARGB format)
	DWORD dwColor = (DWORD)(
		((DWORD)(255) << 24) |
		((DWORD)(color.r * 255.0f) << 16) |
		((DWORD)(color.g * 255.0f) << 8) |
		((DWORD)(color.b * 255.0f))
	);
	SHADERMANAGER.SetFog(bEnabled, fStart, fEnd, dwColor);
}

// Shadow rendering mode (stub for now)
inline bool IsShadowRenderMode() { return false; }
inline void SetShadowRenderMode(bool) {}

// Mesh rendering helpers
inline void BeginShaderMeshRender(bool bSkinned)
{
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.BeginMesh();
		SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
		SHADERMANAGER.CommitChanges();
	}
}

inline void EndShaderMeshRender()
{
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.End();
	}
}

inline void BeginShaderMesh2TexRender()
{
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.BeginMesh2Tex();
		SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

inline void EndShaderMesh2TexRender()
{
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.End();
	}
}

inline void SetShaderMaterial(const TMaterial& mat, const Matrix& world, int renderMode, DWORD dwAddColor)
{
	if (!SHADERMANAGER.IsInitialized())
		return;

	SHADERMANAGER.SetWorldMatrix(&world);
	SHADERMANAGER.SetDiffuseColor(mat.Diffuse.r, mat.Diffuse.g, mat.Diffuse.b, mat.Diffuse.a);
	SHADERMANAGER.CommitChanges();
}

inline void SetShaderTwoTextureBlend(bool bEnabled)
{
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.SetTwoTextureBlend(bEnabled);
		SHADERMANAGER.CommitChanges();
	}
}

// UI rendering helpers
inline void BeginUIShaderRender()
{
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.BeginUI();
	}
}

inline void EndUIShaderRender()
{
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.End();
	}
}

inline void UpdateUIShaderMatrix(const Matrix* pOrthoMatrix)
{
	// UI uses screen-space coordinates, no matrix needed
}

// Shadow rendering helpers
inline void BeginShaderShadowRender()
{
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.BeginShadow();
	}
}

inline void EndShaderShadowRender()
{
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.End();
	}
}

// SpeedTree rendering helpers
inline void BeginShaderSpeedTreeRender(float windStrength = 1.0f)
{
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.BeginSpeedTree();
		// Set wind strength via material params z component
		SHADERMANAGER.SetMaterial(windStrength);  // Using specular power slot for wind
		SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
		SHADERMANAGER.SetTextureFactor(0xFFFFFFFF);
	}
}

inline void EndShaderSpeedTreeRender()
{
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.End();
	}
}

// Enable/disable (for compatibility)
inline void SetRuntimeShadersEnabled(bool) {}
