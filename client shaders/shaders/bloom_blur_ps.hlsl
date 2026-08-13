
cbuffer CBBloom : register(b0)
{
	float4 vBloomParams;
	float4 vTexelSize;
	float4 vBlurDirection;
};

Texture2D    texSource : register(t0);
SamplerState samLinear : register(s0);

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	// 9-tap Gaussian (sigma ~3), weights normalized
	static const float weights[5] = { 0.227027, 0.194594, 0.121622, 0.054054, 0.016216 };

	float2 dir = float2(vBlurDirection.x * vTexelSize.x, vBlurDirection.y * vTexelSize.y);

	float3 result = texSource.Sample(samLinear, input.TexCoord).rgb * weights[0];

	[unroll]
	for (int i = 1; i < 5; i++)
	{
		float2 offset = dir * float(i);
		result += texSource.Sample(samLinear, input.TexCoord + offset).rgb * weights[i];
		result += texSource.Sample(samLinear, input.TexCoord - offset).rgb * weights[i];
	}

	return float4(result, 1.0);
}
