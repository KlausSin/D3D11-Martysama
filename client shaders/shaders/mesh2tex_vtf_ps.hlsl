
#define MAX_LIGHTS 16
#define LIGHT_POINT 1
#define LIGHT_SPOT 2
#define LIGHT_DIRECTIONAL 3

cbuffer CBPerFrame : register(b0)
{
	matrix matView;
	matrix matProjection;
	float4 vCameraPos;
	float4 vFogParams;
	float4 vFogColor;
	float4 vTime;
	float4 vSunDirection;
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

struct Light
{
	float4 Position;
	float4 Direction;
	float4 Color;
	float4 Attenuation;
};

cbuffer CBLighting : register(b2)
{
	Light lights[MAX_LIGHTS];
	float4 globalAmbient;
	int numActiveLights;
	int3 _padLighting;
};

Texture2D    texDiffuse0 : register(t0);
Texture2D    texDiffuse1 : register(t1);
SamplerState samLinear   : register(s0);

struct PS_INPUT
{
	float4 Position   : SV_POSITION;
	float3 WorldPos   : TEXCOORD0;
	float3 WorldNorm  : TEXCOORD1;
	float2 TexCoord0  : TEXCOORD2;
	float2 TexCoord1  : TEXCOORD3;
	float  FogFactor  : TEXCOORD4;
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
	float4 texColor0 = texDiffuse0.Sample(samLinear, input.TexCoord0);
	float4 texColor1 = texDiffuse1.Sample(samLinear, input.TexCoord1);

	if (texColor0.r < 0.001f && texColor0.g < 0.001f && texColor0.b < 0.001f && texColor0.a < 0.001f)
		discard;

	// Edge fade for second texture (shadow map boundary)
	float2 tex1UV = input.TexCoord1;
	float edgeFade = 1.0f;
	float fadeStart = 0.02f;

	float distFromLeft = tex1UV.x;
	float distFromRight = 1.0f - tex1UV.x;
	float distFromBottom = tex1UV.y;
	float distFromTop = 1.0f - tex1UV.y;
	float minDistFromEdge = min(min(distFromLeft, distFromRight), min(distFromBottom, distFromTop));

	if (tex1UV.x < 0.0f || tex1UV.x > 1.0f || tex1UV.y < 0.0f || tex1UV.y > 1.0f)
		edgeFade = 0.0f;
	else if (minDistFromEdge < fadeStart)
		edgeFade = minDistFromEdge / fadeStart;

	// Blend two textures
	float4 blendedTex;
	float tex1Lum = max(texColor1.r, max(texColor1.g, texColor1.b));
	if (tex1Lum > 0.3f && edgeFade > 0.0f)
	{
		float effectiveBlend = vMaterialParams.w * edgeFade;
		blendedTex = texColor0 * lerp(float4(1,1,1,1), texColor1, effectiveBlend);
	}
	else
		blendedTex = texColor0;

	// Alpha test
	if (vMaterialParams.y > 0.5f && blendedTex.a < vMaterialParams.x)
		discard;

	// Apply lighting
	// Apply material diffuse color
	float4 finalColor = blendedTex * vDiffuseColor;

	// Height-based atmospheric fog
	finalColor.rgb = ApplyModernFog(finalColor.rgb, vFogParams, vFogColor, input.FogFactor);

	return finalColor;
}
