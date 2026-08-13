

cbuffer CBPerFrame : register(b0)
{
	matrix matView;
	matrix matProjection;
	float4 vCameraPos;
	float4 vFogParams;
	float4 vFogColor;
	float4 vTime;
};

cbuffer CBPerObject : register(b1)
{
	matrix matWorld;
	matrix matWorldViewProj;
	matrix matTexture0;
	matrix matTexture1;
	float4 vDiffuseColor;
	float4 vSkyTint;     // White (1,1,1) for normal, Gray (0.5,0.5,0.5) for shadow
	float4 vMaterialParams;    // x = alphaRef, y = alphaTestEnabled
	float4 vEmissiveColor;
	float4 vSpecularColor;
	float4 vPBRParams;
	float4 vRenderFlags;
	float4 vParticleColor;
	float4 vParticleParams;
};

Texture2D    texDiffuse : register(t0);
SamplerState samLinear  : register(s0);

struct PS_INPUT
{
	float4 Position  : SV_POSITION;
	float4 Color     : COLOR0;
	float2 TexCoord  : TEXCOORD0;
	float  FogFactor : TEXCOORD1;
};


float3 ApplyModernFog(float3 color, float4 fogParams, float4 fogColor, float viewDepth)
{
	if (fogParams.w < 0.5f)
		return color;

	// viewDepth is the clip-space Z the vertex shader forwarded - the exact value the original
	// fog compared against fogStart/fogEnd, so it is known-good in these world units. The depth
	// TEXTURE route was abandoned: dbg mode 1 showed it resolving to solid black (all zeros).
	//
	// Two independent dials:
	//   fogParams.x = START  - fog begins at this distance; nearer geometry is untouched.
	//   fogColor.a  = DENSITY - how fast it accumulates BEYOND the start.
	// Start only shifts where accumulation begins, density only scales the rate, so neither
	// can affect the other.
	float d      = max(viewDepth - fogParams.x, 0.0f);
	// 0.000015 base: the view distance here is ~25000 units, so the old 0.00010 saturated to
	// solid white past ~3000 and the top half of the slider was unusable. At dial 1.0 this is
	// ~31% haze at max range; at dial 4.0 ~78%. The whole range now does something.
	float dens   = 0.000025f * fogColor.a;
	float fogAmt = saturate(1.0f - exp(-dens * d));

	// Cap short of 1.0 so distant geometry never collapses to one flat colour. Fully saturated
	// fog is what made the far cliff read as a grey WALL: every pixel landed on the same value,
	// so all shape and contrast was gone. Leaving a few percent of the surface through keeps the
	// silhouette readable and the scene feeling deep.
	fogAmt = min(fogAmt, 0.92f);

	return lerp(color, fogColor.rgb, fogAmt);
}

float4 main(PS_INPUT input) : SV_TARGET
{
	// Sample texture
	float4 texColor = texDiffuse.Sample(samLinear, input.TexCoord);

	// Modulate texture with vertex color (static lighting)
	float4 finalColor = texColor * input.Color;

	if (vMaterialParams.y > 0.5f && finalColor.a < vMaterialParams.x)
		discard;

	if (vRenderFlags.x > 0.5f)
	{
		return float4(input.Position.z, 0, 0, 1);
	}

	// Height-based atmospheric fog
	finalColor.rgb = ApplyModernFog(finalColor.rgb, vFogParams, vFogColor, input.FogFactor);

	// Normal rendering - output texture * vertex color
	return finalColor;
}
