
#define MAX_LIGHTS 16
#define LIGHT_POINT 1
#define LIGHT_SPOT 2
#define LIGHT_DIRECTIONAL 3

cbuffer CBPerFrame : register(b0)
{
	matrix matView;
	matrix matProjection;
	float4 vCameraPos;      // xyz = camera pos, w = viewport width
	float4 vFogParams;      // x = start, y = end, z = viewport height, w = enabled
	float4 vFogColor;
	float4 vTime;
};

cbuffer CBPerObject : register(b1)
{
	matrix matWorld;
	matrix matWorldViewProj;
	matrix matTexture0;
	matrix matTexture1;
	float4 vDiffuseColor;   // Material diffuse
	float4 vSkyTint;
	float4 vMaterialParams; // x = alphaRef, y = alphaTestEnabled, z = specularPower, w = twoTexBlend
	float4 vEmissiveColor;
	float4 vSpecularColor;
	float4 vPBRParams;
	float4 vRenderFlags;
	float4 vParticleColor;
	float4 vParticleParams;
};

// Native DX11 multi-light constant buffer
struct Light
{
	float4 Position;        // xyz = position, w = type (0=dir, 1=point, 2=spot)
	float4 Direction;       // xyz = direction, w = enabled
	float4 Color;           // rgb = color, a = intensity
	float4 Attenuation;     // x = constant, y = linear, z = quadratic, w = range
};

cbuffer CBLighting : register(b2)
{
	Light lights[MAX_LIGHTS];
	float4 globalAmbient;   // rgb = ambient color
	int numActiveLights;
	int3 _padLighting;
};

struct VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal   : NORMAL;
	float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 Position   : SV_POSITION;
	float3 WorldPos   : TEXCOORD0;
	float3 WorldNorm  : TEXCOORD1;
	float2 TexCoord   : TEXCOORD2;
	float  FogFactor  : TEXCOORD3;
	float2 ShadowCoord : TEXCOORD4;  // Shadow/projected texture coordinates
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	float4 worldPos = mul(float4(input.Position, 1.0f), matWorld);
	output.Position = mul(float4(input.Position, 1.0f), matWorldViewProj);
	output.WorldPos = worldPos.xyz;
	output.WorldNorm = normalize(mul((float3x3)matWorld, input.Normal));
	output.TexCoord = input.TexCoord;

	float3 shadowBias = float3(-0.447f, -0.258f, 0.894f) * 80.0f; // 80 units bias towards light
	float4 biasedWorldPos = float4(worldPos.xyz + shadowBias, 1.0f);
	float4 shadowCoord = mul(biasedWorldPos, matTexture1);
	// Perspective divide for projected texture coordinates
	if (abs(shadowCoord.w) > 0.0001f)
		output.ShadowCoord = shadowCoord.xy / shadowCoord.w;
	else
		output.ShadowCoord = shadowCoord.xy;

	// Height-based atmospheric fog
	if (vFogParams.w > 0.5f)
	{
		output.FogFactor = output.Position.z;
	}
	else
	{
		output.FogFactor = 1.0f;
	}

	return output;
}
