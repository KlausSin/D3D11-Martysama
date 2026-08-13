
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
	float4 Color    : COLOR0;
	float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 Position  : SV_POSITION;
	float2 TexCoord  : TEXCOORD0;
	float4 Color     : COLOR0;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	output.Position = mul(float4(input.Position, 1.0f), matWorldViewProj);
	output.TexCoord = input.TexCoord;
	output.Color = input.Color;  // Per-vertex color from CS output (NOT vSkyTint)
	return output;
}
