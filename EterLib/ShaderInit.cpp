/*
 * ShaderInit.cpp
 * Shader helper function implementations
 *
 * Contains non-inline shader helper functions that are called via extern
 * from other libraries (EffectLib, GameLib, etc.)
 */

#include "StdAfx.h"
#include "ShaderManager.h"

//////////////////////////////////////////////////////////////////////////
// Particle Shader Functions (called from EffectLib)
//////////////////////////////////////////////////////////////////////////

void BeginParticleShaderRender()
{
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.BeginParticle();
		SHADERMANAGER.SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
		SHADERMANAGER.SetTextureFactor(0xFFFFFFFF);  // Reset to white - particle color is per-vertex
		SHADERMANAGER.SetTextureColorSwap(false);

		SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, TRUE);

		SHADERMANAGER.SetPipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
	}
}

void EndParticleShaderRender()
{
	if (SHADERMANAGER.IsInitialized())
	{
		// Restore default render states after particle rendering
		// Re-enable depth write (default state for 3D rendering)
		SHADERMANAGER.SetPipelineState(PSTATE_DEPTHWRITEMASK, TRUE);
		SHADERMANAGER.SetPipelineState(PSTATE_BLENDENABLE, FALSE);

		SHADERMANAGER.End();
	}
}

void SetParticleShaderColor(DWORD dwColor)
{
	if (SHADERMANAGER.IsInitialized())
	{
		SHADERMANAGER.SetTextureFactor(dwColor);
		SHADERMANAGER.CommitChanges();
	}
}
