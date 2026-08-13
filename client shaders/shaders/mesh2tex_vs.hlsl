
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
	float4 vParticleParams;
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
		output.FogFactor = output.Position.z;
	}
	else
	{
		output.FogFactor = 1.0f;
	}

	return output;
}
