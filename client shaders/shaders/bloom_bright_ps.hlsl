
cbuffer CBBloom : register(b0)
{
	float4 vBloomParams;    // x=threshold, y=intensity
	float4 vTexelSize;      // x=1/bloomW, y=1/bloomH
	float4 vBlurDirection;  // x=H, y=V
};

Texture2D    texScene : register(t0);
SamplerState samLinear : register(s0);

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	float3 color = texScene.Sample(samLinear, input.TexCoord).rgb;
	float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));

	// Soft knee threshold
	float threshold = vBloomParams.x;
	float knee = threshold * 0.5;
	float soft = luminance - threshold + knee;
	soft = clamp(soft, 0.0, 2.0 * knee);
	soft = soft * soft / (4.0 * knee + 0.00001);
	float contribution = max(soft, luminance - threshold) / max(luminance, 0.00001);

	return float4(color * contribution * vBloomParams.y, 1.0);
}
