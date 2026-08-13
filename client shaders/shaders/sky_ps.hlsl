
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
	float4 vParticleParams;
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
