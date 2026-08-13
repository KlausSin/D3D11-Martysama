
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
	float4 vParticleParams;
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

	float3 diffuse = float3(0, 0, 0);

	// Check if light 0 is enabled
	if (lights[0].Direction.w > 0.5f)
	{
		diffuse = CalcDirectionalLight(lights[0], normal);
	}

	float3 lighting = saturate(ambient + diffuse);

	// Base color: texture * material diffuse * lighting
	float meshAlpha = lerp(1.0f, texColor.a, vRenderFlags.y);
	float4 finalColor = float4(texColor.rgb * vDiffuseColor.rgb * lighting, meshAlpha * vDiffuseColor.a * vParticleColor.a);

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
	finalColor.rgb = ApplyModernFog(finalColor.rgb, vFogParams, vFogColor, input.FogFactor);

	return finalColor;
}
