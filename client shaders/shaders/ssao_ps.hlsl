
cbuffer CBSSAO : register(b0)
{
	matrix matProjection;
	matrix matInvProjection;
	float4 vSSAOParams;     // x=radius, y=bias, z=intensity
	float4 vTexelSize;      // x=1/ssaoW, y=1/ssaoH, z=1/fullW, w=1/fullH
	float4 vSampleKernel[16];
};

Texture2D<float> texDepth : register(t0);
Texture2D<float2> texNoise : register(t1);
SamplerState samPoint : register(s1);

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

float3 GetViewPos(float2 uv)
{
	float depth = texDepth.SampleLevel(samPoint, uv, 0);
	// NDC: x,y in [-1,1], z = depth [0,1]
	float4 clipPos = float4(uv * 2.0 - 1.0, depth, 1.0);
	clipPos.y = -clipPos.y;
	float4 viewPos = mul(clipPos, matInvProjection);
	return viewPos.xyz / viewPos.w;
}

float3 ReconstructNormal(float2 uv)
{
	float3 center = GetViewPos(uv);
	// Use smallest depth difference to avoid edge artifacts
	float3 ddx1 = GetViewPos(uv + float2(vTexelSize.z, 0)) - center;
	float3 ddx2 = center - GetViewPos(uv - float2(vTexelSize.z, 0));
	float3 ddy1 = GetViewPos(uv + float2(0, vTexelSize.w)) - center;
	float3 ddy2 = center - GetViewPos(uv - float2(0, vTexelSize.w));

	float3 dx = (abs(ddx1.z) < abs(ddx2.z)) ? ddx1 : ddx2;
	float3 dy = (abs(ddy1.z) < abs(ddy2.z)) ? ddy1 : ddy2;

	return normalize(cross(dx, dy));
}

float4 main(PS_INPUT input) : SV_TARGET
{
	float depth = texDepth.SampleLevel(samPoint, input.TexCoord, 0);
	if (depth >= 0.9999)
		return float4(1, 1, 1, 1); // Sky - no occlusion

	float3 viewPos = GetViewPos(input.TexCoord);
	float3 normal = ReconstructNormal(input.TexCoord);

	float2 noiseUV = input.TexCoord / (vTexelSize.xy * 4.0);
	float2 noiseVal = texNoise.SampleLevel(samPoint, noiseUV, 0) * 2.0 - 1.0;

	// Build TBN from normal + noise rotation
	float3 tangent = normalize(noiseVal.x * cross(normal, float3(0, 0, 1)) +
	                           noiseVal.y * cross(normal, float3(0, 1, 0)));
	if (length(tangent) < 0.001)
		tangent = float3(1, 0, 0);
	float3 bitangent = cross(normal, tangent);
	float3x3 TBN = float3x3(tangent, bitangent, normal);

	float radius = vSSAOParams.x;
	float bias = vSSAOParams.y;
	float occlusion = 0.0;

	[unroll]
	for (int i = 0; i < 16; ++i)
	{
		// Transform sample to view space
		float3 sampleDir = mul(vSampleKernel[i].xyz, TBN);
		float3 samplePos = viewPos + sampleDir * radius;

		// Project to screen UV
		float4 offset = mul(float4(samplePos, 1.0), matProjection);
		offset.xy /= offset.w;
		offset.xy = offset.xy * 0.5 + 0.5;
		offset.y = 1.0 - offset.y;

		// Sample depth at projected position
		float sampleDepth = texDepth.SampleLevel(samPoint, offset.xy, 0);
		float3 sampleViewPos = GetViewPos(offset.xy);

		// Range check + occlusion
		float rangeCheck = smoothstep(0.0, 1.0, radius / max(abs(viewPos.z - sampleViewPos.z), 0.001));
		occlusion += (sampleViewPos.z >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
	}

	float ao = 1.0 - (occlusion / 16.0) * vSSAOParams.z;
	return float4(saturate(ao), 0, 0, 1);
}
