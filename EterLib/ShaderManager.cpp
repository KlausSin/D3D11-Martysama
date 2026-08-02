#include "StdAfx.h"
#include "ShaderManager.h"
#include "Camera.h"
#include "../eterBase/Debug.h"

// Thread-local storage for worker thread CB state

//////////////////////////////////////////////////////////////////////////
// HLSL Shader Code - UI Shader (Screen-space 2D)
//////////////////////////////////////////////////////////////////////////

static const char* g_szUIVertexShader = R"(
cbuffer CBPerFrame : register(b0)
{
	matrix matView;
	matrix matProjection;
	float4 vCameraPos;      // w = viewport width
	float4 vFogParams;      // z = viewport height
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
};

struct VS_INPUT
{
	float3 Position : POSITION;
	float4 Color    : COLOR0;
	float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 Position  : SV_POSITION;
	float4 Color     : COLOR0;
	float2 TexCoord  : TEXCOORD0;
	float2 MaskCoord : TEXCOORD1;  // For minimap circular mask
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	float4 worldPos = mul(float4(input.Position, 1.0f), matWorld);
	float4 viewPos = mul(worldPos, matView);
	float4 projPos = mul(viewPos, matProjection);

	bool usePixelTransform = (abs(matProjection._11 - 1.0f) < 0.001f && abs(matProjection._22 - 1.0f) < 0.001f);

	if (usePixelTransform)
	{
		float2 screenSize = float2(vCameraPos.w, vFogParams.z);
		if (screenSize.x < 1.0f) screenSize.x = 1024.0f;
		if (screenSize.y < 1.0f) screenSize.y = 768.0f;

		// Use world-transformed position, not raw input position
		output.Position.x = (worldPos.x / screenSize.x) * 2.0f - 1.0f;
		output.Position.y = 1.0f - (worldPos.y / screenSize.y) * 2.0f;
		output.Position.z = worldPos.z;
		output.Position.w = 1.0f;
	}
	else
	{
		output.Position = projPos;
	}

	output.Color = input.Color;

	output.TexCoord = input.TexCoord;

	output.MaskCoord = mul(float4(viewPos.xyz, 1.0f), matTexture1).xy;

	return output;
}
)";

static const char* g_szUIPixelShader = R"(
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
)";

static const char* g_szPBRFunctions = R"(
#define PI 3.14159265359f

float DistributionGGX(float NdotH, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float denom = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
	return a2 / (PI * denom * denom + 0.0001f);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = roughness + 1.0f;
	float k = (r * r) / 8.0f;
	return NdotV / (NdotV * (1.0f - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
	return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
	return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float3 EvalPBRLight(float3 L, float3 N, float3 V, float3 lightColor, float atten,
					float3 albedo, float roughness, float metallic, float3 F0)
{
	float3 H = normalize(V + L);
	float NdotL = max(dot(N, L), 0.0f);
	float NdotV = max(dot(N, V), 0.001f);
	float NdotH = max(dot(N, H), 0.0f);
	float HdotV = max(dot(H, V), 0.0f);

	float D = DistributionGGX(NdotH, roughness);
	float G = GeometrySmith(NdotV, NdotL, roughness);
	float3 F = FresnelSchlick(HdotV, F0);

	float3 numerator = D * G * F;
	float denominator = 4.0f * NdotV * NdotL + 0.0001f;
	float3 specular = numerator / denominator;

	float3 kS = F;
	float3 kD = (1.0f - kS) * (1.0f - metallic);

	return (kD * albedo * 0.5f + specular) * lightColor * NdotL * atten;
}
)";


static const char* g_szMeshVertexShader = R"(
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
		output.FogFactor = saturate((vFogParams.y - output.Position.z) / (vFogParams.y - vFogParams.x));
	}
	else
	{
		output.FogFactor = 1.0f;
	}

	return output;
}
)";

static const char* g_szMeshPixelShader = R"(
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
	float4 vShadowParams;     // x = opacity (0-120)
	float4 vCascadeSplits;    // x,y,z,w = view-space depth thresholds for cascades 0-3
	matrix matShadowBig;      // Cascade 3 (far)
	matrix matShadowLocal;    // Cascade 0 (near)
	matrix matShadowMid;      // Cascade 1
	matrix matShadowFar;      // Cascade 2
};

cbuffer CBPerObject : register(b1)
{
	matrix matWorld;
	matrix matWorldViewProj;
	matrix matTexture0;
	matrix matTexture1;
	float4 vDiffuseColor;
	float4 vSkyTint;
	float4 vMaterialParams;  // x = alphaRef, y = alphaTestEnabled, z = specularPower, w = twoTexBlend
	float4 vEmissiveColor;
	float4 vSpecularColor;
	float4 vPBRParams;
	float4 vRenderFlags;
	float4 vParticleColor;
};

struct Light
{
	float4 Position;        // xyz = position, w = type
	float4 Direction;       // xyz = direction, w = enabled
	float4 Color;           // rgb = color, a = intensity
	float4 Attenuation;     // x = constant, y = linear, z = quadratic, w = range
};

cbuffer CBLighting : register(b2)
{
	Light lights[MAX_LIGHTS];
	float4 globalAmbient;
	int numActiveLights;
	int3 _padLighting;
};

Texture2D    texDiffuse     : register(t0);
Texture2D    texDiffuse1    : register(t1);
Texture2D    texShadowBig   : register(t2);  // Cascade 3 (far)
Texture2D    texShadowLocal : register(t3);  // Cascade 0 (near)
Texture2D    texShadowMid   : register(t4);  // Cascade 1
Texture2D    texShadowFar   : register(t5);
Texture2D    texSphere      : register(t6);  // sphere map (moved off t1 so shadows cannot evict it)  // Cascade 2
SamplerState samLinear   : register(s0);
SamplerState samShadow   : register(s1);
SamplerComparisonState samShadowCmp : register(s2);   // hardware PCF

struct PS_INPUT
{
	float4 Position   : SV_POSITION;
	float3 WorldPos   : TEXCOORD0;
	float3 WorldNorm  : TEXCOORD1;
	float2 TexCoord   : TEXCOORD2;
	float  FogFactor  : TEXCOORD3;
	float2 ShadowCoord : TEXCOORD4;  // Shadow/projected texture coordinates
};

// 16-sample Poisson disk for smooth PCF shadow filtering
static const float2 pcfOffsets[16] = {
	float2( 0.0,     0.0),     float2(-0.94201, -0.39906),
	float2( 0.94558, -0.76890), float2(-0.09418, -0.92938),
	float2( 0.34495,  0.29387), float2(-0.91588, 0.45771),
	float2(-0.81544, -0.87912), float2( 0.19984,  0.78882),
	float2(-0.17330,  0.93028), float2( 0.78366, -0.15540),
	float2(-0.61390, -0.23740), float2( 0.44323,  0.74511),
	float2( 0.56071, -0.45678), float2(-0.38291,  0.37461),
	float2(-0.52117,  0.75145), float2( 0.30700, -0.79810)
};

// PCF shadow sampling — same as old proven code
float4 ShadowOffsetPos(float3 worldPos, float3 N, matrix matCascade, float texelScale)
{
	float fScale = length(float3(matCascade._11, matCascade._21, matCascade._31));
	float fWorldTexel = 2.0f * texelScale / max(fScale, 0.000001f);
	return float4(worldPos + N * fWorldTexel * 1.5f, 1.0f);
}

float SampleShadowPCF(Texture2D shadowMap, SamplerState samp,
                      float2 uv, float depth, float texelScale, float bias = 0.0005f)
{
	if (uv.x < 0.01f || uv.x > 0.99f || uv.y < 0.01f || uv.y > 0.99f)
		return 0.0f;
	const float d = depth - bias;
	float lit = 0.0f;
	[unroll] for (int y = -1; y <= 1; ++y)
		[unroll] for (int x = -1; x <= 1; ++x)
			lit += shadowMap.SampleCmpLevelZero(samShadowCmp, uv + float2(x, y) * texelScale, d);
	return 1.0f - (lit / 9.0f);
}

// Calculate attenuation for point/spot lights
float CalcAttenuation(float distance, float4 atten)
{
	if (distance > atten.w) return 0.0f;
	return 1.0f / (atten.x + atten.y * distance + atten.z * distance * distance);
}

// Calculate directional light contribution
float3 CalcDirectionalLight(Light light, float3 normal)
{
	float3 lightDir = light.Direction.xyz;
	float dirLen = length(lightDir);
	if (dirLen < 0.001f)
		return float3(0, 0, 0);

	lightDir /= dirLen;
	float NdotL = saturate(dot(normal, -lightDir));

	float3 lightColor = light.Color.rgb;
	float colorMag = dot(lightColor, lightColor);
	if (colorMag < 0.01f)
		lightColor = float3(1.0f, 1.0f, 1.0f);
	lightColor *= max(light.Color.a, 1.0f);

	return lightColor * NdotL;
}

float4 main(PS_INPUT input) : SV_TARGET
{
	if (vShadowParams.y > 0.001f)
		clip(input.WorldPos.z - vShadowParams.y);

	float4 texColor = texDiffuse.Sample(samLinear, input.TexCoord);

	// DX11 fix: unbound textures return (0,0,0,0)
	if (texColor.r < 0.001f && texColor.g < 0.001f && texColor.b < 0.001f && texColor.a < 0.001f)
	{
		discard;
	}

	// Alpha test
	if (vMaterialParams.y > 0.5f && texColor.a < vMaterialParams.x)
		discard;

	// Calculate lighting
	float3 normal = normalize(input.WorldNorm);
	float3 ambient = max(globalAmbient.rgb, float3(0.4f, 0.4f, 0.4f));

#ifdef PBR_ENABLED
	float3 V = normalize(vCameraPos.xyz - input.WorldPos);
	float3 albedo = texColor.rgb * vDiffuseColor.rgb;
	float roughness = vPBRParams.x;
	if (roughness < 0.01f) roughness = sqrt(2.0f / (max(vMaterialParams.z, 1.0f) + 2.0f));
	roughness = clamp(roughness, 0.04f, 1.0f);
	float metallic = vPBRParams.y;
	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
	float3 Lo = float3(0, 0, 0);

	// Directional light
	if (lights[0].Direction.w > 0.5f)
	{
		float3 L = normalize(-lights[0].Direction.xyz);
		float3 lc = lights[0].Color.rgb * max(lights[0].Color.a, 1.0f);
		float cm = dot(lc, lc);
		if (cm < 0.01f) lc = float3(1, 1, 1);
		Lo += EvalPBRLight(L, normal, V, lc, 1.0f, albedo, roughness, metallic, F0);
	}

	// Point/spot lights
	int maxLights = min(numActiveLights, MAX_LIGHTS);
	for (int i = 1; i < maxLights; ++i)
	{
		if (lights[i].Direction.w < 0.5f) continue;
		float lightType = lights[i].Position.w;
		float3 lightVec = lights[i].Position.xyz - input.WorldPos;
		float dist = length(lightVec);
		float3 L = lightVec / max(dist, 0.001f);
		float atten = CalcAttenuation(dist, lights[i].Attenuation);
		if (atten <= 0.0f) continue;
		if (lightType == LIGHT_SPOT)
		{
			float3 spotDir = normalize(lights[i].Direction.xyz);
			atten *= saturate((dot(-L, spotDir) - 0.5f) * 2.0f);
		}
		float3 lc = lights[i].Color.rgb * max(lights[i].Color.a, 1.0f);
		Lo += EvalPBRLight(L, normal, V, lc, atten, albedo, roughness, metallic, F0);
	}

	float3 lighting = ambient * albedo + Lo;
	float4 finalColor = float4(saturate(lighting), texColor.a * vDiffuseColor.a);
#else
	float3 diffuse = float3(0, 0, 0);

	// Check if light 0 is enabled
	if (lights[0].Direction.w > 0.5f)
	{
		diffuse = CalcDirectionalLight(lights[0], normal);
	}

	float3 lighting = saturate(ambient + diffuse);

	// Base color: texture * material diffuse * lighting
	float4 finalColor = float4(texColor.rgb * vDiffuseColor.rgb * lighting, texColor.a * vDiffuseColor.a);
#endif

	{
		float specMag = dot(vSpecularColor.rgb, vSpecularColor.rgb);
		if (specMag > 0.0001f)
		{
			float  specMask   = texColor.a;   // diffuse alpha = spec mask (stock Metin2 convention)
			float3 V          = normalize(vCameraPos.xyz - input.WorldPos);
			float3 viewNormal = normalize(mul((float3x3)matView, normal));
			float2 sphereUV   = viewNormal.xy * 0.5f + 0.5f;
			float3 env        = texSphere.Sample(samLinear, sphereUV).rgb;
			float3 specular   = env * vSpecularColor.rgb;

			if (lights[0].Direction.w > 0.5f)
			{
				float3 L = normalize(-lights[0].Direction.xyz);
				float NdotL = saturate(dot(normal, L));
				if (NdotL > 0.0f)
				{
					float3 H = normalize(L + V);
					float NdotH = saturate(dot(normal, H));
					float specPower = (vPBRParams.w > 0.0001f) ? vPBRParams.w : max(vSpecularColor.a, 16.0f);
					float3 sunColor = lights[0].Color.rgb * max(lights[0].Color.a, 1.0f);
					specular += sunColor * vSpecularColor.rgb * pow(NdotH, specPower) * NdotL;
				}
			}

			float specScale = (vPBRParams.z > 0.0001f) ? vPBRParams.z : 1.0f;
			finalColor.rgb += specular * specMask * specScale;
		}
	}

	if (vMaterialParams.w > 0.5f)
	{
		if (vShadowParams.x > 0.0f)
		{
			float4 worldPos4 = float4(input.WorldPos, 1.0f);
			float texelScale = (vShadowParams.z > 0.0f) ? vShadowParams.z : (1.0f / 2048.0f);
			float3 nrmWS = normalize(input.WorldNorm);
			float finalShadow = 0.0f;

			float4 sp0 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowLocal, texelScale), matShadowLocal);
			float2 uv0 = sp0.xy / sp0.w * 0.5f + 0.5f; uv0.y = 1.0f - uv0.y;
			float edge0 = min(min(uv0.x, 1.0f - uv0.x), min(uv0.y, 1.0f - uv0.y));

			if (edge0 > 0.05f)
			{
				float s0 = SampleShadowPCF(texShadowLocal, samShadow, uv0, sp0.z / sp0.w, texelScale);

				if (edge0 < 0.15f)
				{
					// Blend zone: mix with cascade 1
					float4 sp1 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowMid, texelScale), matShadowMid);
					float2 uv1 = sp1.xy / sp1.w * 0.5f + 0.5f; uv1.y = 1.0f - uv1.y;
					float s1 = SampleShadowPCF(texShadowMid, samShadow, uv1, sp1.z / sp1.w, texelScale, 0.001f);
					float blend = saturate((edge0 - 0.05f) / 0.10f);
					finalShadow = lerp(s1, s0, blend);
				}
				else
				{
					finalShadow = s0;
				}
			}
			else
			{
				float4 sp1 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowMid, texelScale), matShadowMid);
				float2 uv1 = sp1.xy / sp1.w * 0.5f + 0.5f; uv1.y = 1.0f - uv1.y;
				float edge1 = min(min(uv1.x, 1.0f - uv1.x), min(uv1.y, 1.0f - uv1.y));

				if (edge1 > 0.05f)
				{
					float s1 = SampleShadowPCF(texShadowMid, samShadow, uv1, sp1.z / sp1.w, texelScale, 0.001f);

					if (edge1 < 0.15f)
					{
						float4 sp2 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowFar, texelScale), matShadowFar);
						float2 uv2 = sp2.xy / sp2.w * 0.5f + 0.5f; uv2.y = 1.0f - uv2.y;
						float s2 = SampleShadowPCF(texShadowFar, samShadow, uv2, sp2.z / sp2.w, texelScale, 0.0015f);
						float blend = saturate((edge1 - 0.05f) / 0.10f);
						finalShadow = lerp(s2, s1, blend);
					}
					else
					{
						finalShadow = s1;
					}
				}
				else
				{
					float4 sp2 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowFar, texelScale), matShadowFar);
					float2 uv2 = sp2.xy / sp2.w * 0.5f + 0.5f; uv2.y = 1.0f - uv2.y;
					float edge2 = min(min(uv2.x, 1.0f - uv2.x), min(uv2.y, 1.0f - uv2.y));

					if (edge2 > 0.05f)
					{
						float s2 = SampleShadowPCF(texShadowFar, samShadow, uv2, sp2.z / sp2.w, texelScale, 0.0015f);

						if (edge2 < 0.15f)
						{
							float4 sp3 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowBig, texelScale), matShadowBig);
							float2 uv3 = sp3.xy / sp3.w * 0.5f + 0.5f; uv3.y = 1.0f - uv3.y;
							float s3 = SampleShadowPCF(texShadowBig, samShadow, uv3, sp3.z / sp3.w, texelScale, 0.0025f);
							float blend = saturate((edge2 - 0.05f) / 0.10f);
							finalShadow = lerp(s3, s2, blend);
						}
						else
						{
							finalShadow = s2;
						}
					}
					else
					{
						float4 sp3 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowBig, texelScale), matShadowBig);
						float2 uv3 = sp3.xy / sp3.w * 0.5f + 0.5f; uv3.y = 1.0f - uv3.y;
						finalShadow = SampleShadowPCF(texShadowBig, samShadow, uv3, sp3.z / sp3.w, texelScale, 0.0025f);
					}
				}
			}

			finalShadow *= saturate(vShadowParams.x / 120.0f);
			finalColor.rgb *= (1.0f - finalShadow);
		}
		else
		{
			// Legacy shadow map path: uses projected texture UV
			float2 shadowUV = input.ShadowCoord;
			if (shadowUV.x >= 0.0f && shadowUV.x <= 1.0f &&
			    shadowUV.y >= 0.0f && shadowUV.y <= 1.0f)
			{
				float fadeMargin = 0.1f;
				float edgeFade = saturate(shadowUV.x / fadeMargin)
				               * saturate((1.0f - shadowUV.x) / fadeMargin)
				               * saturate(shadowUV.y / fadeMargin)
				               * saturate((1.0f - shadowUV.y) / fadeMargin);

				float4 shadowColor = texDiffuse1.Sample(samShadow, shadowUV);
				float shadowLum = max(shadowColor.r, max(shadowColor.g, shadowColor.b));
				if (shadowLum > 0.1f)
				{
					float3 blendedShadow = lerp(float3(1,1,1), shadowColor.rgb, edgeFade);
					finalColor.rgb *= blendedShadow;
				}
			}
		}
	}

	// Height-based atmospheric fog
	finalColor.rgb = lerp(vFogColor.rgb, finalColor.rgb, input.FogFactor);

	return finalColor;
}
)";


static const char* g_szMesh2TexVertexShader = R"(
#define MAX_LIGHTS 16

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
};

struct VS_INPUT
{
	float3 Position  : POSITION;
	float3 Normal    : NORMAL;
	float2 TexCoord0 : TEXCOORD0;
	float2 TexCoord1 : TEXCOORD1;
};

struct VS_OUTPUT
{
	float4 Position   : SV_POSITION;
	float3 WorldPos   : TEXCOORD0;
	float3 WorldNorm  : TEXCOORD1;
	float2 TexCoord0  : TEXCOORD2;
	float2 TexCoord1  : TEXCOORD3;
	float  FogFactor  : TEXCOORD4;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	float4 worldPos = mul(float4(input.Position, 1.0f), matWorld);
	output.Position = mul(float4(input.Position, 1.0f), matWorldViewProj);
	output.WorldPos = worldPos.xyz;
	output.WorldNorm = normalize(mul((float3x3)matWorld, input.Normal));
	output.TexCoord0 = input.TexCoord0;
	output.TexCoord1 = input.TexCoord1;

	// Height-based atmospheric fog
	if (vFogParams.w > 0.5f)
	{
		output.FogFactor = saturate((vFogParams.y - output.Position.z) / (vFogParams.y - vFogParams.x));
	}
	else
	{
		output.FogFactor = 1.0f;
	}

	return output;
}
)";

static const char* g_szMesh2TexPixelShader = R"(
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

float CalcAttenuation(float distance, float4 atten)
{
	if (distance > atten.w) return 0.0f;
	return 1.0f / (atten.x + atten.y * distance + atten.z * distance * distance);
}

float4 main(PS_INPUT input) : SV_TARGET
{
	float4 texColor0 = texDiffuse0.Sample(samLinear, input.TexCoord0);
	float4 texColor1 = texDiffuse1.Sample(samLinear, input.TexCoord1);

	// DX11 fix: unbound textures return (0,0,0,0)
	// These pixels should be transparent - discard them
	if (texColor0.r < 0.001f && texColor0.g < 0.001f && texColor0.b < 0.001f && texColor0.a < 0.001f)
	{
		discard;
	}

	float2 tex1UV = input.TexCoord1;
	float edgeFade = 1.0f;
	float fadeStart = 0.02f;  // Start fading at 2% from edge

	// Check distance from edges and calculate falloff
	float distFromLeft = tex1UV.x;
	float distFromRight = 1.0f - tex1UV.x;
	float distFromBottom = tex1UV.y;
	float distFromTop = 1.0f - tex1UV.y;
	float minDistFromEdge = min(min(distFromLeft, distFromRight), min(distFromBottom, distFromTop));

	if (tex1UV.x < 0.0f || tex1UV.x > 1.0f || tex1UV.y < 0.0f || tex1UV.y > 1.0f)
	{
		edgeFade = 0.0f;  // Outside range - ignore second texture
	}
	else if (minDistFromEdge < fadeStart)
	{
		edgeFade = minDistFromEdge / fadeStart;  // Smooth fade near edge
	}

	float4 blendedTex;
	float tex1Lum = max(texColor1.r, max(texColor1.g, texColor1.b));
	if (tex1Lum > 0.3f && edgeFade > 0.0f)
	{
		// Fade the blend amount based on distance from edge
		float effectiveBlend = vMaterialParams.w * edgeFade;
		blendedTex = texColor0 * lerp(float4(1,1,1,1), texColor1, effectiveBlend);
	}
	else
		blendedTex = texColor0;


	// Alpha test
	if (vMaterialParams.y > 0.5f && blendedTex.a < vMaterialParams.x)
		discard;

	// Apply lighting
#ifdef PBR_ENABLED
	float3 normal = normalize(input.WorldNorm);
	float3 V = normalize(vCameraPos.xyz - input.WorldPos);
	float3 albedo = blendedTex.rgb * vDiffuseColor.rgb;
	float roughness = vPBRParams.x;
	if (roughness < 0.01f) roughness = 0.7f;
	roughness = clamp(roughness, 0.04f, 1.0f);
	float metallic = vPBRParams.y;
	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
	float3 ambient = max(globalAmbient.rgb, float3(0.4f, 0.4f, 0.4f));
	float3 Lo = float3(0, 0, 0);

	if (lights[0].Direction.w > 0.5f)
	{
		float3 L = normalize(-lights[0].Direction.xyz);
		float3 lc = lights[0].Color.rgb * max(lights[0].Color.a, 1.0f);
		Lo += EvalPBRLight(L, normal, V, lc, 1.0f, albedo, roughness, metallic, F0);
	}

	int maxLights = min(numActiveLights, MAX_LIGHTS);
	for (int i = 1; i < maxLights; ++i)
	{
		if (lights[i].Direction.w < 0.5f) continue;
		float3 lightVec = lights[i].Position.xyz - input.WorldPos;
		float dist = length(lightVec);
		float3 L = lightVec / max(dist, 0.001f);
		float atten = CalcAttenuation(dist, lights[i].Attenuation);
		if (atten <= 0.0f) continue;
		float3 lc = lights[i].Color.rgb * max(lights[i].Color.a, 1.0f);
		Lo += EvalPBRLight(L, normal, V, lc, atten, albedo, roughness, metallic, F0);
	}

	float4 finalColor = float4(saturate(ambient * albedo + Lo), blendedTex.a * vDiffuseColor.a);
#else
	// Apply material diffuse color
	float4 finalColor = blendedTex * vDiffuseColor;
#endif

	// Height-based atmospheric fog
	finalColor.rgb = lerp(vFogColor.rgb, finalColor.rgb, input.FogFactor);

	return finalColor;
}
)";


static const char* g_szTerrainVertexShader = R"(
#define MAX_LIGHTS 16

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
	matrix matTexture0;    // Texture transform for color texture
	matrix matTexture1;    // Texture transform for splat/alpha texture
	float4 vDiffuseColor;
	float4 vSkyTint;
	float4 vMaterialParams;
	float4 vEmissiveColor;
	float4 vSpecularColor;
	float4 vPBRParams;
	float4 vRenderFlags;
	float4 vParticleColor;
};

struct Light
{
	float4 Position;     // xyz = position, w = type
	float4 Direction;    // xyz = direction, w = enabled
	float4 Color;        // rgb = color, a = intensity
	float4 Attenuation;  // x = const, y = linear, z = quadratic, w = range
};

cbuffer CBLighting : register(b2)
{
	Light lights[MAX_LIGHTS];
	float4 globalAmbient;
	int numActiveLights;
	int3 _padLighting;
};

struct VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal   : NORMAL;
};

struct VS_OUTPUT
{
	float4 Position   : SV_POSITION;
	float3 WorldPos   : TEXCOORD0;
	float3 WorldNorm  : TEXCOORD1;
	float2 TexCoord0  : TEXCOORD2;  // Color texture coords
	float2 TexCoord1  : TEXCOORD3;  // Splat/alpha texture coords
	float  FogFactor  : TEXCOORD4;
	float3 Diffuse    : TEXCOORD5;  // Per-vertex lighting result
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	float4 worldPos = mul(float4(input.Position, 1.0f), matWorld);
	output.Position = mul(float4(input.Position, 1.0f), matWorldViewProj);
	output.WorldPos = worldPos.xyz;

	// Transform normal - for terrain, normal is in world space
	float3 worldNormal = normalize(mul((float3x3)matWorld, input.Normal));
	output.WorldNorm = worldNormal;

	bool shadowMode = (vMaterialParams.z > 0.5f);

	if (shadowMode)
	{
		float4 texCoord0 = mul(worldPos, matTexture0);
		float4 texCoord1 = mul(worldPos, matTexture1);

		// Apply perspective divide if needed
		if (abs(texCoord0.w) > 0.0001f)
			output.TexCoord0 = texCoord0.xy / texCoord0.w;
		else
			output.TexCoord0 = texCoord0.xy;

		if (abs(texCoord1.w) > 0.0001f)
			output.TexCoord1 = texCoord1.xy / texCoord1.w;
		else
			output.TexCoord1 = texCoord1.xy;
	}
	else
	{
		float tileScaleX = vMaterialParams.x;
		float tileScaleY = vMaterialParams.y;

		// If no scale set, use default terrain tiling (1/640)
		if (abs(tileScaleX) < 0.0001f) tileScaleX = 0.0015625f;
		if (abs(tileScaleY) < 0.0001f) tileScaleY = -0.0015625f;

		output.TexCoord0 = float2(worldPos.x * tileScaleX, worldPos.y * tileScaleY);

		// Splat alpha coords: transform position by matTexture1
		float4 texCoord1 = mul(worldPos, matTexture1);
		output.TexCoord1 = texCoord1.xy;
	}

	output.Diffuse = max(globalAmbient.rgb, float3(0.4f, 0.4f, 0.4f));

	// Height-based atmospheric fog
	if (vFogParams.w > 0.5f)
	{
		output.FogFactor = saturate((vFogParams.y - output.Position.z) / (vFogParams.y - vFogParams.x));
	}
	else
	{
		output.FogFactor = 1.0f;
	}

	return output;
}
)";

static const char* g_szTerrainPixelShader = R"(
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
	float4 vShadowParams;     // x = opacity (0-120)
	float4 vCascadeSplits;    // x,y,z,w = view-space depth thresholds for cascades 0-3
	matrix matShadowBig;      // Cascade 3 (far)
	matrix matShadowLocal;    // Cascade 0 (near)
	matrix matShadowMid;      // Cascade 1
	matrix matShadowFar;      // Cascade 2
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

Texture2D    texColor       : register(t0);
Texture2D    texSplat       : register(t1);
Texture2D    texShadowBig   : register(t2);  // Cascade 3 (far)
Texture2D    texShadowLocal : register(t3);  // Cascade 0 (near)
Texture2D    texShadowMid   : register(t4);  // Cascade 1
Texture2D    texShadowFar   : register(t5);
Texture2D    texSphere      : register(t6);  // sphere map (moved off t1 so shadows cannot evict it)  // Cascade 2
SamplerState samLinear : register(s0);
SamplerState samClamp  : register(s1);
SamplerComparisonState samShadowCmp : register(s2);   // hardware PCF

struct PS_INPUT
{
	float4 Position   : SV_POSITION;
	float3 WorldPos   : TEXCOORD0;
	float3 WorldNorm  : TEXCOORD1;
	float2 TexCoord0  : TEXCOORD2;  // Color texture coords OR static shadow coords
	float2 TexCoord1  : TEXCOORD3;  // Splat/alpha coords OR dynamic shadow coords
	float  FogFactor  : TEXCOORD4;
	float3 Diffuse    : TEXCOORD5;  // Per-vertex lighting from VS
};

// 16-sample Poisson disk for smooth PCF shadow filtering
static const float2 pcfOffsets[16] = {
	float2( 0.0,     0.0),     float2(-0.94201, -0.39906),
	float2( 0.94558, -0.76890), float2(-0.09418, -0.92938),
	float2( 0.34495,  0.29387), float2(-0.91588, 0.45771),
	float2(-0.81544, -0.87912), float2( 0.19984,  0.78882),
	float2(-0.17330,  0.93028), float2( 0.78366, -0.15540),
	float2(-0.61390, -0.23740), float2( 0.44323,  0.74511),
	float2( 0.56071, -0.45678), float2(-0.38291,  0.37461),
	float2(-0.52117,  0.75145), float2( 0.30700, -0.79810)
};

float4 ShadowOffsetPos(float3 worldPos, float3 N, matrix matCascade, float texelScale)
{
	float fScale = length(float3(matCascade._11, matCascade._21, matCascade._31));
	float fWorldTexel = 2.0f * texelScale / max(fScale, 0.000001f);
	return float4(worldPos + N * fWorldTexel * 1.5f, 1.0f);
}

float SampleShadowPCF(Texture2D shadowMap, SamplerState samp,
                      float2 uv, float depth, float texelScale, float bias = 0.0005f)
{
	if (uv.x < 0.01f || uv.x > 0.99f || uv.y < 0.01f || uv.y > 0.99f)
		return 0.0f;
	const float d = depth - bias;
	float lit = 0.0f;
	[unroll] for (int y = -1; y <= 1; ++y)
		[unroll] for (int x = -1; x <= 1; ++x)
			lit += shadowMap.SampleCmpLevelZero(samShadowCmp, uv + float2(x, y) * texelScale, d);
	return 1.0f - (lit / 9.0f);
}

float CalcAttenuation(float distance, float4 atten)
{
	if (distance > atten.w) return 0.0f;
	return 1.0f / (atten.x + atten.y * distance + atten.z * distance * distance);
}

float4 main(PS_INPUT input) : SV_TARGET
{
	if (vShadowParams.y > 0.001f)
		clip(input.WorldPos.z - vShadowParams.y);

	if (vMaterialParams.z > 0.5f)
	{
		float3 shadowColor = float3(1.0f, 1.0f, 1.0f);

		if (vShadowParams.x > 0.0f)
		{
			float4 worldPos4 = float4(input.WorldPos, 1.0f);
			float texelScale = (vShadowParams.z > 0.0f) ? vShadowParams.z : (1.0f / 2048.0f);
			float3 nrmWS = normalize(input.WorldNorm);
			float shadow = 0.0f;

			float4 sp0 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowLocal, texelScale), matShadowLocal);
			float2 uv0 = sp0.xy / sp0.w * 0.5f + 0.5f; uv0.y = 1.0f - uv0.y;
			float edge0 = min(min(uv0.x, 1.0f - uv0.x), min(uv0.y, 1.0f - uv0.y));

			if (edge0 > 0.05f)
			{
				float s0 = SampleShadowPCF(texShadowLocal, samClamp, uv0, sp0.z / sp0.w, texelScale);
				if (edge0 < 0.15f)
				{
					float4 sp1 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowMid, texelScale), matShadowMid);
					float2 uv1 = sp1.xy / sp1.w * 0.5f + 0.5f; uv1.y = 1.0f - uv1.y;
					float s1 = SampleShadowPCF(texShadowMid, samClamp, uv1, sp1.z / sp1.w, texelScale, 0.001f);
					shadow = lerp(s1, s0, saturate((edge0 - 0.05f) / 0.10f));
				}
				else { shadow = s0; }
			}
			else
			{
				float4 sp1 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowMid, texelScale), matShadowMid);
				float2 uv1 = sp1.xy / sp1.w * 0.5f + 0.5f; uv1.y = 1.0f - uv1.y;
				float edge1 = min(min(uv1.x, 1.0f - uv1.x), min(uv1.y, 1.0f - uv1.y));

				if (edge1 > 0.05f)
				{
					float s1 = SampleShadowPCF(texShadowMid, samClamp, uv1, sp1.z / sp1.w, texelScale, 0.001f);
					if (edge1 < 0.15f)
					{
						float4 sp2 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowFar, texelScale), matShadowFar);
						float2 uv2 = sp2.xy / sp2.w * 0.5f + 0.5f; uv2.y = 1.0f - uv2.y;
						float s2 = SampleShadowPCF(texShadowFar, samClamp, uv2, sp2.z / sp2.w, texelScale, 0.0015f);
						shadow = lerp(s2, s1, saturate((edge1 - 0.05f) / 0.10f));
					}
					else { shadow = s1; }
				}
				else
				{
					float4 sp2 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowFar, texelScale), matShadowFar);
					float2 uv2 = sp2.xy / sp2.w * 0.5f + 0.5f; uv2.y = 1.0f - uv2.y;
					float edge2 = min(min(uv2.x, 1.0f - uv2.x), min(uv2.y, 1.0f - uv2.y));

					if (edge2 > 0.05f)
					{
						float s2 = SampleShadowPCF(texShadowFar, samClamp, uv2, sp2.z / sp2.w, texelScale, 0.0015f);
						if (edge2 < 0.15f)
						{
							float4 sp3 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowBig, texelScale), matShadowBig);
							float2 uv3 = sp3.xy / sp3.w * 0.5f + 0.5f; uv3.y = 1.0f - uv3.y;
							float s3 = SampleShadowPCF(texShadowBig, samClamp, uv3, sp3.z / sp3.w, texelScale, 0.0025f);
							shadow = lerp(s3, s2, saturate((edge2 - 0.05f) / 0.10f));
						}
						else { shadow = s2; }
					}
					else
					{
						float4 sp3 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowBig, texelScale), matShadowBig);
						float2 uv3 = sp3.xy / sp3.w * 0.5f + 0.5f; uv3.y = 1.0f - uv3.y;
						shadow = SampleShadowPCF(texShadowBig, samClamp, uv3, sp3.z / sp3.w, texelScale, 0.0025f);
					}
				}
			}

			float opacity = saturate(vShadowParams.x / 120.0f);
			shadowColor = float3(1.0f - shadow * opacity, 1.0f - shadow * opacity, 1.0f - shadow * opacity);
		}
		else
		{
			// Legacy shadow mode - sample from texSplat
			float2 dynShadowUV = input.TexCoord1;
			bool validDynamicUV = (dynShadowUV.x >= 0.0f && dynShadowUV.x <= 1.0f &&
			                       dynShadowUV.y >= 0.0f && dynShadowUV.y <= 1.0f);

			if (validDynamicUV)
			{
				float fadeMargin = 0.30f;
				float fadeFromLeft = saturate(dynShadowUV.x / fadeMargin);
				float fadeFromRight = saturate((1.0f - dynShadowUV.x) / fadeMargin);
				float fadeFromTop = saturate(dynShadowUV.y / fadeMargin);
				float fadeFromBottom = saturate((1.0f - dynShadowUV.y) / fadeMargin);
				float edgeFade = min(min(fadeFromLeft, fadeFromRight), min(fadeFromTop, fadeFromBottom));
				edgeFade = smoothstep(0.0f, 1.0f, edgeFade);

				// Sample dynamic shadow map
				float4 dynamicShadow = texSplat.Sample(samClamp, dynShadowUV);
				shadowColor = lerp(float3(1.0f, 1.0f, 1.0f), dynamicShadow.rgb, edgeFade);
			}
		}

		return float4(shadowColor, 1.0f);
	}

	// Normal terrain rendering mode
	// Sample terrain color texture
	float4 colorTex = texColor.Sample(samLinear, input.TexCoord0);

	float2 splatUV = clamp(input.TexCoord1, 0.001f, 0.999f);
	float4 splatTex = texSplat.Sample(samClamp, splatUV);

	// Use only the alpha channel from splat texture
	float splatAlpha = splatTex.a;
	float4 finalTexColor = float4(colorTex.rgb, splatAlpha);


	// ========== PER-PIXEL LIGHTING ==========
	float3 N = normalize(input.WorldNorm);
	float3 V = normalize(vCameraPos.xyz - input.WorldPos);

	// Start with ambient (passed from vertex shader)
	float3 ambient = input.Diffuse;

#ifdef PBR_ENABLED
	float3 albedo = finalTexColor.rgb * vDiffuseColor.rgb;
	float roughness = vPBRParams.x;
	if (roughness < 0.01f) roughness = 0.85f;
	roughness = clamp(roughness, 0.04f, 1.0f);
	float metallic = vPBRParams.y;
	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
	float3 Lo = float3(0, 0, 0);

	if (lights[0].Direction.w > 0.5f)
	{
		float3 L = normalize(-lights[0].Direction.xyz);
		float3 lc = lights[0].Color.rgb * max(lights[0].Color.a, 1.0f);
		Lo += EvalPBRLight(L, N, V, lc, 1.0f, albedo, roughness, metallic, F0);
	}

	int maxLights = min(numActiveLights, MAX_LIGHTS);
	for (int i = 1; i < maxLights; ++i)
	{
		if (lights[i].Direction.w < 0.5f) continue;
		float lightType = lights[i].Position.w;
		float3 lightVec = lights[i].Position.xyz - input.WorldPos;
		float dist = length(lightVec);
		float3 L = lightVec / max(dist, 0.001f);
		float atten = CalcAttenuation(dist, lights[i].Attenuation);
		if (atten <= 0.0f) continue;
		if (lightType == LIGHT_SPOT)
		{
			float3 spotDir = normalize(lights[i].Direction.xyz);
			atten *= saturate((dot(-L, spotDir) - 0.5f) * 2.0f);
		}
		float3 lc = lights[i].Color.rgb * max(lights[i].Color.a, 1.0f);
		Lo += EvalPBRLight(L, N, V, lc, atten, albedo, roughness, metallic, F0);
	}

	float4 finalColor = float4(saturate(ambient * albedo + Lo), finalTexColor.a * vDiffuseColor.a);
#else
	float3 diffuse = float3(0.0f, 0.0f, 0.0f);
	float3 specular = float3(0.0f, 0.0f, 0.0f);

	// Specular parameters from material
	float specPower = max(vSpecularColor.a, 16.0f);
	float3 specColor = vSpecularColor.rgb;

	// Directional light (sun) - lights[0]
	if (lights[0].Direction.w > 0.5f)
	{
		float3 L = normalize(-lights[0].Direction.xyz);
		float NdotL = saturate(dot(N, L));
		float3 lightColor = lights[0].Color.rgb * max(lights[0].Color.a, 1.0f);

		diffuse += lightColor * NdotL;

		if (NdotL > 0.0f)
		{
			float3 H = normalize(L + V);
			float NdotH = saturate(dot(N, H));
			specular += lightColor * specColor * pow(NdotH, specPower);
		}
	}

	// Point/spot lights
	int maxLights = min(numActiveLights, MAX_LIGHTS);
	for (int i = 1; i < maxLights; ++i)
	{
		if (lights[i].Direction.w < 0.5f) continue;

		float lightType = lights[i].Position.w;
		float3 lightVec = lights[i].Position.xyz - input.WorldPos;
		float dist = length(lightVec);
		float3 L = lightVec / max(dist, 0.001f);
		float atten = CalcAttenuation(dist, lights[i].Attenuation);
		if (atten <= 0.0f) continue;

		if (lightType == LIGHT_SPOT)
		{
			float3 spotDir = normalize(lights[i].Direction.xyz);
			atten *= saturate((dot(-L, spotDir) - 0.5f) * 2.0f);
		}

		float NdotL = saturate(dot(N, L));
		float3 lightColor = lights[i].Color.rgb * max(lights[i].Color.a, 1.0f);
		diffuse += lightColor * NdotL * atten;

		if (NdotL > 0.0f)
		{
			float3 H = normalize(L + V);
			float NdotH = saturate(dot(N, H));
			specular += lightColor * specColor * pow(NdotH, specPower) * atten;
		}
	}

	float3 finalDiffuse = (ambient + diffuse) * vDiffuseColor.rgb;
	float3 finalLighting = saturate(finalDiffuse + specular);
	float outAlpha = finalTexColor.a * vDiffuseColor.a;
	float4 finalColor = float4(finalTexColor.rgb * finalLighting, outAlpha);
#endif

	// Height-based atmospheric fog
	finalColor.rgb = lerp(vFogColor.rgb, finalColor.rgb, input.FogFactor);


	return finalColor;
}
)";

//////////////////////////////////////////////////////////////////////////
// HLSL Shader Code - Tessellated Terrain Shader
// Uses Hull/Domain shaders for GPU-driven LOD
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// HLSL Shader Code - Water Shader
//////////////////////////////////////////////////////////////////////////

static const char* g_szWaterVertexShader = R"(
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
};

#ifdef WATER_REFLECTION_ENABLED
cbuffer CBWater : register(b5)
{
	float4 vWaterParams_VS;
	float4 vWaterColor_VS;
	float4 vScreenParams_VS;
	matrix matReflectVP;
};
#endif

struct VS_INPUT
{
	float3 Position : POSITION;
	float4 Color    : COLOR0;
};

struct VS_OUTPUT
{
	float4 Position  : SV_POSITION;
	float4 Color     : COLOR0;
	float2 TexCoord  : TEXCOORD0;
	float3 WorldPos  : TEXCOORD1;
	float4 ClipPos   : TEXCOORD2;
	float  FogFactor : TEXCOORD3;
#ifdef WATER_REFLECTION_ENABLED
	float4 ReflProj  : TEXCOORD4;
#endif
};

float waterNoise(float2 p)
{
	return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	float3 displacedPos = input.Position;

	float4 worldPos = mul(float4(displacedPos, 1.0f), matWorld);
	output.Position = mul(float4(displacedPos, 1.0f), matWorldViewProj);
	output.WorldPos = worldPos.xyz;
	output.ClipPos  = output.Position;

	output.Color = input.Color;

	// Water texture coordinates based on world position
	float scaleU = matTexture0._11;
	float scaleV = matTexture0._22;
	if (abs(scaleU) < 0.00001f) scaleU = 0.0003f;
	if (abs(scaleV) < 0.00001f) scaleV = -0.0003f;

	output.TexCoord = float2(worldPos.x * scaleU, worldPos.y * scaleV);

#ifdef WATER_REFLECTION_ENABLED
	output.ReflProj = mul(worldPos, matReflectVP);
#endif

	// Height-based atmospheric fog
	if (vFogParams.w > 0.5f)
	{
		output.FogFactor = saturate((vFogParams.y - output.Position.z) / (vFogParams.y - vFogParams.x));
	}
	else
	{
		output.FogFactor = 1.0f;
	}

	return output;
}
)";

static const char* g_szWaterPixelShader = R"(
cbuffer CBPerFrame : register(b0)
{
	matrix matView;
	matrix matProjection;
	float4 vCameraPos;
	float4 vFogParams;
	float4 vFogColor;
	float4 vTime;
	float4 vSunDirection;
	// Cascaded shadow parameters — must match the C++ CBPerFrame layout.
	float4 vShadowParams;     // x = opacity (0-120)
	float4 vCascadeSplits;    // unused by water (single-cascade blend)
	matrix matShadowBig;      // Cascade 3 (far)
	matrix matShadowLocal;    // Cascade 0 (near)
	matrix matShadowMid;      // Cascade 1 (unused)
	matrix matShadowFar;      // Cascade 2 (unused)
};

#ifdef WATER_REFLECTION_ENABLED
cbuffer CBWater : register(b5)
{
	float4 vWaterParams;   // x = normalScale, y = refrDistortion, z = specPower, w = specIntensity
	float4 vWaterColor;    // rgb = deep water tint, a = tint strength
	float4 vScreenParams;  // x = 1/width, y = 1/height, z = time, w = reflDistortion
	matrix matReflectVP;
};
#endif

Texture2D    texDiffuse    : register(t0);
SamplerState samLinear     : register(s0);
SamplerState samClamp      : register(s1);

#ifdef WATER_REFLECTION_ENABLED
Texture2D    texNormal     : register(t4);
Texture2D    texReflection : register(t5);
Texture2D    texRefraction : register(t6);
Texture2D    texFoam       : register(t7);
Texture2D    texNormal2    : register(t8);
Texture2D    texMarginFoam : register(t9);
Texture2D    texWaterShadowLocal : register(t10);  // near cascade (Cascade 0)
Texture2D    texWaterShadowBig   : register(t11);  // far cascade (Cascade 3)
#endif

static const float2 waterPcfOffsets[16] = {
	float2( 0.0,     0.0),     float2(-0.94201, -0.39906),
	float2( 0.94558, -0.76890), float2(-0.09418, -0.92938),
	float2( 0.34495,  0.29387), float2(-0.91588, 0.45771),
	float2(-0.81544, -0.87912), float2( 0.19984,  0.78882),
	float2(-0.17330,  0.93028), float2( 0.78366, -0.15540),
	float2(-0.61390, -0.23740), float2( 0.44323,  0.74511),
	float2( 0.56071, -0.45678), float2(-0.38291,  0.37461),
	float2(-0.52117,  0.75145), float2( 0.30700, -0.79810)
};

float SampleWaterShadowPCF(Texture2D shadowMap, SamplerState samp,
                           float2 uv, float depth, float texelScale)
{
	if (uv.x < 0.02 || uv.x > 0.98 || uv.y < 0.02 || uv.y > 0.98)
		return 0.0;
	float shadow = 0.0;
	float scale  = texelScale * 2.0;
	[unroll] for (int i = 0; i < 16; ++i) {
		float sampled = shadowMap.Sample(samp, uv + waterPcfOffsets[i] * scale).r;
		shadow += (depth - 0.0015 > sampled) ? 1.0 : 0.0;
	}
	return shadow / 16.0;
}

struct PS_INPUT
{
	float4 Position  : SV_POSITION;
	float4 Color     : COLOR0;
	float2 TexCoord  : TEXCOORD0;
	float3 WorldPos  : TEXCOORD1;
	float4 ClipPos   : TEXCOORD2;
	float  FogFactor : TEXCOORD3;
#ifdef WATER_REFLECTION_ENABLED
	float4 ReflProj  : TEXCOORD4;
#endif
};

float4 main(PS_INPUT input) : SV_TARGET
{
#ifdef WATER_REFLECTION_ENABLED

	float t = vScreenParams.z;   // seconds
	float2 p = input.WorldPos.xy;
	float2 p37  = float2(p.x *  0.7986 - p.y *  0.6018, p.x *  0.6018 + p.y *  0.7986);
	float2 p73  = float2(p.x *  0.2924 - p.y *  0.9563, p.x *  0.9563 + p.y *  0.2924);
	float2 p113 = float2(p.x * -0.3907 - p.y *  0.9205, p.x *  0.9205 + p.y * -0.3907);
	float2 uv1a = p    * 0.0012 + t * float2( 0.045,  0.025);
	float2 uv1b = p37  * 0.0043 + t * float2(-0.020,  0.030);
	float2 uv2a = p73  * 0.0021 + t * float2(-0.035,  0.055);
	float2 uv2b = p113 * 0.0085 + t * float2( 0.025, -0.015);

	float3 s1 = ((texNormal.Sample (samLinear, uv1a).xyz - 0.5) * 0.62 +
	             (texNormal.Sample (samLinear, uv1b).xyz - 0.5) * 0.38) * 5.0;
	float3 s2 = ((texNormal2.Sample(samLinear, uv2a).xyz - 0.5) * 0.62 +
	             (texNormal2.Sample(samLinear, uv2b).xyz - 0.5) * 0.38) * 5.0;
	float2 ripple = s1.xy + s2.xy;
	float3 normal = normalize(float3(ripple, 1.45));

	float3 viewDir = normalize(vCameraPos.xyz - input.WorldPos);

	float rpw = max(abs(input.ReflProj.w), 0.001);
	float2 reflUV = (input.ReflProj.xy / rpw) * float2(0.5, -0.5) + 0.5;
	reflUV += normal.xy * 0.015;
	reflUV = clamp(reflUV, 0.005, 0.995);
	float3 reflRTSample = texReflection.Sample(samClamp, reflUV).rgb;
	float3 reflColor = reflRTSample;

	// Lighting
	float3 toSun = -vSunDirection.xyz;
	float NdotL = saturate(dot(normal, toSun));

	float3 ambient  = float3(0.55, 0.55, 0.55);
	float3 diffuse  = float3(0.55, 0.55, 0.55) * NdotL;

	float3 reflectDir = reflect(-toSun, normal);
	float  VdotR      = saturate(dot(viewDir, reflectDir));
	float  specCore  = pow(VdotR, 220.0) * 5.0;
	float  specMed   = pow(VdotR,  50.0) * 1.2;
	float  specBroad = pow(VdotR,   8.0) * 0.35;
	float3 sunColor  = float3(1.0, 0.92, 0.72);    // warm sun tint
	float3 specular  = (specCore + specMed + specBroad) * sunColor;

	float3 LightResult = ambient + diffuse + specular;


	float3 tintedReflection = reflColor * LightResult;

	// Sun brightness boost — modest so texture and reflection detail
	// remain visible. Reference uses 3.3× but with different base tones;
	// at our tuning 1.8× keeps water well-lit without saturating to white.
	float sunBoost = 1.0 + 0.8 * NdotL;
	tintedReflection *= sunBoost;

	float sunUp = saturate(toSun.z);
	float3 waterBaseDay   = float3(0.12, 0.34, 0.56);
	float3 waterBaseNight = float3(0.04, 0.12, 0.22);
	float3 waterBase = lerp(waterBaseNight, waterBaseDay, sunUp);

	float3 waterColor = lerp(waterBase, tintedReflection, 0.58);

	float waveTilt  = 1.0 - abs(normal.z);                        // 0 = pointing straight, 1 = sideways
	float waveShade = lerp(1.0, 0.65, saturate(waveTilt));        // tilted pixels = darker troughs
	float waveKick  = saturate(normal.x + normal.y * 0.5) * 0.30; // asymmetric crest highlights
	waterColor      *= (waveShade + waveKick);

	float2 foamUVa = input.WorldPos.xy * 0.0009 + t * float2( 0.018,  0.012);
	float2 foamUVb = input.WorldPos.xy * 0.0023 + t * float2(-0.010,  0.022);
	foamUVa += normal.xy * 0.04;
	foamUVb += normal.xy * 0.02;
	float4 foamA = texFoam.Sample(samLinear, foamUVa);
	float4 foamB = texFoam.Sample(samLinear, foamUVb);
	float  foamIntA = dot(foamA.rgb, float3(0.30, 0.59, 0.11));
	float  foamIntB = dot(foamB.rgb, float3(0.30, 0.59, 0.11));
	float  foamCoverage = max(foamIntA, foamIntB);
	float3 foamTint    = float3(0.78, 0.78, 0.78);
	float3 foamRGB     = foamTint * foamCoverage;

	float shoreFoam  = 1.0 - smoothstep(0.10, 1.00, input.Color.a);
	float foamWeight = 0.15 + shoreFoam * 0.55;   // deep=0.15, shore~0.70
	float foamAmount = foamCoverage * foamWeight;
	waterColor = lerp(waterColor, foamRGB, foamAmount);

	float2 marginUVa = input.WorldPos.xy * 0.0019 + t * float2( 0.028,  0.018);
	float2 marginUVb = input.WorldPos.xy * 0.0047 + t * float2(-0.020,  0.030);
	marginUVa += normal.xy * 0.025;
	marginUVb += normal.xy * 0.015;
	float4 marginA = texMarginFoam.Sample(samLinear, marginUVa);
	float4 marginB = texMarginFoam.Sample(samLinear, marginUVb);
	float  marginIntA = dot(marginA.rgb, float3(0.30, 0.59, 0.11));
	float  marginIntB = dot(marginB.rgb, float3(0.30, 0.59, 0.11));
	float  marginCoverage = max(marginIntA, marginIntB);
	float3 marginTint    = float3(0.82, 0.82, 0.82);
	float3 marginRGB     = marginTint * marginCoverage;

	float  marginFade    = 1.0 - smoothstep(0.02, 0.95, input.Color.a);
	float  marginAmount  = marginCoverage * marginFade * 0.20;
	float3 marginLit     = marginRGB * float3(1.10, 1.08, 1.05);
	waterColor  = waterColor * (1.0 - marginAmount * 0.30);
	waterColor += marginLit * marginAmount;

	const float maxOpacity = 0.85;
	float waterDiffuse = saturate(input.Color.a * 6.0) * maxOpacity;
	waterDiffuse *= (1.0 - saturate(marginAmount) * 0.30);
	return float4(waterColor, waterDiffuse);
#else
	// Legacy path: simple textured water
	float4 texColor = texDiffuse.Sample(samLinear, input.TexCoord);
	float4 finalColor;
	finalColor.rgb = texColor.rgb;
	finalColor.a = input.Color.a;
	finalColor.rgb = lerp(vFogColor.rgb, finalColor.rgb, input.FogFactor);
	return finalColor;
#endif
}
)";


static const char* g_szSkyVertexShader = R"(
cbuffer CBPerFrame : register(b0)
{
	matrix matView;
	matrix matProjection;
	float4 vCameraPos;
	float4 vFogParams;
	float4 vFogColor;
	float4 vTime;           // x = total time, y = delta, z = layer2 speed mult
	float4 vSunDirection;   // xyz = sun dir, w = intensity
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
};

struct VS_INPUT
{
	float3 Position : POSITION;
	float4 Color    : COLOR0;
	float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 Position  : SV_POSITION;
	float4 Color     : COLOR0;
	float2 TexCoord  : TEXCOORD0;
	float2 TexCoord2 : TEXCOORD1;  // Second cloud layer UV
	float3 WorldPos  : TEXCOORD2;  // For sun lighting calculation
	float2 ScreenPos : TEXCOORD3;  // For sun glow effect
	float  Height    : TEXCOORD4;  // Normalized gradient height [0=top, 1=bottom]
	float2 LocalPos  : TEXCOORD5;  // Object-space XY for cloud quad edge fade
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	float4 worldPos = mul(float4(input.Position, 1.0f), matWorld);
	output.Position = mul(float4(input.Position, 1.0f), matWorldViewProj);
	output.WorldPos = worldPos.xyz;
	output.Color = input.Color * vDiffuseColor * vSkyTint;

	output.Height = (1.0f - input.Position.z) * 0.5f;

	// Primary cloud layer UV with texture matrix scrolling
	float4 transformedTC = mul(float4(input.TexCoord, 0.0f, 1.0f), matTexture0);
	output.TexCoord = transformedTC.xy;

	float layer2SpeedMult = vTime.z > 0.01f ? vTime.z : 0.5f;
	float4 tc2 = float4(input.TexCoord * 0.7f, 0.0f, 1.0f);  // Smaller scale
	tc2.x += vTime.x * 0.02f * layer2SpeedMult;  // Slower scroll
	tc2.y += vTime.x * 0.015f * layer2SpeedMult;
	output.TexCoord2 = tc2.xy;

	// Screen position for sun glow
	output.ScreenPos = output.Position.xy / output.Position.w;

	output.LocalPos = input.Position.xy;

	return output;
}
)";

static const char* g_szSkyPixelShader = R"(
cbuffer CBPerFrame : register(b0)
{
	matrix matView;
	matrix matProjection;
	float4 vCameraPos;
	float4 vFogParams;
	float4 vFogColor;
	float4 vTime;           // x = total time, y = delta, z = layer2 speed mult
	float4 vSunDirection;   // xyz = sun dir, w = intensity
};

cbuffer CBPerObject : register(b1)
{
	matrix matWorld;
	matrix matWorldViewProj;
	matrix matTexture0;
	matrix matTexture1;
	float4 vDiffuseColor;
	float4 vSkyTint;
	float4 vMaterialParams;   // w > 0.5 = use texture (clouds), else skybox gradient
	float4 vEmissiveColor;
	float4 vSpecularColor;
	float4 vPBRParams;
	float4 vRenderFlags;
	float4 vParticleColor;
};

cbuffer CBSkyGradient : register(b2)
{
	float4 skyColors[8];
	int    skyColorCount;
	int    skyUpperSegments;
	float2 skyPad;
};

Texture2D    texDiffuse : register(t0);
SamplerState samLinear  : register(s0);

struct PS_INPUT
{
	float4 Position  : SV_POSITION;
	float4 Color     : COLOR0;
	float2 TexCoord  : TEXCOORD0;
	float2 TexCoord2 : TEXCOORD1;
	float3 WorldPos  : TEXCOORD2;
	float2 ScreenPos : TEXCOORD3;
	float  Height    : TEXCOORD4;
	float2 LocalPos  : TEXCOORD5;
};

// Procedural noise for organic cloud distortion
float hash(float2 p)
{
	return frac(sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453f);
}

float noise(float2 p)
{
	float2 i = floor(p);
	float2 f = frac(p);
	f = f * f * (3.0f - 2.0f * f);  // Smoothstep

	float a = hash(i);
	float b = hash(i + float2(1.0f, 0.0f));
	float c = hash(i + float2(0.0f, 1.0f));
	float d = hash(i + float2(1.0f, 1.0f));

	return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float fbm(float2 p)
{
	float value = 0.0f;
	float amplitude = 0.5f;
	for (int i = 0; i < 3; i++)
	{
		value += amplitude * noise(p);
		p *= 2.0f;
		amplitude *= 0.5f;
	}
	return value;
}

float4 main(PS_INPUT input) : SV_TARGET
{

	// Skybox gradient mode — per-pixel gradient from constant buffer
	if (vMaterialParams.w < 0.25f)
	{
		float4 color = input.Color;
		if (skyColorCount > 1)
		{
			float t = saturate(input.Height) * (float)(skyColorCount - 1);
			int seg = min((int)t, skyColorCount - 2);
			float f = t - (float)seg;
			f = f * f * (3.0f - 2.0f * f); // smoothstep
			color = lerp(skyColors[seg], skyColors[seg + 1], f);
		}

		return color;
	}

	if (vMaterialParams.w < 0.5f)
	{
		float4 texColor = texDiffuse.Sample(samLinear, input.TexCoord);
		return texColor * input.Color;
	}

	// === ENHANCED CLOUD RENDERING ===

	float time = vTime.x;
	float3 sunDir = normalize(vSunDirection.xyz);
	float sunIntensity = vSunDirection.w;

	// Noise-based UV distortion for organic movement
	float2 distortion = float2(
		fbm(input.TexCoord * 3.0f + time * 0.1f) - 0.5f,
		fbm(input.TexCoord * 3.0f + float2(5.2f, 1.3f) + time * 0.08f) - 0.5f
	) * 0.03f;

	// Sample primary cloud layer with distortion
	float2 uv1 = input.TexCoord + distortion;
	float4 cloud1 = texDiffuse.Sample(samLinear, uv1);

	// Sample secondary cloud layer (parallax depth effect)
	float2 uv2 = input.TexCoord2 + distortion * 0.7f;
	float4 cloud2 = texDiffuse.Sample(samLinear, uv2);

	// Blend two cloud layers for depth
	float4 clouds = cloud1 * 0.6f + cloud2 * 0.4f;

	float softAlpha = clouds.a;
	softAlpha = smoothstep(0.1f, 0.6f, softAlpha);  // Soften edges

	// === SUN-AWARE LIGHTING ===

	// Calculate view direction from cloud position
	float3 viewDir = normalize(input.WorldPos - vCameraPos.xyz);

	// Sun dot product for lighting
	float sunDot = dot(viewDir, sunDir);

	float rimLight = saturate(pow(saturate(sunDot + 0.3f), 3.0f));

	float sunHeight = saturate(sunDir.z);  // z is up
	float3 warmColor = float3(1.0f, 0.85f, 0.7f);   // Sunset orange
	float3 coolColor = float3(0.9f, 0.95f, 1.0f);   // Midday slight blue
	float3 sunTint = lerp(warmColor, coolColor, sunHeight);

	// Apply sun-based coloring
	float3 cloudColor = clouds.rgb * input.Color.rgb;

	// Add silver lining / sun glow on cloud edges
	float3 rimColor = sunTint * rimLight * sunIntensity * 0.5f;
	cloudColor += rimColor * softAlpha;

	float dist = length(input.WorldPos.xy) * 0.00001f;
	float3 atmosphereColor = float3(0.7f, 0.8f, 1.0f);
	cloudColor = lerp(cloudColor, cloudColor * atmosphereColor, saturate(dist));

	// HDR-style brightness boost near sun
	float sunProximity = saturate(pow(saturate(sunDot), 8.0f));
	cloudColor += sunTint * sunProximity * sunIntensity * 0.3f * softAlpha;

	float edgeDist = max(abs(input.LocalPos.x), abs(input.LocalPos.y));
	float edgeFade = 1.0f - saturate((edgeDist - 0.7f) / 0.3f);

	float cloudMask = smoothstep(0.0f, 0.1f, clouds.a);

	float4 finalColor;
	finalColor.rgb = cloudColor * cloudMask * edgeFade;
	finalColor.a = softAlpha * edgeFade * input.Color.a;

	return finalColor;
}
)";


static const char* g_szParticleVertexShader = R"(
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
};

struct VS_INPUT
{
	float3 Position : POSITION;
	float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 Position  : SV_POSITION;
	float2 TexCoord  : TEXCOORD0;
	float4 Color     : COLOR0;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	output.Position = mul(float4(input.Position, 1.0f), matWorldViewProj);
	output.TexCoord = input.TexCoord;
	output.Color = vParticleColor;
	return output;
}
)";

static const char* g_szParticlePixelShader = R"(
Texture2D    texDiffuse : register(t0);
SamplerState samLinear  : register(s0);

cbuffer CBPerObject : register(b1)
{
	matrix matWorld;
	matrix matWorldViewProj;
	matrix matTexture0;
	matrix matTexture1;
	float4 vDiffuseColor;
	float4 vSkyTint;
	float4 vMaterialParams;  // w = colorOp: 0=MODULATE, 1=SELECTARG1(factor), 2=SELECTARG2(texture)
	float4 vEmissiveColor;
	float4 vSpecularColor;
	float4 vPBRParams;
	float4 vRenderFlags;
	float4 vParticleColor;
};

struct PS_INPUT
{
	float4 Position  : SV_POSITION;
	float2 TexCoord  : TEXCOORD0;
	float4 Color     : COLOR0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	// Sample the particle texture
	float4 texColor = texDiffuse.Sample(samLinear, input.TexCoord);


	float4 finalColor;
	float colorOp = vMaterialParams.w;

	if (colorOp < -0.5f)
	{
		// DISABLE: discard this pixel (make fully transparent)
		discard;
	}
	else if (colorOp > 4.5f)
	{
		// ADD: texture + factor (clamped)
		finalColor.rgb = saturate(texColor.rgb + input.Color.rgb);
		finalColor.a = texColor.a * input.Color.a;
	}
	else if (colorOp > 3.5f)
	{
		// MODULATE4X: (texture * factor) * 4
		finalColor.rgb = saturate(texColor.rgb * input.Color.rgb * 4.0f);
		finalColor.a = texColor.a * input.Color.a;
	}
	else if (colorOp > 2.5f)
	{
		// MODULATE2X: (texture * factor) * 2
		finalColor.rgb = saturate(texColor.rgb * input.Color.rgb * 2.0f);
		finalColor.a = texColor.a * input.Color.a;
	}
	else if (colorOp > 1.5f)
	{
		// SELECTARG2: texture color only
		finalColor.rgb = texColor.rgb;
		finalColor.a = texColor.a * input.Color.a;
	}
	else if (colorOp > 0.5f)
	{
		// SELECTARG1: factor color only
		finalColor.rgb = input.Color.rgb;
		finalColor.a = texColor.a * input.Color.a;
	}
	else
	{
		// MODULATE (default): texture * factor
		finalColor.rgb = texColor.rgb * input.Color.rgb;
		finalColor.a = texColor.a * input.Color.a;
	}

	if (finalColor.a < 0.004f)  // ~1/255, essentially zero alpha
	{
		discard;
	}

	return finalColor;
}
)";

static const char* g_szParticlePCTVertexShader = R"(
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
};

struct VS_INPUT
{
	float3 Position : POSITION;
	float4 Color    : COLOR0;
	float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 Position  : SV_POSITION;
	float2 TexCoord  : TEXCOORD0;
	float4 Color     : COLOR0;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	output.Position = mul(float4(input.Position, 1.0f), matWorldViewProj);
	output.TexCoord = input.TexCoord;
	output.Color = input.Color;  // Per-vertex color from CS output (NOT vSkyTint)
	return output;
}
)";

//////////////////////////////////////////////////////////////////////////
// HLSL Compute Shader - Particle Billboard Generation
//////////////////////////////////////////////////////////////////////////
static const char* g_szParticleBillboardCS = R"(
struct ParticleInput
{
	float3 pos;
	float3 lastPos;
	float2 halfSize;
	float2 scale;
	float  rotation;
	uint   color;
	uint   flags;
	float3 _pad;
};

StructuredBuffer<ParticleInput> g_Particles : register(t0);
RWByteAddressBuffer g_OutputVB : register(u0);

cbuffer CBParticleCS : register(b0)
{
	float3 camUp;      float _pad0;
	float3 camCross;   float _pad1;
	float3 camView;    float _pad2;
	float4x4 attachMatrix;
	uint   particleCount;
	uint   facesPerParticle;
	uint   hasAttachMatrix;
	uint   _pad3;
	float4 faceRotations;
};

[numthreads(64, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	uint idx = DTid.x;
	if (idx >= particleCount) return;

	ParticleInput p = g_Particles[idx];
	uint bbType = p.flags & 0xF;
	bool bStretch = (p.flags & 0x10) != 0;

	float3 center = p.pos;
	if (hasAttachMatrix)
		center = mul(float4(center, 1), attachMatrix).xyz;

	for (uint face = 0; face < facesPerParticle; face++)
	{
		float3 v3Up, v3Cross;
		float totalRot = p.rotation;

		if (bStretch)
		{
			v3Up = p.pos - p.lastPos;
			if (hasAttachMatrix)
				v3Up = mul(float4(v3Up, 0), attachMatrix).xyz;
			float len = length(v3Up);
			if (len == 0) v3Up = float3(0, 0, 1);
			else v3Up *= (1.0f + log(1.0f + len)) / len;
			v3Cross = normalize(cross(v3Up, camView));
		}
		else
		{
			switch (bbType)
			{
				case 3: // BILLBOARD_TYPE_LIE
				{
					float c = cos(totalRot), s = sin(totalRot);
					v3Up = float3(c, -s, 0);
					v3Cross = float3(s, c, 0);
					break;
				}
				case 2: // BILLBOARD_TYPE_Y
				case 4: // BILLBOARD_TYPE_2FACE
				case 5: // BILLBOARD_TYPE_3FACE
				{
					v3Up = float3(0, 0, 1);
					if (v3Up.x * camView.y - v3Up.y * camView.x < 0)
						v3Up = -v3Up;
					float3 viewXY = float3(camView.x, camView.y, 0);
					v3Cross = normalize(cross(v3Up, viewXY));
					if (totalRot != 0)
					{
						float c = -sin(totalRot), s = cos(totalRot);
						float3 tmpUp = v3Up * c - v3Cross * s;
						v3Cross = v3Cross * c + v3Up * s;
						v3Up = tmpUp;
					}
					break;
				}
				default: // BILLBOARD_TYPE_ALL (1) and BILLBOARD_TYPE_NONE (0)
				{
					if (totalRot == 0)
					{
						v3Up = -camCross;
						v3Cross = camUp;
					}
					else
					{
						// Rodrigues' rotation around camView
						float c = cos(totalRot), s = sin(totalRot), omc = 1 - c;
						float3 k = camView;
						float3 v = -camCross;
						float kDotV = dot(k, v);
						v3Up = v * c + cross(k, v) * s + k * kDotV * omc;

						float kDotU = dot(k, camUp);
						v3Cross = camUp * c + cross(k, camUp) * s + k * kDotU * omc;
					}
					break;
				}
			}
		}

		// Apply face Z-rotation (for 2FACE/3FACE extra faces)
		float faceRot = (face == 0) ? faceRotations.x : ((face == 1) ? faceRotations.y : faceRotations.z);
		if (faceRot != 0)
		{
			float fc = cos(faceRot), fs = sin(faceRot);
			float ux = v3Up.x, uy = v3Up.y;
			v3Up.x = ux * fc - uy * fs;
			v3Up.y = uy * fc + ux * fs;
			float cx = v3Cross.x, cy = v3Cross.y;
			v3Cross.x = cx * fc - cy * fs;
			v3Cross.y = cy * fc + cx * fs;
		}

		// Scale
		v3Cross = -(p.halfSize.x * p.scale.x) * v3Cross;
		v3Up = (p.halfSize.y * p.scale.y) * v3Up;

		// Generate 4 vertices for this quad
		uint quadIdx = idx * facesPerParticle + face;
		uint baseOffset = quadIdx * 4 * 24; // 4 verts x 24 bytes each

		float3 positions[4];
		positions[0] = center - v3Up + v3Cross;
		positions[1] = center - v3Up - v3Cross;
		positions[2] = center + v3Up + v3Cross;
		positions[3] = center + v3Up - v3Cross;

		// UV coords match CPU particle mesh initialization
		float2 uvs[4] = { float2(0,1), float2(0,0), float2(1,1), float2(1,0) };

		[unroll]
		for (uint v = 0; v < 4; v++)
		{
			uint off = baseOffset + v * 24;
			g_OutputVB.Store3(off,      asuint(positions[v]));
			g_OutputVB.Store (off + 12, p.color);
			g_OutputVB.Store2(off + 16, asuint(uvs[v]));
		}
	}
}
)";

//////////////////////////////////////////////////////////////////////////
// HLSL Compute Shader - Fly Trace Billboard Generation
//////////////////////////////////////////////////////////////////////////

static const char* g_szFlyTraceCS = R"(
struct FlyTraceSegment
{
	float3 pos1;
	float  size1;
	float3 pos2;
	float  size2;
	uint   color;
	uint3  _pad;
};

StructuredBuffer<FlyTraceSegment> g_Segments : register(t0);
RWByteAddressBuffer g_OutputVB : register(u0);

cbuffer CBFlyTraceCS : register(b0)
{
	float3 camEye;     float _pad0;
	float3 camFwd;     float _pad1;
	uint   segmentCount; uint3 _pad2;
};

[numthreads(64, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	uint idx = DTid.x;
	if (idx >= segmentCount) return;

	FlyTraceSegment seg = g_Segments[idx];
	float3 pos1 = seg.pos1;
	float3 pos2 = seg.pos2;

	float3 B = pos2 - pos1;           // segment direction
	float3 E = camEye - pos1;         // to camera
	float3 P = cross(B, E);           // perpendicular
	float3 rawU = cross(camFwd, P);
	float lenU = length(rawU);
	float3 U = (lenU > 0.0001f) ? (rawU / lenU) : float3(0, 0, 1);
	float3 R = cross(camFwd, U);

	float3 positions[6];
	positions[0] = pos1 + U * seg.size1;   // top
	positions[1] = pos1 - R * seg.size1;   // left
	positions[2] = pos1 + R * seg.size1;   // right
	positions[3] = pos2 - R * seg.size2;   // left
	positions[4] = pos2 + R * seg.size2;   // right
	positions[5] = pos2 - U * seg.size2;   // bottom

	float2 uvs[6] = {
		float2(0.0, 0.0), float2(0.0, 0.5), float2(0.5, 0.0),
		float2(0.5, 1.0), float2(1.0, 0.5), float2(1.0, 1.0)
	};

	uint baseOffset = idx * 6 * 24;  // 6 verts x 24 bytes
	[unroll] for (uint v = 0; v < 6; v++)
	{
		uint off = baseOffset + v * 24;
		g_OutputVB.Store3(off,      asuint(positions[v]));
		g_OutputVB.Store (off + 12, seg.color);
		g_OutputVB.Store2(off + 16, asuint(uvs[v]));
	}
}
)";

//////////////////////////////////////////////////////////////////////////
// HLSL Shader Code - Weapon Trace Spline CS
//////////////////////////////////////////////////////////////////////////

static const char* g_szWeaponTraceCS = R"(
struct SplineSegment
{
	float3 a; float timeStart;
	float3 b; float timeEnd;
	float3 c; float _pad0;
	float3 d; float _pad1;
};

StructuredBuffer<SplineSegment> g_Segments : register(t0);
RWByteAddressBuffer g_OutputVB : register(u0);

cbuffer CBWeaponTraceCS : register(b0)
{
	uint numSegments;
	uint numSamples;
	float lifetime;
	float samplingTime;
	float firstPointTime;
	float totalLength;
	uint2 _pad;
};

[numthreads(64, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	uint sampleIdx = DTid.x;
	if (sampleIdx >= numSamples) return;

	float t = sampleIdx * samplingTime;
	if (t > totalLength) t = totalLength;

	uint seg = 0;
	for (uint i = 0; i < numSegments; i++)
	{
		if (t <= g_Segments[i].timeEnd) { seg = i; break; }
		seg = i;
	}

	SplineSegment ss = g_Segments[seg];
	float cc = t - ss.timeStart;
	float3 shortPos = ss.a + cc * (ss.b + cc * (ss.c + cc * ss.d));

	SplineSegment ls = g_Segments[seg + numSegments];
	cc = t - ls.timeStart;
	float3 longPos = ls.a + cc * (ls.b + cc * (ls.c + cc * ls.d));

	// Alpha fade for long vertex (short is always alpha=0)
	float ttt = saturate((t + firstPointTime) / lifetime);
	float alpha = saturate((1.0 - ttt) * (1.0 - ttt) / 2.5 - 0.1);

	// Pack color as ARGB DWORD: Color(0.3, 0.8, 1.0, alpha)
	uint R = (uint)(0.3 * 255.0 + 0.5);
	uint G = (uint)(0.8 * 255.0 + 0.5);
	uint B = 255;
	uint A = (uint)(alpha * 255.0 + 0.5);
	uint longColor = (A << 24) | (R << 16) | (G << 8) | B;

	float texU = t / lifetime;

	uint baseOff = sampleIdx * 2 * 24;

	// Long vertex
	g_OutputVB.Store3(baseOff,      asuint(longPos));
	g_OutputVB.Store (baseOff + 12, longColor);
	g_OutputVB.Store2(baseOff + 16, asuint(float2(texU, 0.0)));

	// Short vertex (alpha = 0, fully transparent)
	g_OutputVB.Store3(baseOff + 24, asuint(shortPos));
	g_OutputVB.Store (baseOff + 36, 0);
	g_OutputVB.Store2(baseOff + 40, asuint(float2(texU, 1.0)));
}
)";


static const char* g_szShadowVertexShader = R"(
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
};

struct VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal   : NORMAL;
	float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	float  Depth    : TEXCOORD1;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	output.Position = mul(float4(input.Position, 1.0f), matWorldViewProj);
	output.TexCoord = input.TexCoord;
	output.Depth = output.Position.z / output.Position.w;
	return output;
}
)";

static const char* g_szShadowPixelShader = R"(

cbuffer CBPerObject : register(b1)
{
	matrix matWorld;
	matrix matWorldViewProj;
	matrix matTexture0;
	matrix matTexture1;
	float4 vDiffuseColor;
	float4 vSkyTint;
	float4 vMaterialParams;   // x = alphaRef, y = alphaTestEnabled
	float4 vEmissiveColor;
	float4 vSpecularColor;
	float4 vPBRParams;
	float4 vRenderFlags;
	float4 vParticleColor;
};

Texture2D    texDiffuse : register(t0);
SamplerState samLinear  : register(s0);

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	float  Depth    : TEXCOORD1;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	if (vMaterialParams.y > 0.5f)
	{
		float4 texColor = texDiffuse.Sample(samLinear, input.TexCoord);
		if (texColor.a < vMaterialParams.x)
			discard;
	}

	// Output depth to R32F shadow map for PCF sampling
	return float4(input.Depth, 0, 0, 1);
}
)";


static const char* g_szShadowSkinnedVertexShader = R"(
#define MAX_BONES 256

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
};

cbuffer CBSkinning : register(b3)
{
	row_major matrix boneMatrices[MAX_BONES];
};

struct VS_INPUT
{
	float3 Position     : POSITION;
	float3 Normal       : NORMAL;
	float2 TexCoord     : TEXCOORD0;
	float4 BlendWeights : BLENDWEIGHT;
	uint4  BlendIndices : BLENDINDICES;
};

struct VS_OUTPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	float  Depth    : TEXCOORD1;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	// GPU Skinning: blend position by bone weights
	float4 skinnedPos = float4(0, 0, 0, 0);
	float4 inputPos = float4(input.Position, 1.0f);

	[unroll]
	for (int i = 0; i < 4; i++)
	{
		float weight = input.BlendWeights[i];
		if (weight > 0.0f)
		{
			uint boneIndex = input.BlendIndices[i];
			if (boneIndex < MAX_BONES)
			{
				skinnedPos += weight * mul(inputPos, boneMatrices[boneIndex]);
			}
		}
	}

	float totalWeight = input.BlendWeights.x + input.BlendWeights.y + input.BlendWeights.z + input.BlendWeights.w;
	if (totalWeight < 0.001f)
		skinnedPos = inputPos;
	else
		skinnedPos.w = 1.0f;

	output.Position = mul(skinnedPos, matWorldViewProj);
	output.TexCoord = input.TexCoord;
	output.Depth = output.Position.z / output.Position.w;

	return output;
}
)";

// Use the same pixel shader as regular shadow
static const char* g_szShadowSkinnedPixelShader = R"(
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
};

Texture2D    texDiffuse : register(t0);
SamplerState samLinear  : register(s0);

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	float  Depth    : TEXCOORD1;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	// Alpha test support
	if (vMaterialParams.y > 0.5f)
	{
		float4 texColor = texDiffuse.Sample(samLinear, input.TexCoord);
		if (texColor.a < vMaterialParams.x)
			discard;
	}

	return float4(input.Depth, 0, 0, 1);
}
)";


static const char* g_szSpeedTreeVertexShader = R"(
#define NUM_WIND_MATRICES 4

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
};

// SpeedTree-specific constant buffer
cbuffer CBSpeedTree : register(b3)
{
	row_major matrix matWindMatrices[NUM_WIND_MATRICES];  // Wind rotation matrices (row-major)
	float4 vTreePos;                             // Tree world position
	float4 vLeafTables[48];                      // Leaf billboard tables
	float4 vLeafLightingAdj;
	float4 vLightDir;
	float4 vLightDiffuse;
	float4 vLightAmbient;
	float4 vMaterialDiffuse;
	float4 vMaterialAmbient;
	float4 vSpeedTreeFog;
	int nNumLeafTables;
	int3 _padST;
};

struct VS_INPUT
{
	float3 Position   : POSITION;
	float4 Color      : COLOR0;       // Vertex color (static lighting)
	float2 TexCoord   : TEXCOORD0;
	float2 ShadowCoord: TEXCOORD1;    // Self-shadow texture coords
	float2 WindData   : TEXCOORD2;    // x = wind matrix index, y = wind weight
};

struct VS_OUTPUT
{
	float4 Position  : SV_POSITION;
	float4 Color     : COLOR0;
	float2 TexCoord  : TEXCOORD0;
	float  FogFactor : TEXCOORD1;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	float3 localPos = input.Position;

	int   nWindMat = clamp(int(input.WindData.x * 0.25f), 0, NUM_WIND_MATRICES - 1);
	float fWindAmt = saturate(input.WindData.y);
	float3 windPos = mul(float4(localPos, 1.0f), matWindMatrices[nWindMat]).xyz;
	float3 animatedPos = lerp(localPos, windPos, fWindAmt);

	float4 worldPos = mul(float4(animatedPos, 1.0f), matWorld);
	float4 viewPos = mul(worldPos, matView);
	output.Position = mul(viewPos, matProjection);

	output.Color = input.Color;

	// Pass texture coordinates
	output.TexCoord = input.TexCoord;

	// Height-based atmospheric fog
	if (vFogParams.w > 0.5f)
	{
		output.FogFactor = saturate((vFogParams.y - output.Position.z) / (vFogParams.y - vFogParams.x));
	}
	else
	{
		output.FogFactor = 1.0f;
	}

	return output;
}
)";

static const char* g_szSpeedTreePixelShader = R"(

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
	finalColor.rgb = lerp(vFogColor.rgb, finalColor.rgb, input.FogFactor);

	// Normal rendering - output texture * vertex color
	return finalColor;
}
)";

//////////////////////////////////////////////////////////////////////////
// SpeedTree Leaf Shader (GPU leaf placement + wind)
//////////////////////////////////////////////////////////////////////////

static const char* g_szSpeedTreeLeafVertexShader = R"(

cbuffer CBPerFrame : register(b0)
{
	matrix matView;
	matrix matProjection;
	float4 vCameraPos;
	float4 vFogParams;
	float4 vFogColor;
	float4 vTime;
};

cbuffer CBSpeedTree : register(b3)
{
	row_major matrix matWindMatrices[4];
	float4 vTreePos;
	float4 vLeafTables[48];
	float4 vLeafLightingAdj;
	float4 vLightDir;
	float4 vLightDiffuse;
	float4 vLightAmbient;
	float4 vMaterialDiffuse;
	float4 vMaterialAmbient;
	float4 vSpeedTreeFog;
	int nNumLeafTables;
	int3 _padST;
};

struct VS_INPUT
{
	float3 Position  : POSITION;
	float4 Color     : COLOR0;
	float2 TexCoord  : TEXCOORD0;
	float4 LeafData  : TEXCOORD2;
};

struct VS_OUTPUT
{
	float4 Position  : SV_POSITION;
	float4 Color     : COLOR0;
	float2 TexCoord  : TEXCOORD0;
	float  FogFactor : TEXCOORD1;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	float3 localPos = input.Position;

	int   nWindMat = clamp(int(input.LeafData.x * 0.25f), 0, 3);
	float fWindAmt = saturate(input.LeafData.y);
	float3 windPos = mul(float4(localPos, 1.0f), matWindMatrices[nWindMat]).xyz;
	localPos = lerp(localPos, windPos, fWindAmt);

	// Transform to clip space
	float3 worldPos = localPos + vTreePos.xyz;
	float4 viewPos = mul(float4(worldPos, 1.0f), matView);
	output.Position = mul(viewPos, matProjection);

	output.Color = input.Color;
	output.TexCoord = input.TexCoord;

	if (vFogParams.w > 0.5f)
	{
		output.FogFactor = saturate((vFogParams.y - output.Position.z) / (vFogParams.y - vFogParams.x));
	}
	else
	{
		output.FogFactor = 1.0f;
	}

	return output;
}
)";

static const char* g_szSpeedTreeLeafPixelShader = R"(
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
	float4 vMaterialParams;  // x = alphaRef, y = alphaTestEnabled
	float4 vEmissiveColor;
	float4 vSpecularColor;
	float4 vPBRParams;
	float4 vRenderFlags;
	float4 vParticleColor;
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

float4 main(PS_INPUT input) : SV_TARGET
{
	float4 texColor = texDiffuse.Sample(samLinear, input.TexCoord);
	float4 finalColor = texColor * input.Color;

	// Alpha test
	if (vMaterialParams.y > 0.5f && finalColor.a < vMaterialParams.x)
		discard;

	// Shadow mode - output depth for R32F shadow map
	if (vRenderFlags.x > 0.5f)
		return float4(input.Position.z, 0, 0, 1);

	// Height-based atmospheric fog
	finalColor.rgb = lerp(vFogColor.rgb, finalColor.rgb, input.FogFactor);

	return finalColor;
}
)";

//////////////////////////////////////////////////////////////////////////
// HLSL Shader Code - Normal Mapped Mesh
//////////////////////////////////////////////////////////////////////////

static const char* g_szMeshNormalVS = R"(
#define MAX_LIGHTS 16

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
};

struct VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal   : NORMAL;
	float2 TexCoord : TEXCOORD0;
	float3 Tangent  : TANGENT;
	float3 Binormal : BINORMAL;
};

struct VS_OUTPUT
{
	float4 Position  : SV_POSITION;
	float2 TexCoord  : TEXCOORD0;
	float3 WorldPos  : TEXCOORD1;
	float3 Normal    : TEXCOORD2;
	float3 Tangent   : TEXCOORD3;
	float3 Binormal  : TEXCOORD4;
	float  FogFactor : TEXCOORD5;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	float4 worldPos = mul(float4(input.Position, 1.0f), matWorld);
	output.Position = mul(worldPos, mul(matView, matProjection));
	output.WorldPos = worldPos.xyz;
	output.TexCoord = input.TexCoord;

	output.Normal = normalize(mul((float3x3)matWorld, input.Normal));
	output.Tangent = normalize(mul((float3x3)matWorld, input.Tangent));
	output.Binormal = normalize(mul((float3x3)matWorld, input.Binormal));

	// Height-based atmospheric fog
	if (vFogParams.w > 0.5f)
	{
		output.FogFactor = saturate((vFogParams.y - output.Position.z) / (vFogParams.y - vFogParams.x));
	}
	else
	{
		output.FogFactor = 1.0f;
	}

	return output;
}
)";

static const char* g_szMeshNormalPS = R"(
#define MAX_LIGHTS 16

struct Light
{
	float4 Position;
	float4 Direction;
	float4 Color;
	float4 Attenuation;
};

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
};

cbuffer CBLighting : register(b2)
{
	Light lights[MAX_LIGHTS];
	float4 globalAmbient;
	int numActiveLights;
	int3 _padL;
};

Texture2D texDiffuse : register(t0);
Texture2D texNormal : register(t1);
SamplerState samLinear : register(s0);

struct PS_INPUT
{
	float4 Position  : SV_POSITION;
	float2 TexCoord  : TEXCOORD0;
	float3 WorldPos  : TEXCOORD1;
	float3 Normal    : TEXCOORD2;
	float3 Tangent   : TEXCOORD3;
	float3 Binormal  : TEXCOORD4;
	float  FogFactor : TEXCOORD5;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	// Sample textures
	float4 diffuseColor = texDiffuse.Sample(samLinear, input.TexCoord);
	float3 normalMap = texNormal.Sample(samLinear, input.TexCoord).xyz * 2.0f - 1.0f;

	// Build TBN matrix
	float3x3 TBN = float3x3(
		normalize(input.Tangent),
		normalize(input.Binormal),
		normalize(input.Normal)
	);

	// Transform normal from tangent to world space
	float3 worldNormal = normalize(mul(normalMap, TBN));

	// Lighting calculation
	float3 viewDir = normalize(vCameraPos.xyz - input.WorldPos);

#ifdef PBR_ENABLED
	float3 albedo = diffuseColor.rgb * vDiffuseColor.rgb;
	float roughness = vPBRParams.x;
	if (roughness < 0.01f) roughness = sqrt(2.0f / (max(vMaterialParams.z, 1.0f) + 2.0f));
	roughness = clamp(roughness, 0.04f, 1.0f);
	float metallic = vPBRParams.y;
	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
	float3 ambient = globalAmbient.rgb;
	float3 Lo = float3(0, 0, 0);

	for (int i = 0; i < min(numActiveLights, MAX_LIGHTS); i++)
	{
		if (lights[i].Direction.w < 0.5f) continue;

		float3 lightDir;
		float attenuation = 1.0f;

		int lightType = (int)lights[i].Position.w;
		if (lightType == 3)
		{
			lightDir = -normalize(lights[i].Direction.xyz);
		}
		else
		{
			float3 toLight = lights[i].Position.xyz - input.WorldPos;
			float dist = length(toLight);
			lightDir = toLight / max(dist, 0.001f);
			attenuation = 1.0f / (lights[i].Attenuation.x +
				lights[i].Attenuation.y * dist +
				lights[i].Attenuation.z * dist * dist);
		}

		float3 lc = lights[i].Color.rgb * max(lights[i].Color.a, 1.0f);
		Lo += EvalPBRLight(lightDir, worldNormal, viewDir, lc, attenuation, albedo, roughness, metallic, F0);
	}

	float3 finalColor = saturate(ambient * albedo + Lo) + vEmissiveColor.rgb;
#else
	float3 finalColor = globalAmbient.rgb * diffuseColor.rgb;

	for (int i = 0; i < min(numActiveLights, MAX_LIGHTS); i++)
	{
		if (lights[i].Direction.w < 0.5f) continue;

		float3 lightDir;
		float attenuation = 1.0f;

		int lightType = (int)lights[i].Position.w;
		if (lightType == 3) // Directional
		{
			lightDir = -normalize(lights[i].Direction.xyz);
		}
		else // Point/Spot
		{
			float3 toLight = lights[i].Position.xyz - input.WorldPos;
			float dist = length(toLight);
			lightDir = toLight / dist;
			attenuation = 1.0f / (lights[i].Attenuation.x +
				lights[i].Attenuation.y * dist +
				lights[i].Attenuation.z * dist * dist);
		}

		// Diffuse
		float NdotL = max(0, dot(worldNormal, lightDir));
		finalColor += diffuseColor.rgb * lights[i].Color.rgb * NdotL * attenuation;

		// Specular
		float3 halfVec = normalize(lightDir + viewDir);
		float NdotH = max(0, dot(worldNormal, halfVec));
		float specPower = vMaterialParams.z > 0.0f ? vMaterialParams.z : 32.0f;
		finalColor += vSpecularColor.rgb * lights[i].Color.rgb * pow(NdotH, specPower) * attenuation;
	}

	finalColor += vEmissiveColor.rgb;
#endif

	// Height-based atmospheric fog
	float4 finalOut = float4(finalColor, diffuseColor.a * vDiffuseColor.a);
	finalOut.rgb = lerp(vFogColor.rgb, finalOut.rgb, input.FogFactor);

	return finalOut;
}
)";

//////////////////////////////////////////////////////////////////////////
// HLSL Shader Code - Skinned Mesh (GPU Skinning)
//////////////////////////////////////////////////////////////////////////

static const char* g_szSkinnedMeshVertexShader = R"(
#define MAX_LIGHTS 16
#define MAX_BONES 256
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

// Bone matrices for GPU skinning
// row_major: Granny stores matrices in row-major format
cbuffer CBSkinning : register(b3)
{
	row_major matrix boneMatrices[MAX_BONES];
};

struct VS_INPUT
{
	float3 Position     : POSITION;
	float3 Normal       : NORMAL;
	float2 TexCoord     : TEXCOORD0;
	float4 BlendWeights : BLENDWEIGHT;
	uint4  BlendIndices : BLENDINDICES;
};

struct VS_OUTPUT
{
	float4 Position   : SV_POSITION;
	float3 WorldPos   : TEXCOORD0;
	float3 WorldNorm  : TEXCOORD1;
	float2 TexCoord   : TEXCOORD2;
	float  FogFactor  : TEXCOORD3;
	float2 ShadowCoord : TEXCOORD4;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	// GPU Skinning: blend position and normal by bone weights
	float4 skinnedPos = float4(0, 0, 0, 0);
	float3 skinnedNormal = float3(0, 0, 0);

	float4 inputPos = float4(input.Position, 1.0f);

	// Apply bone transforms weighted by blend weights
	// Granny uses up to 4 bone influences per vertex
	[unroll]
	for (int i = 0; i < 4; i++)
	{
		float weight = input.BlendWeights[i];
		if (weight > 0.0f)
		{
			uint boneIndex = input.BlendIndices[i];
			if (boneIndex < MAX_BONES)
			{
				skinnedPos += weight * mul(inputPos, boneMatrices[boneIndex]);
				skinnedNormal += weight * mul(input.Normal, (float3x3)boneMatrices[boneIndex]);
			}
		}
	}

	float totalWeight = input.BlendWeights.x + input.BlendWeights.y + input.BlendWeights.z + input.BlendWeights.w;
	if (totalWeight < 0.001f)
	{
		// No skinning - use original position
		skinnedPos = inputPos;
		skinnedNormal = input.Normal;
	}
	else
	{
		// Ensure w component is correct for position
		skinnedPos.w = 1.0f;

		float normalLen = length(skinnedNormal);
		if (normalLen > 0.0001f)
			skinnedNormal = skinnedNormal / normalLen;
		else
			skinnedNormal = input.Normal;  // Fallback to original normal
	}

	// Apply world transform
	float4 worldPos = mul(skinnedPos, matWorld);
	output.Position = mul(skinnedPos, matWorldViewProj);
	output.WorldPos = worldPos.xyz;

	// Transform normal to world space
	output.WorldNorm = normalize(mul((float3x3)matWorld, skinnedNormal));
	output.TexCoord = input.TexCoord;

	float3 shadowBias = float3(-0.447f, -0.258f, 0.894f) * 80.0f; // 80 units bias towards light
	float4 biasedWorldPos = float4(worldPos.xyz + shadowBias, 1.0f);
	float4 shadowCoord = mul(biasedWorldPos, matTexture1);
	if (abs(shadowCoord.w) > 0.0001f)
		output.ShadowCoord = shadowCoord.xy / shadowCoord.w;
	else
		output.ShadowCoord = shadowCoord.xy;

	// Height-based atmospheric fog
	if (vFogParams.w > 0.5f)
	{
		output.FogFactor = saturate((vFogParams.y - output.Position.z) / (vFogParams.y - vFogParams.x));
	}
	else
	{
		output.FogFactor = 1.0f;
	}

	return output;
}
)";

static const char* g_szSkinnedMeshPixelShader = R"(
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
	float4 vShadowParams;     // x = opacity (0-120)
	float4 vCascadeSplits;    // x,y,z,w = view-space depth thresholds for cascades 0-3
	matrix matShadowBig;      // Cascade 3 (far)
	matrix matShadowLocal;    // Cascade 0 (near)
	matrix matShadowMid;      // Cascade 1
	matrix matShadowFar;      // Cascade 2
};

cbuffer CBPerObject : register(b1)
{
	matrix matWorld;
	matrix matWorldViewProj;
	matrix matTexture0;
	matrix matTexture1;
	float4 vDiffuseColor;
	float4 vSkyTint;
	float4 vMaterialParams;  // x = alphaRef, y = alphaTestEnabled, z = specularPower, w = twoTexBlend
	float4 vEmissiveColor;
	float4 vSpecularColor;
	float4 vPBRParams;
	float4 vRenderFlags;
	float4 vParticleColor;
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

Texture2D    texDiffuse     : register(t0);
Texture2D    texDiffuse1    : register(t1);
Texture2D    texShadowBig   : register(t2);  // Cascade 3 (far)
Texture2D    texShadowLocal : register(t3);  // Cascade 0 (near)
Texture2D    texShadowMid   : register(t4);  // Cascade 1
Texture2D    texShadowFar   : register(t5);
Texture2D    texSphere      : register(t6);  // sphere map (moved off t1 so shadows cannot evict it)  // Cascade 2
SamplerState samLinear   : register(s0);
SamplerState samShadow   : register(s1);
SamplerComparisonState samShadowCmp : register(s2);   // hardware PCF

struct PS_INPUT
{
	float4 Position   : SV_POSITION;
	float3 WorldPos   : TEXCOORD0;
	float3 WorldNorm  : TEXCOORD1;
	float2 TexCoord   : TEXCOORD2;
	float  FogFactor  : TEXCOORD3;
	float2 ShadowCoord : TEXCOORD4;
};

// 16-sample Poisson disk for smooth PCF shadow filtering
static const float2 pcfOffsets[16] = {
	float2( 0.0,     0.0),     float2(-0.94201, -0.39906),
	float2( 0.94558, -0.76890), float2(-0.09418, -0.92938),
	float2( 0.34495,  0.29387), float2(-0.91588, 0.45771),
	float2(-0.81544, -0.87912), float2( 0.19984,  0.78882),
	float2(-0.17330,  0.93028), float2( 0.78366, -0.15540),
	float2(-0.61390, -0.23740), float2( 0.44323,  0.74511),
	float2( 0.56071, -0.45678), float2(-0.38291,  0.37461),
	float2(-0.52117,  0.75145), float2( 0.30700, -0.79810)
};

float4 ShadowOffsetPos(float3 worldPos, float3 N, matrix matCascade, float texelScale)
{
	float fScale = length(float3(matCascade._11, matCascade._21, matCascade._31));
	float fWorldTexel = 2.0f * texelScale / max(fScale, 0.000001f);
	return float4(worldPos + N * fWorldTexel * 1.5f, 1.0f);
}

float SampleShadowPCF(Texture2D shadowMap, SamplerState samp,
                      float2 uv, float depth, float texelScale, float bias = 0.0005f)
{
	if (uv.x < 0.01f || uv.x > 0.99f || uv.y < 0.01f || uv.y > 0.99f)
		return 0.0f;
	const float d = depth - bias;
	float lit = 0.0f;
	[unroll] for (int y = -1; y <= 1; ++y)
		[unroll] for (int x = -1; x <= 1; ++x)
			lit += shadowMap.SampleCmpLevelZero(samShadowCmp, uv + float2(x, y) * texelScale, d);
	return 1.0f - (lit / 9.0f);
}

// Calculate light contribution for a single light
float3 CalcLight(Light light, float3 worldPos, float3 normal)
{
	int lightType = (int)light.Position.w;

	float3 lightColor = light.Color.rgb;
	float colorMag = dot(lightColor, lightColor);
	if (colorMag < 0.01f)
		lightColor = float3(1.0f, 1.0f, 1.0f);  // Default to white light
	lightColor *= max(light.Color.a, 1.0f);  // Apply intensity (minimum 1.0)

	float3 lightDir = float3(0, -1, 0);  // Default: light from above
	float attenuation = 1.0f;

	if (lightType == LIGHT_DIRECTIONAL)
	{
		// Directional light: use stored direction
		float3 dir = light.Direction.xyz;
		float dirLen = length(dir);
		if (dirLen > 0.001f)
			lightDir = dir / dirLen;
	}
	else if (lightType == LIGHT_POINT || lightType == LIGHT_SPOT)
	{
		// Point or Spot light: calculate direction from position
		float3 toLight = light.Position.xyz - worldPos;
		float distance = length(toLight);

		// Check range and valid distance
		if (distance < 0.001f)
			distance = 0.001f;
		if (distance > light.Attenuation.w && light.Attenuation.w > 0.0f)
			return float3(0, 0, 0);

		lightDir = toLight / distance;

		// Attenuation formula: 1 / (att0 + att1*d + att2*d*d)
		float denom = light.Attenuation.x + light.Attenuation.y * distance + light.Attenuation.z * distance * distance;
		attenuation = (denom > 0.001f) ? (1.0f / denom) : 1.0f;
		attenuation = min(attenuation, 2.0f);  // Cap attenuation to prevent over-bright

		// Spot light cone
		if (lightType == LIGHT_SPOT)
		{
			float3 spotDir = light.Direction.xyz;
			float spotLen = length(spotDir);
			if (spotLen > 0.001f)
			{
				spotDir /= spotLen;
				float cosAngle = dot(-lightDir, spotDir);
				// Soft falloff from center (60 degree half-angle default)
				float spotFactor = saturate((cosAngle - 0.5f) * 2.0f);
				attenuation *= spotFactor;
			}
		}

		float NdotL = saturate(dot(normal, lightDir));
		return lightColor * NdotL * attenuation;
	}
	else
	{
		float NdotL = saturate(dot(normal, float3(0, -1, 0)));
		return lightColor * max(NdotL, 0.5f);  // At least 50% brightness
	}

	float NdotL = saturate(dot(normal, -lightDir));
	return lightColor * NdotL * attenuation;
}

float4 main(PS_INPUT input) : SV_TARGET
{
	if (vShadowParams.y > 0.001f)
		clip(input.WorldPos.z - vShadowParams.y + 10.0);

	float4 texColor = texDiffuse.Sample(samLinear, input.TexCoord);

	// DX11 fix: unbound textures return (0,0,0,0)
	if (texColor.r < 0.001f && texColor.g < 0.001f && texColor.b < 0.001f && texColor.a < 0.001f)
		discard;
	// Alpha test
	if (vMaterialParams.y > 0.5f && texColor.a < vMaterialParams.x)
		discard;

	// Start with ambient lighting
	float3 ambient = max(globalAmbient.rgb, float3(0.3f, 0.3f, 0.3f));
	float3 normal = normalize(input.WorldNorm);

#ifdef PBR_ENABLED
	float3 V = normalize(vCameraPos.xyz - input.WorldPos);
	float3 albedo = texColor.rgb * vDiffuseColor.rgb;
	float roughness = vPBRParams.x;
	if (roughness < 0.01f) roughness = sqrt(2.0f / (max(vMaterialParams.z, 1.0f) + 2.0f));
	roughness = clamp(roughness, 0.04f, 1.0f);
	float metallic = vPBRParams.y;
	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
	float3 Lo = float3(0, 0, 0);
	bool anyLightEnabled = false;

	[unroll]
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		if (lights[i].Direction.w > 0.5f)
		{
			anyLightEnabled = true;
			int lightType = (int)lights[i].Position.w;
			float3 L;
			float atten = 1.0f;
			if (lightType == LIGHT_DIRECTIONAL)
			{
				L = normalize(-lights[i].Direction.xyz);
			}
			else
			{
				float3 toLight = lights[i].Position.xyz - input.WorldPos;
				float dist = length(toLight);
				L = toLight / max(dist, 0.001f);
				float denom = lights[i].Attenuation.x + lights[i].Attenuation.y * dist + lights[i].Attenuation.z * dist * dist;
				atten = (denom > 0.001f) ? min(1.0f / denom, 2.0f) : 1.0f;
				if (dist > lights[i].Attenuation.w && lights[i].Attenuation.w > 0.0f) atten = 0.0f;
				if (lightType == LIGHT_SPOT)
				{
					float3 spotDir = normalize(lights[i].Direction.xyz);
					atten *= saturate((dot(-L, spotDir) - 0.5f) * 2.0f);
				}
			}
			float3 lc = lights[i].Color.rgb * max(lights[i].Color.a, 1.0f);
			float cm = dot(lc, lc);
			if (cm < 0.01f) lc = float3(1, 1, 1);
			Lo += EvalPBRLight(L, normal, V, lc, atten, albedo, roughness, metallic, F0);
		}
	}

	float3 lighting;
	if (anyLightEnabled)
		lighting = saturate(ambient * albedo + Lo);
	else
		lighting = albedo;

	float4 finalColor = float4(lighting, texColor.a * vDiffuseColor.a);
#else
	float3 diffuse = float3(0, 0, 0);

	// Accumulate light contributions from all enabled lights
	bool anyLightEnabled = false;
	[unroll]
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		if (lights[i].Direction.w > 0.5f)
		{
			anyLightEnabled = true;
			diffuse += CalcLight(lights[i], input.WorldPos, normal);
		}
	}

	float3 lighting;
	if (anyLightEnabled)
		lighting = saturate(ambient + diffuse);
	else
		lighting = float3(1.0f, 1.0f, 1.0f);

	// Base color: texture * material diffuse * lighting
	float4 finalColor = float4(texColor.rgb * vDiffuseColor.rgb * lighting, texColor.a * vDiffuseColor.a);
#endif

	{
		float specMag = dot(vSpecularColor.rgb, vSpecularColor.rgb);
		if (specMag > 0.0001f)
		{
			float  specMask   = texColor.a;   // diffuse alpha = spec mask (stock Metin2 convention)
			float3 V          = normalize(vCameraPos.xyz - input.WorldPos);
			float3 viewNormal = normalize(mul((float3x3)matView, normal));
			float2 sphereUV   = viewNormal.xy * 0.5f + 0.5f;
			float3 env        = texSphere.Sample(samLinear, sphereUV).rgb;
			float3 specular   = env * vSpecularColor.rgb;

			if (lights[0].Direction.w > 0.5f)
			{
				float3 L = normalize(-lights[0].Direction.xyz);
				float NdotL = saturate(dot(normal, L));
				if (NdotL > 0.0f)
				{
					float3 H = normalize(L + V);
					float NdotH = saturate(dot(normal, H));
					float specPower = (vPBRParams.w > 0.0001f) ? vPBRParams.w : max(vSpecularColor.a, 16.0f);
					float3 sunColor = lights[0].Color.rgb * max(lights[0].Color.a, 1.0f);
					specular += sunColor * vSpecularColor.rgb * pow(NdotH, specPower) * NdotL;
				}
			}

			float specScale = (vPBRParams.z > 0.0001f) ? vPBRParams.z : 1.0f;
			finalColor.rgb += specular * specMask * specScale;
		}
	}

	if (vMaterialParams.w > 0.5f)
	{
		if (vShadowParams.x > 0.0f)
		{
			float4 worldPos4 = float4(input.WorldPos, 1.0f);
			float texelScale = (vShadowParams.z > 0.0f) ? vShadowParams.z : (1.0f / 2048.0f);
			float3 nrmWS = normalize(input.WorldNorm);
			float finalShadow = 0.0f;

			float4 sp0 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowLocal, texelScale), matShadowLocal);
			float2 uv0 = sp0.xy / sp0.w * 0.5f + 0.5f; uv0.y = 1.0f - uv0.y;
			float edge0 = min(min(uv0.x, 1.0f - uv0.x), min(uv0.y, 1.0f - uv0.y));

			if (edge0 > 0.05f)
			{
				float s0 = SampleShadowPCF(texShadowLocal, samShadow, uv0, sp0.z / sp0.w, texelScale);

				if (edge0 < 0.15f)
				{
					// Blend zone: mix with cascade 1
					float4 sp1 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowMid, texelScale), matShadowMid);
					float2 uv1 = sp1.xy / sp1.w * 0.5f + 0.5f; uv1.y = 1.0f - uv1.y;
					float s1 = SampleShadowPCF(texShadowMid, samShadow, uv1, sp1.z / sp1.w, texelScale, 0.001f);
					float blend = saturate((edge0 - 0.05f) / 0.10f);
					finalShadow = lerp(s1, s0, blend);
				}
				else
				{
					finalShadow = s0;
				}
			}
			else
			{
				float4 sp1 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowMid, texelScale), matShadowMid);
				float2 uv1 = sp1.xy / sp1.w * 0.5f + 0.5f; uv1.y = 1.0f - uv1.y;
				float edge1 = min(min(uv1.x, 1.0f - uv1.x), min(uv1.y, 1.0f - uv1.y));

				if (edge1 > 0.05f)
				{
					float s1 = SampleShadowPCF(texShadowMid, samShadow, uv1, sp1.z / sp1.w, texelScale, 0.001f);

					if (edge1 < 0.15f)
					{
						float4 sp2 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowFar, texelScale), matShadowFar);
						float2 uv2 = sp2.xy / sp2.w * 0.5f + 0.5f; uv2.y = 1.0f - uv2.y;
						float s2 = SampleShadowPCF(texShadowFar, samShadow, uv2, sp2.z / sp2.w, texelScale, 0.0015f);
						float blend = saturate((edge1 - 0.05f) / 0.10f);
						finalShadow = lerp(s2, s1, blend);
					}
					else
					{
						finalShadow = s1;
					}
				}
				else
				{
					float4 sp2 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowFar, texelScale), matShadowFar);
					float2 uv2 = sp2.xy / sp2.w * 0.5f + 0.5f; uv2.y = 1.0f - uv2.y;
					float edge2 = min(min(uv2.x, 1.0f - uv2.x), min(uv2.y, 1.0f - uv2.y));

					if (edge2 > 0.05f)
					{
						float s2 = SampleShadowPCF(texShadowFar, samShadow, uv2, sp2.z / sp2.w, texelScale, 0.0015f);

						if (edge2 < 0.15f)
						{
							float4 sp3 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowBig, texelScale), matShadowBig);
							float2 uv3 = sp3.xy / sp3.w * 0.5f + 0.5f; uv3.y = 1.0f - uv3.y;
							float s3 = SampleShadowPCF(texShadowBig, samShadow, uv3, sp3.z / sp3.w, texelScale, 0.0025f);
							float blend = saturate((edge2 - 0.05f) / 0.10f);
							finalShadow = lerp(s3, s2, blend);
						}
						else
						{
							finalShadow = s2;
						}
					}
					else
					{
						float4 sp3 = mul(ShadowOffsetPos(input.WorldPos, nrmWS, matShadowBig, texelScale), matShadowBig);
						float2 uv3 = sp3.xy / sp3.w * 0.5f + 0.5f; uv3.y = 1.0f - uv3.y;
						finalShadow = SampleShadowPCF(texShadowBig, samShadow, uv3, sp3.z / sp3.w, texelScale, 0.0025f);
					}
				}
			}

			finalShadow *= saturate(vShadowParams.x / 120.0f);
			finalColor.rgb *= (1.0f - finalShadow);
		}
		else
		{
			float2 shadowUV = input.ShadowCoord;
			if (shadowUV.x >= 0.0f && shadowUV.x <= 1.0f &&
			    shadowUV.y >= 0.0f && shadowUV.y <= 1.0f)
			{
				float fadeMargin = 0.1f;
				float edgeFade = saturate(shadowUV.x / fadeMargin)
				               * saturate((1.0f - shadowUV.x) / fadeMargin)
				               * saturate(shadowUV.y / fadeMargin)
				               * saturate((1.0f - shadowUV.y) / fadeMargin);

				float4 shadowColor = texDiffuse1.Sample(samShadow, shadowUV);
				float shadowLum = max(shadowColor.r, max(shadowColor.g, shadowColor.b));
				if (shadowLum > 0.1f)
				{
					float3 blendedShadow = lerp(float3(1,1,1), shadowColor.rgb, edgeFade);
					finalColor.rgb *= blendedShadow;
				}
			}
		}
	}

	// Height-based atmospheric fog
	finalColor.rgb = lerp(vFogColor.rgb, finalColor.rgb, input.FogFactor);

	return finalColor;
}
)";


static const char* g_szGodRaysVertexShader = R"(
struct VS_INPUT
{
	float3 Position : POSITION;
	float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	// Full-screen quad: position is already in NDC
	output.Position = float4(input.Position.xy, 0.0f, 1.0f);
	output.TexCoord = input.TexCoord;
	return output;
}
)";

static const char* g_szGodRaysPixelShader = R"(
cbuffer CBGodRays : register(b0)
{
	float4 vLightScreenPos;  // xy = light position in screen UV space, z = intensity, w = decay
	float4 vRayParams;       // x = density, y = weight, z = exposure, w = numSamples
	float4 vRayColor;        // rgb = light color, a = unused
};

Texture2D    texScene : register(t0);  // Scene color/occlusion texture
SamplerState samLinear : register(s0);

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	float2 lightPos = vLightScreenPos.xy;
	float intensity = vLightScreenPos.z;
	float decay = vLightScreenPos.w;

	float density = vRayParams.x;
	float weight = vRayParams.y;
	float exposure = vRayParams.z;
	int numSamples = (int)vRayParams.w;

	// Calculate ray direction from current pixel to light
	float2 deltaTexCoord = input.TexCoord - lightPos;
	deltaTexCoord *= 1.0f / float(numSamples) * density;

	float2 uv = input.TexCoord;
	float illuminationDecay = 1.0f;
	float3 color = float3(0, 0, 0);

	// Ray march toward light source
	[loop]
	for (int i = 0; i < numSamples; i++)
	{
		uv -= deltaTexCoord;

		// Sample scene - bright areas contribute to rays
		float3 sample = texScene.Sample(samLinear, saturate(uv)).rgb;

		// Accumulate with decay
		sample *= illuminationDecay * weight;
		color += sample;

		illuminationDecay *= decay;
	}

	// Apply exposure and light color
	color *= exposure * intensity;
	color *= vRayColor.rgb;

	return float4(color, 1.0f);
}
)";

static const D3D11_INPUT_ELEMENT_DESC g_GodRaysInputLayout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
};


#ifdef ENABLE_BLOOM

static const char* g_szBloomBrightPS = R"(
cbuffer CBBloom : register(b0)
{
	float4 vBloomParams;    // x=threshold, y=intensity
	float4 vTexelSize;      // x=1/bloomW, y=1/bloomH
	float4 vBlurDirection;  // x=H, y=V
};

Texture2D    texScene : register(t0);
SamplerState samLinear : register(s0);

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	float3 color = texScene.Sample(samLinear, input.TexCoord).rgb;
	float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));

	// Soft knee threshold
	float threshold = vBloomParams.x;
	float knee = threshold * 0.5;
	float soft = luminance - threshold + knee;
	soft = clamp(soft, 0.0, 2.0 * knee);
	soft = soft * soft / (4.0 * knee + 0.00001);
	float contribution = max(soft, luminance - threshold) / max(luminance, 0.00001);

	return float4(color * contribution * vBloomParams.y, 1.0);
}
)";

static const char* g_szBloomBlurPS = R"(
cbuffer CBBloom : register(b0)
{
	float4 vBloomParams;
	float4 vTexelSize;
	float4 vBlurDirection;
};

Texture2D    texSource : register(t0);
SamplerState samLinear : register(s0);

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	// 9-tap Gaussian (sigma ~3), weights normalized
	static const float weights[5] = { 0.227027, 0.194594, 0.121622, 0.054054, 0.016216 };

	float2 dir = float2(vBlurDirection.x * vTexelSize.x, vBlurDirection.y * vTexelSize.y);

	float3 result = texSource.Sample(samLinear, input.TexCoord).rgb * weights[0];

	[unroll]
	for (int i = 1; i < 5; i++)
	{
		float2 offset = dir * float(i);
		result += texSource.Sample(samLinear, input.TexCoord + offset).rgb * weights[i];
		result += texSource.Sample(samLinear, input.TexCoord - offset).rgb * weights[i];
	}

	return float4(result, 1.0);
}
)";

static const char* g_szBloomCompositePS = R"(
Texture2D    texScene    : register(t0);
Texture2D    texBloom    : register(t1);
Texture2D    texGodRays  : register(t2);
Texture2D    texSSAO     : register(t3);
SamplerState samLinear : register(s0);

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	float3 scene = texScene.Sample(samLinear, input.TexCoord).rgb;
	float3 bloom = texBloom.Sample(samLinear, input.TexCoord).rgb;
	float3 godRays = texGodRays.Sample(samLinear, input.TexCoord).rgb;
	float4 aoSample = texSSAO.Sample(samLinear, input.TexCoord);
	float ao = (aoSample.a > 0.5) ? aoSample.r : 1.0;
	return float4(scene * ao + bloom + godRays, 1.0);
}
)";

#endif // ENABLE_BLOOM

#ifdef ENABLE_SSAO
//////////////////////////////////////////////////////////////////////////
// HLSL Shader Code - SSAO (Screen-Space Ambient Occlusion)
//////////////////////////////////////////////////////////////////////////

static const char* g_szDepthResolvePS = R"(
Texture2DMS<float> texDepthMS : register(t0);

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	int2 coord = int2(input.Position.xy);
	float depth = texDepthMS.Load(coord, 0).r;
	return float4(depth, 0, 0, 0);
}
)";

static const char* g_szSSAO_PS = R"(
cbuffer CBSSAO : register(b0)
{
	matrix matProjection;
	matrix matInvProjection;
	float4 vSSAOParams;     // x=radius, y=bias, z=intensity
	float4 vTexelSize;      // x=1/ssaoW, y=1/ssaoH, z=1/fullW, w=1/fullH
	float4 vSampleKernel[16];
};

Texture2D<float> texDepth : register(t0);
Texture2D<float2> texNoise : register(t1);
SamplerState samPoint : register(s1);

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

float3 GetViewPos(float2 uv)
{
	float depth = texDepth.SampleLevel(samPoint, uv, 0);
	// NDC: x,y in [-1,1], z = depth [0,1]
	float4 clipPos = float4(uv * 2.0 - 1.0, depth, 1.0);
	clipPos.y = -clipPos.y;
	float4 viewPos = mul(clipPos, matInvProjection);
	return viewPos.xyz / viewPos.w;
}

float3 ReconstructNormal(float2 uv)
{
	float3 center = GetViewPos(uv);
	// Use smallest depth difference to avoid edge artifacts
	float3 ddx1 = GetViewPos(uv + float2(vTexelSize.z, 0)) - center;
	float3 ddx2 = center - GetViewPos(uv - float2(vTexelSize.z, 0));
	float3 ddy1 = GetViewPos(uv + float2(0, vTexelSize.w)) - center;
	float3 ddy2 = center - GetViewPos(uv - float2(0, vTexelSize.w));

	float3 dx = (abs(ddx1.z) < abs(ddx2.z)) ? ddx1 : ddx2;
	float3 dy = (abs(ddy1.z) < abs(ddy2.z)) ? ddy1 : ddy2;

	return normalize(cross(dx, dy));
}

float4 main(PS_INPUT input) : SV_TARGET
{
	float depth = texDepth.SampleLevel(samPoint, input.TexCoord, 0);
	if (depth >= 0.9999)
		return float4(1, 1, 1, 1); // Sky - no occlusion

	float3 viewPos = GetViewPos(input.TexCoord);
	float3 normal = ReconstructNormal(input.TexCoord);

	float2 noiseUV = input.TexCoord / (vTexelSize.xy * 4.0);
	float2 noiseVal = texNoise.SampleLevel(samPoint, noiseUV, 0) * 2.0 - 1.0;

	// Build TBN from normal + noise rotation
	float3 tangent = normalize(noiseVal.x * cross(normal, float3(0, 0, 1)) +
	                           noiseVal.y * cross(normal, float3(0, 1, 0)));
	if (length(tangent) < 0.001)
		tangent = float3(1, 0, 0);
	float3 bitangent = cross(normal, tangent);
	float3x3 TBN = float3x3(tangent, bitangent, normal);

	float radius = vSSAOParams.x;
	float bias = vSSAOParams.y;
	float occlusion = 0.0;

	[unroll]
	for (int i = 0; i < 16; ++i)
	{
		// Transform sample to view space
		float3 sampleDir = mul(vSampleKernel[i].xyz, TBN);
		float3 samplePos = viewPos + sampleDir * radius;

		// Project to screen UV
		float4 offset = mul(float4(samplePos, 1.0), matProjection);
		offset.xy /= offset.w;
		offset.xy = offset.xy * 0.5 + 0.5;
		offset.y = 1.0 - offset.y;

		// Sample depth at projected position
		float sampleDepth = texDepth.SampleLevel(samPoint, offset.xy, 0);
		float3 sampleViewPos = GetViewPos(offset.xy);

		// Range check + occlusion
		float rangeCheck = smoothstep(0.0, 1.0, radius / max(abs(viewPos.z - sampleViewPos.z), 0.001));
		occlusion += (sampleViewPos.z >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
	}

	float ao = 1.0 - (occlusion / 16.0) * vSSAOParams.z;
	return float4(saturate(ao), 0, 0, 1);
}
)";

static const char* g_szSSAOBlurPS = R"(
cbuffer CBSSAO : register(b0)
{
	matrix matProjection;
	matrix matInvProjection;
	float4 vSSAOParams;
	float4 vTexelSize;      // x=1/ssaoW, y=1/ssaoH
	float4 vSampleKernel[16];
};

Texture2D<float> texSSAO  : register(t0);
Texture2D<float> texDepth : register(t1);
SamplerState samPoint : register(s1);

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	float centerDepth = texDepth.SampleLevel(samPoint, input.TexCoord, 0);
	float result = 0.0;
	float totalWeight = 0.0;

	[unroll]
	for (int x = -2; x <= 2; ++x)
	{
		[unroll]
		for (int y = -2; y <= 2; ++y)
		{
			float2 offset = float2(float(x), float(y)) * vTexelSize.xy;
			float2 sampleUV = input.TexCoord + offset;
			float sampleAO = texSSAO.SampleLevel(samPoint, sampleUV, 0);
			float sampleDepth = texDepth.SampleLevel(samPoint, sampleUV, 0);

			// Bilateral weight: reject samples across depth edges
			float depthDiff = abs(centerDepth - sampleDepth);
			float w = exp(-depthDiff * 1000.0);
			result += sampleAO * w;
			totalWeight += w;
		}
	}

	return float4(result / max(totalWeight, 0.001), 0, 0, 1);
}
)";
#endif // ENABLE_SSAO


static const char* g_szMeshVTFVertexShader = R"(
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
	matrix matWorld;         // unused in VTF mode, but kept for CB layout compatibility
	matrix matWorldViewProj; // unused
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
	Light lights[16];
	float4 globalAmbient;
	int numActiveLights;
	int3 _padLighting;
};

// VTF instance data texture (bound to VS t8)
Texture2D<float4> texInstanceData : register(t8);

struct VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal   : NORMAL;
	float2 TexCoord : TEXCOORD0;
	uint   InstanceID : SV_InstanceID;
};

struct VS_OUTPUT
{
	float4 Position   : SV_POSITION;
	float3 WorldPos   : TEXCOORD0;
	float3 WorldNorm  : TEXCOORD1;
	float2 TexCoord   : TEXCOORD2;
	float  FogFactor  : TEXCOORD3;
	float4 InstColor  : TEXCOORD4;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	uint baseTexel = input.InstanceID * 4;
	float4 row0 = texInstanceData.Load(int3(baseTexel + 0, 0, 0));
	float4 row1 = texInstanceData.Load(int3(baseTexel + 1, 0, 0));
	float4 row2 = texInstanceData.Load(int3(baseTexel + 2, 0, 0));
	float4 row3 = texInstanceData.Load(int3(baseTexel + 3, 0, 0));

	// Reconstruct full world matrix (row 3 = translation)
	matrix instWorld = matrix(row0, row1, row2, row3);

	float4 worldPos = mul(float4(input.Position, 1.0f), instWorld);
	float4 viewPos = mul(worldPos, matView);
	output.Position = mul(viewPos, matProjection);
	output.WorldPos = worldPos.xyz;
	output.WorldNorm = normalize(mul((float3x3)instWorld, input.Normal));
	output.TexCoord = input.TexCoord;
	output.InstColor = float4(1, 1, 1, 1);  // Diffuse color from cbuffer

	// Height-based atmospheric fog
	if (vFogParams.w > 0.5f)
	{
		output.FogFactor = saturate((vFogParams.y - output.Position.z) / (vFogParams.y - vFogParams.x));
	}
	else
	{
		output.FogFactor = 1.0f;
	}

	return output;
}
)";

static const char* g_szMeshVTFPixelShader = R"(
#define MAX_LIGHTS 16

struct Light
{
	float4 Position;
	float4 Direction;
	float4 Color;
	float4 Attenuation;
};

cbuffer CBPerFrame : register(b0)
{
	matrix matView;
	matrix matProjection;
	float4 vCameraPos;
	float4 vFogParams;
	float4 vFogColor;
	float4 vTime;
	float4 vSunDirection;
	float4 vShadowParams;     // x = opacity (0-120)
	float4 vCascadeSplits;    // x,y,z,w = view-space depth thresholds for cascades 0-3
	matrix matShadowBig;      // Cascade 3 (far)
	matrix matShadowLocal;    // Cascade 0 (near)
	matrix matShadowMid;      // Cascade 1
	matrix matShadowFar;      // Cascade 2
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
};

cbuffer CBLighting : register(b2)
{
	Light lights[MAX_LIGHTS];
	float4 globalAmbient;
	int numActiveLights;
	int3 _padLighting;
};

Texture2D    texDiffuse : register(t0);
Texture2D    texDiffuse1 : register(t1);
Texture2D    texSphere : register(t6);   // sphere map (off t1 so shadow-receive cannot evict it)
SamplerState samLinear  : register(s0);

struct PS_INPUT
{
	float4 Position   : SV_POSITION;
	float3 WorldPos   : TEXCOORD0;
	float3 WorldNorm  : TEXCOORD1;
	float2 TexCoord   : TEXCOORD2;
	float  FogFactor  : TEXCOORD3;
	float4 InstColor  : TEXCOORD4;
};

float3 CalcDirectionalLight(Light light, float3 normal)
{
	float3 lightDir = light.Direction.xyz;
	float dirLen = length(lightDir);
	if (dirLen < 0.001f)
		return float3(0, 0, 0);

	lightDir /= dirLen;
	float NdotL = saturate(dot(normal, -lightDir));

	float3 lightColor = light.Color.rgb;
	float colorMag = dot(lightColor, lightColor);
	if (colorMag < 0.01f)
		lightColor = float3(1.0f, 1.0f, 1.0f);
	lightColor *= max(light.Color.a, 1.0f);

	return lightColor * NdotL;
}

float4 main(PS_INPUT input) : SV_TARGET
{
	float4 texColor = texDiffuse.Sample(samLinear, input.TexCoord);

	// DX11 fix: unbound textures return (0,0,0,0)
	if (texColor.r < 0.001f && texColor.g < 0.001f && texColor.b < 0.001f && texColor.a < 0.001f)
	{
		discard;
	}

	// Alpha test
	if (vMaterialParams.y > 0.5f && texColor.a < vMaterialParams.x)
		discard;

	// Lighting
	float3 normal = normalize(input.WorldNorm);
	float3 ambient = max(globalAmbient.rgb, float3(0.4f, 0.4f, 0.4f));

#ifdef PBR_ENABLED
	float3 V = normalize(vCameraPos.xyz - input.WorldPos);
	float3 albedo = texColor.rgb * vDiffuseColor.rgb;
	float roughness = vPBRParams.x;
	if (roughness < 0.01f) roughness = sqrt(2.0f / (max(vMaterialParams.z, 1.0f) + 2.0f));
	roughness = clamp(roughness, 0.04f, 1.0f);
	float metallic = vPBRParams.y;
	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
	float3 Lo = float3(0, 0, 0);

	if (lights[0].Direction.w > 0.5f)
	{
		float3 L = normalize(-lights[0].Direction.xyz);
		float3 lc = lights[0].Color.rgb * max(lights[0].Color.a, 1.0f);
		float cm = dot(lc, lc);
		if (cm < 0.01f) lc = float3(1, 1, 1);
		Lo += EvalPBRLight(L, normal, V, lc, 1.0f, albedo, roughness, metallic, F0);
	}

	int maxLights = min(numActiveLights, MAX_LIGHTS);
	for (int i = 1; i < maxLights; ++i)
	{
		if (lights[i].Direction.w < 0.5f) continue;
		float3 lightVec = lights[i].Position.xyz - input.WorldPos;
		float dist = length(lightVec);
		float3 L = lightVec / max(dist, 0.001f);
		float atten = 1.0f / (lights[i].Attenuation.x + lights[i].Attenuation.y * dist + lights[i].Attenuation.z * dist * dist);
		if (dist > lights[i].Attenuation.w && lights[i].Attenuation.w > 0.0f) atten = 0.0f;
		if (atten <= 0.0f) continue;
		float3 lc = lights[i].Color.rgb * max(lights[i].Color.a, 1.0f);
		Lo += EvalPBRLight(L, normal, V, lc, atten, albedo, roughness, metallic, F0);
	}

	float4 finalColor = float4(saturate(ambient * albedo + Lo), texColor.a * vDiffuseColor.a);
#else
	float3 diffuse = float3(0, 0, 0);

	if (lights[0].Direction.w > 0.5f)
	{
		diffuse = CalcDirectionalLight(lights[0], normal);
	}

	float3 lighting = saturate(ambient + diffuse);

	float4 finalColor = float4(texColor.rgb * vDiffuseColor.rgb * lighting, texColor.a * vDiffuseColor.a);
#endif

	{
		float specMag = dot(vSpecularColor.rgb, vSpecularColor.rgb);
		if (specMag > 0.0001f)
		{
			float  specMask   = texColor.a;
			float3 V          = normalize(vCameraPos.xyz - input.WorldPos);
			float3 viewNormal = normalize(mul((float3x3)matView, normal));
			float2 sphereUV   = viewNormal.xy * 0.5f + 0.5f;
			float3 env        = texSphere.Sample(samLinear, sphereUV).rgb;
			float3 specular   = env * vSpecularColor.rgb;

			if (lights[0].Direction.w > 0.5f)
			{
				float3 L = normalize(-lights[0].Direction.xyz);
				float NdotL = saturate(dot(normal, L));
				if (NdotL > 0.0f)
				{
					float3 H = normalize(L + V);
					float NdotH = saturate(dot(normal, H));
					float specPower = (vPBRParams.w > 0.0001f) ? vPBRParams.w : max(vSpecularColor.a, 16.0f);
					float3 sunColor = lights[0].Color.rgb * max(lights[0].Color.a, 1.0f);
					specular += sunColor * vSpecularColor.rgb * pow(NdotH, specPower) * NdotL;
				}
			}

			float specScale = (vPBRParams.z > 0.0001f) ? vPBRParams.z : 1.0f;
			finalColor.rgb += specular * specMask * specScale;
		}
	}

	// Height-based atmospheric fog
	finalColor.rgb = lerp(vFogColor.rgb, finalColor.rgb, input.FogFactor);

	return finalColor;
}
)";

static const D3D11_INPUT_ELEMENT_DESC g_MeshVTFInputLayout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
};


static const char* g_szShadowVTFVertexShader = R"(
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
};

Texture2D<float4> texInstanceData : register(t8);

struct VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal   : NORMAL;
	float2 TexCoord : TEXCOORD0;
	uint   InstanceID : SV_InstanceID;
};

struct VS_OUTPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	float  Depth    : TEXCOORD1;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	uint baseTexel = input.InstanceID * 4;
	float4 row0 = texInstanceData.Load(int3(baseTexel + 0, 0, 0));
	float4 row1 = texInstanceData.Load(int3(baseTexel + 1, 0, 0));
	float4 row2 = texInstanceData.Load(int3(baseTexel + 2, 0, 0));
	float4 row3 = texInstanceData.Load(int3(baseTexel + 3, 0, 0));

	matrix instWorld = matrix(row0, row1, row2, row3);

	float4 worldPos = mul(float4(input.Position, 1.0f), instWorld);
	float4 viewPos = mul(worldPos, matView);
	output.Position = mul(viewPos, matProjection);
	output.TexCoord = input.TexCoord;
	output.Depth = output.Position.z / output.Position.w;

	return output;
}
)";

static const char* g_szShadowVTFPixelShader = R"(
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
};

Texture2D    texDiffuse : register(t0);
SamplerState samLinear  : register(s0);

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	float  Depth    : TEXCOORD1;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	if (vMaterialParams.y > 0.5f)
	{
		float4 texColor = texDiffuse.Sample(samLinear, input.TexCoord);
		if (texColor.a < vMaterialParams.x)
			discard;
	}
	return float4(input.Depth, 0, 0, 1);
}
)";

// Shadow VTF input layout: same as shadow (PNT)
static const D3D11_INPUT_ELEMENT_DESC g_ShadowVTFInputLayout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
};


static const char* g_szSpeedTreeVTFVertexShader = R"(
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
};

Texture2D<float4> texInstanceData : register(t8);

struct VS_INPUT
{
	float3 Position    : POSITION;
	float4 Color       : COLOR0;
	float2 TexCoord    : TEXCOORD0;
	float2 ShadowCoord : TEXCOORD1;
	float2 WindData    : TEXCOORD2;
	uint   InstanceID  : SV_InstanceID;
};

struct VS_OUTPUT
{
	float4 Position  : SV_POSITION;
	float4 Color     : COLOR0;
	float2 TexCoord  : TEXCOORD0;
	float  FogFactor : TEXCOORD1;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	// Fetch per-instance tree position from VTF
	uint baseTexel = input.InstanceID * 4;
	float4 row0 = texInstanceData.Load(int3(baseTexel + 0, 0, 0));
	float4 row1 = texInstanceData.Load(int3(baseTexel + 1, 0, 0));
	float4 row2 = texInstanceData.Load(int3(baseTexel + 2, 0, 0));
	float4 row3 = texInstanceData.Load(int3(baseTexel + 3, 0, 0));

	// Tree world matrix (row 3 = translation)
	matrix instWorld = matrix(row0, row1, row2, row3);

	float3 localPos = input.Position;

	// Wind animation (same as non-VTF version)
	float heightFactor = saturate(localPos.z * 0.01f);
	float time = vTime.x;
	float3 treeWorldPos = row3.xyz;
	float primaryWave = sin(time * 1.5f + treeWorldPos.x * 0.05f + treeWorldPos.y * 0.05f);
	float secondaryWave = sin(time * 3.7f + treeWorldPos.x * 0.15f + treeWorldPos.y * 0.12f) * 0.3f;
	float swayAmount = (primaryWave + secondaryWave) * heightFactor * 3.0f;
	localPos += float3(swayAmount, swayAmount * 0.7f, 0.0f);

	float4 worldPos = mul(float4(localPos, 1.0f), instWorld);
	float4 viewPos = mul(worldPos, matView);
	output.Position = mul(viewPos, matProjection);
	output.Color = input.Color;
	output.TexCoord = input.TexCoord;

	// Height-based atmospheric fog
	if (vFogParams.w > 0.5f)
	{
		output.FogFactor = saturate((vFogParams.y - output.Position.z) / (vFogParams.y - vFogParams.x));
	}
	else
	{
		output.FogFactor = 1.0f;
	}

	return output;
}
)";

static const char* g_szSpeedTreeVTFPixelShader = R"(
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

float4 main(PS_INPUT input) : SV_TARGET
{
	float4 texColor = texDiffuse.Sample(samLinear, input.TexCoord);
	float4 finalColor = texColor * input.Color;

	if (vMaterialParams.y > 0.5f && finalColor.a < vMaterialParams.x)
		discard;

	if (vRenderFlags.x > 0.5f)
	{
		// Shadow mode - output depth for R32F shadow map
		return float4(input.Position.z, 0, 0, 1);
	}

	// Height-based atmospheric fog
	finalColor.rgb = lerp(vFogColor.rgb, finalColor.rgb, input.FogFactor);

	return finalColor;
}
)";

static const D3D11_INPUT_ELEMENT_DESC g_SpeedTreeVTFInputLayout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT,    0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
};


static const char* g_szMesh2TexVTFVertexShader = R"(
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
};

// VTF instance data texture (bound to VS t8)
Texture2D<float4> texInstanceData : register(t8);

struct VS_INPUT
{
	float3 Position  : POSITION;
	float3 Normal    : NORMAL;
	float2 TexCoord0 : TEXCOORD0;
	float2 TexCoord1 : TEXCOORD1;
	uint   InstanceID : SV_InstanceID;
};

struct VS_OUTPUT
{
	float4 Position   : SV_POSITION;
	float3 WorldPos   : TEXCOORD0;
	float3 WorldNorm  : TEXCOORD1;
	float2 TexCoord0  : TEXCOORD2;
	float2 TexCoord1  : TEXCOORD3;
	float  FogFactor  : TEXCOORD4;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	// Fetch instance world matrix from VTF texture
	uint baseTexel = input.InstanceID * 4;
	float4 row0 = texInstanceData.Load(int3(baseTexel + 0, 0, 0));
	float4 row1 = texInstanceData.Load(int3(baseTexel + 1, 0, 0));
	float4 row2 = texInstanceData.Load(int3(baseTexel + 2, 0, 0));
	float4 row3 = texInstanceData.Load(int3(baseTexel + 3, 0, 0));

	matrix instWorld = matrix(row0, row1, row2, row3);

	float4 worldPos = mul(float4(input.Position, 1.0f), instWorld);
	float4 viewPos = mul(worldPos, matView);
	output.Position = mul(viewPos, matProjection);
	output.WorldPos = worldPos.xyz;
	output.WorldNorm = normalize(mul((float3x3)instWorld, input.Normal));
	output.TexCoord0 = input.TexCoord0;
	output.TexCoord1 = input.TexCoord1;

	// Height-based atmospheric fog
	if (vFogParams.w > 0.5f)
	{
		output.FogFactor = saturate((vFogParams.y - output.Position.z) / (vFogParams.y - vFogParams.x));
	}
	else
	{
		output.FogFactor = 1.0f;
	}

	return output;
}
)";

static const char* g_szMesh2TexVTFPixelShader = R"(
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
#ifdef PBR_ENABLED
	float3 normal = normalize(input.WorldNorm);
	float3 V = normalize(vCameraPos.xyz - input.WorldPos);
	float3 albedo = blendedTex.rgb * vDiffuseColor.rgb;
	float roughness = vPBRParams.x;
	if (roughness < 0.01f) roughness = 0.7f;
	roughness = clamp(roughness, 0.04f, 1.0f);
	float metallic = vPBRParams.y;
	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
	float3 ambient = max(globalAmbient.rgb, float3(0.4f, 0.4f, 0.4f));
	float3 Lo = float3(0, 0, 0);

	if (lights[0].Direction.w > 0.5f)
	{
		float3 L = normalize(-lights[0].Direction.xyz);
		float3 lc = lights[0].Color.rgb * max(lights[0].Color.a, 1.0f);
		Lo += EvalPBRLight(L, normal, V, lc, 1.0f, albedo, roughness, metallic, F0);
	}

	int maxLights = min(numActiveLights, MAX_LIGHTS);
	for (int i = 1; i < maxLights; ++i)
	{
		if (lights[i].Direction.w < 0.5f) continue;
		float3 lightVec = lights[i].Position.xyz - input.WorldPos;
		float dist = length(lightVec);
		float3 L = lightVec / max(dist, 0.001f);
		float atten = 1.0f / (lights[i].Attenuation.x + lights[i].Attenuation.y * dist + lights[i].Attenuation.z * dist * dist);
		if (dist > lights[i].Attenuation.w && lights[i].Attenuation.w > 0.0f) atten = 0.0f;
		if (atten <= 0.0f) continue;
		float3 lc = lights[i].Color.rgb * max(lights[i].Color.a, 1.0f);
		Lo += EvalPBRLight(L, normal, V, lc, atten, albedo, roughness, metallic, F0);
	}

	float4 finalColor = float4(saturate(ambient * albedo + Lo), blendedTex.a * vDiffuseColor.a);
#else
	// Apply material diffuse color
	float4 finalColor = blendedTex * vDiffuseColor;
#endif

	// Height-based atmospheric fog
	finalColor.rgb = lerp(vFogColor.rgb, finalColor.rgb, input.FogFactor);

	return finalColor;
}
)";

static const D3D11_INPUT_ELEMENT_DESC g_Mesh2TexVTFInputLayout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,    0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

//////////////////////////////////////////////////////////////////////////
// Input Layout Definitions
//////////////////////////////////////////////////////////////////////////

static const D3D11_INPUT_ELEMENT_DESC g_UIInputLayout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},  // Colour is BGRA in memory
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

static const D3D11_INPUT_ELEMENT_DESC g_MeshInputLayout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

static const D3D11_INPUT_ELEMENT_DESC g_Mesh2TexInputLayout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,    0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

static const D3D11_INPUT_ELEMENT_DESC g_TerrainInputLayout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

static const D3D11_INPUT_ELEMENT_DESC g_WaterInputLayout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},  // Colour is BGRA in memory
};

static const D3D11_INPUT_ELEMENT_DESC g_SkyInputLayout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

static const D3D11_INPUT_ELEMENT_DESC g_ParticleInputLayout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

// PDT input layout for effects (same as particle)
static const D3D11_INPUT_ELEMENT_DESC g_PDTInputLayout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

// Shadow shader uses same layout as Mesh (PNT vertices)
static const D3D11_INPUT_ELEMENT_DESC g_ShadowInputLayout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

static const D3D11_INPUT_ELEMENT_DESC g_SpeedTreeInputLayout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},  // 12 bytes
	{"COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},  // 4 bytes
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},  // 8 bytes
	{"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},  // 8 bytes (shadow coords)
	{"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT,    0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},  // 8 bytes (wind index + weight)
};

static const D3D11_INPUT_ELEMENT_DESC g_SpeedTreeLeafInputLayout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},  // 12 bytes
	{"COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,     0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},  // 4 bytes
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},  // 8 bytes
	{"TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},  // 16 bytes (wind/leaf data)
};

static const D3D11_INPUT_ELEMENT_DESC g_MeshNormalInputLayout[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

static const D3D11_INPUT_ELEMENT_DESC g_SkinnedMeshInputLayout[] = {
	{"POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},  // 12 bytes
	{"NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},  // 12 bytes
	{"TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},  // 8 bytes
	{"BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},  // 16 bytes (4 weights)
	{"BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,      0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0},  // 4 bytes (4 indices)
};

//////////////////////////////////////////////////////////////////////////
// CShaderManager Implementation
//////////////////////////////////////////////////////////////////////////

thread_local UINT CShaderManager::t_subsystemDrawCount = 0;

CShaderManager::CShaderManager()
	: m_pDevice(nullptr)
	, m_pContext(nullptr)
	, m_bInitialized(false)
	, m_iFrameCount(0)
	, m_eCurrentShader(SHADER_NONE)
	, m_CurrentTopology(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
	, m_pCBPerFrame(nullptr)
	, m_pCBPerObject(nullptr)
	, m_pCBLighting(nullptr)
	, m_pCBSpeedTree(nullptr)
	, m_pCBSkinning(nullptr)
	, m_pCBGodRays(nullptr)
	, m_pCBSkyGradient(nullptr)
	, m_bSkyGradientDirty(false)
	, m_bPerFrameDirty(true)
	, m_bPerObjectDirty(true)
	, m_bLightingDirty(true)
	, m_bSpeedTreeDirty(true)
	, m_bSkinningDirty(false)
	, m_iActiveBoneCount(0)
	, m_bParticleBatchingActive(false)
	, m_globalDrawCount(0)
	, m_bGodRaysDirty(false)
	, m_bGodRaysEnabled(false)
#ifdef ENABLE_BLOOM
	, m_pCBBloom(nullptr)
	, m_bBloomEnabled(true)
#endif
#ifdef ENABLE_SSAO
	, m_pCBSSAO(nullptr)
	, m_bSSAODirty(true)
	, m_pSSAONoiseTex(nullptr)
	, m_pSSAONoiseSRV(nullptr)
#endif
	, m_pDefaultTexture(nullptr)
	, m_pDefaultTextureSRV(nullptr)
	, m_pTransparentTexture(nullptr)
	, m_pTransparentTextureSRV(nullptr)
	, m_pActiveDefaultTextureSRV(nullptr)
	, m_pSamplerLinear(nullptr)
	, m_pSamplerPoint(nullptr)
	, m_pSamplerClamp(nullptr)
	, m_pSamplerShadowCmp(nullptr)
	// Render state management
	, m_pStateCache(nullptr)
	, m_pCurrentBlendState(nullptr)
	, m_pCurrentRasterizerState(nullptr)
	, m_pCurrentDepthStencilState(nullptr)
	, m_bBlendStateDirty(true)
	, m_bRasterizerStateDirty(true)
	, m_bDepthStencilStateDirty(true)
	, m_pCurrentIndexBuffer(nullptr)
	, m_IndexFormat(DXGI_FORMAT_R16_UINT)
	, m_IndexOffset(0)
	, m_pDynamicVertexBuffer(nullptr)
	, m_pDynamicIndexBuffer(nullptr)
	, m_CurrentInputLayout(INPUT_LAYOUT_PDT)
	, m_SavedInputLayout(INPUT_LAYOUT_PDT)
	, m_bLightingEnabled(true)
	, m_bFogEnabled(false)
	, m_bAlphaTestEnabled(false)
	, m_dwAlphaTestRef(0)
	, m_dwSkyTint(0xFFFFFFFF)
	, m_dwParticleColor(0xFFFFFFFF)
	, m_dwDynamicVBOffset(0)
	, m_dwDynamicIBOffset(0)
	, m_bDynamicBufferNeedsDiscard(true)
	, m_dwSkinningPoolIndex(0)
	, m_pCBParticleCS(nullptr)
	, m_pParticleCSIB(nullptr)
	, m_bComputeParticlesAvailable(false)
	, m_pCBFlyTraceCS(nullptr)
	, m_pFlyTraceCSIB(nullptr)
	, m_bFlyTraceCSAvailable(false)
	, m_pCBWeaponTraceCS(nullptr)
	, m_bWeaponTraceCSAvailable(false)
{
	ZeroMemory(m_ComputeShaders, sizeof(m_ComputeShaders));
	ZeroMemory(&m_cbParticleCS, sizeof(m_cbParticleCS));
	ZeroMemory(&m_cbFlyTraceCS, sizeof(m_cbFlyTraceCS));
	ZeroMemory(&m_cbWeaponTraceCS, sizeof(m_cbWeaponTraceCS));
	ZeroMemory(m_pSkinningCBPool, sizeof(m_pSkinningCBPool));
	ZeroMemory(&m_cbPerFrame, sizeof(m_cbPerFrame));
	ZeroMemory(&m_cbPerObject, sizeof(m_cbPerObject));
	ZeroMemory(&m_cbLighting, sizeof(m_cbLighting));
	ZeroMemory(&m_cbSpeedTree, sizeof(m_cbSpeedTree));
#ifdef ENABLE_SSAO
	ZeroMemory(&m_cbSSAO, sizeof(m_cbSSAO));
#endif
	ZeroMemory(m_Shaders, sizeof(m_Shaders));

	// Initialize SpeedTree wind matrices to identity
	for (int i = 0; i < SPEEDTREE_NUM_WIND_MATRICES; ++i)
	{
		m_cbSpeedTree.matWindMatrices[i] = XMMatrixIdentity();
	}
	ZeroMemory(m_Streams, sizeof(m_Streams));
	ZeroMemory(m_Matrices, sizeof(m_Matrices));
	ZeroMemory(m_SavedMatrices, sizeof(m_SavedMatrices));
	ZeroMemory(&m_CurrentMaterial, sizeof(m_CurrentMaterial));
	ZeroMemory(&m_SavedMaterial, sizeof(m_SavedMaterial));
	ZeroMemory(m_SamplerStates, sizeof(m_SamplerStates));
	ZeroMemory(m_pTextures, sizeof(m_pTextures));

	// Initialize sampler states to defaults
	for (DWORD i = 0; i < MAX_SAMPLER_SLOTS; ++i)
	{
		m_SamplerStates[i].minFilter = FILTER_LINEAR;
		m_SamplerStates[i].magFilter = FILTER_LINEAR;
		m_SamplerStates[i].mipFilter = FILTER_LINEAR;
		m_SamplerStates[i].addressU = ADDRESS_WRAP;
		m_SamplerStates[i].addressV = ADDRESS_WRAP;
		m_SamplerStates[i].addressW = ADDRESS_WRAP;
		m_SamplerStates[i].dirty = true;
	}

	// Initialize default material
	m_CurrentMaterial.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
	m_CurrentMaterial.Specular = Color(1.0f, 1.0f, 1.0f, 1.0f);
	m_CurrentMaterial.Emissive = Color(0.0f, 0.0f, 0.0f, 0.0f);
	m_CurrentMaterial.Ambient = Color(0.2f, 0.2f, 0.2f, 1.0f);
	m_CurrentMaterial.Power = 32.0f;

	m_RenderState.SetDefaults();

	// Initialize matrices to identity
	for (DWORD i = 0; i < MAX_TRANSFORMS; ++i)
	{
		MatrixIdentity(&m_Matrices[i]);
		MatrixIdentity(&m_SavedMatrices[i]);
	}

	m_cbLighting.globalAmbient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);  // Moderate ambient
	m_cbLighting.numActiveLights = 1;

	m_cbLighting.lights[0].Position = XMFLOAT4(0.0f, 0.0f, 0.0f, (float)LIGHT_DIRECTIONAL);
	m_cbLighting.lights[0].Direction = XMFLOAT4(-0.5f, -0.5f, -0.707f, 1.0f);  // w=1 enabled, matches default sun direction
	m_cbLighting.lights[0].Color = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);  // Soft white directional
	m_cbLighting.lights[0].Attenuation = XMFLOAT4(1.0f, 0.0f, 0.0f, 10000.0f);

	// Defaults
	m_cbPerFrame.matView = XMMatrixIdentity();
	m_cbPerFrame.matProjection = XMMatrixIdentity();
	m_cbPerFrame.vCameraPos = XMFLOAT4(0.0f, 0.0f, 0.0f, 1024.0f);  // w = viewport width
	m_cbPerFrame.vFogParams = XMFLOAT4(0.0f, 1000.0f, 768.0f, 0.0f);  // z = viewport height, w = fog disabled
	m_cbPerFrame.vFogColor = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	m_cbPerFrame.vTime = XMFLOAT4(0.0f, 0.0f, 0.5f, 0.0f);  // z = cloud layer2 speed multiplier
	m_cbPerFrame.vSunDirection = XMFLOAT4(-0.5f, -0.5f, -0.707f, 1.0f);  // Default sun direction behind/above camera, w = intensity
	// Note: Lighting data is now in CBLighting (m_cbLighting)

	m_cbPerObject.matWorld = XMMatrixIdentity();
	m_cbPerObject.matWorldViewProj = XMMatrixIdentity();
	m_cbPerObject.matTexture0 = XMMatrixIdentity();
	m_cbPerObject.matTexture1 = XMMatrixIdentity();
	m_cbPerObject.vDiffuseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_cbPerObject.vSkyTint = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_cbPerObject.vParticleColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_cbPerObject.vSpecularColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 32.0f);  // RGB = color, A = power
	m_cbPerObject.vEmissiveColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	m_cbPerObject.vMaterialParams = XMFLOAT4(0.0f, 0.0f, 32.0f, 0.0f);
	m_cbPerObject.vPBRParams = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);  // roughness=0 means auto-derive, metallic=0
}

CShaderManager::~CShaderManager()
{
	Shutdown();
}

bool CShaderManager::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	if (m_bInitialized)
		return true;

	if (!pDevice || !pContext)
		return false;

	m_pDevice = pDevice;
	m_pContext = pContext;

	{
		m_pContext1 = nullptr;
		pContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)&m_pContext1);

		D3D11_FEATURE_DATA_D3D11_OPTIONS opts = {};
		if (SUCCEEDED(pDevice->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &opts, sizeof(opts))))
		{
			m_bCBRingSupported = (m_pContext1 != nullptr)
			                  && opts.ConstantBufferOffsetting
			                  && opts.MapNoOverwriteOnDynamicConstantBuffer;
		}

		m_bCBRingSupported = false;
	}

	if (!CompileAllShaders())
	{
		TraceError("CShaderManager: Failed to compile shaders");
		Shutdown();
		return false;
	}

	if (!CreateConstantBuffers())
	{
		TraceError("CShaderManager: Failed to create constant buffers");
		Shutdown();
		return false;
	}

	if (!CreateDefaultTexture())
	{
		TraceError("CShaderManager: Failed to create default texture");
		Shutdown();
		return false;
	}

	if (!CreateSamplerStates())
	{
		TraceError("CShaderManager: Failed to create sampler states");
		Shutdown();
		return false;
	}


	// Create state object cache
	m_pStateCache = new CStateObjectCache(pDevice);
	if (!m_pStateCache)
	{
		TraceError("CShaderManager: Failed to create state object cache");
		Shutdown();
		return false;
	}

	// Create dynamic buffers for DrawDynamic
	if (!CreateDynamicBuffers())
	{
		TraceError("CShaderManager: Failed to create dynamic buffers");
		Shutdown();
		return false;
	}

	if (!InitParticleCSResources())
	{
		Tracef("CShaderManager: Particle CS not available, using CPU fallback\n");
		m_bComputeParticlesAvailable = false;
	}

	if (!InitFlyTraceCSResources())
	{
		Tracef("CShaderManager: FlyTrace CS not available, using CPU fallback\n");
		m_bFlyTraceCSAvailable = false;
	}

	if (!InitWeaponTraceCSResources())
	{
		Tracef("CShaderManager: WeaponTrace CS not available, using CPU fallback\n");
		m_bWeaponTraceCSAvailable = false;
	}

	m_bInitialized = true;

	D3D11_VIEWPORT viewport = {};
	UINT numViewports = 1;
	pContext->RSGetViewports(&numViewports, &viewport);
	if (viewport.Width > 0 && viewport.Height > 0)
	{
		SetViewportSize(viewport.Width, viewport.Height);
	}
	else
	{
		SetViewportSize(1024.0f, 768.0f);
	}

	SetSkyTint(0xFFFFFFFF);
	SetParticleColor(0xFFFFFFFF);

	m_bShadowCullActive = false;
	memset(m_afShadowCullPlane, 0, sizeof(m_afShadowCullPlane));

	// Upload initial constant buffer data with default values
	m_bPerFrameDirty = true;
	m_bPerObjectDirty = true;
	CommitChanges();

	for (UINT i = 0; i < STATEMANAGER_MAX_STAGES; ++i)
	{
		SetShaderResource(i, NULL);  // Will use transparent fallback
	}

	Tracef("CShaderManager: Initialized (DX11 native)\n");
	return true;
}

void CShaderManager::Shutdown()
{
	for (int i = 0; i < SHADER_COUNT; ++i)
	{
		if (m_Shaders[i].pVertexShader) { m_Shaders[i].pVertexShader->Release(); m_Shaders[i].pVertexShader = nullptr; }
		if (m_Shaders[i].pPixelShader) { m_Shaders[i].pPixelShader->Release(); m_Shaders[i].pPixelShader = nullptr; }
		if (m_Shaders[i].pInputLayout) { m_Shaders[i].pInputLayout->Release(); m_Shaders[i].pInputLayout = nullptr; }
		if (m_Shaders[i].pVSBlob) { m_Shaders[i].pVSBlob->Release(); m_Shaders[i].pVSBlob = nullptr; }
	}

	for (int i = 0; i < CS_COUNT; ++i)
	{
		if (m_ComputeShaders[i]) { m_ComputeShaders[i]->Release(); m_ComputeShaders[i] = nullptr; }
	}

	if (m_pCBPerFrame) { m_pCBPerFrame->Release(); m_pCBPerFrame = nullptr; }
	if (m_pCBPerObject) { m_pCBPerObject->Release(); m_pCBPerObject = nullptr; }
	if (m_pCBLighting) { m_pCBLighting->Release(); m_pCBLighting = nullptr; }
	if (m_pCBSpeedTree) { m_pCBSpeedTree->Release(); m_pCBSpeedTree = nullptr; }
	if (m_pCBSkinning) { m_pCBSkinning->Release(); m_pCBSkinning = nullptr; }
	for (UINT i = 0; i < SKINNING_CB_POOL_SIZE; ++i)
	{
		if (m_pSkinningCBPool[i]) { m_pSkinningCBPool[i]->Release(); m_pSkinningCBPool[i] = nullptr; }
	}
	if (m_pCBGodRays) { m_pCBGodRays->Release(); m_pCBGodRays = nullptr; }
#ifdef ENABLE_BLOOM
	if (m_pCBBloom) { m_pCBBloom->Release(); m_pCBBloom = nullptr; }
#endif
#ifdef ENABLE_SSAO
	if (m_pCBSSAO) { m_pCBSSAO->Release(); m_pCBSSAO = nullptr; }
	if (m_pSSAONoiseSRV) { m_pSSAONoiseSRV->Release(); m_pSSAONoiseSRV = nullptr; }
	if (m_pSSAONoiseTex) { m_pSSAONoiseTex->Release(); m_pSSAONoiseTex = nullptr; }
#endif
	if (m_pCBSkyGradient) { m_pCBSkyGradient->Release(); m_pCBSkyGradient = nullptr; }
	if (m_pDefaultTextureSRV) { m_pDefaultTextureSRV->Release(); m_pDefaultTextureSRV = nullptr; }
	if (m_pDefaultTexture) { m_pDefaultTexture->Release(); m_pDefaultTexture = nullptr; }
	if (m_pTransparentTextureSRV) { m_pTransparentTextureSRV->Release(); m_pTransparentTextureSRV = nullptr; }
	if (m_pTransparentTexture) { m_pTransparentTexture->Release(); m_pTransparentTexture = nullptr; }
	if (m_pSamplerLinear) { m_pSamplerLinear->Release(); m_pSamplerLinear = nullptr; }
	if (m_pSamplerPoint) { m_pSamplerPoint->Release(); m_pSamplerPoint = nullptr; }
	if (m_pSamplerClamp) { m_pSamplerClamp->Release(); m_pSamplerClamp = nullptr; }
	if (m_pSamplerShadowCmp) { m_pSamplerShadowCmp->Release(); m_pSamplerShadowCmp = nullptr; }

	// Clean up particle CS resources
	ReleaseGpuBuffer(m_particleCSInput);
	ReleaseGpuBuffer(m_particleCSOutput);
	if (m_pCBParticleCS) { m_pCBParticleCS->Release(); m_pCBParticleCS = nullptr; }
	if (m_pParticleCSIB) { m_pParticleCSIB->Release(); m_pParticleCSIB = nullptr; }
	m_bComputeParticlesAvailable = false;

	// Clean up fly trace CS resources
	ReleaseGpuBuffer(m_flyTraceCSInput);
	ReleaseGpuBuffer(m_flyTraceCSOutput);
	if (m_pCBFlyTraceCS) { m_pCBFlyTraceCS->Release(); m_pCBFlyTraceCS = nullptr; }
	if (m_pFlyTraceCSIB) { m_pFlyTraceCSIB->Release(); m_pFlyTraceCSIB = nullptr; }
	m_bFlyTraceCSAvailable = false;

	// Clean up weapon trace CS resources
	ReleaseGpuBuffer(m_weaponTraceCSInput);
	ReleaseGpuBuffer(m_weaponTraceCSOutput);
	if (m_pCBWeaponTraceCS) { m_pCBWeaponTraceCS->Release(); m_pCBWeaponTraceCS = nullptr; }
	m_bWeaponTraceCSAvailable = false;

	// Clean up render state management resources
	if (m_pDynamicVertexBuffer) { m_pDynamicVertexBuffer->Release(); m_pDynamicVertexBuffer = nullptr; }
	if (m_pDynamicIndexBuffer) { m_pDynamicIndexBuffer->Release(); m_pDynamicIndexBuffer = nullptr; }
	if (m_pStateCache) { delete m_pStateCache; m_pStateCache = nullptr; }

	m_pCurrentBlendState = nullptr;
	m_pCurrentRasterizerState = nullptr;
	m_pCurrentDepthStencilState = nullptr;
	m_SavedRenderStates.clear();

	m_pDevice = nullptr;
	m_pContext = nullptr;
	m_bInitialized = false;
	m_eCurrentShader = SHADER_NONE;
}

void CShaderManager::SetDefaultState()
{
	if (!m_bInitialized || !GetActiveContext())
		return;

	// Reset render state tracking
	m_RenderState.SetDefaults();
	m_SavedRenderStates.clear();

	// Mark all states as needing update
	m_bBlendStateDirty = true;
	m_bRasterizerStateDirty = true;
	m_bDepthStencilStateDirty = true;

	m_pCurrentBlendState = nullptr;
	m_pCurrentRasterizerState = nullptr;
	m_pCurrentDepthStencilState = nullptr;

	// Reset sampler states
	for (int i = 0; i < MAX_SAMPLER_SLOTS; ++i)
	{
		m_SamplerStates[i].dirty = true;
	}

	// Reset texture bindings
	for (int i = 0; i < STATEMANAGER_MAX_STAGES; ++i)
	{
		m_pTextures[i] = nullptr;
	}

	// Reset current shader
	m_eCurrentShader = SHADER_NONE;

	m_CurrentTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	for (DWORD i = 0; i < MAX_STREAMS; ++i)
	{
		m_Streams[i].pBuffer = nullptr;
		m_Streams[i].stride = 0;
		m_Streams[i].offset = 0;
	}
	m_pCurrentIndexBuffer = nullptr;
	m_IndexFormat = DXGI_FORMAT_UNKNOWN;
	m_IndexOffset = 0;

	// Apply default blend state (alpha blending enabled)
	ApplyRenderStates();
}

bool CShaderManager::CompileAllShaders()
{
	static const char* s_ShaderNames[] = {
		"UI", "Mesh", "Mesh2Tex", "Terrain", "Water", "Sky", "Particle", "Shadow",
		"ShadowSkinned", "SpeedTree", "SpeedTreeLeaf", "MeshNormal", "MeshSkinned", "GodRays",
		"MeshVTF", "ShadowVTF", "SpeedTreeVTF", "Mesh2TexVTF", "ParticlePCT",
#ifdef ENABLE_BLOOM
		"BloomBright", "BloomBlur", "BloomComposite",
#endif
#ifdef ENABLE_SSAO
		"SSAO", "SSAOBlur", "DepthResolve",
#endif
	};

	bool bSuccess = true;

	if (!CompileShader(SHADER_UI, g_szUIVertexShader, g_szUIPixelShader, g_UIInputLayout, ARRAYSIZE(g_UIInputLayout)))
	{
		TraceError("CompileAllShaders: Failed to compile %s shader", s_ShaderNames[SHADER_UI]);
		bSuccess = false;
	}
	{
#ifdef ENABLE_PBR
		std::string pbrPrefix = "#define PBR_ENABLED\n";
		pbrPrefix += g_szPBRFunctions;
		std::string meshPS = pbrPrefix + g_szMeshPixelShader;
		if (!CompileShader(SHADER_MESH, g_szMeshVertexShader, meshPS.c_str(), g_MeshInputLayout, ARRAYSIZE(g_MeshInputLayout)))
#else
		if (!CompileShader(SHADER_MESH, g_szMeshVertexShader, g_szMeshPixelShader, g_MeshInputLayout, ARRAYSIZE(g_MeshInputLayout)))
#endif
		{
			TraceError("CompileAllShaders: Failed to compile %s shader", s_ShaderNames[SHADER_MESH]);
			bSuccess = false;
		}
	}
	{
#ifdef ENABLE_PBR
		std::string pbrPrefix = "#define PBR_ENABLED\n";
		pbrPrefix += g_szPBRFunctions;
		std::string mesh2TexPS = pbrPrefix + g_szMesh2TexPixelShader;
		if (!CompileShader(SHADER_MESH_2TEX, g_szMesh2TexVertexShader, mesh2TexPS.c_str(), g_Mesh2TexInputLayout, ARRAYSIZE(g_Mesh2TexInputLayout)))
#else
		if (!CompileShader(SHADER_MESH_2TEX, g_szMesh2TexVertexShader, g_szMesh2TexPixelShader, g_Mesh2TexInputLayout, ARRAYSIZE(g_Mesh2TexInputLayout)))
#endif
		{
			TraceError("CompileAllShaders: Failed to compile %s shader", s_ShaderNames[SHADER_MESH_2TEX]);
			bSuccess = false;
		}
	}
	{
#ifdef ENABLE_PBR
		std::string pbrPrefix = "#define PBR_ENABLED\n";
		pbrPrefix += g_szPBRFunctions;
		std::string terrainPS = pbrPrefix + g_szTerrainPixelShader;
		if (!CompileShader(SHADER_TERRAIN, g_szTerrainVertexShader, terrainPS.c_str(), g_TerrainInputLayout, ARRAYSIZE(g_TerrainInputLayout)))
#else
		if (!CompileShader(SHADER_TERRAIN, g_szTerrainVertexShader, g_szTerrainPixelShader, g_TerrainInputLayout, ARRAYSIZE(g_TerrainInputLayout)))
#endif
		{
			TraceError("CompileAllShaders: Failed to compile %s shader", s_ShaderNames[SHADER_TERRAIN]);
			bSuccess = false;
		}
	}
	{
		if (!CompileShader(SHADER_WATER, g_szWaterVertexShader, g_szWaterPixelShader, g_WaterInputLayout, ARRAYSIZE(g_WaterInputLayout)))
		{
			TraceError("CompileAllShaders: Failed to compile %s shader", s_ShaderNames[SHADER_WATER]);
			bSuccess = false;
		}
	}
	if (!CompileShader(SHADER_SKY, g_szSkyVertexShader, g_szSkyPixelShader, g_SkyInputLayout, ARRAYSIZE(g_SkyInputLayout)))
	{
		TraceError("CompileAllShaders: Failed to compile %s shader", s_ShaderNames[SHADER_SKY]);
		bSuccess = false;
	}
	if (!CompileShader(SHADER_PARTICLE, g_szParticleVertexShader, g_szParticlePixelShader, g_ParticleInputLayout, ARRAYSIZE(g_ParticleInputLayout)))
	{
		TraceError("CompileAllShaders: Failed to compile %s shader", s_ShaderNames[SHADER_PARTICLE]);
		bSuccess = false;
	}
	if (!CompileShader(SHADER_SHADOW, g_szShadowVertexShader, g_szShadowPixelShader, g_ShadowInputLayout, ARRAYSIZE(g_ShadowInputLayout)))
	{
		TraceError("CompileAllShaders: Failed to compile %s shader", s_ShaderNames[SHADER_SHADOW]);
		bSuccess = false;
	}
	if (!CompileShader(SHADER_SHADOW_SKINNED, g_szShadowSkinnedVertexShader, g_szShadowSkinnedPixelShader, g_SkinnedMeshInputLayout, ARRAYSIZE(g_SkinnedMeshInputLayout)))
	{
		TraceError("CompileAllShaders: Failed to compile %s shader", s_ShaderNames[SHADER_SHADOW_SKINNED]);
		bSuccess = false;
	}
	if (!CompileShader(SHADER_SPEEDTREE, g_szSpeedTreeVertexShader, g_szSpeedTreePixelShader, g_SpeedTreeInputLayout, ARRAYSIZE(g_SpeedTreeInputLayout)))
	{
		TraceError("CompileAllShaders: Failed to compile %s shader", s_ShaderNames[SHADER_SPEEDTREE]);
		bSuccess = false;
	}
	if (!CompileShader(SHADER_SPEEDTREE_LEAF, g_szSpeedTreeLeafVertexShader, g_szSpeedTreeLeafPixelShader, g_SpeedTreeLeafInputLayout, ARRAYSIZE(g_SpeedTreeLeafInputLayout)))
	{
		TraceError("CompileAllShaders: Failed to compile %s shader", s_ShaderNames[SHADER_SPEEDTREE_LEAF]);
		bSuccess = false;
	}
	// Normal mapped mesh shader
	{
#ifdef ENABLE_PBR
		std::string pbrPrefix = "#define PBR_ENABLED\n";
		pbrPrefix += g_szPBRFunctions;
		std::string normalPS = pbrPrefix + g_szMeshNormalPS;
		if (!CompileShader(SHADER_MESH_NORMAL, g_szMeshNormalVS, normalPS.c_str(), g_MeshNormalInputLayout, ARRAYSIZE(g_MeshNormalInputLayout)))
#else
		if (!CompileShader(SHADER_MESH_NORMAL, g_szMeshNormalVS, g_szMeshNormalPS, g_MeshNormalInputLayout, ARRAYSIZE(g_MeshNormalInputLayout)))
#endif
		{
			TraceError("CompileAllShaders: Failed to compile %s shader", s_ShaderNames[SHADER_MESH_NORMAL]);
			bSuccess = false;
		}
	}
	// GPU Skinned mesh shader
	{
#ifdef ENABLE_PBR
		std::string pbrPrefix = "#define PBR_ENABLED\n";
		pbrPrefix += g_szPBRFunctions;
		std::string skinnedPS = pbrPrefix + g_szSkinnedMeshPixelShader;
		if (!CompileShader(SHADER_MESH_SKINNED, g_szSkinnedMeshVertexShader, skinnedPS.c_str(), g_SkinnedMeshInputLayout, ARRAYSIZE(g_SkinnedMeshInputLayout)))
#else
		if (!CompileShader(SHADER_MESH_SKINNED, g_szSkinnedMeshVertexShader, g_szSkinnedMeshPixelShader, g_SkinnedMeshInputLayout, ARRAYSIZE(g_SkinnedMeshInputLayout)))
#endif
		{
			TraceError("CompileAllShaders: Failed to compile MeshSkinned shader");
			bSuccess = false;
		}
	}
	// God Rays post-process shader
	if (!CompileShader(SHADER_GODRAYS, g_szGodRaysVertexShader, g_szGodRaysPixelShader, g_GodRaysInputLayout, ARRAYSIZE(g_GodRaysInputLayout)))
	{
		TraceError("CompileAllShaders: Failed to compile GodRays shader");
		bSuccess = false;
	}
#ifdef ENABLE_BLOOM
	if (!CompileShader(SHADER_BLOOM_BRIGHT, g_szGodRaysVertexShader, g_szBloomBrightPS, g_GodRaysInputLayout, ARRAYSIZE(g_GodRaysInputLayout)))
	{
		TraceError("CompileAllShaders: Failed to compile BloomBright shader");
		bSuccess = false;
	}
	if (!CompileShader(SHADER_BLOOM_BLUR, g_szGodRaysVertexShader, g_szBloomBlurPS, g_GodRaysInputLayout, ARRAYSIZE(g_GodRaysInputLayout)))
	{
		TraceError("CompileAllShaders: Failed to compile BloomBlur shader");
		bSuccess = false;
	}
	if (!CompileShader(SHADER_BLOOM_COMPOSITE, g_szGodRaysVertexShader, g_szBloomCompositePS, g_GodRaysInputLayout, ARRAYSIZE(g_GodRaysInputLayout)))
	{
		TraceError("CompileAllShaders: Failed to compile BloomComposite shader");
		bSuccess = false;
	}
#endif
#ifdef ENABLE_SSAO
	// SSAO shaders (reuse god rays VS + input layout)
	if (!CompileShader(SHADER_SSAO, g_szGodRaysVertexShader, g_szSSAO_PS, g_GodRaysInputLayout, ARRAYSIZE(g_GodRaysInputLayout)))
	{
		TraceError("CompileAllShaders: Failed to compile SSAO shader");
		bSuccess = false;
	}
	if (!CompileShader(SHADER_SSAO_BLUR, g_szGodRaysVertexShader, g_szSSAOBlurPS, g_GodRaysInputLayout, ARRAYSIZE(g_GodRaysInputLayout)))
	{
		TraceError("CompileAllShaders: Failed to compile SSAO Blur shader");
		bSuccess = false;
	}
	if (!CompileShader(SHADER_DEPTH_RESOLVE, g_szGodRaysVertexShader, g_szDepthResolvePS, g_GodRaysInputLayout, ARRAYSIZE(g_GodRaysInputLayout)))
	{
		TraceError("CompileAllShaders: Failed to compile Depth Resolve shader");
		bSuccess = false;
	}
#endif
	// VTF Batched Mesh shader (instanced rendering)
	{
#ifdef ENABLE_PBR
		std::string pbrPrefix = "#define PBR_ENABLED\n";
		pbrPrefix += g_szPBRFunctions;
		std::string meshVTFPS = pbrPrefix + g_szMeshVTFPixelShader;
		if (!CompileShader(SHADER_MESH_VTF, g_szMeshVTFVertexShader, meshVTFPS.c_str(), g_MeshVTFInputLayout, ARRAYSIZE(g_MeshVTFInputLayout)))
#else
		if (!CompileShader(SHADER_MESH_VTF, g_szMeshVTFVertexShader, g_szMeshVTFPixelShader, g_MeshVTFInputLayout, ARRAYSIZE(g_MeshVTFInputLayout)))
#endif
		{
			TraceError("CompileAllShaders: Failed to compile MeshVTF shader");
			bSuccess = false;
		}
	}
	// VTF Batched Shadow shader (instanced depth-only)
	if (!CompileShader(SHADER_SHADOW_VTF, g_szShadowVTFVertexShader, g_szShadowVTFPixelShader, g_ShadowVTFInputLayout, ARRAYSIZE(g_ShadowVTFInputLayout)))
	{
		TraceError("CompileAllShaders: Failed to compile ShadowVTF shader");
		bSuccess = false;
	}
	// VTF Batched SpeedTree shader (instanced vegetation)
	if (!CompileShader(SHADER_SPEEDTREE_VTF, g_szSpeedTreeVTFVertexShader, g_szSpeedTreeVTFPixelShader, g_SpeedTreeVTFInputLayout, ARRAYSIZE(g_SpeedTreeVTFInputLayout)))
	{
		TraceError("CompileAllShaders: Failed to compile SpeedTreeVTF shader");
		bSuccess = false;
	}
	{
#ifdef ENABLE_PBR
		std::string pbrPrefix = "#define PBR_ENABLED\n";
		pbrPrefix += g_szPBRFunctions;
		std::string mesh2TexVTFPS = pbrPrefix + g_szMesh2TexVTFPixelShader;
		if (!CompileShader(SHADER_MESH_2TEX_VTF, g_szMesh2TexVTFVertexShader, mesh2TexVTFPS.c_str(), g_Mesh2TexVTFInputLayout, ARRAYSIZE(g_Mesh2TexVTFInputLayout)))
#else
		if (!CompileShader(SHADER_MESH_2TEX_VTF, g_szMesh2TexVTFVertexShader, g_szMesh2TexVTFPixelShader, g_Mesh2TexVTFInputLayout, ARRAYSIZE(g_Mesh2TexVTFInputLayout)))
#endif
		{
			TraceError("CompileAllShaders: Failed to compile Mesh2TexVTF shader");
			bSuccess = false;
		}
	}
	if (!CompileShader(SHADER_PARTICLE_PCT, g_szParticlePCTVertexShader, g_szParticlePixelShader, g_PDTInputLayout, ARRAYSIZE(g_PDTInputLayout)))
	{
		TraceError("CompileAllShaders: Failed to compile ParticlePCT shader");
		bSuccess = false;
	}
	// Particle billboard compute shader (optional)
	if (!CompileComputeShader(CS_PARTICLE_BILLBOARD, g_szParticleBillboardCS, "CSMain"))
	{
		Tracef("CompileAllShaders: Particle billboard CS not available, using CPU fallback\n");
		// Not a fatal error — CPU fallback will be used
	}

	// Fly trace billboard compute shader (optional)
	if (!CompileComputeShader(CS_FLYTRACE, g_szFlyTraceCS, "CSMain"))
	{
		Tracef("CompileAllShaders: FlyTrace CS not available, using CPU fallback\n");
		// Not a fatal error — CPU fallback will be used
	}

	// Weapon trace spline compute shader (optional)
	if (!CompileComputeShader(CS_WEAPONTRACE, g_szWeaponTraceCS, "CSMain"))
	{
		Tracef("CompileAllShaders: WeaponTrace CS not available, using CPU fallback\n");
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
// Shader Cache Implementation
//////////////////////////////////////////////////////////////////////////

const char* CShaderManager::GetShaderCachePath()
{
	static char s_szCachePath[MAX_PATH] = {0};
	if (s_szCachePath[0] == 0)
	{
		// Create cache directory in current working directory
		strcpy_s(s_szCachePath, "cache\\shaders");
		CreateDirectoryA("cache", NULL);
		CreateDirectoryA("cache\\shaders", NULL);
	}
	return s_szCachePath;
}

UINT CShaderManager::ComputeShaderHash(const char* szVSCode, const char* szPSCode)
{
	// Simple FNV-1a hash for shader source
	const UINT FNV_PRIME = 16777619u;
	const UINT FNV_OFFSET = 2166136261u;

	UINT hash = FNV_OFFSET;

	// Hash vertex shader
	if (szVSCode)
	{
		for (const char* p = szVSCode; *p; ++p)
		{
			hash ^= (UINT)*p;
			hash *= FNV_PRIME;
		}
	}

	// Hash pixel shader
	if (szPSCode)
	{
		for (const char* p = szPSCode; *p; ++p)
		{
			hash ^= (UINT)*p;
			hash *= FNV_PRIME;
		}
	}

	return hash;
}

bool CShaderManager::LoadShaderFromCache(EShaderType type, UINT hash, ID3DBlob** ppVSBlob, ID3DBlob** ppPSBlob)
{
	char szVSPath[MAX_PATH], szPSPath[MAX_PATH], szHashPath[MAX_PATH];
	sprintf_s(szVSPath, "%s\\shader_%d_vs.cso", GetShaderCachePath(), type);
	sprintf_s(szPSPath, "%s\\shader_%d_ps.cso", GetShaderCachePath(), type);
	sprintf_s(szHashPath, "%s\\shader_%d.hash", GetShaderCachePath(), type);

	// Check if hash file exists and matches
	FILE* fp = nullptr;
	if (fopen_s(&fp, szHashPath, "rb") != 0 || !fp)
		return false;

	UINT storedHash = 0;
	fread(&storedHash, sizeof(UINT), 1, fp);
	fclose(fp);

	if (storedHash != hash)
		return false;  // Hash mismatch, need to recompile

	// Load VS bytecode
	if (fopen_s(&fp, szVSPath, "rb") != 0 || !fp)
		return false;

	fseek(fp, 0, SEEK_END);
	long vsSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (vsSize <= 0)
	{
		fclose(fp);
		return false;
	}

	HRESULT hr = D3DCreateBlob(vsSize, ppVSBlob);
	if (FAILED(hr))
	{
		fclose(fp);
		return false;
	}

	fread((*ppVSBlob)->GetBufferPointer(), 1, vsSize, fp);
	fclose(fp);

	// Load PS bytecode
	if (fopen_s(&fp, szPSPath, "rb") != 0 || !fp)
	{
		(*ppVSBlob)->Release();
		*ppVSBlob = nullptr;
		return false;
	}

	fseek(fp, 0, SEEK_END);
	long psSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (psSize <= 0)
	{
		fclose(fp);
		(*ppVSBlob)->Release();
		*ppVSBlob = nullptr;
		return false;
	}

	hr = D3DCreateBlob(psSize, ppPSBlob);
	if (FAILED(hr))
	{
		fclose(fp);
		(*ppVSBlob)->Release();
		*ppVSBlob = nullptr;
		return false;
	}

	fread((*ppPSBlob)->GetBufferPointer(), 1, psSize, fp);
	fclose(fp);

	return true;
}

bool CShaderManager::SaveShaderToCache(EShaderType type, UINT hash, ID3DBlob* pVSBlob, ID3DBlob* pPSBlob)
{
	char szVSPath[MAX_PATH], szPSPath[MAX_PATH], szHashPath[MAX_PATH];
	sprintf_s(szVSPath, "%s\\shader_%d_vs.cso", GetShaderCachePath(), type);
	sprintf_s(szPSPath, "%s\\shader_%d_ps.cso", GetShaderCachePath(), type);
	sprintf_s(szHashPath, "%s\\shader_%d.hash", GetShaderCachePath(), type);

	FILE* fp = nullptr;

	// Save VS bytecode
	if (fopen_s(&fp, szVSPath, "wb") != 0 || !fp)
		return false;
	fwrite(pVSBlob->GetBufferPointer(), 1, pVSBlob->GetBufferSize(), fp);
	fclose(fp);

	// Save PS bytecode
	if (fopen_s(&fp, szPSPath, "wb") != 0 || !fp)
		return false;
	fwrite(pPSBlob->GetBufferPointer(), 1, pPSBlob->GetBufferSize(), fp);
	fclose(fp);

	// Save hash
	if (fopen_s(&fp, szHashPath, "wb") != 0 || !fp)
		return false;
	fwrite(&hash, sizeof(UINT), 1, fp);
	fclose(fp);

	return true;
}

bool CShaderManager::CompileShader(EShaderType type, const char* szVSCode, const char* szPSCode,
	const D3D11_INPUT_ELEMENT_DESC* pElements, UINT numElements)
{
	HRESULT hr;
	ID3DBlob* pErrorBlob = nullptr;
	ID3DBlob* pVSBlob = nullptr;
	ID3DBlob* pPSBlob = nullptr;

	// Compute hash of shader source for cache validation
	UINT shaderHash = ComputeShaderHash(szVSCode, szPSCode);

	// Try to load from cache first
	if (LoadShaderFromCache(type, shaderHash, &pVSBlob, &pPSBlob))
	{
		// Create shaders from cached bytecode
		hr = m_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_Shaders[type].pVertexShader);
		if (FAILED(hr))
		{
			pVSBlob->Release();
			pPSBlob->Release();
			goto compile_from_source;
		}

		hr = m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_Shaders[type].pPixelShader);
		if (FAILED(hr))
		{
			m_Shaders[type].pVertexShader->Release();
			m_Shaders[type].pVertexShader = nullptr;
			pVSBlob->Release();
			pPSBlob->Release();
			goto compile_from_source;
		}

		hr = m_pDevice->CreateInputLayout(pElements, numElements, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_Shaders[type].pInputLayout);
		if (FAILED(hr))
		{
			m_Shaders[type].pVertexShader->Release();
			m_Shaders[type].pVertexShader = nullptr;
			m_Shaders[type].pPixelShader->Release();
			m_Shaders[type].pPixelShader = nullptr;
			pVSBlob->Release();
			pPSBlob->Release();
			goto compile_from_source;
		}

		pPSBlob->Release();
		m_Shaders[type].pVSBlob = pVSBlob;
		// Shader loaded from cache (fast path)
		return true;
	}

compile_from_source:
	pVSBlob = nullptr;
	pPSBlob = nullptr;

	hr = D3DCompile(szVSCode, strlen(szVSCode), nullptr, nullptr, nullptr,
		"main", "vs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pVSBlob, &pErrorBlob);
	if (FAILED(hr))
	{
		if (pErrorBlob) { TraceError("Shader %d VS: %s", type, (char*)pErrorBlob->GetBufferPointer()); pErrorBlob->Release(); }
		return false;
	}

	hr = m_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_Shaders[type].pVertexShader);
	if (FAILED(hr)) { pVSBlob->Release(); return false; }

	hr = D3DCompile(szPSCode, strlen(szPSCode), nullptr, nullptr, nullptr,
		"main", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pPSBlob, &pErrorBlob);
	if (FAILED(hr))
	{
		if (pErrorBlob) { TraceError("Shader %d PS: %s", type, (char*)pErrorBlob->GetBufferPointer()); pErrorBlob->Release(); }
		pVSBlob->Release();
		return false;
	}

	hr = m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_Shaders[type].pPixelShader);
	if (FAILED(hr)) { pVSBlob->Release(); pPSBlob->Release(); return false; }

	hr = m_pDevice->CreateInputLayout(pElements, numElements, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_Shaders[type].pInputLayout);
	if (FAILED(hr)) { pVSBlob->Release(); pPSBlob->Release(); return false; }

	// Save compiled bytecode to cache for next launch
	SaveShaderToCache(type, shaderHash, pVSBlob, pPSBlob);

	pPSBlob->Release();
	m_Shaders[type].pVSBlob = pVSBlob;
	return true;
}

bool CShaderManager::CreateConstantBuffers()
{
	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	HRESULT hr;

	cbDesc.ByteWidth = sizeof(CBPerFrame);
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pCBPerFrame);
	if (FAILED(hr))
	{
		TraceError("CreateConstantBuffers: Failed to create PerFrame buffer (size=%d, hr=0x%08X)", sizeof(CBPerFrame), hr);
		return false;
	}

	cbDesc.ByteWidth = m_bCBRingSupported
	                 ? (CBRingAlign256(sizeof(CBPerObject)) * CB_RING_SLOTS_PEROBJECT)
	                 : sizeof(CBPerObject);
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pCBPerObject);
	if (FAILED(hr))
	{
		TraceError("CreateConstantBuffers: Failed to create PerObject buffer (size=%d, hr=0x%08X)", sizeof(CBPerObject), hr);
		return false;
	}

	cbDesc.ByteWidth = sizeof(CBLighting);
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pCBLighting);
	if (FAILED(hr))
	{
		TraceError("CreateConstantBuffers: Failed to create Lighting buffer (size=%d, hr=0x%08X)", sizeof(CBLighting), hr);
		return false;
	}

	cbDesc.ByteWidth = sizeof(CBSpeedTree);
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pCBSpeedTree);
	if (FAILED(hr))
	{
		TraceError("CreateConstantBuffers: Failed to create SpeedTree buffer (size=%d, hr=0x%08X)", sizeof(CBSpeedTree), hr);
		return false;
	}

	cbDesc.ByteWidth = sizeof(CBSkinning);
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pCBSkinning);
	if (FAILED(hr))
	{
		TraceError("CreateConstantBuffers: Failed to create Skinning buffer (size=%d, hr=0x%08X)", sizeof(CBSkinning), hr);
		return false;
	}

	{
		D3D11_BUFFER_DESC poolDesc = {};
		poolDesc.ByteWidth = sizeof(CBSkinning);
		poolDesc.Usage = D3D11_USAGE_DEFAULT;
		poolDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		poolDesc.CPUAccessFlags = 0;
		for (UINT i = 0; i < SKINNING_CB_POOL_SIZE; ++i)
		{
			hr = m_pDevice->CreateBuffer(&poolDesc, nullptr, &m_pSkinningCBPool[i]);
			if (FAILED(hr))
			{
				TraceError("CreateConstantBuffers: Failed to create Skinning pool buffer %d (hr=0x%08X)", i, hr);
				// Non-fatal: fall back to WRITE_DISCARD path for remaining
				break;
			}
		}
	}

	cbDesc.ByteWidth = sizeof(CBGodRays);
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pCBGodRays);
	if (FAILED(hr))
	{
		TraceError("CreateConstantBuffers: Failed to create GodRays buffer (size=%d, hr=0x%08X)", sizeof(CBGodRays), hr);
		return false;
	}

#ifdef ENABLE_BLOOM
	cbDesc.ByteWidth = sizeof(CBBloom);
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pCBBloom);
	if (FAILED(hr))
	{
		TraceError("CreateConstantBuffers: Failed to create Bloom buffer (size=%d, hr=0x%08X)", sizeof(CBBloom), hr);
		return false;
	}
#endif

#ifdef ENABLE_SSAO
	cbDesc.ByteWidth = sizeof(CBSSAO);
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pCBSSAO);
	if (FAILED(hr))
	{
		TraceError("CreateConstantBuffers: Failed to create SSAO buffer (size=%d, hr=0x%08X)", sizeof(CBSSAO), hr);
		return false;
	}

	// Initialize SSAO parameters
	m_cbSSAO.vSSAOParams = XMFLOAT4(0.5f, 0.025f, 1.5f, 0.0f); // radius, bias, intensity

	{
		// Simple deterministic pseudo-random using a fixed seed
		unsigned int seed = 12345u;
		auto nextRand = [&seed]() -> float {
			seed = seed * 1103515245u + 12345u;
			return (float)(seed & 0x7FFFFFFFu) / (float)0x7FFFFFFFu;
		};

		for (int i = 0; i < SSAO_KERNEL_SIZE; ++i)
		{
			// Random direction in upper hemisphere
			float x = nextRand() * 2.0f - 1.0f;
			float y = nextRand() * 2.0f - 1.0f;
			float z = nextRand(); // z > 0 (upper hemisphere)

			// Normalize
			float len = sqrtf(x * x + y * y + z * z);
			if (len > 0.001f) { x /= len; y /= len; z /= len; }

			// Quadratic scale: more samples near origin
			float scale = (float)i / (float)SSAO_KERNEL_SIZE;
			scale = 0.1f + scale * scale * 0.9f; // lerp(0.1, 1.0, scale*scale)
			m_cbSSAO.vSampleKernel[i] = XMFLOAT4(x * scale, y * scale, z * scale, 0.0f);
		}
	}

	{
		unsigned int noiseSeed = 54321u;
		auto noiseRand = [&noiseSeed]() -> BYTE {
			noiseSeed = noiseSeed * 1103515245u + 12345u;
			return (BYTE)((noiseSeed >> 16) & 0xFF);
		};

		BYTE noiseData[4 * 4 * 2]; // 4x4 texels, 2 bytes each (RG)
		for (int i = 0; i < 4 * 4; ++i)
		{
			noiseData[i * 2 + 0] = noiseRand();
			noiseData[i * 2 + 1] = noiseRand();
		}

		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = 4;
		texDesc.Height = 4;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_IMMUTABLE;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = noiseData;
		initData.SysMemPitch = 4 * 2;

		hr = m_pDevice->CreateTexture2D(&texDesc, &initData, &m_pSSAONoiseTex);
		if (FAILED(hr))
		{
			TraceError("CreateConstantBuffers: Failed to create SSAO noise texture (hr=0x%08X)", hr);
			return false;
		}

		hr = m_pDevice->CreateShaderResourceView(m_pSSAONoiseTex, nullptr, &m_pSSAONoiseSRV);
		if (FAILED(hr))
		{
			TraceError("CreateConstantBuffers: Failed to create SSAO noise SRV (hr=0x%08X)", hr);
			return false;
		}
	}

	m_bSSAODirty = true;
#endif

	cbDesc.ByteWidth = sizeof(CBSkyGradient);
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pCBSkyGradient);
	if (FAILED(hr))
	{
		TraceError("CreateConstantBuffers: Failed to create SkyGradient buffer (size=%d, hr=0x%08X)", sizeof(CBSkyGradient), hr);
		return false;
	}
	memset(&m_cbSkyGradient, 0, sizeof(m_cbSkyGradient));
	m_bSkyGradientDirty = false;

	// Initialize bone matrices to identity
	for (int i = 0; i < MAX_BONES; ++i)
		m_cbSkinning.boneMatrices[i] = XMMatrixIdentity();

	// Initialize god rays with default values
	m_cbGodRays.vLightScreenPos = XMFLOAT4(0.5f, 0.3f, 1.0f, 0.97f);  // Center-top, intensity=1, decay=0.97
	m_cbGodRays.vRayParams = XMFLOAT4(0.5f, 0.5f, 0.3f, 64.0f);      // density, weight, exposure, samples
	m_cbGodRays.vRayColor = XMFLOAT4(1.0f, 0.9f, 0.7f, 1.0f);        // Warm sun color


#ifdef ENABLE_BLOOM
	// Initialize bloom with default values
	m_cbBloom.vBloomParams = XMFLOAT4(BLOOM_DEFAULT_THRESHOLD, BLOOM_DEFAULT_INTENSITY, 0.0f, 0.0f);
	m_cbBloom.vTexelSize = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	m_cbBloom.vBlurDirection = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
#endif

	return true;
}

bool CShaderManager::CreateDefaultTexture()
{
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = 1;
	texDesc.Height = 1;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_IMMUTABLE;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	UINT32 whitePixel = 0xFFFFFFFF;
	D3D11_SUBRESOURCE_DATA initData = { &whitePixel, sizeof(UINT32), 0 };

	if (FAILED(m_pDevice->CreateTexture2D(&texDesc, &initData, &m_pDefaultTexture))) return false;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	if (FAILED(m_pDevice->CreateShaderResourceView(m_pDefaultTexture, &srvDesc, &m_pDefaultTextureSRV))) return false;

	UINT32 transparentPixel = 0x00000000;
	D3D11_SUBRESOURCE_DATA initDataTransparent = { &transparentPixel, sizeof(UINT32), 0 };

	if (FAILED(m_pDevice->CreateTexture2D(&texDesc, &initDataTransparent, &m_pTransparentTexture))) return false;
	if (FAILED(m_pDevice->CreateShaderResourceView(m_pTransparentTexture, &srvDesc, &m_pTransparentTextureSRV))) return false;

	m_pActiveDefaultTextureSRV = m_pTransparentTextureSRV;

	return true;
}

bool CShaderManager::CreateSamplerStates()
{
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	if (FAILED(m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerLinear))) return false;

	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = 0;  // Force base mip level only (no mipmap selection)
	if (FAILED(m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerPoint))) return false;

	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.BorderColor[0] = 1.0f;  // White border = no shadow outside projection
	sampDesc.BorderColor[1] = 1.0f;
	sampDesc.BorderColor[2] = 1.0f;
	sampDesc.BorderColor[3] = 1.0f;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	if (FAILED(m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerClamp))) return false;
	D3D11_SAMPLER_DESC cmpDesc = {};
	cmpDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	cmpDesc.AddressU = cmpDesc.AddressV = cmpDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	cmpDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
	cmpDesc.MaxLOD = D3D11_FLOAT32_MAX;
	if (FAILED(m_pDevice->CreateSamplerState(&cmpDesc, &m_pSamplerShadowCmp))) return false;



	return true;
}

void CShaderManager::BindShader(EShaderType type)
{
	if (!m_bInitialized || !GetActiveContext() || type < 0 || type >= SHADER_COUNT) return;

	EShaderType& eCurrentShader = m_eCurrentShader;
	if (eCurrentShader == type) return;

	ShaderProgram& shader = m_Shaders[type];
	if (!shader.pVertexShader || !shader.pPixelShader || !shader.pInputLayout)
	{
		TraceError("CShaderManager::BindShader - Shader %d not compiled", type);
		return;
	}
	GetActiveContext()->VSSetShader(shader.pVertexShader, nullptr, 0);
	GetActiveContext()->PSSetShader(shader.pPixelShader, nullptr, 0);
	GetActiveContext()->IASetInputLayout(shader.pInputLayout);

	if (type == SHADER_UI && m_pSamplerPoint)
		GetActiveContext()->PSSetSamplers(0, 1, &m_pSamplerPoint);
	else if (m_pSamplerLinear)
		GetActiveContext()->PSSetSamplers(0, 1, &m_pSamplerLinear);

	if (m_pSamplerShadowCmp)
		GetActiveContext()->PSSetSamplers(2, 1, &m_pSamplerShadowCmp);

	if ((type == SHADER_TERRAIN || type == SHADER_MESH || type == SHADER_MESH_2TEX || type == SHADER_MESH_SKINNED || type == SHADER_MESH_VTF || type == SHADER_MESH_2TEX_VTF) && m_pSamplerClamp)
		GetActiveContext()->PSSetSamplers(1, 1, &m_pSamplerClamp);

	ID3D11Buffer* pCBPerFrame  = m_pCBPerFrame;
	ID3D11Buffer* pCBPerObject = m_pCBPerObject;
	ID3D11Buffer* pCBLighting  = m_pCBLighting;

	if (pCBPerFrame)
	{
		GetActiveContext()->VSSetConstantBuffers(0, 1, &pCBPerFrame);
		GetActiveContext()->PSSetConstantBuffers(0, 1, &pCBPerFrame);
	}
	if (pCBPerObject)
	{
		__BindCBRing(GetActiveContext(), m_pContext1,
		             pCBPerObject,
		             m_cbPerObjectBound,
		             sizeof(CBPerObject), 1, 1, true);
	}
	if (pCBLighting)
	{
		GetActiveContext()->VSSetConstantBuffers(2, 1, &pCBLighting);
		GetActiveContext()->PSSetConstantBuffers(2, 1, &pCBLighting);
	}

	if (type == SHADER_SPEEDTREE || type == SHADER_SPEEDTREE_LEAF || type == SHADER_SPEEDTREE_VTF)
	{
		ID3D11Buffer* pCBSpeedTree = m_pCBSpeedTree;
		if (pCBSpeedTree)
			GetActiveContext()->VSSetConstantBuffers(3, 1, &pCBSpeedTree);
	}

	// Bind sky gradient constant buffer for sky shader
	if (type == SHADER_SKY && m_pCBSkyGradient)
	{
		GetActiveContext()->VSSetConstantBuffers(2, 1, &m_pCBSkyGradient);
		GetActiveContext()->PSSetConstantBuffers(2, 1, &m_pCBSkyGradient);
	}

	eCurrentShader = type;

		D3D11_MAPPED_SUBRESOURCE mapped;
		if (m_bLightingDirty && m_pCBLighting)
		{
			if (SUCCEEDED(GetActiveContext()->Map(m_pCBLighting, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
			{
				memcpy(mapped.pData, &m_cbLighting, sizeof(m_cbLighting));
				GetActiveContext()->Unmap(m_pCBLighting, 0);
				m_bLightingDirty = false;
			}
		}
}

void CShaderManager::BeginUI()        { BindShader(SHADER_UI); }
void CShaderManager::BeginMesh()      { BindShader(SHADER_MESH); }
void CShaderManager::BeginMesh2Tex()  { BindShader(SHADER_MESH_2TEX); }
void CShaderManager::BeginTerrain()   { BindShader(SHADER_TERRAIN); }
void CShaderManager::BeginWater()
{
	BindShader(SHADER_WATER);
}
void CShaderManager::BeginSky()       { BindShader(SHADER_SKY); }

void CShaderManager::SetSkyGradient(const float* pColors, int count, int upperSegments)
{
	if (!m_pCBSkyGradient || !GetActiveContext()) return;
	count = min(count, SKY_GRADIENT_MAX_POINTS);
	for (int i = 0; i < count; ++i)
		m_cbSkyGradient.colors[i] = XMFLOAT4(pColors[i * 4], pColors[i * 4 + 1], pColors[i * 4 + 2], pColors[i * 4 + 3]);
	m_cbSkyGradient.colorCount = count;
	m_cbSkyGradient.upperSegments = upperSegments;
	m_bSkyGradientDirty = true;
}


void CShaderManager::BeginParticle()  { BindShader(SHADER_PARTICLE); }
void CShaderManager::BeginShadow()    { BindShader(SHADER_SHADOW); }
void CShaderManager::BeginShadowSkinned()
{
	BindShader(SHADER_SHADOW_SKINNED);
	ID3D11Buffer* pSkinCB = m_pCBSkinning;
	if (pSkinCB)
		__BindCBRing(GetActiveContext(), m_pContext1,
		             pSkinCB, 0,
		             sizeof(CBSkinning), 3, -1, false);
}
void CShaderManager::BeginSpeedTree() { BindShader(SHADER_SPEEDTREE); }
void CShaderManager::BeginSpeedTreeLeaf() { BindShader(SHADER_SPEEDTREE_LEAF); }
void CShaderManager::BeginMeshNormal() { BindShader(SHADER_MESH_NORMAL); }

void CShaderManager::BeginMeshSkinned()
{
	BindShader(SHADER_MESH_SKINNED);
	ID3D11Buffer* pSkinCB = m_pCBSkinning;
	if (pSkinCB)
		__BindCBRing(GetActiveContext(), m_pContext1,
		             pSkinCB, 0,
		             sizeof(CBSkinning), 3, -1, false);
}

void CShaderManager::BeginGodRays()
{
	BindShader(SHADER_GODRAYS);
	// Bind the god rays constant buffer to slot 0
	if (m_pCBGodRays)
		GetActiveContext()->PSSetConstantBuffers(0, 1, &m_pCBGodRays);
}

#ifdef ENABLE_BLOOM
void CShaderManager::BeginBloomBright()
{
	BindShader(SHADER_BLOOM_BRIGHT);
	if (m_pCBBloom)
		GetActiveContext()->PSSetConstantBuffers(0, 1, &m_pCBBloom);
}

void CShaderManager::BeginBloomBlur()
{
	BindShader(SHADER_BLOOM_BLUR);
	if (m_pCBBloom)
		GetActiveContext()->PSSetConstantBuffers(0, 1, &m_pCBBloom);
}

void CShaderManager::BeginBloomComposite()
{
	BindShader(SHADER_BLOOM_COMPOSITE);
}

void CShaderManager::SetBloomEnabled(bool bEnabled)
{
	m_bBloomEnabled = bEnabled;
}

void CShaderManager::SetBloomParams(float threshold, float intensity)
{
	m_cbBloom.vBloomParams.x = threshold;
	m_cbBloom.vBloomParams.y = intensity;
}
#endif

void CShaderManager::BeginMeshVTF()
{
	BindShader(SHADER_MESH_VTF);
}

void CShaderManager::BeginShadowVTF()
{
	BindShader(SHADER_SHADOW_VTF);
}

void CShaderManager::BeginSpeedTreeVTF()
{
	BindShader(SHADER_SPEEDTREE_VTF);
}

void CShaderManager::BeginMesh2TexVTF()
{
	BindShader(SHADER_MESH_2TEX_VTF);
}

void CShaderManager::BeginParticlePCT() { BindShader(SHADER_PARTICLE_PCT); }

void CShaderManager::End()
{
	if (GetActiveContext())
	{
		GetActiveContext()->VSSetShader(nullptr, nullptr, 0);
		GetActiveContext()->PSSetShader(nullptr, nullptr, 0);
	}
		m_eCurrentShader = SHADER_NONE;
}

void CShaderManager::BindForInputLayout(EInputLayoutType type)
{
	EShaderType eCur = m_eCurrentShader;
	if (eCur != SHADER_NONE)
	{
		return;
	}

	switch (type)
	{
	case INPUT_LAYOUT_TRANSFORMED:
		BeginUI();
		break;
	case INPUT_LAYOUT_PNT:
	case INPUT_LAYOUT_SKINNED:
		BeginMesh();
		break;
	case INPUT_LAYOUT_PNT2:
		BeginMesh2Tex();
		break;
	case INPUT_LAYOUT_PN:
	case INPUT_LAYOUT_TERRAIN_HTP:
		BeginTerrain();
		break;
	case INPUT_LAYOUT_PD:
	case INPUT_LAYOUT_WATER:
		BeginWater();
		break;
	case INPUT_LAYOUT_PDT:
	case INPUT_LAYOUT_PDT2:
	default:
		BeginUI();
		break;
	case INPUT_LAYOUT_PT:
		BeginParticle();
		break;
	}
}

ID3D11InputLayout* CShaderManager::GetInputLayout(EShaderType type) const
{
	return (type >= 0 && type < SHADER_COUNT) ? m_Shaders[type].pInputLayout : nullptr;
}

// Per-Frame Updates
void CShaderManager::SetViewProjection(const Matrix* pView, const Matrix* pProj)
{
	m_cbPerFrame.matView = XMMatrixTranspose(XMLoadFloat4x4((XMFLOAT4X4*)pView));
	m_cbPerFrame.matProjection = XMMatrixTranspose(XMLoadFloat4x4((XMFLOAT4X4*)pProj));
	m_bPerFrameDirty = true;
	m_Matrices[MATRIX_VIEW] = *pView;
	m_Matrices[MATRIX_PROJECTION] = *pProj;
}

void CShaderManager::GetProjectionMatrix(Matrix* pProj) const
{
	if (!pProj) return;
	// Transpose back since we store transposed
	XMMATRIX mat = XMMatrixTranspose(m_cbPerFrame.matProjection);
	XMStoreFloat4x4((XMFLOAT4X4*)pProj, mat);
}

void CShaderManager::SetCameraPosition(const Vector3* pCameraPos)
{
	if (pCameraPos) { m_cbPerFrame.vCameraPos.x = pCameraPos->x; m_cbPerFrame.vCameraPos.y = pCameraPos->y; m_cbPerFrame.vCameraPos.z = pCameraPos->z; }
	m_bPerFrameDirty = true;
}

void CShaderManager::SetViewportSize(float width, float height)
{
	m_cbPerFrame.vCameraPos.w = width;
	m_cbPerFrame.vFogParams.z = height;
	m_bPerFrameDirty = true;
}

void CShaderManager::SetFog(bool bEnabled, float fStart, float fEnd, DWORD dwColor)
{
	m_cbPerFrame.vFogParams.x = fStart;
	m_cbPerFrame.vFogParams.y = fEnd;
	m_cbPerFrame.vFogParams.w = bEnabled ? 1.0f : 0.0f;
	m_cbPerFrame.vFogColor = XMFLOAT4(((dwColor >> 16) & 0xFF) / 255.0f, ((dwColor >> 8) & 0xFF) / 255.0f, (dwColor & 0xFF) / 255.0f, 1.0f);
	m_bPerFrameDirty = true;
}

void CShaderManager::SetLight(const Vector3* pDirection, const Color* pColor, float fIntensity)
{
	// Legacy API - sets light 0 as a directional light
	DX11Light light;
	light.Position = XMFLOAT4(0, 0, 0, LIGHT_DIRECTIONAL);  // w = type
	light.Direction = XMFLOAT4(
		pDirection ? pDirection->x : 0.0f,
		pDirection ? pDirection->y : -1.0f,
		pDirection ? pDirection->z : 0.0f,
		1.0f);  // w = enabled
	light.Color = XMFLOAT4(
		pColor ? pColor->r : 1.0f,
		pColor ? pColor->g : 1.0f,
		pColor ? pColor->b : 1.0f,
		fIntensity);
	light.Attenuation = XMFLOAT4(1.0f, 0.0f, 0.0f, 10000.0f);  // No attenuation for directional
	SetLight(0, light);
}

void CShaderManager::SetAmbient(const Color* pColor)
{
	if (pColor)
		SetGlobalAmbient(pColor->r, pColor->g, pColor->b, 1.0f);
}

void CShaderManager::SetTime(float fTotalTime, float fDeltaTime)
{
	m_cbPerFrame.vTime.x = fTotalTime;
	m_cbPerFrame.vTime.y = fDeltaTime;
	// z = cloud layer2 speed multiplier (set separately)
	m_bPerFrameDirty = true;
}

void CShaderManager::SetSunDirection(float x, float y, float z, float intensity)
{
	float len = sqrtf(x*x + y*y + z*z);
	if (len > 0.0001f)
	{
		x /= len;
		y /= len;
		z /= len;
	}
	m_cbPerFrame.vSunDirection = XMFLOAT4(x, y, z, intensity);
	m_bPerFrameDirty = true;
}

void CShaderManager::SetShadowOpacity(float fOpacity)
{
	m_cbPerFrame.vShadowParams.x = fOpacity;
	m_bPerFrameDirty = true;
}

void CShaderManager::SetShadowTexelSize(float fTexelSize)
{
	m_cbPerFrame.vShadowParams.z = fTexelSize;
	m_bPerFrameDirty = true;
}

void CShaderManager::SetShadowCullPlanes(const float* pafPlanes4x4)
{
	if (!pafPlanes4x4)
	{
		m_bShadowCullActive = false;
		return;
	}

	memcpy(m_afShadowCullPlane, pafPlanes4x4, sizeof(m_afShadowCullPlane));
	m_bShadowCullActive = true;
}

bool CShaderManager::IsInShadowCull(float x, float y, float z, float fRadius) const
{
	if (!m_bShadowCullActive)
		return true;

	for (int i = 0; i < 4; ++i)
	{
		const float* p = m_afShadowCullPlane[i];
		if (p[0] * x + p[1] * y + p[2] * z + p[3] < -fRadius)
			return false;
	}

	return true;
}

void CShaderManager::SetReflectionClipZ(float fWaterZ)
{
	m_cbPerFrame.vShadowParams.y = fWaterZ;
	m_bPerFrameDirty = true;
}

void CShaderManager::SetShadowMatrices(const Matrix* pBig, const Matrix* pLocal)
{
	if (pBig)
		m_cbPerFrame.matShadowBig = XMMatrixTranspose(XMLoadFloat4x4((const XMFLOAT4X4*)pBig));
	if (pLocal)
		m_cbPerFrame.matShadowLocal = XMMatrixTranspose(XMLoadFloat4x4((const XMFLOAT4X4*)pLocal));
	m_bPerFrameDirty = true;
}

void CShaderManager::SetShadowMidFarMatrices(const Matrix* pMid, const Matrix* pFar)
{
	if (pMid)
		m_cbPerFrame.matShadowMid = XMMatrixTranspose(XMLoadFloat4x4((const XMFLOAT4X4*)pMid));
	if (pFar)
		m_cbPerFrame.matShadowFar = XMMatrixTranspose(XMLoadFloat4x4((const XMFLOAT4X4*)pFar));
	m_bPerFrameDirty = true;
}

void CShaderManager::SetCascadeSplits(float s0, float s1, float s2, float s3)
{
	m_cbPerFrame.vCascadeSplits.x = s0;
	m_cbPerFrame.vCascadeSplits.y = s1;
	m_cbPerFrame.vCascadeSplits.z = s2;
	m_cbPerFrame.vCascadeSplits.w = s3;
	m_bPerFrameDirty = true;
}

void CShaderManager::SetShadowTextures(ID3D11ShaderResourceView* pBig, ID3D11ShaderResourceView* pLocal)
{
	if (!GetActiveContext())
		return;
	ID3D11ShaderResourceView* shadowTextures[2] = { pBig, pLocal };
	m_pTextures[2] = pBig;
	m_pTextures[3] = pLocal;
	GetActiveContext()->PSSetShaderResources(2, 2, shadowTextures);
}

void CShaderManager::SetShadowMidFarTextures(ID3D11ShaderResourceView* pMid, ID3D11ShaderResourceView* pFar)
{
	if (!GetActiveContext())
		return;
	ID3D11ShaderResourceView* shadowTextures[2] = { pMid, pFar };
	m_pTextures[4] = pMid;
	m_pTextures[5] = pFar;
	GetActiveContext()->PSSetShaderResources(4, 2, shadowTextures);
}

void CShaderManager::SetLightingEnabled(bool bEnabled)
{
	m_bLightingEnabled = bEnabled;
	// Enable/disable light 0 (the primary directional light)
	EnableLight(0, bEnabled);
}

// Per-Object Updates
void CShaderManager::SetWorldMatrix(const Matrix* pWorld)
{
	XMMATRIX matWorld = XMLoadFloat4x4((XMFLOAT4X4*)pWorld);
	XMMATRIX matWVP;
		matWVP = matWorld * XMMatrixTranspose(m_cbPerFrame.matView) * XMMatrixTranspose(m_cbPerFrame.matProjection);
		m_cbPerObject.matWorld = XMMatrixTranspose(matWorld);
		m_cbPerObject.matWorldViewProj = XMMatrixTranspose(matWVP);
		m_bPerObjectDirty = true;
}

void CShaderManager::SetDiffuseColor(float r, float g, float b, float a)
{
		XMFLOAT4& cur = m_cbPerObject.vDiffuseColor;
		if (cur.x == r && cur.y == g && cur.z == b && cur.w == a)
			return;
		cur = XMFLOAT4(r, g, b, a);
		m_bPerObjectDirty = true;
}

void CShaderManager::SetAlphaTest(bool bEnabled, float fRef)
{
	float fEnabledVal = bEnabled ? 1.0f : 0.0f;
		if (m_cbPerObject.vMaterialParams.x == fRef &&
			m_cbPerObject.vMaterialParams.y == fEnabledVal)
			return;
		m_cbPerObject.vMaterialParams.x = fRef;
		m_cbPerObject.vMaterialParams.y = fEnabledVal;
		m_bPerObjectDirty = true;
}

void CShaderManager::SetMaterial(float fSpecularPower)
{
		if (m_cbPerObject.vMaterialParams.z == fSpecularPower)
			return;
		m_cbPerObject.vMaterialParams.z = fSpecularPower;
		m_bPerObjectDirty = true;
}

void CShaderManager::SetTextureColorSwap(bool bEnabled)
{
	float v = bEnabled ? 1.0f : 0.0f;
		if (m_cbPerObject.vMaterialParams.z == v) return;
		m_cbPerObject.vMaterialParams.z = v;
		m_bPerObjectDirty = true;
}

void CShaderManager::SetSpecularTune(float fIntensity, float fPower)
{
	XMFLOAT4& p = m_cbPerObject.vPBRParams;
	if (p.z == fIntensity && p.w == fPower) return;
	p.z = fIntensity;
	p.w = fPower;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetSpecularColor(float r, float g, float b)
{
		XMFLOAT4& cur = m_cbPerObject.vSpecularColor;
		if (cur.x == r && cur.y == g && cur.z == b) return;
		cur.x = r; cur.y = g; cur.z = b;
		m_bPerObjectDirty = true;
}

void CShaderManager::SetSpecularPower(float power)
{
		if (m_cbPerObject.vSpecularColor.w == power) return;
		m_cbPerObject.vSpecularColor.w = power;
		m_bPerObjectDirty = true;
}

void CShaderManager::SetPBRRoughness(float roughness)
{
		if (m_cbPerObject.vPBRParams.x == roughness) return;
		m_cbPerObject.vPBRParams.x = roughness;
		m_bPerObjectDirty = true;
}

void CShaderManager::SetPBRMetallic(float metallic)
{
		if (m_cbPerObject.vPBRParams.y == metallic) return;
		m_cbPerObject.vPBRParams.y = metallic;
		m_bPerObjectDirty = true;
}

void CShaderManager::SetEmissiveColor(float r, float g, float b)
{
		XMFLOAT4& cur = m_cbPerObject.vEmissiveColor;
		if (cur.x == r && cur.y == g && cur.z == b) return;
		cur = XMFLOAT4(r, g, b, 0.0f);
		m_bPerObjectDirty = true;
}

void CShaderManager::SetTwoTextureBlend(bool bEnabled)
{
	float v = bEnabled ? 1.0f : 0.0f;
		if (m_cbPerObject.vMaterialParams.w == v) return;
		m_cbPerObject.vMaterialParams.w = v;
		m_bPerObjectDirty = true;
}

bool CShaderManager::IsTwoTextureBlendEnabled() const
{
	return m_cbPerObject.vMaterialParams.w > 0.5f;
}

void CShaderManager::SetParticleColorOp(BYTE byColorOp)
{
	// Map texture-op values to shader float values:
	// ARG1 = factor, ARG2 = texture
	//   DISABLE (1) -> -1.0 (disable texture stage, use factor only with zero alpha)
	//   SELECTARG1 (2) -> 1.0 (factor/color only, ARG1=TFACTOR)
	//   SELECTARG2 (3) -> 2.0 (texture only, ARG2=TEXTURE)
	//   MODULATE (4) -> 0.0 (texture * factor - default)
	//   MODULATE2X (5) -> 3.0 (texture * factor * 2)
	//   MODULATE4X (6) -> 4.0 (texture * factor * 4)
	//   ADD (7) -> 5.0 (texture + factor)
	//   Other values -> 0.0 (fallback to modulate)
	float fColorOp = 0.0f;
	switch (byColorOp)
	{
	case 1:  // DISABLE - disables texture stage output
		fColorOp = -1.0f;
		break;
	case 2:  // SELECTARG1
		fColorOp = 1.0f;
		break;
	case 3:  // SELECTARG2
		fColorOp = 2.0f;
		break;
	case 5:  // MODULATE2X
		fColorOp = 3.0f;
		break;
	case 6:  // MODULATE4X
		fColorOp = 4.0f;
		break;
	case 7:  // ADD
		fColorOp = 5.0f;
		break;
	case 4:  // MODULATE
	default:
		fColorOp = 0.0f;
		break;
	}
		if (m_cbPerObject.vMaterialParams.w == fColorOp)
			return;
		m_cbPerObject.vMaterialParams.w = fColorOp;
		m_bPerObjectDirty = true;
}

void CShaderManager::SetMaterialParams(float x, float y, float z, float w)
{
		XMFLOAT4& cur = m_cbPerObject.vMaterialParams;
		if (cur.x == x && cur.y == y && cur.z == z && cur.w == w)
			return;
		cur.x = x;
		cur.y = y;
		cur.z = z;
		cur.w = w;
		m_bPerObjectDirty = true;
}

void CShaderManager::SetTextureMatrix(int slot, const Matrix* pMatrix)
{
	if (!pMatrix) return;

	XMMATRIX mat = XMLoadFloat4x4((const XMFLOAT4X4*)pMatrix);
	mat = XMMatrixTranspose(mat);

		if (slot == 0)
			m_cbPerObject.matTexture0 = mat;
		else if (slot == 1)
			m_cbPerObject.matTexture1 = mat;
		m_bPerObjectDirty = true;
}

void CShaderManager::SetCharacterShadowPass(bool bEnabled)
{
	float v = bEnabled ? 1.0f : 0.0f;
	if (m_cbPerObject.vRenderFlags.x == v)
		return;
	m_cbPerObject.vRenderFlags.x = v;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetParticleColor(DWORD dwColor)
{
	m_dwParticleColor = dwColor;
	XMFLOAT4 v(
		((dwColor >> 16) & 0xFF) / 255.0f,
		((dwColor >> 8) & 0xFF) / 255.0f,
		(dwColor & 0xFF) / 255.0f,
		((dwColor >> 24) & 0xFF) / 255.0f
	);
	XMFLOAT4& cur = m_cbPerObject.vParticleColor;
	if (cur.x == v.x && cur.y == v.y && cur.z == v.z && cur.w == v.w)
		return;
	cur = v;
	m_bPerObjectDirty = true;
}

void CShaderManager::SetSkyTint(DWORD dwColor)
{
	XMFLOAT4 vFactor(
		((dwColor >> 16) & 0xFF) / 255.0f,
		((dwColor >> 8) & 0xFF) / 255.0f,
		(dwColor & 0xFF) / 255.0f,
		((dwColor >> 24) & 0xFF) / 255.0f
	);
		m_dwSkyTint = dwColor;
		XMFLOAT4& cur = m_cbPerObject.vSkyTint;
		if (cur.x == vFactor.x && cur.y == vFactor.y && cur.z == vFactor.z && cur.w == vFactor.w)
			return;
		cur = vFactor;
		m_bPerObjectDirty = true;
}

void CShaderManager::__CommitCBRing(ID3D11DeviceContext* pCtx, ID3D11DeviceContext1* pCtx1,
                                    ID3D11Buffer* pBuf, UINT& rOffset, UINT& rBound, UINT ringBytes,
                                    const void* pSrc, UINT srcBytes, int vsSlot, int psSlot,
                                    bool* pForceDiscard)
{
	if (!pCtx || !pBuf || !pSrc) return;

	D3D11_MAPPED_SUBRESOURCE mapped;
	const UINT stride = CBRingAlign256(srcBytes);

	// Fallback: no offset-binding support (pre-D3D11.1 machines) — original behaviour.
	if (!m_bCBRingSupported || !pCtx1 || stride > ringBytes)
	{
		if (SUCCEEDED(pCtx->Map(pBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, pSrc, srcBytes);
			pCtx->Unmap(pBuf, 0);
			rBound = 0;
			if (vsSlot >= 0) pCtx->VSSetConstantBuffers((UINT)vsSlot, 1, &pBuf);
			if (psSlot >= 0) pCtx->PSSetConstantBuffers((UINT)psSlot, 1, &pBuf);
		}
		return;
	}

	// Wrap → DISCARD once (the sync point); otherwise append with NO_OVERWRITE.
	D3D11_MAP mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
	if (pForceDiscard && *pForceDiscard)
	{
		rOffset = 0;
		mapType = D3D11_MAP_WRITE_DISCARD;
		*pForceDiscard = false;
	}
	else if (rOffset + stride > ringBytes)
	{
		rOffset = 0;
		mapType = D3D11_MAP_WRITE_DISCARD;
	}

	if (FAILED(pCtx->Map(pBuf, 0, mapType, 0, &mapped)))
		return;
	memcpy(static_cast<BYTE*>(mapped.pData) + rOffset, pSrc, srcBytes);
	pCtx->Unmap(pBuf, 0);

	const UINT firstConstant = rOffset / 16u;
	const UINT numConstants  = stride  / 16u;
	if (vsSlot >= 0) pCtx1->VSSetConstantBuffers1((UINT)vsSlot, 1, &pBuf, &firstConstant, &numConstants);
	if (psSlot >= 0) pCtx1->PSSetConstantBuffers1((UINT)psSlot, 1, &pBuf, &firstConstant, &numConstants);

	rBound   = rOffset;
	rOffset += stride;
}

void CShaderManager::__BindCBRing(ID3D11DeviceContext* pCtx, ID3D11DeviceContext1* pCtx1,
                                  ID3D11Buffer* pBuf, UINT boundOffset, UINT srcBytes,
                                  int vsSlot, int psSlot, bool bRing)
{
	if (!pCtx || !pBuf) return;

	if (bRing && m_bCBRingSupported && pCtx1)
	{
		const UINT firstConstant = boundOffset / 16u;
		const UINT numConstants  = CBRingAlign256(srcBytes) / 16u;
		if (vsSlot >= 0) pCtx1->VSSetConstantBuffers1((UINT)vsSlot, 1, &pBuf, &firstConstant, &numConstants);
		if (psSlot >= 0) pCtx1->PSSetConstantBuffers1((UINT)psSlot, 1, &pBuf, &firstConstant, &numConstants);
		return;
	}

	if (vsSlot >= 0) pCtx->VSSetConstantBuffers((UINT)vsSlot, 1, &pBuf);
	if (psSlot >= 0) pCtx->PSSetConstantBuffers((UINT)psSlot, 1, &pBuf);
}


void CShaderManager::CommitChanges()
{
	if (!GetActiveContext()) return;

	D3D11_MAPPED_SUBRESOURCE mapped;


	// ===== Main thread path: existing code =====
	if (m_bPerFrameDirty && m_pCBPerFrame)
	{
		if (SUCCEEDED(GetActiveContext()->Map(m_pCBPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbPerFrame, sizeof(m_cbPerFrame));
			GetActiveContext()->Unmap(m_pCBPerFrame, 0);
			GetActiveContext()->VSSetConstantBuffers(0, 1, &m_pCBPerFrame);
			GetActiveContext()->PSSetConstantBuffers(0, 1, &m_pCBPerFrame);
			m_bPerFrameDirty = false;
		}
		else
		{
			TraceError("CommitChanges: Failed to map PerFrame constant buffer!");
		}
	}

	if (m_bPerObjectDirty && m_pCBPerObject)
	{
		__CommitCBRing(GetActiveContext(), m_pContext1, m_pCBPerObject,
		               m_cbPerObjectOffset, m_cbPerObjectBound,
		               CBRingAlign256(sizeof(CBPerObject)) * CB_RING_SLOTS_PEROBJECT,
		               &m_cbPerObject, sizeof(m_cbPerObject), 1, 1);
		m_bPerObjectDirty = false;
	}

	if (m_bLightingDirty && m_pCBLighting)
	{
		if (SUCCEEDED(GetActiveContext()->Map(m_pCBLighting, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbLighting, sizeof(m_cbLighting));
			GetActiveContext()->Unmap(m_pCBLighting, 0);
			GetActiveContext()->VSSetConstantBuffers(2, 1, &m_pCBLighting);
			GetActiveContext()->PSSetConstantBuffers(2, 1, &m_pCBLighting);
			m_bLightingDirty = false;
		}
		else
		{
			TraceError("CommitChanges: Failed to map Lighting constant buffer!");
		}
	}

	if (m_bSpeedTreeDirty && m_pCBSpeedTree && (m_eCurrentShader == SHADER_SPEEDTREE || m_eCurrentShader == SHADER_SPEEDTREE_LEAF))
	{
		if (SUCCEEDED(GetActiveContext()->Map(m_pCBSpeedTree, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbSpeedTree, sizeof(m_cbSpeedTree));
			GetActiveContext()->Unmap(m_pCBSpeedTree, 0);
			GetActiveContext()->VSSetConstantBuffers(3, 1, &m_pCBSpeedTree);
			m_bSpeedTreeDirty = false;
		}
		else
		{
			TraceError("CommitChanges: Failed to map SpeedTree constant buffer!");
		}
	}

	if (m_bSkinningDirty && m_pCBSkinning &&
		(m_eCurrentShader == SHADER_MESH_SKINNED || m_eCurrentShader == SHADER_SHADOW_SKINNED))
	{
		UINT poolIdx = m_dwSkinningPoolIndex;
		if (poolIdx < SKINNING_CB_POOL_SIZE && m_pSkinningCBPool[poolIdx])
		{
			GetActiveContext()->UpdateSubresource(m_pSkinningCBPool[poolIdx], 0, nullptr, &m_cbSkinning, 0, 0);
			GetActiveContext()->VSSetConstantBuffers(3, 1, &m_pSkinningCBPool[poolIdx]);
			m_dwSkinningPoolIndex++;
			m_bSkinningDirty = false;
		}
		else
		{
			if (SUCCEEDED(GetActiveContext()->Map(m_pCBSkinning, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
			{
				memcpy(mapped.pData, &m_cbSkinning, sizeof(CBSkinning));
				GetActiveContext()->Unmap(m_pCBSkinning, 0);
				GetActiveContext()->VSSetConstantBuffers(3, 1, &m_pCBSkinning);
				m_bSkinningDirty = false;
			}
			else
			{
				TraceError("CommitChanges: Failed to map Skinning constant buffer!");
			}
		}
	}

	if (m_bSkyGradientDirty && m_pCBSkyGradient && m_eCurrentShader == SHADER_SKY)
	{
		if (SUCCEEDED(GetActiveContext()->Map(m_pCBSkyGradient, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbSkyGradient, sizeof(m_cbSkyGradient));
			GetActiveContext()->Unmap(m_pCBSkyGradient, 0);
			GetActiveContext()->PSSetConstantBuffers(2, 1, &m_pCBSkyGradient);
			m_bSkyGradientDirty = false;
		}
	}

}

void CShaderManager::SetShaderResource(UINT slot, ID3D11ShaderResourceView* pSRV)
{
	if (!GetActiveContext()) return;

	ID3D11ShaderResourceView* pActualSRV = pSRV;
	if (!pSRV && m_pActiveDefaultTextureSRV)
		pActualSRV = m_pActiveDefaultTextureSRV;

	if (slot < STATEMANAGER_MAX_STAGES)
	{
		if (m_pTextures[slot] == pActualSRV)
			return;
		m_pTextures[slot] = pActualSRV;
	}
	GetActiveContext()->PSSetShaderResources(slot, 1, &pActualSRV);
}

void CShaderManager::SetDefaultTexture(UINT slot)
{
	if (!GetActiveContext()) return;

	ID3D11ShaderResourceView* pTexture = m_pDefaultTextureSRV;
	if (!pTexture) return;

	// Only update shared texture tracking on main thread
	if (slot < STATEMANAGER_MAX_STAGES)
		m_pTextures[slot] = pTexture;
	GetActiveContext()->PSSetShaderResources(slot, 1, &pTexture);
}

void CShaderManager::OnFrameComplete()
{
	++m_iFrameCount;

	if (m_iFrameCount == 15 && m_pActiveDefaultTextureSRV == m_pTransparentTextureSRV)
	{
		m_pActiveDefaultTextureSRV = m_pDefaultTextureSRV;
	}
}

//////////////////////////////////////////////////////////////////////////
// Multi-Light Support (Native DX11)
//////////////////////////////////////////////////////////////////////////

void CShaderManager::SetLight(UINT index, const DX11Light& light)
{
	if (index >= MAX_SHADER_LIGHTS) return;


	m_cbLighting.lights[index] = light;
	m_bLightingDirty = true;

	// Update active light count
	int numActive = 0;
	for (int i = 0; i < MAX_SHADER_LIGHTS; ++i)
	{
		if (m_cbLighting.lights[i].Direction.w > 0.5f)
			++numActive;
	}
	m_cbLighting.numActiveLights = numActive;
}

void CShaderManager::GetLight(UINT index, DX11Light* pLight) const
{
	if (index >= MAX_SHADER_LIGHTS || !pLight) return;
	*pLight = m_cbLighting.lights[index];
}

void CShaderManager::GetLight(UINT index, TLight* pLight) const
{
	if (index >= MAX_SHADER_LIGHTS || !pLight) return;

	const DX11Light& src = m_cbLighting.lights[index];

	// Convert DX11Light back to TLight format
	pLight->Type = (ELightType)(int)src.Position.w;
	pLight->Position.x = src.Position.x;
	pLight->Position.y = src.Position.y;
	pLight->Position.z = src.Position.z;
	pLight->Direction.x = src.Direction.x;
	pLight->Direction.y = src.Direction.y;
	pLight->Direction.z = src.Direction.z;
	pLight->Diffuse.r = src.Color.x;
	pLight->Diffuse.g = src.Color.y;
	pLight->Diffuse.b = src.Color.z;
	pLight->Diffuse.a = src.Color.w;
	pLight->Attenuation0 = src.Attenuation.x;
	pLight->Attenuation1 = src.Attenuation.y;
	pLight->Attenuation2 = src.Attenuation.z;
	pLight->Range = src.Attenuation.w;
	// Initialize other fields to defaults
	pLight->Ambient = Color(0.0f, 0.0f, 0.0f, 1.0f);
	pLight->Specular = Color(0.0f, 0.0f, 0.0f, 1.0f);
	pLight->Falloff = 1.0f;
	pLight->Theta = 0.0f;
	pLight->Phi = 0.0f;
}

bool CShaderManager::IsLightEnabled(UINT index) const
{
	if (index >= MAX_SHADER_LIGHTS) return false;
	return m_cbLighting.lights[index].Direction.w > 0.5f;
}

void CShaderManager::EnableLight(UINT index, bool bEnable)
{
	if (index >= MAX_SHADER_LIGHTS) return;


	m_cbLighting.lights[index].Direction.w = bEnable ? 1.0f : 0.0f;
	m_bLightingDirty = true;

	// Update active light count
	int numActive = 0;
	for (int i = 0; i < MAX_SHADER_LIGHTS; ++i)
	{
		if (m_cbLighting.lights[i].Direction.w > 0.5f)
			++numActive;
	}
	m_cbLighting.numActiveLights = numActive;
}

void CShaderManager::SetGlobalAmbient(const XMFLOAT4& color)
{
	m_cbLighting.globalAmbient = color;
	m_bLightingDirty = true;
}

void CShaderManager::SetGlobalAmbient(float r, float g, float b, float a)
{
	SetGlobalAmbient(XMFLOAT4(r, g, b, a));
}


void PendingRenderState::SetDefaults()
{
	bAlphaBlendEnable = false;
	srcBlend = D3D11_BLEND_SRC_ALPHA;
	destBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendOp = D3D11_BLEND_OP_ADD;
	srcBlendAlpha = D3D11_BLEND_ONE;
	destBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendOpAlpha = D3D11_BLEND_OP_ADD;
	colorWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	// Rasterizer state defaults
	fillMode = D3D11_FILL_SOLID;
	cullMode = D3D11_CULL_FRONT;  // Match game convention: CULL_FRONT culls CW (front) faces, keeps CCW visible
	bScissorEnable = false;
	bMultisampleEnable = false;
	bAntialiasedLineEnable = false;
	depthBias = 0;
	depthBiasClamp = 0.0f;
	slopeScaledDepthBias = 0.0f;
	bDepthClipEnable = true;

	// Depth stencil state defaults
	bDepthEnable = true;
	bDepthWriteEnable = true;
	depthFunc = D3D11_COMPARISON_LESS_EQUAL;
	bStencilEnable = false;
	stencilReadMask = 0xFF;
	stencilWriteMask = 0xFF;
}

void CShaderManager::SetPipelineState(EPipelineState state, DWORD value)
{
	PendingRenderState& rs = m_RenderState;
	bool& bBlendDirty = m_bBlendStateDirty;
	bool& bRasterDirty = m_bRasterizerStateDirty;
	bool& bDepthDirty = m_bDepthStencilStateDirty;

	switch (state)
	{
	// Blend states
	case PSTATE_BLENDENABLE:
		{ bool v = (value != 0); if (rs.bAlphaBlendEnable == v) return; rs.bAlphaBlendEnable = v; }
		bBlendDirty = true;
		break;
	case PSTATE_SRCBLEND:
		if (rs.srcBlend == (D3D11_BLEND)value) return;
		rs.srcBlend = (D3D11_BLEND)value;
		bBlendDirty = true;
		break;
	case PSTATE_DESTBLEND:
		if (rs.destBlend == (D3D11_BLEND)value) return;
		rs.destBlend = (D3D11_BLEND)value;
		bBlendDirty = true;
		break;
	case PSTATE_BLENDOP:
		if (rs.blendOp == (D3D11_BLEND_OP)value) return;
		rs.blendOp = (D3D11_BLEND_OP)value;
		bBlendDirty = true;
		break;
	case PSTATE_RTWRITEMASK:
		if (rs.colorWriteMask == (UINT8)value) return;
		rs.colorWriteMask = (UINT8)value;
		bBlendDirty = true;
		break;

	// Rasterizer states
	case PSTATE_FILLMODE:
		rs.fillMode = (value == FILL_WIREFRAME) ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
		bRasterDirty = true;
		break;
	case PSTATE_CULLMODE:
		{
			D3D11_CULL_MODE newCull = rs.cullMode;
			switch (value)
			{
			case CULL_NONE: newCull = D3D11_CULL_NONE; break;
			case CULL_FRONT:   newCull = D3D11_CULL_FRONT; break;
			case CULL_BACK:  newCull = D3D11_CULL_BACK; break;
			}
			if (rs.cullMode == newCull) return;
			rs.cullMode = newCull;
		}
		bRasterDirty = true;
		break;
	case PSTATE_SCISSORENABLE:
		{ bool v = (value != 0); if (rs.bScissorEnable == v) return; rs.bScissorEnable = v; }
		bRasterDirty = true;
		break;
	case PSTATE_DEPTHBIAS:
		if (rs.depthBias == (INT)value) return;
		rs.depthBias = (INT)value;
		bRasterDirty = true;
		break;
	case PSTATE_SLOPESCALEDDEPTHBIAS:
		{ float v = *(float*)&value; if (rs.slopeScaledDepthBias == v) return; rs.slopeScaledDepthBias = v; }
		bRasterDirty = true;
		break;

	// Depth stencil states
	case PSTATE_DEPTHENABLE:
		{ bool v = (value != 0); if (rs.bDepthEnable == v) return; rs.bDepthEnable = v; }
		bDepthDirty = true;
		break;
	case PSTATE_DEPTHWRITEMASK:
		{ bool v = (value != 0); if (rs.bDepthWriteEnable == v) return; rs.bDepthWriteEnable = v; }
		bDepthDirty = true;
		break;
	case PSTATE_DEPTHFUNC:
		if (rs.depthFunc == (D3D11_COMPARISON_FUNC)value) return;
		rs.depthFunc = (D3D11_COMPARISON_FUNC)value;
		bDepthDirty = true;
		break;
	case PSTATE_STENCILENABLE:
		{ bool v = (value != 0); if (rs.bStencilEnable == v) return; rs.bStencilEnable = v; }
		bDepthDirty = true;
		break;
	case PSTATE_STENCILREADMASK:
		if (rs.stencilReadMask == (UINT8)value) return;
		rs.stencilReadMask = (UINT8)value;
		bDepthDirty = true;
		break;
	case PSTATE_STENCILWRITEMASK:
		if (rs.stencilWriteMask == (UINT8)value) return;
		rs.stencilWriteMask = (UINT8)value;
		bDepthDirty = true;
		break;
	default:
		break;
	}
}

DWORD CShaderManager::GetPipelineState(EPipelineState state)
{
	const PendingRenderState& rs = m_RenderState;

	switch (state)
	{
	case PSTATE_BLENDENABLE: return rs.bAlphaBlendEnable ? 1 : 0;
	case PSTATE_SRCBLEND: return (DWORD)rs.srcBlend;
	case PSTATE_DESTBLEND: return (DWORD)rs.destBlend;
	case PSTATE_BLENDOP: return (DWORD)rs.blendOp;
	case PSTATE_RTWRITEMASK: return rs.colorWriteMask;
	case PSTATE_FILLMODE: return (rs.fillMode == D3D11_FILL_WIREFRAME) ? FILL_WIREFRAME : FILL_SOLID;
	case PSTATE_CULLMODE:
		switch (rs.cullMode)
		{
		case D3D11_CULL_NONE: return CULL_NONE;
		case D3D11_CULL_FRONT: return CULL_FRONT;
		case D3D11_CULL_BACK: return CULL_BACK;
		}
		return CULL_NONE;
	case PSTATE_DEPTHENABLE: return rs.bDepthEnable ? 1 : 0;
	case PSTATE_DEPTHWRITEMASK: return rs.bDepthWriteEnable ? 1 : 0;
	case PSTATE_DEPTHFUNC: return (DWORD)rs.depthFunc;
	case PSTATE_STENCILENABLE: return rs.bStencilEnable ? 1 : 0;
	case PSTATE_STENCILREADMASK: return rs.stencilReadMask;
	case PSTATE_STENCILWRITEMASK: return rs.stencilWriteMask;
	default: return 0;
	}
}

void CShaderManager::SavePipelineState(EPipelineState state, DWORD value)
{
	auto& savedStates = m_SavedRenderStates;
	savedStates[state] = GetPipelineState(state);
	SetPipelineState(state, value);
}

void CShaderManager::RestorePipelineState(EPipelineState state)
{
	auto& savedStates = m_SavedRenderStates;
	auto it = savedStates.find(state);
	if (it != savedStates.end())
	{
		SetPipelineState(state, it->second);
		savedStates.erase(it);
	}
}

void CShaderManager::UpdateBlendState()
{
	bool& bDirty = m_bBlendStateDirty;
	if (!bDirty || !GetActiveContext() || !m_pStateCache) return;

	const PendingRenderState& rs = m_RenderState;

	D3D11_BLEND_DESC desc = {};
	desc.RenderTarget[0].BlendEnable = rs.bAlphaBlendEnable;
	desc.RenderTarget[0].SrcBlend = rs.srcBlend;
	desc.RenderTarget[0].DestBlend = rs.destBlend;
	desc.RenderTarget[0].BlendOp = rs.blendOp;
	desc.RenderTarget[0].SrcBlendAlpha = rs.srcBlendAlpha;
	desc.RenderTarget[0].DestBlendAlpha = rs.destBlendAlpha;
	desc.RenderTarget[0].BlendOpAlpha = rs.blendOpAlpha;
	desc.RenderTarget[0].RenderTargetWriteMask = rs.colorWriteMask;

	ID3D11BlendState*& pCurrentState = m_pCurrentBlendState;
	ID3D11BlendState* pState = m_pStateCache->GetBlendState(desc);
	if (pState && pState != pCurrentState)
	{
		float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		GetActiveContext()->OMSetBlendState(pState, blendFactor, 0xFFFFFFFF);
		pCurrentState = pState;
	}
	bDirty = false;
}

void CShaderManager::UpdateRasterizerState()
{
	bool& bDirty = m_bRasterizerStateDirty;
	if (!bDirty || !GetActiveContext() || !m_pStateCache) return;

	const PendingRenderState& rs = m_RenderState;

	D3D11_RASTERIZER_DESC desc = {};
	desc.FillMode = rs.fillMode;
	desc.CullMode = rs.cullMode;
	desc.FrontCounterClockwise = FALSE;
	desc.DepthBias = rs.depthBias;
	desc.DepthBiasClamp = rs.depthBiasClamp;
	desc.SlopeScaledDepthBias = rs.slopeScaledDepthBias;
	desc.DepthClipEnable = rs.bDepthClipEnable;
	desc.ScissorEnable = rs.bScissorEnable;
	desc.MultisampleEnable = rs.bMultisampleEnable;
	desc.AntialiasedLineEnable = rs.bAntialiasedLineEnable;

	ID3D11RasterizerState*& pCurrentState = m_pCurrentRasterizerState;
	ID3D11RasterizerState* pState = m_pStateCache->GetRasterizerState(desc);
	if (pState && pState != pCurrentState)
	{
		GetActiveContext()->RSSetState(pState);
		pCurrentState = pState;
	}
	bDirty = false;
}

void CShaderManager::UpdateDepthStencilState()
{
	bool& bDirty = m_bDepthStencilStateDirty;
	if (!bDirty || !GetActiveContext() || !m_pStateCache) return;

	const PendingRenderState& rs = m_RenderState;

	D3D11_DEPTH_STENCIL_DESC desc = {};
	desc.DepthEnable = rs.bDepthEnable;
	desc.DepthWriteMask = rs.bDepthWriteEnable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = rs.depthFunc;
	desc.StencilEnable = rs.bStencilEnable;
	desc.StencilReadMask = rs.stencilReadMask;
	desc.StencilWriteMask = rs.stencilWriteMask;
	desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	desc.BackFace = desc.FrontFace;

	ID3D11DepthStencilState*& pCurrentState = m_pCurrentDepthStencilState;
	ID3D11DepthStencilState* pState = m_pStateCache->GetDepthStencilState(desc);
	if (pState && pState != pCurrentState)
	{
		GetActiveContext()->OMSetDepthStencilState(pState, 0);
		pCurrentState = pState;
	}
	bDirty = false;
}

void CShaderManager::CommitRenderState()
{
	UpdateBlendState();
	UpdateRasterizerState();
	UpdateDepthStencilState();
}

void CShaderManager::ApplyRenderStates()
{
	CommitRenderState();
}

void CShaderManager::SetVertexBuffer(UINT stream, ID3D11Buffer* pBuffer, UINT stride, UINT offset)
{
	if (stream >= MAX_STREAMS) return;
	if (!GetActiveContext()) return;

		if (m_Streams[stream].pBuffer == pBuffer &&
			m_Streams[stream].stride == stride &&
			m_Streams[stream].offset == offset)
			return;
		m_Streams[stream].pBuffer = pBuffer;
		m_Streams[stream].stride = stride;
		m_Streams[stream].offset = offset;

	GetActiveContext()->IASetVertexBuffers(stream, 1, &pBuffer, &stride, &offset);
}

void CShaderManager::SetIndexBuffer(ID3D11Buffer* pBuffer, DXGI_FORMAT format, UINT offset)
{
	if (!GetActiveContext()) return;

		if (m_pCurrentIndexBuffer == pBuffer &&
			m_IndexFormat == format &&
			m_IndexOffset == offset)
			return;
		m_pCurrentIndexBuffer = pBuffer;
		m_IndexFormat = format;
		m_IndexOffset = offset;

	GetActiveContext()->IASetIndexBuffer(pBuffer, format, offset);
}

void CShaderManager::SetPrimitiveTopologyIfChanged(D3D11_PRIMITIVE_TOPOLOGY topology)
{
	if (!GetActiveContext()) return;
	if (topology == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED) return;

		if (m_CurrentTopology == topology) return;
		m_CurrentTopology = topology;

	GetActiveContext()->IASetPrimitiveTopology(topology);
}

void CShaderManager::InvalidateIACache()
{
		for (DWORD i = 0; i < MAX_STREAMS; ++i)
		{
			m_Streams[i].pBuffer = nullptr;
			m_Streams[i].stride = 0;
			m_Streams[i].offset = 0;
		}
		m_pCurrentIndexBuffer = nullptr;
		m_IndexFormat = DXGI_FORMAT_UNKNOWN;
		m_IndexOffset = 0;
		m_CurrentTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

static D3D11_PRIMITIVE_TOPOLOGY GetD3D11Topology(EPrimitiveTopology type)
{
	switch (type)
	{
	case TOPOLOGY_NONE:          return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;  // Don't change topology
	case TOPOLOGY_POINTLIST:     return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	case TOPOLOGY_LINELIST:      return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	case TOPOLOGY_LINESTRIP:     return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case TOPOLOGY_TRIANGLELIST:  return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case TOPOLOGY_TRIANGLESTRIP: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	default:               return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
}

static UINT GetVertexCount(EPrimitiveTopology type, UINT primitiveCount)
{
	switch (type)
	{
	case TOPOLOGY_NONE:          return primitiveCount * 4;  // Tessellation patches (4 control points each)
	case TOPOLOGY_POINTLIST:     return primitiveCount;
	case TOPOLOGY_LINELIST:      return primitiveCount * 2;
	case TOPOLOGY_LINESTRIP:     return primitiveCount + 1;
	case TOPOLOGY_TRIANGLELIST:  return primitiveCount * 3;
	case TOPOLOGY_TRIANGLESTRIP: return primitiveCount + 2;
	default:               return primitiveCount * 3;
	}
}

static UINT GetIndexCount(EPrimitiveTopology type, UINT primitiveCount)
{
	switch (type)
	{
	case TOPOLOGY_NONE:          return primitiveCount * 4;  // Tessellation patches (4 control points each)
	case TOPOLOGY_POINTLIST:     return primitiveCount;
	case TOPOLOGY_LINELIST:      return primitiveCount * 2;
	case TOPOLOGY_LINESTRIP:     return primitiveCount + 1;
	case TOPOLOGY_TRIANGLELIST:  return primitiveCount * 3;
	case TOPOLOGY_TRIANGLESTRIP: return primitiveCount + 2;
	default:               return primitiveCount * 3;
	}
}

void CShaderManager::Draw(EPrimitiveTopology type, UINT startVertex, UINT primitiveCount)
{
	if (!GetActiveContext()) return;
	if (primitiveCount == 0) return;  // Prevent empty draws

	UINT vertexCount = GetVertexCount(type, primitiveCount);
	if (vertexCount == 0) return;

	// Use thread-local shader tracking on worker threads
	EShaderType eActiveShader = m_eCurrentShader;

	if (eActiveShader == SHADER_NONE)
	{
		BeginUI();
	}

	CommitRenderState();
	CommitChanges();

	SetPrimitiveTopologyIfChanged(GetD3D11Topology(type));
	GetActiveContext()->Draw(vertexCount, startVertex);
	IncrementGlobalDrawCount();
}

void CShaderManager::DrawIndexed(EPrimitiveTopology type, UINT minIndex, UINT numVertices, UINT startIndex, UINT primitiveCount, INT baseVertex)
{
	if (!GetActiveContext())
	{
		TraceError("DrawIndexed: Context is NULL!");
		return;
	}
	if (primitiveCount == 0) return;  // Prevent empty draws

	// Use thread-local shader tracking on worker threads
	EShaderType eActiveShader = m_eCurrentShader;

	// Ensure a shader is bound — default to UI shader which is compatible
	// with PDT vertex format (POSITION+COLOR+TEXCOORD).
	if (eActiveShader == SHADER_NONE)
	{
		BeginUI();
		eActiveShader = m_eCurrentShader;
	}

	CommitRenderState();
	CommitChanges();

	{
		UINT indexCount = GetIndexCount(type, primitiveCount);
		if (indexCount == 0) return;
		if (type != TOPOLOGY_NONE)
			SetPrimitiveTopologyIfChanged(GetD3D11Topology(type));
		GetActiveContext()->DrawIndexed(indexCount, startIndex, baseVertex);
	}
	IncrementGlobalDrawCount();
}

void CShaderManager::DrawIndexed(EPrimitiveTopology type, UINT minIndex, UINT numVertices, UINT startIndex, UINT primitiveCount)
{
	// 5-argument overload with baseVertex=0
	DrawIndexed(type, minIndex, numVertices, startIndex, primitiveCount, 0);
}

void CShaderManager::DrawIndexedInstanced(EPrimitiveTopology type, UINT indexCountPerInstance, UINT instanceCount, UINT startIndex, INT baseVertex, UINT startInstance)
{
	if (!GetActiveContext() || instanceCount == 0 || indexCountPerInstance == 0) return;

	EShaderType eActiveShader = m_eCurrentShader;
	if (eActiveShader == SHADER_NONE)
	{
		TraceError("DrawIndexedInstanced: No shader bound!");
		return;
	}

	CommitRenderState();
	CommitChanges();

	if (type != TOPOLOGY_NONE)
		SetPrimitiveTopologyIfChanged(GetD3D11Topology(type));

	GetActiveContext()->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndex, baseVertex, startInstance);
	IncrementGlobalDrawCount();
}

void CShaderManager::DrawInstanced(EPrimitiveTopology type, UINT vertexCountPerInstance, UINT instanceCount, UINT startVertex, UINT startInstance)
{
	if (!GetActiveContext() || instanceCount == 0 || vertexCountPerInstance == 0) return;

	EShaderType eActiveShader = m_eCurrentShader;
	if (eActiveShader == SHADER_NONE)
	{
		TraceError("DrawInstanced: No shader bound!");
		return;
	}

	CommitRenderState();
	CommitChanges();

	if (type != TOPOLOGY_NONE)
		SetPrimitiveTopologyIfChanged(GetD3D11Topology(type));

	GetActiveContext()->DrawInstanced(vertexCountPerInstance, instanceCount, startVertex, startInstance);
	IncrementGlobalDrawCount();
}

bool CShaderManager::CreateDynamicBuffers()
{
	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.ByteWidth = DYNAMIC_VB_SIZE;
	vbDesc.Usage = D3D11_USAGE_DYNAMIC;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (FAILED(m_pDevice->CreateBuffer(&vbDesc, nullptr, &m_pDynamicVertexBuffer)))
		return false;

	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = DYNAMIC_IB_SIZE;
	ibDesc.Usage = D3D11_USAGE_DYNAMIC;
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (FAILED(m_pDevice->CreateBuffer(&ibDesc, nullptr, &m_pDynamicIndexBuffer)))
		return false;

	// Initialize ring buffer tracking
	m_dwDynamicVBOffset = 0;
	m_dwDynamicIBOffset = 0;
	m_bDynamicBufferNeedsDiscard = true;

	return true;
}

void CShaderManager::ResetDynamicBuffers()
{
	// Called at frame start to reset ring buffer positions
	m_dwDynamicVBOffset = 0;
	m_dwDynamicIBOffset = 0;
	m_bDynamicBufferNeedsDiscard = true;
	m_dwSkinningPoolIndex = 0;
}

void CShaderManager::DrawDynamic(EPrimitiveTopology type, UINT primitiveCount, const void* pVertexData, UINT stride)
{
	if (!GetActiveContext() || !pVertexData) return;
	if (stride == 0 || primitiveCount == 0) return;  // Prevent division by zero and empty draws

	UINT vertexCount = GetVertexCount(type, primitiveCount);
	if (vertexCount == 0) return;

	EShaderType eActiveShader = m_eCurrentShader;
	if (eActiveShader == SHADER_NONE)
	{
		BeginUI();
	}

	UINT dataSize = vertexCount * stride;

	if (dataSize > DYNAMIC_VB_SIZE) return;

	// Select per-thread or shared dynamic buffer
	ID3D11Buffer*& pDynVB   = m_pDynamicVertexBuffer;
	DWORD&         dwVBOff  = m_dwDynamicVBOffset;
	bool&          bDiscard = m_bDynamicBufferNeedsDiscard;

	if (!pDynVB) return;

	// Check if we need to wrap around or discard
	D3D11_MAP mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
	UINT bufferOffset = dwVBOff;

	if (bDiscard || (dwVBOff + dataSize > DYNAMIC_VB_SIZE))
	{
		mapType = D3D11_MAP_WRITE_DISCARD;
		bufferOffset = 0;
		dwVBOff = 0;
		bDiscard = false;
	}

	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = GetActiveContext()->Map(pDynVB, 0, mapType, 0, &mapped);
	if (SUCCEEDED(hr))
	{
		memcpy((BYTE*)mapped.pData + bufferOffset, pVertexData, dataSize);
		GetActiveContext()->Unmap(pDynVB, 0);
	}
	else
	{
		TraceError("DrawDynamic: Failed to map vertex buffer (hr=0x%08X, size=%d, offset=%d)", hr, dataSize, bufferOffset);
		return;
	}

	// Update offset for next call
	dwVBOff = bufferOffset + dataSize;

	SetVertexBuffer(0, pDynVB, stride, bufferOffset);

	CommitRenderState();
	CommitChanges();

	SetPrimitiveTopologyIfChanged(GetD3D11Topology(type));
	GetActiveContext()->Draw(vertexCount, 0);
	IncrementGlobalDrawCount();
}

//--------------------------------------------------------------------
// Batched Rendering Support
//--------------------------------------------------------------------
bool CShaderManager::MapDynamicVB(UINT requiredBytes, MappedDynamicVB& outMapped)
{
	if (!GetActiveContext()) return false;

	ID3D11Buffer*& pDynVB   = m_pDynamicVertexBuffer;
	DWORD&         dwVBOff  = m_dwDynamicVBOffset;
	bool&          bDiscard = m_bDynamicBufferNeedsDiscard;

	if (!pDynVB || requiredBytes > DYNAMIC_VB_SIZE) return false;

	D3D11_MAP mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
	UINT bufferOffset = dwVBOff;

	if (bDiscard || (dwVBOff + requiredBytes > DYNAMIC_VB_SIZE))
	{
		mapType = D3D11_MAP_WRITE_DISCARD;
		bufferOffset = 0;
		dwVBOff = 0;
		bDiscard = false;
	}

	D3D11_MAPPED_SUBRESOURCE mapped;
	if (FAILED(GetActiveContext()->Map(pDynVB, 0, mapType, 0, &mapped)))
		return false;

	outMapped.pData = (BYTE*)mapped.pData + bufferOffset;
	outMapped.byteOffset = bufferOffset;
	outMapped.maxBytes = DYNAMIC_VB_SIZE - bufferOffset;
	return true;
}

void CShaderManager::UnmapDynamicVB()
{
	ID3D11Buffer*& pDynVB = m_pDynamicVertexBuffer;
	if (pDynVB && GetActiveContext())
		GetActiveContext()->Unmap(pDynVB, 0);
}

void CShaderManager::AdvanceDynamicVBOffset(UINT bytesUsed)
{
	DWORD& dwVBOff = m_dwDynamicVBOffset;
	dwVBOff += bytesUsed;
}

void CShaderManager::DrawBatchedQuads(UINT stride, UINT vbByteOffset, UINT firstQuad, UINT quadCount)
{
	if (!GetActiveContext() || quadCount == 0) return;

	ID3D11Buffer*& pDynVB = m_pDynamicVertexBuffer;
	if (!pDynVB) return;

	SetVertexBuffer(0, pDynVB, stride, vbByteOffset);

	CommitRenderState();
	CommitChanges();

	SetPrimitiveTopologyIfChanged(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	GetActiveContext()->DrawIndexed(quadCount * 6, firstQuad * 6, 0);
	IncrementGlobalDrawCount();
}

void CShaderManager::DrawIndexedDynamic(EPrimitiveTopology type, UINT minIndex, UINT numVertices, UINT primitiveCount, const void* pIndexData, DXGI_FORMAT indexFormat, const void* pVertexData, UINT stride)
{
	if (!GetActiveContext() || !pVertexData || !pIndexData) return;
	if (stride == 0 || primitiveCount == 0 || numVertices == 0) return;  // Prevent division by zero and empty draws

	UINT indexCount = GetIndexCount(type, primitiveCount);
	if (indexCount == 0) return;

	EShaderType eActiveShader = m_eCurrentShader;
	if (eActiveShader == SHADER_NONE)
	{
		BeginUI();
	}

	UINT indexSize = (indexFormat == DXGI_FORMAT_R32_UINT) ? 4 : 2;
	UINT indexDataSize = indexCount * indexSize;
	UINT vertexDataSize = numVertices * stride;

	if (indexDataSize > DYNAMIC_IB_SIZE || vertexDataSize > DYNAMIC_VB_SIZE) return;

	// Select per-thread or shared dynamic buffers
	ID3D11Buffer*& pDynVB   = m_pDynamicVertexBuffer;
	ID3D11Buffer*& pDynIB   = m_pDynamicIndexBuffer;
	DWORD&         dwVBOff  = m_dwDynamicVBOffset;
	DWORD&         dwIBOff  = m_dwDynamicIBOffset;
	bool&          bDiscard = m_bDynamicBufferNeedsDiscard;

	if (!pDynVB || !pDynIB) return;

	// Determine if we need to discard
	bool bNeedVBDiscard = bDiscard || (dwVBOff + vertexDataSize > DYNAMIC_VB_SIZE);
	bool bNeedIBDiscard = bDiscard || (dwIBOff + indexDataSize > DYNAMIC_IB_SIZE);

	// Track offsets
	UINT vbOffset = bNeedVBDiscard ? 0 : dwVBOff;
	UINT ibOffset = bNeedIBDiscard ? 0 : dwIBOff;

	// Map vertex buffer
	D3D11_MAP vbMapType = bNeedVBDiscard ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = GetActiveContext()->Map(pDynVB, 0, vbMapType, 0, &mapped);
	if (SUCCEEDED(hr))
	{
		memcpy((BYTE*)mapped.pData + vbOffset, pVertexData, vertexDataSize);
		GetActiveContext()->Unmap(pDynVB, 0);
	}
	else
	{
		TraceError("DrawIndexedDynamic: Failed to map vertex buffer (hr=0x%08X, size=%d, offset=%d)", hr, vertexDataSize, vbOffset);
		return;
	}

	// Map index buffer
	D3D11_MAP ibMapType = bNeedIBDiscard ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
	hr = GetActiveContext()->Map(pDynIB, 0, ibMapType, 0, &mapped);
	if (SUCCEEDED(hr))
	{
		memcpy((BYTE*)mapped.pData + ibOffset, pIndexData, indexDataSize);
		GetActiveContext()->Unmap(pDynIB, 0);
	}
	else
	{
		TraceError("DrawIndexedDynamic: Failed to map index buffer (hr=0x%08X, size=%d, offset=%d)", hr, indexDataSize, ibOffset);
		return;
	}

	// Update offsets for next call
	dwVBOff = bNeedVBDiscard ? vertexDataSize : (dwVBOff + vertexDataSize);
	dwIBOff = bNeedIBDiscard ? indexDataSize : (dwIBOff + indexDataSize);
	bDiscard = false;

	SetVertexBuffer(0, pDynVB, stride, vbOffset);
	SetIndexBuffer(pDynIB, indexFormat, ibOffset);

	CommitRenderState();
	CommitChanges();

	SetPrimitiveTopologyIfChanged(GetD3D11Topology(type));
	GetActiveContext()->DrawIndexed(indexCount, 0, 0);
	IncrementGlobalDrawCount();
}

void CShaderManager::SetMatrix(EMatrixSlot state, const Matrix* pMatrix)
{
	if (!pMatrix) return;

	if (state < MAX_TRANSFORMS)
	{
		m_Matrices[state] = *pMatrix;
	}
	else
	{
		return;
	}

	// Sync with shader constant buffers
	if (state == MATRIX_WORLD)
	{
		SetWorldMatrix(pMatrix);
	}
	else if (state == MATRIX_VIEW || state == MATRIX_PROJECTION)
	{
			SetViewProjection(&m_Matrices[MATRIX_VIEW], &m_Matrices[MATRIX_PROJECTION]);
	}
	else if (state == MATRIX_TEXTURE0)
	{
		SetTextureMatrix(0, pMatrix);
	}
	else if (state == MATRIX_TEXTURE1)
	{
		SetTextureMatrix(1, pMatrix);
	}
}

void CShaderManager::GetMatrix(EMatrixSlot state, Matrix* pMatrix)
{
	if (!pMatrix) return;

	if (state < MAX_TRANSFORMS)
	{
		*pMatrix = m_Matrices[state];
	}
}

void CShaderManager::SaveTransform(EMatrixSlot state, const Matrix* pMatrix)
{
	if (state < MAX_TRANSFORMS)
	{
		m_SavedMatrices[state] = m_Matrices[state];
	}
	else
	{
		return;
	}

	if (pMatrix)
		SetMatrix(state, pMatrix);
}

void CShaderManager::RestoreTransform(EMatrixSlot state)
{
	if (state < MAX_TRANSFORMS)
	{
		SetMatrix(state, &m_SavedMatrices[state]);
	}
}

void CShaderManager::SetInputLayout(EInputLayoutType type)
{
		m_CurrentInputLayout = type;
	BindForInputLayout(type);
}

void CShaderManager::SetInputLayout(ID3D11InputLayout* pLayout)
{
	if (GetActiveContext() && pLayout)
		GetActiveContext()->IASetInputLayout(pLayout);
}

void CShaderManager::SaveInputLayout(EInputLayoutType type)
{
		m_SavedInputLayout = m_CurrentInputLayout;
	SetInputLayout(type);
}

void CShaderManager::RestoreInputLayout()
{
		SetInputLayout(m_SavedInputLayout);
}

//--------------------------------------------------------------------
// Sampler State Management
//--------------------------------------------------------------------

void CShaderManager::SetSamplerState(UINT slot, ESamplerState state, DWORD value)
{
	if (slot >= MAX_SAMPLER_SLOTS || !GetActiveContext()) return;

	// Use thread-local sampler state on worker threads
	SamplerSlotState* pSlotStates = m_SamplerStates;

	// Store the state value
	switch (state)
	{
	case SAMPLER_MINFILTER: pSlotStates[slot].minFilter = value; break;
	case SAMPLER_MAGFILTER: pSlotStates[slot].magFilter = value; break;
	case SAMPLER_MIPFILTER: pSlotStates[slot].mipFilter = value; break;
	case SAMPLER_ADDRESSU:  pSlotStates[slot].addressU = value; break;
	case SAMPLER_ADDRESSV:  pSlotStates[slot].addressV = value; break;
	default: return;
	}

	// Create and set sampler state
	if (m_pStateCache)
	{
		D3D11_SAMPLER_DESC desc = {};
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;  // Default
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.MipLODBias = 0.0f;
		desc.MaxAnisotropy = 1;
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		desc.MinLOD = 0;
		desc.MaxLOD = D3D11_FLOAT32_MAX;
		desc.BorderColor[0] = 1.0f;
		desc.BorderColor[1] = 1.0f;
		desc.BorderColor[2] = 1.0f;
		desc.BorderColor[3] = 1.0f;

		// Apply stored state
		bool bMinPoint = (pSlotStates[slot].minFilter == FILTER_POINT);
		bool bMagPoint = (pSlotStates[slot].magFilter == FILTER_POINT);
		bool bMipNone = (pSlotStates[slot].mipFilter == FILTER_NONE);

		if (bMinPoint && bMagPoint)
			desc.Filter = bMipNone ? D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
		else if (bMinPoint)
			desc.Filter = bMipNone ? D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT : D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
		else if (bMagPoint)
			desc.Filter = bMipNone ? D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR : D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
		else
			desc.Filter = bMipNone ? D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_LINEAR;

		if (pSlotStates[slot].minFilter == FILTER_ANISOTROPIC || pSlotStates[slot].magFilter == FILTER_ANISOTROPIC)
		{
			desc.Filter = D3D11_FILTER_ANISOTROPIC;
			desc.MaxAnisotropy = 16;
		}

		switch (pSlotStates[slot].addressU)
		{
		case ADDRESS_WRAP:   desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP; break;
		case ADDRESS_MIRROR: desc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR; break;
		case ADDRESS_CLAMP:  desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP; break;
		case ADDRESS_BORDER: desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER; break;
		}
		switch (pSlotStates[slot].addressV)
		{
		case ADDRESS_WRAP:   desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP; break;
		case ADDRESS_MIRROR: desc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR; break;
		case ADDRESS_CLAMP:  desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP; break;
		case ADDRESS_BORDER: desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER; break;
		}

		ID3D11SamplerState* pSampler = m_pStateCache->GetSamplerState(desc);
		if (pSampler)
			GetActiveContext()->PSSetSamplers(slot, 1, &pSampler);
	}
}

void CShaderManager::SaveSamplerState(UINT slot, ESamplerState state, DWORD value)
{
	if (slot >= MAX_SAMPLER_SLOTS) return;

	SamplerSlotState* pSlotStates = m_SamplerStates;
	auto& savedMap = m_SavedSamplerStates;

	// Save current value
	DWORD currentValue = 0;
	switch (state)
	{
	case SAMPLER_MINFILTER: currentValue = pSlotStates[slot].minFilter; break;
	case SAMPLER_MAGFILTER: currentValue = pSlotStates[slot].magFilter; break;
	case SAMPLER_MIPFILTER: currentValue = pSlotStates[slot].mipFilter; break;
	case SAMPLER_ADDRESSU:  currentValue = pSlotStates[slot].addressU; break;
	case SAMPLER_ADDRESSV:  currentValue = pSlotStates[slot].addressV; break;
	default: return;
	}
	savedMap[slot][state] = currentValue;

	// Set new value
	SetSamplerState(slot, state, value);
}

void CShaderManager::RestoreSamplerState(UINT slot, ESamplerState state)
{
	auto& savedMap = m_SavedSamplerStates;

	auto slotIt = savedMap.find(slot);
	if (slotIt == savedMap.end()) return;

	auto stateIt = slotIt->second.find(state);
	if (stateIt == slotIt->second.end()) return;

	SetSamplerState(slot, state, stateIt->second);
	slotIt->second.erase(stateIt);
}

//--------------------------------------------------------------------
// Fog/Lighting/AlphaTest State
//--------------------------------------------------------------------

void CShaderManager::SetFogEnabled(bool bEnabled)
{
	m_bFogEnabled = bEnabled;
	m_cbPerFrame.vFogParams.w = bEnabled ? 1.0f : 0.0f;
	m_bPerFrameDirty = true;
}

void CShaderManager::SetFogColor(DWORD dwColor)
{
	float a = ((dwColor >> 24) & 0xFF) / 255.0f;
	float r = ((dwColor >> 16) & 0xFF) / 255.0f;
	float g = ((dwColor >> 8) & 0xFF) / 255.0f;
	float b = (dwColor & 0xFF) / 255.0f;
	m_cbPerFrame.vFogColor = XMFLOAT4(r, g, b, a);
	m_bPerFrameDirty = true;
}

void CShaderManager::SetFogParams(float fStart, float fEnd, DWORD dwColor)
{
	m_cbPerFrame.vFogParams.x = fStart;
	m_cbPerFrame.vFogParams.y = fEnd;
	m_bPerFrameDirty = true;
	SetFogColor(dwColor);
}

void CShaderManager::SetBestFiltering(UINT slot)
{
	if (slot >= MAX_SAMPLER_SLOTS) return;
	SetSamplerState(slot, SAMPLER_MINFILTER, FILTER_LINEAR);
	SetSamplerState(slot, SAMPLER_MAGFILTER, FILTER_LINEAR);
	SetSamplerState(slot, SAMPLER_MIPFILTER, FILTER_LINEAR);
}

void CShaderManager::SetAlphaTestEnabled(bool bEnabled)
{
		m_bAlphaTestEnabled = bEnabled;
		m_cbPerObject.vMaterialParams.y = bEnabled ? 1.0f : 0.0f;
		m_bPerObjectDirty = true;
}

void CShaderManager::SetAlphaTestRefByte(DWORD dwRef)
{
		m_dwAlphaTestRef = dwRef;
		m_cbPerObject.vMaterialParams.x = (float)dwRef / 255.0f;
		m_bPerObjectDirty = true;
}

//--------------------------------------------------------------------
// Legacy Material API
//--------------------------------------------------------------------

void CShaderManager::SetMaterial(const TMaterial* pMaterial)
{
	if (!pMaterial) return;

		m_CurrentMaterial = *pMaterial;

	SetDiffuseColor(pMaterial->Diffuse.r, pMaterial->Diffuse.g, pMaterial->Diffuse.b, pMaterial->Diffuse.a);
	SetSpecularColor(pMaterial->Specular.r, pMaterial->Specular.g, pMaterial->Specular.b);
	SetEmissiveColor(pMaterial->Emissive.r, pMaterial->Emissive.g, pMaterial->Emissive.b);
	SetMaterial(pMaterial->Power);

	float power = pMaterial->Power;
	float roughness = (power > 0.0f) ? sqrtf(2.0f / (power + 2.0f)) : 0.0f;
	SetPBRRoughness(roughness);
	SetPBRMetallic(0.0f);
}

void CShaderManager::GetMaterial(TMaterial* pMaterial) const
{
	if (pMaterial)
		*pMaterial = m_CurrentMaterial;
}

void CShaderManager::SaveMaterial()
{
		m_SavedMaterial = m_CurrentMaterial;
}

void CShaderManager::RestoreMaterial()
{
		SetMaterial(&m_SavedMaterial);
}

//--------------------------------------------------------------------
// Legacy Light API
//--------------------------------------------------------------------

void CShaderManager::SetLight(UINT index, const TLight* pLight)
{
	if (!pLight || index >= MAX_SHADER_LIGHTS) return;

	DX11Light dx11Light;
	dx11Light.Position = XMFLOAT4(pLight->Position.x, pLight->Position.y, pLight->Position.z, (float)pLight->Type);
	dx11Light.Direction = XMFLOAT4(pLight->Direction.x, pLight->Direction.y, pLight->Direction.z,
		m_cbLighting.lights[index].Direction.w);  // Preserve enabled state
	dx11Light.Color = XMFLOAT4(pLight->Diffuse.r, pLight->Diffuse.g, pLight->Diffuse.b, 1.0f);
	dx11Light.Attenuation = XMFLOAT4(pLight->Attenuation0, pLight->Attenuation1, pLight->Attenuation2, pLight->Range);

	SetLight(index, dx11Light);
}

void CShaderManager::LightEnable(UINT index, BOOL bEnable)
{
	EnableLight(index, bEnable != FALSE);
}

//--------------------------------------------------------------------
// GPU Skinning - Bone Matrix Upload
//--------------------------------------------------------------------

void CShaderManager::SetBoneMatrices(const Matrix* pMatrices, int count)
{
	if (!pMatrices || count <= 0)
		return;

	// Clamp to maximum bones
	if (count > MAX_BONES)
		count = MAX_BONES;

	const size_t bytes = (size_t)count * sizeof(XMMATRIX);

		memcpy(m_cbSkinning.boneMatrices, pMatrices, bytes);
		static thread_local int s_lastBoneCount = 0;
		const int fillEnd = (s_lastBoneCount > count) ? s_lastBoneCount : count;
		for (int i = count; i < fillEnd; ++i)
			m_cbSkinning.boneMatrices[i] = XMMatrixIdentity();
		s_lastBoneCount = count;
		m_iActiveBoneCount = count;
		m_bSkinningDirty = true;
}

//--------------------------------------------------------------------
// Multithreaded Rendering Support (Deferred Contexts)
//--------------------------------------------------------------------

void CShaderManager::SyncPerFrameToContext(ID3D11DeviceContext* pDeferredCtx, ID3D11Buffer* pCBPerFrame)
{
	if (!pDeferredCtx || !pCBPerFrame)
		return;

	// Map and copy the per-frame constant buffer data
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(pDeferredCtx->Map(pCBPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, &m_cbPerFrame, sizeof(CBPerFrame));
		pDeferredCtx->Unmap(pCBPerFrame, 0);
	}

	// Bind the constant buffer to the deferred context
	pDeferredCtx->VSSetConstantBuffers(0, 1, &pCBPerFrame);
	pDeferredCtx->PSSetConstantBuffers(0, 1, &pCBPerFrame);
}

void CShaderManager::SyncLightingToContext(ID3D11DeviceContext* pDeferredCtx, ID3D11Buffer* pCBLighting)
{
	if (!pDeferredCtx || !pCBLighting)
		return;

	// Map and copy the lighting constant buffer data
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(pDeferredCtx->Map(pCBLighting, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, &m_cbLighting, sizeof(CBLighting));
		pDeferredCtx->Unmap(pCBLighting, 0);
	}

	pDeferredCtx->VSSetConstantBuffers(2, 1, &pCBLighting);
	pDeferredCtx->PSSetConstantBuffers(2, 1, &pCBLighting);
}

void CShaderManager::SyncAllConstantBuffers(ID3D11DeviceContext* pDeferredCtx,
	ID3D11Buffer* pCBPerFrame, ID3D11Buffer* pCBPerObject,
	ID3D11Buffer* pCBLighting, ID3D11Buffer* pCBSkinning)
{
	if (!pDeferredCtx)
		return;

	// Sync per-frame data
	if (pCBPerFrame)
	{
		SyncPerFrameToContext(pDeferredCtx, pCBPerFrame);
	}

	// Sync per-object data (initial state)
	if (pCBPerObject)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(pDeferredCtx->Map(pCBPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbPerObject, sizeof(CBPerObject));
			pDeferredCtx->Unmap(pCBPerObject, 0);
		}
		pDeferredCtx->VSSetConstantBuffers(1, 1, &pCBPerObject);
		pDeferredCtx->PSSetConstantBuffers(1, 1, &pCBPerObject);
	}

	// Sync lighting data
	if (pCBLighting)
	{
		SyncLightingToContext(pDeferredCtx, pCBLighting);
	}

	if (pCBSkinning)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(pDeferredCtx->Map(pCBSkinning, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbSkinning, sizeof(CBSkinning));
			pDeferredCtx->Unmap(pCBSkinning, 0);
		}
		pDeferredCtx->VSSetConstantBuffers(4, 1, &pCBSkinning);
	}
}

void CShaderManager::BindShaderToContext(ID3D11DeviceContext* pDeferredCtx, EShaderType type)
{
	if (!pDeferredCtx || type < 0 || type >= SHADER_COUNT)
		return;

	const ShaderProgram& shader = m_Shaders[type];

	// Bind vertex shader
	pDeferredCtx->VSSetShader(shader.pVertexShader, nullptr, 0);

	// Bind pixel shader
	pDeferredCtx->PSSetShader(shader.pPixelShader, nullptr, 0);

	// Bind input layout
	if (shader.pInputLayout)
		pDeferredCtx->IASetInputLayout(shader.pInputLayout);

	// Bind default sampler states
	ID3D11SamplerState* samplers[] = { m_pSamplerLinear, m_pSamplerLinear };
	pDeferredCtx->PSSetSamplers(0, 2, samplers);
}

void CShaderManager::UpdatePerObjectOnContext(ID3D11DeviceContext* pDeferredCtx, ID3D11Buffer* pCBPerObject,
	const Matrix* pWorld, const XMFLOAT4* pDiffuseColor)
{
	if (!pDeferredCtx || !pCBPerObject || !pWorld)
		return;

	// Build the per-object constant buffer data
	CBPerObject cbData = m_cbPerObject;  // Start with current state

	// Update world matrix
	cbData.matWorld = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(pWorld));

	// Calculate world-view-proj
	XMMATRIX matWVP = cbData.matWorld * m_cbPerFrame.matView * m_cbPerFrame.matProjection;
	cbData.matWorldViewProj = matWVP;

	// Update diffuse color if provided
	if (pDiffuseColor)
		cbData.vDiffuseColor = *pDiffuseColor;

	// Map and update
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(pDeferredCtx->Map(pCBPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, &cbData, sizeof(CBPerObject));
		pDeferredCtx->Unmap(pCBPerObject, 0);
	}
}

void CShaderManager::UpdateSkinningOnContext(ID3D11DeviceContext* pDeferredCtx, ID3D11Buffer* pCBSkinning,
	const Matrix* pBoneMatrices, int boneCount)
{
	if (!pDeferredCtx || !pCBSkinning || !pBoneMatrices || boneCount <= 0)
		return;

	// Clamp to maximum bones
	if (boneCount > MAX_BONES)
		boneCount = MAX_BONES;

	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(pDeferredCtx->Map(pCBSkinning, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		CBSkinning* pData = reinterpret_cast<CBSkinning*>(mapped.pData);

		// Copy bone matrices
		for (int i = 0; i < boneCount; ++i)
		{
			pData->boneMatrices[i] = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&pBoneMatrices[i]));
		}

		// Fill remaining with identity
		for (int i = boneCount; i < MAX_BONES; ++i)
		{
			pData->boneMatrices[i] = XMMatrixIdentity();
		}

		pDeferredCtx->Unmap(pCBSkinning, 0);
	}
}


void CShaderManager::SetSpeedTreeWindMatrix(int nIndex, const float* p)
{
	if (!p || nIndex < 0 || nIndex >= SPEEDTREE_NUM_WIND_MATRICES)
		return;

	m_cbSpeedTree.matWindMatrices[nIndex] = XMMATRIX(
		p[0],  p[1],  p[2],  p[3],
		p[4],  p[5],  p[6],  p[7],
		p[8],  p[9],  p[10], p[11],
		p[12], p[13], p[14], p[15]);
	m_bSpeedTreeDirty = true;
}

void CShaderManager::SetSpeedTreeTreePosition(const float* p)
{
	if (!p) return;
	m_cbSpeedTree.vTreePos = XMFLOAT4(p[0], p[1], p[2], p[3]);
	m_bSpeedTreeDirty = true;
}

void CShaderManager::SetSpeedTreeLeafTables(int nFirstEntry, const float* p, UINT uiEntryCount)
{
	if (!p || nFirstEntry < 0) return;

	const int nMax = min((int)uiEntryCount, SPEEDTREE_MAX_LEAF_TABLES - nFirstEntry);
	for (int i = 0; i < nMax; ++i)
	{
		m_cbSpeedTree.vLeafTables[nFirstEntry + i] =
			XMFLOAT4(p[i * 4 + 0], p[i * 4 + 1], p[i * 4 + 2], p[i * 4 + 3]);
	}
	m_cbSpeedTree.nNumLeafTables = max(m_cbSpeedTree.nNumLeafTables, nFirstEntry + nMax);
	m_bSpeedTreeDirty = true;
}

void CShaderManager::SetSpeedTreeLeafLightingAdjustment(const float* p)
{
	if (!p) return;
	m_cbSpeedTree.vLeafLightingAdj = XMFLOAT4(p[0], p[1], p[2], p[3]);
	m_bSpeedTreeDirty = true;
}

void CShaderManager::SetSpeedTreeLight(const float* p)
{
	if (!p) return;
	m_cbSpeedTree.vLightDir     = XMFLOAT4(p[0], p[1], p[2],  p[3]);
	m_cbSpeedTree.vLightDiffuse = XMFLOAT4(p[4], p[5], p[6],  p[7]);
	m_cbSpeedTree.vLightAmbient = XMFLOAT4(p[8], p[9], p[10], p[11]);
	m_bSpeedTreeDirty = true;
}

void CShaderManager::SetSpeedTreeMaterial(const float* p)
{
	if (!p) return;
	m_cbSpeedTree.vMaterialDiffuse = XMFLOAT4(p[0], p[1], p[2], p[3]);
	m_cbSpeedTree.vMaterialAmbient = XMFLOAT4(p[4], p[5], p[6], p[7]);
	m_bSpeedTreeDirty = true;
}

void CShaderManager::SetSpeedTreeFogParams(const float* p)
{
	if (!p) return;
	m_cbSpeedTree.vFogParams = XMFLOAT4(p[0], p[1], p[2], p[3]);
	m_bSpeedTreeDirty = true;
}

void CShaderManager::SetSpeedTreeCompoundMatrix(const float* p)
{
	if (!p) return;
	m_cbPerObject.matWorldViewProj = XMMATRIX(
		p[0],  p[1],  p[2],  p[3],
		p[4],  p[5],  p[6],  p[7],
		p[8],  p[9],  p[10], p[11],
		p[12], p[13], p[14], p[15]);
	m_bPerObjectDirty = true;
}

//--------------------------------------------------------------------
// God Rays (Volumetric Light Scattering)
//--------------------------------------------------------------------

void CShaderManager::SetGodRaysParams(float fScreenX, float fScreenY, float fIntensity, float fDecay)
{
	m_cbGodRays.vLightScreenPos.x = fScreenX;
	m_cbGodRays.vLightScreenPos.y = fScreenY;
	m_cbGodRays.vLightScreenPos.z = fIntensity;
	m_cbGodRays.vLightScreenPos.w = fDecay;
	m_bGodRaysDirty = true;
}

void CShaderManager::SetGodRaysRayParams(float fDensity, float fWeight, float fExposure, int nSamples)
{
	m_cbGodRays.vRayParams.x = fDensity;
	m_cbGodRays.vRayParams.y = fWeight;
	m_cbGodRays.vRayParams.z = fExposure;
	m_cbGodRays.vRayParams.w = (float)nSamples;
	m_bGodRaysDirty = true;
}

void CShaderManager::SetGodRaysColor(float r, float g, float b)
{
	m_cbGodRays.vRayColor.x = r;
	m_cbGodRays.vRayColor.y = g;
	m_cbGodRays.vRayColor.z = b;
	m_bGodRaysDirty = true;
}

#ifdef ENABLE_GODRAYS
void CShaderManager::RenderGodRaysPass(
	ID3D11ShaderResourceView* pSceneSRV,
	ID3D11RenderTargetView* pGodRaysRTV,
	UINT w, UINT h)
{
	if (!m_bGodRaysEnabled || !pSceneSRV || !pGodRaysRTV || !m_pContext)
		return;

	// Fullscreen quad vertices (NDC: -1..1, UV: 0..1)
	struct PTVertex { float x, y, z; float u, v; };
	PTVertex quad[4] = {
		{ -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
		{  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
		{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
		{  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
	};

	// Clear god rays RTV to black
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_pContext->ClearRenderTargetView(pGodRaysRTV, clearColor);

	// Set viewport to quarter res
	D3D11_VIEWPORT vpGodRays = { 0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f };
	m_pContext->RSSetViewports(1, &vpGodRays);

	// Bind god rays RTV (no depth)
	ID3D11ShaderResourceView* nullSRV = nullptr;
	m_pContext->PSSetShaderResources(0, 1, &nullSRV);
	m_pContext->OMSetRenderTargets(1, &pGodRaysRTV, nullptr);

	// Bind god rays shader + CB
	BeginGodRays();

	// Update god rays constant buffer if dirty
	if (m_bGodRaysDirty && m_pCBGodRays)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(m_pContext->Map(m_pCBGodRays, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbGodRays, sizeof(m_cbGodRays));
			m_pContext->Unmap(m_pCBGodRays, 0);
			m_bGodRaysDirty = false;
		}
	}

	// Bind scene texture SRV to t0
	m_pContext->PSSetShaderResources(0, 1, &pSceneSRV);

	// Draw fullscreen quad
	DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, quad, sizeof(PTVertex));

	// Unbind SRV from t0
	m_pContext->PSSetShaderResources(0, 1, &nullSRV);
}
#endif // ENABLE_GODRAYS

#ifdef ENABLE_BLOOM
void CShaderManager::RenderBloom(
	ID3D11ShaderResourceView* pSceneSRV,
	ID3D11RenderTargetView* pBloomRTA_RTV, ID3D11ShaderResourceView* pBloomRTA_SRV,
	ID3D11RenderTargetView* pBloomRTB_RTV, ID3D11ShaderResourceView* pBloomRTB_SRV,
	UINT bloomW, UINT bloomH,
	ID3D11ShaderResourceView* pGodRaysSRV,
	ID3D11ShaderResourceView* pSSAO_SRV,
	ID3D11RenderTargetView* pOutputRTV, UINT outputW, UINT outputH)
{
	if (!m_bBloomEnabled || !pSceneSRV || !m_pContext || !m_pCBBloom)
		return;

	// Fullscreen quad vertices (NDC: -1..1, UV: 0..1)
	struct PTVertex { float x, y, z; float u, v; };
	PTVertex quad[4] = {
		{ -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
		{  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
		{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
		{  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
	};

	// Save state
	EShaderType savedShader = m_eCurrentShader;

	// Disable depth testing for post-process
	DWORD savedZEnable = GetPipelineState(PSTATE_DEPTHENABLE);
	DWORD savedZWrite = GetPipelineState(PSTATE_DEPTHWRITEMASK);
	SetPipelineState(PSTATE_DEPTHENABLE, FALSE);
	SetPipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
	SetPipelineState(PSTATE_BLENDENABLE, FALSE);
	CommitRenderState();

	{
		ID3D11ShaderResourceView* nullSRV = nullptr;
		m_pContext->PSSetShaderResources(0, 1, &nullSRV);  // Unbind scene SRV from output

		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		m_pContext->ClearRenderTargetView(pBloomRTA_RTV, clearColor);

		m_pContext->OMSetRenderTargets(1, &pBloomRTA_RTV, nullptr);

		D3D11_VIEWPORT vpBloom = { 0.0f, 0.0f, (float)bloomW, (float)bloomH, 0.0f, 1.0f };
		m_pContext->RSSetViewports(1, &vpBloom);

		BeginBloomBright();

		// Update CB
		m_cbBloom.vTexelSize = XMFLOAT4(1.0f / bloomW, 1.0f / bloomH, 0.0f, 0.0f);
		m_cbBloom.vBlurDirection = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(m_pContext->Map(m_pCBBloom, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbBloom, sizeof(m_cbBloom));
			m_pContext->Unmap(m_pCBBloom, 0);
		}

		m_pContext->PSSetShaderResources(0, 1, &pSceneSRV);
		DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, quad, sizeof(PTVertex));
	}

	// --- Pass 2: Horizontal blur (bloom RTA -> bloom RTB) ---
	{
		ID3D11ShaderResourceView* nullSRV = nullptr;
		m_pContext->PSSetShaderResources(0, 1, &nullSRV);

		m_pContext->OMSetRenderTargets(1, &pBloomRTB_RTV, nullptr);

		BeginBloomBlur();

		m_cbBloom.vBlurDirection = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(m_pContext->Map(m_pCBBloom, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbBloom, sizeof(m_cbBloom));
			m_pContext->Unmap(m_pCBBloom, 0);
		}

		m_pContext->PSSetShaderResources(0, 1, &pBloomRTA_SRV);
		DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, quad, sizeof(PTVertex));
	}

	// --- Pass 3: Vertical blur (bloom RTB -> bloom RTA) ---
	{
		ID3D11ShaderResourceView* nullSRV = nullptr;
		m_pContext->PSSetShaderResources(0, 1, &nullSRV);

		m_pContext->OMSetRenderTargets(1, &pBloomRTA_RTV, nullptr);

		// Reuse bloom blur shader (already bound)

		m_cbBloom.vBlurDirection = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(m_pContext->Map(m_pCBBloom, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &m_cbBloom, sizeof(m_cbBloom));
			m_pContext->Unmap(m_pCBBloom, 0);
		}

		m_pContext->PSSetShaderResources(0, 1, &pBloomRTB_SRV);
		DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, quad, sizeof(PTVertex));
	}

	{
		ID3D11ShaderResourceView* nullSRVs[4] = { nullptr, nullptr, nullptr, nullptr };
		m_pContext->PSSetShaderResources(0, 4, nullSRVs);

		m_pContext->OMSetRenderTargets(1, &pOutputRTV, nullptr);

		D3D11_VIEWPORT vpFull = { 0.0f, 0.0f, (float)outputW, (float)outputH, 0.0f, 1.0f };
		m_pContext->RSSetViewports(1, &vpFull);

		BeginBloomComposite();

		m_pContext->PSSetShaderResources(0, 1, &pSceneSRV);
		m_pContext->PSSetShaderResources(1, 1, &pBloomRTA_SRV);
		m_pContext->PSSetShaderResources(2, 1, pGodRaysSRV ? &pGodRaysSRV : &nullSRVs[0]);
		ID3D11ShaderResourceView* pSSAOBind = pSSAO_SRV ? pSSAO_SRV : m_pDefaultTextureSRV;
		m_pContext->PSSetShaderResources(3, 1, &pSSAOBind);
		DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, quad, sizeof(PTVertex));
	}

	{
		ID3D11ShaderResourceView* nullSRVs[4] = { nullptr, nullptr, nullptr, nullptr };
		m_pContext->PSSetShaderResources(0, 4, nullSRVs);

		// Restore MSAA render target + depth stencil
		ID3D11RenderTargetView* pRTV = CGraphicBase::GetRenderTargetView();
		ID3D11DepthStencilView* pDSV = CGraphicBase::GetDepthStencilView();
		m_pContext->OMSetRenderTargets(1, &pRTV, pDSV);

		// Restore viewport
		D3D11_VIEWPORT vpRestore = CGraphicBase::GetViewport();
		m_pContext->RSSetViewports(1, &vpRestore);
	}

	// Restore depth state
	SetPipelineState(PSTATE_DEPTHENABLE, savedZEnable);
	SetPipelineState(PSTATE_DEPTHWRITEMASK, savedZWrite);
	CommitRenderState();

	// Restore previous shader
	if (savedShader != SHADER_NONE)
		BindShader(savedShader);
	else
		End();
}
#endif // ENABLE_BLOOM

#ifdef ENABLE_SSAO
void CShaderManager::SetSSAOEnabled(bool bEnabled)
{
	CGraphicBase::SetSSAOEnabled(bEnabled);
}

void CShaderManager::RenderDepthResolve(
	ID3D11ShaderResourceView* pMSAADepthSRV,
	ID3D11RenderTargetView* pResolvedRTV,
	UINT w, UINT h)
{
	if (!pMSAADepthSRV || !pResolvedRTV || !m_pContext)
		return;

	struct PTVertex { float x, y, z; float u, v; };
	PTVertex quad[4] = {
		{ -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
		{  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
		{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
		{  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
	};

	// Save state
	DWORD savedZEnable = GetPipelineState(PSTATE_DEPTHENABLE);
	DWORD savedZWrite = GetPipelineState(PSTATE_DEPTHWRITEMASK);
	SetPipelineState(PSTATE_DEPTHENABLE, FALSE);
	SetPipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
	SetPipelineState(PSTATE_BLENDENABLE, FALSE);
	CommitRenderState();

	ID3D11ShaderResourceView* nullSRV = nullptr;
	m_pContext->PSSetShaderResources(0, 1, &nullSRV);
	m_pContext->OMSetRenderTargets(1, &pResolvedRTV, nullptr);

	D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f };
	m_pContext->RSSetViewports(1, &vp);

	BindShader(SHADER_DEPTH_RESOLVE);
	m_pContext->PSSetShaderResources(0, 1, &pMSAADepthSRV);
	m_pContext->PSSetSamplers(1, 1, &m_pSamplerPoint);
	DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, quad, sizeof(PTVertex));

	m_pContext->PSSetShaderResources(0, 1, &nullSRV);

	SetPipelineState(PSTATE_DEPTHENABLE, savedZEnable);
	SetPipelineState(PSTATE_DEPTHWRITEMASK, savedZWrite);
	CommitRenderState();
}

void CShaderManager::RenderSSAOPass(
	ID3D11ShaderResourceView* pDepthSRV,
	ID3D11RenderTargetView* pSSAO_RTV,
	UINT ssaoW, UINT ssaoH, UINT fullW, UINT fullH)
{
	if (!pDepthSRV || !pSSAO_RTV || !m_pContext || !m_pCBSSAO)
		return;

	struct PTVertex { float x, y, z; float u, v; };
	PTVertex quad[4] = {
		{ -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
		{  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
		{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
		{  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
	};

	// Save state
	DWORD savedZEnable = GetPipelineState(PSTATE_DEPTHENABLE);
	DWORD savedZWrite = GetPipelineState(PSTATE_DEPTHWRITEMASK);
	SetPipelineState(PSTATE_DEPTHENABLE, FALSE);
	SetPipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
	SetPipelineState(PSTATE_BLENDENABLE, FALSE);
	CommitRenderState();

	// Update CBSSAO
	m_cbSSAO.matProjection = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&CGraphicBase::GetProjectionMatrix()));
	XMMATRIX matProj = m_cbSSAO.matProjection;
	XMVECTOR det;
	m_cbSSAO.matInvProjection = XMMatrixInverse(&det, matProj);
	m_cbSSAO.vTexelSize = XMFLOAT4(1.0f / ssaoW, 1.0f / ssaoH, 1.0f / fullW, 1.0f / fullH);

	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(m_pContext->Map(m_pCBSSAO, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, &m_cbSSAO, sizeof(m_cbSSAO));
		m_pContext->Unmap(m_pCBSSAO, 0);
	}

	// Clear SSAO RT to white (1.0 = no occlusion)
	float clearColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	m_pContext->ClearRenderTargetView(pSSAO_RTV, clearColor);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	m_pContext->PSSetShaderResources(0, 1, &nullSRV);
	m_pContext->PSSetShaderResources(1, 1, &nullSRV);
	m_pContext->OMSetRenderTargets(1, &pSSAO_RTV, nullptr);

	D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)ssaoW, (float)ssaoH, 0.0f, 1.0f };
	m_pContext->RSSetViewports(1, &vp);

	BindShader(SHADER_SSAO);

	// Bind CBSSAO at PS slot b0
	m_pContext->PSSetConstantBuffers(0, 1, &m_pCBSSAO);

	// Bind depth at t0, noise at t1
	m_pContext->PSSetShaderResources(0, 1, &pDepthSRV);
	m_pContext->PSSetShaderResources(1, 1, &m_pSSAONoiseSRV);

	// Point sampler at s1
	m_pContext->PSSetSamplers(1, 1, &m_pSamplerPoint);

	DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, quad, sizeof(PTVertex));

	// Unbind
	m_pContext->PSSetShaderResources(0, 1, &nullSRV);
	m_pContext->PSSetShaderResources(1, 1, &nullSRV);

	SetPipelineState(PSTATE_DEPTHENABLE, savedZEnable);
	SetPipelineState(PSTATE_DEPTHWRITEMASK, savedZWrite);
	CommitRenderState();
}

void CShaderManager::RenderSSAOBlur(
	ID3D11ShaderResourceView* pSSAO_SRV,
	ID3D11ShaderResourceView* pDepthSRV,
	ID3D11RenderTargetView* pBlurRTV,
	UINT w, UINT h)
{
	if (!pSSAO_SRV || !pDepthSRV || !pBlurRTV || !m_pContext || !m_pCBSSAO)
		return;

	struct PTVertex { float x, y, z; float u, v; };
	PTVertex quad[4] = {
		{ -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
		{  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
		{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
		{  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
	};

	// Save state
	DWORD savedZEnable = GetPipelineState(PSTATE_DEPTHENABLE);
	DWORD savedZWrite = GetPipelineState(PSTATE_DEPTHWRITEMASK);
	SetPipelineState(PSTATE_DEPTHENABLE, FALSE);
	SetPipelineState(PSTATE_DEPTHWRITEMASK, FALSE);
	SetPipelineState(PSTATE_BLENDENABLE, FALSE);
	CommitRenderState();

	// Update texel size for blur dimensions
	m_cbSSAO.vTexelSize.x = 1.0f / w;
	m_cbSSAO.vTexelSize.y = 1.0f / h;
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(m_pContext->Map(m_pCBSSAO, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, &m_cbSSAO, sizeof(m_cbSSAO));
		m_pContext->Unmap(m_pCBSSAO, 0);
	}

	float clearColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	m_pContext->ClearRenderTargetView(pBlurRTV, clearColor);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	m_pContext->PSSetShaderResources(0, 1, &nullSRV);
	m_pContext->PSSetShaderResources(1, 1, &nullSRV);
	m_pContext->OMSetRenderTargets(1, &pBlurRTV, nullptr);

	D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f };
	m_pContext->RSSetViewports(1, &vp);

	BindShader(SHADER_SSAO_BLUR);

	// Bind CBSSAO at PS slot b0
	m_pContext->PSSetConstantBuffers(0, 1, &m_pCBSSAO);

	// Bind SSAO at t0, depth at t1
	m_pContext->PSSetShaderResources(0, 1, &pSSAO_SRV);
	m_pContext->PSSetShaderResources(1, 1, &pDepthSRV);

	// Point sampler at s1
	m_pContext->PSSetSamplers(1, 1, &m_pSamplerPoint);

	DrawDynamic(TOPOLOGY_TRIANGLESTRIP, 2, quad, sizeof(PTVertex));

	// Unbind
	m_pContext->PSSetShaderResources(0, 1, &nullSRV);
	m_pContext->PSSetShaderResources(1, 1, &nullSRV);

	SetPipelineState(PSTATE_DEPTHENABLE, savedZEnable);
	SetPipelineState(PSTATE_DEPTHWRITEMASK, savedZWrite);
	CommitRenderState();
}
#endif // ENABLE_SSAO

//--------------------------------------------------------------------
// GPU Compute Shader Support
//--------------------------------------------------------------------

bool CShaderManager::CompileComputeShader(EComputeShader type, const char* szCSCode, const char* szEntryPoint)
{
	if (!m_pDevice || type < 0 || type >= CS_COUNT || !szCSCode)
		return false;

	// Release existing shader if any
	if (m_ComputeShaders[type])
	{
		m_ComputeShaders[type]->Release();
		m_ComputeShaders[type] = nullptr;
	}

	ID3DBlob* pCSBlob = nullptr;
	ID3DBlob* pErrorBlob = nullptr;
	UINT compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#ifdef _DEBUG
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT hr = D3DCompile(szCSCode, strlen(szCSCode), nullptr, nullptr, nullptr,
		szEntryPoint, "cs_5_0", compileFlags, 0, &pCSBlob, &pErrorBlob);
	if (FAILED(hr))
	{
		if (pErrorBlob)
		{
			TraceError("CompileComputeShader(%d): %s", type, (char*)pErrorBlob->GetBufferPointer());
			pErrorBlob->Release();
		}
		return false;
	}
	if (pErrorBlob) pErrorBlob->Release();

	hr = m_pDevice->CreateComputeShader(pCSBlob->GetBufferPointer(), pCSBlob->GetBufferSize(),
		nullptr, &m_ComputeShaders[type]);
	pCSBlob->Release();

	if (FAILED(hr))
	{
		TraceError("CompileComputeShader(%d): CreateComputeShader failed (hr=0x%08X)", type, hr);
		return false;
	}

	return true;
}

void CShaderManager::DispatchCompute(EComputeShader type, UINT groupsX, UINT groupsY, UINT groupsZ)
{
	if (type < 0 || type >= CS_COUNT || !m_ComputeShaders[type] || !GetActiveContext())
		return;

	GetActiveContext()->CSSetShader(m_ComputeShaders[type], nullptr, 0);
	GetActiveContext()->Dispatch(groupsX, groupsY, groupsZ);
}

bool CShaderManager::CreateStructuredBuffer(UINT elementSize, UINT elementCount, bool bCpuWrite, GpuBuffer& outBuffer)
{
	if (!m_pDevice || elementSize == 0 || elementCount == 0)
		return false;

	ReleaseGpuBuffer(outBuffer);

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = elementSize * elementCount;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = elementSize;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	if (bCpuWrite)
	{
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}
	else
	{
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}

	HRESULT hr = m_pDevice->CreateBuffer(&desc, nullptr, &outBuffer.pBuffer);
	if (FAILED(hr))
	{
		TraceError("CreateStructuredBuffer: CreateBuffer failed (size=%d, hr=0x%08X)", desc.ByteWidth, hr);
		return false;
	}

	// Create SRV
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = elementCount;

	hr = m_pDevice->CreateShaderResourceView(outBuffer.pBuffer, &srvDesc, &outBuffer.pSRV);
	if (FAILED(hr))
	{
		TraceError("CreateStructuredBuffer: CreateSRV failed (hr=0x%08X)", hr);
		ReleaseGpuBuffer(outBuffer);
		return false;
	}

	// Create UAV for non-CPU-write buffers
	if (!bCpuWrite)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = elementCount;

		hr = m_pDevice->CreateUnorderedAccessView(outBuffer.pBuffer, &uavDesc, &outBuffer.pUAV);
		if (FAILED(hr))
		{
			TraceError("CreateStructuredBuffer: CreateUAV failed (hr=0x%08X)", hr);
			ReleaseGpuBuffer(outBuffer);
			return false;
		}
	}

	outBuffer.elementCount = elementCount;
	outBuffer.elementSize = elementSize;
	return true;
}

bool CShaderManager::CreateRawVertexUAVBuffer(UINT byteWidth, GpuBuffer& outBuffer)
{
	if (!m_pDevice || byteWidth == 0)
		return false;

	ReleaseGpuBuffer(outBuffer);

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = byteWidth;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_UNORDERED_ACCESS;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

	HRESULT hr = m_pDevice->CreateBuffer(&desc, nullptr, &outBuffer.pBuffer);
	if (FAILED(hr))
	{
		TraceError("CreateRawVertexUAVBuffer: CreateBuffer failed (size=%d, hr=0x%08X)", byteWidth, hr);
		return false;
	}

	// Create raw UAV (RWByteAddressBuffer in HLSL)
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = byteWidth / 4;
	uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

	hr = m_pDevice->CreateUnorderedAccessView(outBuffer.pBuffer, &uavDesc, &outBuffer.pUAV);
	if (FAILED(hr))
	{
		TraceError("CreateRawVertexUAVBuffer: CreateUAV failed (hr=0x%08X)", hr);
		ReleaseGpuBuffer(outBuffer);
		return false;
	}

	outBuffer.elementCount = byteWidth;
	outBuffer.elementSize = 1;
	return true;
}

void CShaderManager::ReleaseGpuBuffer(GpuBuffer& buffer)
{
	if (buffer.pUAV) { buffer.pUAV->Release(); buffer.pUAV = nullptr; }
	if (buffer.pSRV) { buffer.pSRV->Release(); buffer.pSRV = nullptr; }
	if (buffer.pBuffer) { buffer.pBuffer->Release(); buffer.pBuffer = nullptr; }
	buffer.elementCount = 0;
	buffer.elementSize = 0;
}

void CShaderManager::CSSetSRV(UINT slot, ID3D11ShaderResourceView* pSRV)
{
	if (GetActiveContext())
		GetActiveContext()->CSSetShaderResources(slot, 1, &pSRV);
}

void CShaderManager::CSSetUAV(UINT slot, ID3D11UnorderedAccessView* pUAV)
{
	if (GetActiveContext())
	{
		UINT initialCount = (UINT)-1;
		GetActiveContext()->CSSetUnorderedAccessViews(slot, 1, &pUAV, &initialCount);
	}
}

void CShaderManager::CSSetCB(UINT slot, ID3D11Buffer* pCB)
{
	if (GetActiveContext())
		GetActiveContext()->CSSetConstantBuffers(slot, 1, &pCB);
}

void CShaderManager::CSUnbindResources()
{
	if (!GetActiveContext()) return;

	ID3D11ShaderResourceView* nullSRV = nullptr;
	ID3D11UnorderedAccessView* nullUAV = nullptr;
	UINT initialCount = (UINT)-1;

	GetActiveContext()->CSSetShaderResources(0, 1, &nullSRV);
	GetActiveContext()->CSSetUnorderedAccessViews(0, 1, &nullUAV, &initialCount);
	GetActiveContext()->CSSetShader(nullptr, nullptr, 0);
}

//////////////////////////////////////////////////////////////////////////
// Particle Compute Shader Billboard System
//////////////////////////////////////////////////////////////////////////

bool CShaderManager::InitParticleCSResources()
{
	if (!m_pDevice || !m_ComputeShaders[CS_PARTICLE_BILLBOARD])
		return false;

	// Verify SHADER_PARTICLE_PCT compiled successfully
	if (!m_Shaders[SHADER_PARTICLE_PCT].pVertexShader)
		return false;

	// Create input structured buffer (DYNAMIC, CPU write)
	if (!CreateStructuredBuffer(sizeof(ParticleGPUInput), MAX_CS_PARTICLES, true, m_particleCSInput))
	{
		TraceError("InitParticleCSResources: Failed to create input structured buffer");
		return false;
	}

	// Create output raw VB+UAV buffer
	UINT outputVBSize = MAX_CS_QUADS * 4 * 24;  // 4 verts * 24 bytes per vert
	if (!CreateRawVertexUAVBuffer(outputVBSize, m_particleCSOutput))
	{
		TraceError("InitParticleCSResources: Failed to create output VB/UAV buffer");
		return false;
	}

	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.ByteWidth = sizeof(CBParticleCS);
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pCBParticleCS)))
	{
		TraceError("InitParticleCSResources: Failed to create CB");
		return false;
	}

	// Create index buffer for CS output quads
	UINT ibSize = MAX_CS_QUADS * 6 * sizeof(WORD);
	std::vector<WORD> indices(MAX_CS_QUADS * 6);
	for (UINT q = 0; q < MAX_CS_QUADS; ++q)
	{
		WORD base = (WORD)(q * 4);
		indices[q * 6 + 0] = base + 0;
		indices[q * 6 + 1] = base + 2;
		indices[q * 6 + 2] = base + 1;
		indices[q * 6 + 3] = base + 2;
		indices[q * 6 + 4] = base + 3;
		indices[q * 6 + 5] = base + 1;
	}

	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = ibSize;
	ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = indices.data();
	if (FAILED(m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pParticleCSIB)))
	{
		TraceError("InitParticleCSResources: Failed to create IB");
		return false;
	}

	m_bComputeParticlesAvailable = true;
	Tracef("InitParticleCSResources: Particle CS billboard system ready (max %d particles)\n", MAX_CS_PARTICLES);
	return true;
}

bool CShaderManager::DispatchParticleBillboardCS(const ParticleGPUInput* pParticles, UINT count,
	UINT facesPerParticle, const float fRotations[3], const Matrix* pAttachMatrix)
{
	if (!m_bComputeParticlesAvailable || !pParticles || count == 0)
		return false;

	if (count > MAX_CS_PARTICLES)
		count = MAX_CS_PARTICLES;

	ID3D11DeviceContext* pCtx = GetActiveContext();
	if (!pCtx) return false;

	// Upload particle data to structured buffer
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (FAILED(pCtx->Map(m_particleCSInput.pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return false;
	memcpy(mapped.pData, pParticles, count * sizeof(ParticleGPUInput));
	pCtx->Unmap(m_particleCSInput.pBuffer, 0);

	// Fill and upload CS constant buffer
	// Get camera vectors from current camera
	CCamera* pCam = CCameraManager::Instance().GetCurrentCamera();
	if (!pCam) return false;

	const Vector3& vUp = pCam->GetUp();
	const Vector3& vCross = pCam->GetCross();
	const Vector3& vView = pCam->GetView();

	m_cbParticleCS.camUp = XMFLOAT3(vUp.x, vUp.y, vUp.z);
	m_cbParticleCS.camCross = XMFLOAT3(vCross.x, vCross.y, vCross.z);
	m_cbParticleCS.camView = XMFLOAT3(vView.x, vView.y, vView.z);
	m_cbParticleCS.particleCount = count;
	m_cbParticleCS.facesPerParticle = facesPerParticle;
	m_cbParticleCS.faceRotations = XMFLOAT4(
		fRotations[0], fRotations[1], fRotations[2], 0.0f);

	if (pAttachMatrix)
	{
		m_cbParticleCS.hasAttachMatrix = 1;
		m_cbParticleCS.attachMatrix = XMMatrixTranspose(XMLoadFloat4x4(
			reinterpret_cast<const XMFLOAT4X4*>(pAttachMatrix)));
	}
	else
	{
		m_cbParticleCS.hasAttachMatrix = 0;
		m_cbParticleCS.attachMatrix = XMMatrixIdentity();
	}

	if (FAILED(pCtx->Map(m_pCBParticleCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return false;
	memcpy(mapped.pData, &m_cbParticleCS, sizeof(CBParticleCS));
	pCtx->Unmap(m_pCBParticleCS, 0);

	// Bind CS resources and dispatch
	CSSetSRV(0, m_particleCSInput.pSRV);
	CSSetUAV(0, m_particleCSOutput.pUAV);
	CSSetCB(0, m_pCBParticleCS);

	UINT totalThreads = count;
	UINT groups = (totalThreads + 63) / 64;
	DispatchCompute(CS_PARTICLE_BILLBOARD, groups);

	CSUnbindResources();

	InvalidateIACache();

	return true;
}

void CShaderManager::DrawParticleCSOutput(UINT quadCount)
{
	if (!m_bComputeParticlesAvailable || quadCount == 0)
		return;

	ID3D11DeviceContext* pCtx = GetActiveContext();
	if (!pCtx) return;

	// Bind CS output buffer as vertex buffer
	UINT stride = 24;  // float3 pos + DWORD color + float2 texcoord
	UINT offset = 0;
	SetVertexBuffer(0, m_particleCSOutput.pBuffer, stride, offset);

	// Bind particle CS index buffer
	SetIndexBuffer(m_pParticleCSIB, DXGI_FORMAT_R16_UINT, 0);

	// Commit render state and constant buffers
	CommitRenderState();
	CommitChanges();

	SetPrimitiveTopologyIfChanged(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCtx->DrawIndexed(quadCount * 6, 0, 0);
	IncrementGlobalDrawCount();
}

//--------------------------------------------------------------------
// Cross-PSI Particle Batcher Implementation
//--------------------------------------------------------------------
void CShaderManager::ResetParticleBatcher()
{
	for (auto& pair : m_particleBatches)
		pair.second.clear();
}

void CShaderManager::AddParticleToBatch(const ParticleBatchKey& key, const ParticleGPUInput& input)
{
	m_particleBatches[key].push_back(input);
	m_particleBatchStats.particlesBatched++;
}

void CShaderManager::FlushParticleBatches()
{
	if (!m_bComputeParticlesAvailable || m_particleBatches.empty())
		return;

	BeginParticle();
	BeginParticlePCT();

	for (auto& groupPair : m_particleBatches)
	{
		const ParticleBatchKey& key = groupPair.first;
		std::vector<ParticleGPUInput>& particles = groupPair.second;
		if (particles.empty())
			continue;

		m_particleBatchStats.batchGroups++;

		SetShaderResource(0, key.pTexture);
		SetPipelineState(PSTATE_SRCBLEND, key.srcBlend);
		SetPipelineState(PSTATE_DESTBLEND, key.destBlend);
		SetParticleColorOp(key.colorOp);

		const float fRots[3] = { key.rot0, key.rot1, key.rot2 };
		const UINT faces = (UINT)key.facesPerParticle;
		const UINT totalParticles = (UINT)particles.size();

		UINT offset = 0;
		while (offset < totalParticles)
		{
			UINT chunkSize = min(totalParticles - offset, MAX_CS_PARTICLES);
			if (!DispatchParticleBillboardCS(particles.data() + offset, chunkSize, faces, fRots, nullptr))
				break;
			DrawParticleCSOutput(chunkSize * faces);
			m_particleBatchStats.dispatches++;
			m_particleBatchStats.drawsIssued++;
			offset += chunkSize;
		}

		particles.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
// Fly Trace Compute Shader Billboard System
//////////////////////////////////////////////////////////////////////////

bool CShaderManager::InitFlyTraceCSResources()
{
	if (!m_pDevice || !m_ComputeShaders[CS_FLYTRACE])
		return false;

	if (!m_Shaders[SHADER_PARTICLE_PCT].pVertexShader)
		return false;

	// Create input structured buffer (DYNAMIC, CPU write)
	if (!CreateStructuredBuffer(sizeof(FlyTraceSegmentInput), MAX_FLYTRACE_SEGMENTS, true, m_flyTraceCSInput))
	{
		TraceError("InitFlyTraceCSResources: Failed to create input structured buffer");
		return false;
	}

	UINT outputVBSize = MAX_FLYTRACE_SEGMENTS * 6 * 24;
	if (!CreateRawVertexUAVBuffer(outputVBSize, m_flyTraceCSOutput))
	{
		TraceError("InitFlyTraceCSResources: Failed to create output VB/UAV buffer");
		return false;
	}

	// Create constant buffer for CS
	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.ByteWidth = sizeof(CBFlyTraceCS);
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pCBFlyTraceCS)))
	{
		TraceError("InitFlyTraceCSResources: Failed to create CB");
		return false;
	}

	UINT ibSize = MAX_FLYTRACE_SEGMENTS * 12 * sizeof(WORD);
	std::vector<WORD> indices(MAX_FLYTRACE_SEGMENTS * 12);
	for (UINT s = 0; s < MAX_FLYTRACE_SEGMENTS; ++s)
	{
		WORD base = (WORD)(s * 6);
		UINT i = s * 12;
		// tri0: 0,1,2
		indices[i + 0] = base + 0; indices[i + 1] = base + 1; indices[i + 2] = base + 2;
		// tri1: 2,1,3
		indices[i + 3] = base + 2; indices[i + 4] = base + 1; indices[i + 5] = base + 3;
		// tri2: 2,3,4
		indices[i + 6] = base + 2; indices[i + 7] = base + 3; indices[i + 8] = base + 4;
		// tri3: 4,3,5
		indices[i + 9] = base + 4; indices[i + 10] = base + 3; indices[i + 11] = base + 5;
	}

	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = ibSize;
	ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = indices.data();
	if (FAILED(m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pFlyTraceCSIB)))
	{
		TraceError("InitFlyTraceCSResources: Failed to create IB");
		return false;
	}

	m_bFlyTraceCSAvailable = true;
	Tracef("InitFlyTraceCSResources: FlyTrace CS system ready (max %d segments)\n", MAX_FLYTRACE_SEGMENTS);
	return true;
}

bool CShaderManager::DispatchFlyTraceCS(const FlyTraceSegmentInput* pSegments, UINT count)
{
	if (!m_bFlyTraceCSAvailable || !pSegments || count == 0)
		return false;

	if (count > MAX_FLYTRACE_SEGMENTS)
		count = MAX_FLYTRACE_SEGMENTS;

	ID3D11DeviceContext* pCtx = GetActiveContext();
	if (!pCtx) return false;

	// Upload segment data to structured buffer
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (FAILED(pCtx->Map(m_flyTraceCSInput.pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return false;
	memcpy(mapped.pData, pSegments, count * sizeof(FlyTraceSegmentInput));
	pCtx->Unmap(m_flyTraceCSInput.pBuffer, 0);

	// Fill and upload CS constant buffer
	CCamera* pCam = CCameraManager::Instance().GetCurrentCamera();
	if (!pCam) return false;

	const Vector3& vEye = pCam->GetEye();
	const Vector3& vView = pCam->GetView();

	m_cbFlyTraceCS.camEye = XMFLOAT3(vEye.x, vEye.y, vEye.z);
	m_cbFlyTraceCS.camFwd = XMFLOAT3(vView.x, vView.y, vView.z);
	m_cbFlyTraceCS.segmentCount = count;

	if (FAILED(pCtx->Map(m_pCBFlyTraceCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return false;
	memcpy(mapped.pData, &m_cbFlyTraceCS, sizeof(CBFlyTraceCS));
	pCtx->Unmap(m_pCBFlyTraceCS, 0);

	// Bind CS resources and dispatch
	CSSetSRV(0, m_flyTraceCSInput.pSRV);
	CSSetUAV(0, m_flyTraceCSOutput.pUAV);
	CSSetCB(0, m_pCBFlyTraceCS);

	UINT groups = (count + 63) / 64;
	DispatchCompute(CS_FLYTRACE, groups);

	CSUnbindResources();

	// UAV write auto-unbinds buffer from IA — invalidate cache so re-bind happens.
	InvalidateIACache();

	return true;
}

void CShaderManager::DrawFlyTraceCSOutput(UINT segmentCount)
{
	if (!m_bFlyTraceCSAvailable || segmentCount == 0)
		return;

	ID3D11DeviceContext* pCtx = GetActiveContext();
	if (!pCtx) return;

	// Bind CS output buffer as vertex buffer
	UINT stride = 24;  // float3 pos + DWORD color + float2 texcoord
	UINT offset = 0;
	SetVertexBuffer(0, m_flyTraceCSOutput.pBuffer, stride, offset);

	// Bind fly trace CS index buffer
	SetIndexBuffer(m_pFlyTraceCSIB, DXGI_FORMAT_R16_UINT, 0);

	// Commit render state and constant buffers
	CommitRenderState();
	CommitChanges();

	SetPrimitiveTopologyIfChanged(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCtx->DrawIndexed(segmentCount * 12, 0, 0);  // 12 indices per segment (4 triangles)
}

//////////////////////////////////////////////////////////////////////////
// Weapon Trace Compute Shader Spline System
//////////////////////////////////////////////////////////////////////////

bool CShaderManager::InitWeaponTraceCSResources()
{
	if (!m_pDevice || !m_ComputeShaders[CS_WEAPONTRACE])
		return false;

	if (!m_Shaders[SHADER_PARTICLE_PCT].pVertexShader)
		return false;

	if (!CreateStructuredBuffer(sizeof(WeaponTraceSplineSegment), MAX_WEAPONTRACE_SEGMENTS * 2, true, m_weaponTraceCSInput))
	{
		TraceError("InitWeaponTraceCSResources: Failed to create input structured buffer");
		return false;
	}

	UINT outputVBSize = MAX_WEAPONTRACE_SAMPLES * 2 * 24;
	if (!CreateRawVertexUAVBuffer(outputVBSize, m_weaponTraceCSOutput))
	{
		TraceError("InitWeaponTraceCSResources: Failed to create output VB/UAV buffer");
		return false;
	}

	// Constant buffer
	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.ByteWidth = sizeof(CBWeaponTraceCS);
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pCBWeaponTraceCS)))
	{
		TraceError("InitWeaponTraceCSResources: Failed to create CB");
		return false;
	}

	m_bWeaponTraceCSAvailable = true;
	Tracef("InitWeaponTraceCSResources: WeaponTrace CS system ready (max %d segments, %d samples)\n",
		MAX_WEAPONTRACE_SEGMENTS, MAX_WEAPONTRACE_SAMPLES);
	return true;
}

bool CShaderManager::DispatchWeaponTraceCS(const WeaponTraceSplineSegment* pSegments, UINT numSegments, const CBWeaponTraceCS& params)
{
	if (!m_bWeaponTraceCSAvailable || !pSegments || numSegments == 0)
		return false;

	if (numSegments > MAX_WEAPONTRACE_SEGMENTS)
		numSegments = MAX_WEAPONTRACE_SEGMENTS;

	UINT numSamples = params.numSamples;
	if (numSamples > MAX_WEAPONTRACE_SAMPLES)
		numSamples = MAX_WEAPONTRACE_SAMPLES;

	ID3D11DeviceContext* pCtx = GetActiveContext();
	if (!pCtx) return false;

	D3D11_MAPPED_SUBRESOURCE mapped;
	if (FAILED(pCtx->Map(m_weaponTraceCSInput.pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return false;
	memcpy(mapped.pData, pSegments, numSegments * 2 * sizeof(WeaponTraceSplineSegment));
	pCtx->Unmap(m_weaponTraceCSInput.pBuffer, 0);

	// Upload CB
	m_cbWeaponTraceCS = params;
	m_cbWeaponTraceCS.numSegments = numSegments;
	m_cbWeaponTraceCS.numSamples = numSamples;

	if (FAILED(pCtx->Map(m_pCBWeaponTraceCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return false;
	memcpy(mapped.pData, &m_cbWeaponTraceCS, sizeof(CBWeaponTraceCS));
	pCtx->Unmap(m_pCBWeaponTraceCS, 0);

	// Bind and dispatch
	CSSetSRV(0, m_weaponTraceCSInput.pSRV);
	CSSetUAV(0, m_weaponTraceCSOutput.pUAV);
	CSSetCB(0, m_pCBWeaponTraceCS);

	UINT groups = (numSamples + 63) / 64;
	DispatchCompute(CS_WEAPONTRACE, groups);

	CSUnbindResources();

	// UAV write auto-unbinds buffer from IA — invalidate cache so re-bind happens.
	InvalidateIACache();

	return true;
}

void CShaderManager::DrawWeaponTraceCSOutput(UINT numSamples)
{
	if (!m_bWeaponTraceCSAvailable || numSamples < 2)
		return;

	ID3D11DeviceContext* pCtx = GetActiveContext();
	if (!pCtx) return;

	// Bind CS output buffer as vertex buffer
	UINT stride = 24;  // float3 pos + DWORD color + float2 texcoord (PDT)
	UINT offset = 0;
	SetVertexBuffer(0, m_weaponTraceCSOutput.pBuffer, stride, offset);

	// Commit render state and constant buffers
	CommitRenderState();
	CommitChanges();

	SetPrimitiveTopologyIfChanged(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pCtx->Draw(numSamples * 2, 0);
}
