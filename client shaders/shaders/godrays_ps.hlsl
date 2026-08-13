
cbuffer CBGodRays : register(b0)
{
	float4 vLightScreenPos;  // xy = light position in screen UV space, z = intensity, w = decay
	float4 vRayParams;       // x = density, y = weight, z = exposure, w = numSamples
	float4 vRayColor;        // rgb = light color, a = unused
};

Texture2D    texScene : register(t0);  // Scene color/occlusion texture
SamplerState samLinear : register(s0);

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	float2 lightPos = vLightScreenPos.xy;
	float intensity = vLightScreenPos.z;
	float decay = vLightScreenPos.w;

	float density = vRayParams.x;
	float weight = vRayParams.y;
	float exposure = vRayParams.z;
	int numSamples = (int)vRayParams.w;

	// Calculate ray direction from current pixel to light
	float2 deltaTexCoord = input.TexCoord - lightPos;
	deltaTexCoord *= 1.0f / float(numSamples) * density;

	float2 uv = input.TexCoord;
	float illuminationDecay = 1.0f;
	float3 color = float3(0, 0, 0);

	// Ray march toward light source
	[loop]
	for (int i = 0; i < numSamples; i++)
	{
		uv -= deltaTexCoord;

		// Sample scene - bright areas contribute to rays
		float3 sample = texScene.Sample(samLinear, saturate(uv)).rgb;

		// Accumulate with decay
		sample *= illuminationDecay * weight;
		color += sample;

		illuminationDecay *= decay;
	}

	// Apply exposure and light color
	color *= exposure * intensity;
	color *= vRayColor.rgb;

	return float4(color, 1.0f);
}
