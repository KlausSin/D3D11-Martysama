
cbuffer CBSSAO : register(b0)
{
	matrix matProjection;
	matrix matInvProjection;
	float4 vSSAOParams;
	float4 vTexelSize;      // x=1/ssaoW, y=1/ssaoH
	float4 vSampleKernel[16];
};

Texture2D<float> texSSAO  : register(t0);
Texture2D<float> texDepth : register(t1);
SamplerState samPoint : register(s1);

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	float centerDepth = texDepth.SampleLevel(samPoint, input.TexCoord, 0);
	float result = 0.0;
	float totalWeight = 0.0;

	[unroll]
	for (int x = -2; x <= 2; ++x)
	{
		[unroll]
		for (int y = -2; y <= 2; ++y)
		{
			float2 offset = float2(float(x), float(y)) * vTexelSize.xy;
			float2 sampleUV = input.TexCoord + offset;
			float sampleAO = texSSAO.SampleLevel(samPoint, sampleUV, 0);
			float sampleDepth = texDepth.SampleLevel(samPoint, sampleUV, 0);

			// Bilateral weight: reject samples across depth edges
			float depthDiff = abs(centerDepth - sampleDepth);
			float w = exp(-depthDiff * 1000.0);
			result += sampleAO * w;
			totalWeight += w;
		}
	}

	return float4(result / max(totalWeight, 0.001), 0, 0, 1);
}
