
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
	float4 vParticleParams;
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
