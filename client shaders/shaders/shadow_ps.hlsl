

cbuffer CBPerObject : register(b1)
{
	matrix matWorld;
	matrix matWorldViewProj;
	matrix matTexture0;
	matrix matTexture1;
	float4 vDiffuseColor;
	float4 vSkyTint;
	float4 vMaterialParams;   // x = alphaRef, y = alphaTestEnabled
	float4 vEmissiveColor;
	float4 vSpecularColor;
	float4 vPBRParams;
	float4 vRenderFlags;
	float4 vParticleColor;
	float4 vParticleParams;
};

Texture2D    texDiffuse : register(t0);
SamplerState samLinear  : register(s0);

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	float  Depth    : TEXCOORD1;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	if (vMaterialParams.y > 0.5f)
	{
		float4 texColor = texDiffuse.Sample(samLinear, input.TexCoord);
		if (texColor.a < vMaterialParams.x)
			discard;
	}

	// Output depth to R32F shadow map for PCF sampling
	return float4(input.Depth, 0, 0, 1);
}
