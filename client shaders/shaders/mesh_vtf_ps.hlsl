
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
	float4 vParticleParams;
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

	float3 diffuse = float3(0, 0, 0);

	if (lights[0].Direction.w > 0.5f)
	{
		diffuse = CalcDirectionalLight(lights[0], normal);
	}

	float3 lighting = saturate(ambient + diffuse);

	float meshAlpha = lerp(1.0f, texColor.a, vRenderFlags.y);
	float4 finalColor = float4(texColor.rgb * vDiffuseColor.rgb * lighting, meshAlpha * vDiffuseColor.a * vParticleColor.a);

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
	finalColor.rgb = ApplyModernFog(finalColor.rgb, vFogParams, vFogColor, input.FogFactor);

	return finalColor;
}
