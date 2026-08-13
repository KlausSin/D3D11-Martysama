
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

	// Height-based atmospheric fog
	finalColor.rgb = ApplyModernFog(finalColor.rgb, vFogParams, vFogColor, input.FogFactor);


	return finalColor;
}
