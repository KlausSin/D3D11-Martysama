
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
		output.FogFactor = output.Position.z;
	}
	else
	{
		output.FogFactor = 1.0f;
	}

	return output;
}
