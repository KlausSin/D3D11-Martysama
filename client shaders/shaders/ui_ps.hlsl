
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
	float4 vSkyTint;
	float4 vMaterialParams;
	float4 vEmissiveColor;
	float4 vSpecularColor;
	float4 vPBRParams;
	float4 vRenderFlags;
	float4 vParticleColor;
	float4 vParticleParams;
};

Texture2D    texDiffuse : register(t0);
Texture2D    texMask    : register(t1);  // Optional mask texture (minimap filter)
SamplerState samPoint   : register(s0);  // UI uses POINT filtering
SamplerState samLinear  : register(s1);  // Linear sampler for mask

struct PS_INPUT
{
	float4 Position  : SV_POSITION;
	float4 Color     : COLOR0;
	float2 TexCoord  : TEXCOORD0;
	float2 MaskCoord : TEXCOORD1;  // For minimap circular mask
};

float4 main(PS_INPUT input) : SV_TARGET
{
	// ColorOp MODULATE: texture * vertex colour
	float4 texColor = texDiffuse.Sample(samPoint, input.TexCoord);
	float4 finalColor = texColor * input.Color;

	// Two-texture masking mode (minimap circular filter)
	// vMaterialParams.w > 0 enables mask texture
	if (vMaterialParams.w > 0.5f)
	{
		float2 maskUV = input.Position.xy * float2(matTexture1._11, matTexture1._22) + float2(matTexture1._41, matTexture1._42);

		float4 maskColor = texMask.Sample(samLinear, maskUV);
		// Use mask alpha for clipping (from texture 1)
		finalColor.a = maskColor.a;
		// Modulate colors
		finalColor.rgb *= maskColor.rgb;
	}

	// Alpha test (shader-based)
	if (vMaterialParams.y > 0.5f && finalColor.a < vMaterialParams.x)
		discard;

	return finalColor;
}
