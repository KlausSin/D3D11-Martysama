
Texture2D    texDiffuse : register(t0);
SamplerState samLinear  : register(s0);

cbuffer CBPerObject : register(b1)
{
	matrix matWorld;
	matrix matWorldViewProj;
	matrix matTexture0;
	matrix matTexture1;
	float4 vDiffuseColor;
	float4 vSkyTint;
	float4 vMaterialParams;  // w = colorOp: 0=MODULATE, 1=SELECTARG1(factor), 2=SELECTARG2(texture)
	float4 vEmissiveColor;
	float4 vSpecularColor;
	float4 vPBRParams;
	float4 vRenderFlags;
	float4 vParticleColor;
	float4 vParticleParams;
};

struct PS_INPUT
{
	float4 Position  : SV_POSITION;
	float2 TexCoord  : TEXCOORD0;
	float4 Color     : COLOR0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	// Sample the particle texture
	float4 texColor = texDiffuse.Sample(samLinear, input.TexCoord);


	float4 finalColor;
	uint colorOp = (uint)(vParticleParams.x + 0.5f);

	finalColor.a = texColor.a * input.Color.a;
	switch (colorOp)
	{
	case 1:  discard; break;
	case 2:  finalColor.rgb = input.Color.rgb; break;
	case 3:  finalColor.rgb = texColor.rgb; break;
	case 5:  finalColor.rgb = saturate(texColor.rgb * input.Color.rgb * 2.0f); break;
	case 6:  finalColor.rgb = saturate(texColor.rgb * input.Color.rgb * 4.0f); break;
	case 7:  finalColor.rgb = saturate(texColor.rgb + input.Color.rgb); break;
	default: finalColor.rgb = texColor.rgb * input.Color.rgb; break;
	}

	if (finalColor.a < 0.004f)  // ~1/255, essentially zero alpha
	{
		discard;
	}

	return finalColor;
}
