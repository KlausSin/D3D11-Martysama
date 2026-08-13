
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
	float4 vParticleParams;
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

	// Height-based atmospheric fog
	float4 finalOut = float4(finalColor, diffuseColor.a * vDiffuseColor.a);
	finalOut.rgb = lerp(vFogColor.rgb, finalOut.rgb, input.FogFactor);

	return finalOut;
}
