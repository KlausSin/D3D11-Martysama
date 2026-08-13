
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
		output.FogFactor = output.Position.z;
	}
	else
	{
		output.FogFactor = 1.0f;
	}

	return output;
}
