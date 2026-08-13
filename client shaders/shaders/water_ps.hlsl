
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
	finalColor.rgb = ApplyModernFog(finalColor.rgb, vFogParams, vFogColor, input.FogFactor);
	return finalColor;
#endif
}
