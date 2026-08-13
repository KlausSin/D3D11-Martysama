
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
	matrix matTexture0;    // Texture transform for color texture
	matrix matTexture1;    // Texture transform for splat/alpha texture
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

struct Light
{
	float4 Position;     // xyz = position, w = type
	float4 Direction;    // xyz = direction, w = enabled
	float4 Color;        // rgb = color, a = intensity
	float4 Attenuation;  // x = const, y = linear, z = quadratic, w = range
};

cbuffer CBLighting : register(b2)
{
	Light lights[MAX_LIGHTS];
	float4 globalAmbient;
	int numActiveLights;
	int3 _padLighting;
};

struct VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal   : NORMAL;
};

struct VS_OUTPUT
{
	float4 Position   : SV_POSITION;
	float3 WorldPos   : TEXCOORD0;
	float3 WorldNorm  : TEXCOORD1;
	float2 TexCoord0  : TEXCOORD2;  // Color texture coords
	float2 TexCoord1  : TEXCOORD3;  // Splat/alpha texture coords
	float  FogFactor  : TEXCOORD4;
	float3 Diffuse    : TEXCOORD5;  // Per-vertex lighting result
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	float4 worldPos = mul(float4(input.Position, 1.0f), matWorld);
	output.Position = mul(float4(input.Position, 1.0f), matWorldViewProj);
	output.WorldPos = worldPos.xyz;

	// Transform normal - for terrain, normal is in world space
	float3 worldNormal = normalize(mul((float3x3)matWorld, input.Normal));
	output.WorldNorm = worldNormal;

	bool shadowMode = (vMaterialParams.z > 0.5f);

	if (shadowMode)
	{
		float4 texCoord0 = mul(worldPos, matTexture0);
		float4 texCoord1 = mul(worldPos, matTexture1);

		// Apply perspective divide if needed
		if (abs(texCoord0.w) > 0.0001f)
			output.TexCoord0 = texCoord0.xy / texCoord0.w;
		else
			output.TexCoord0 = texCoord0.xy;

		if (abs(texCoord1.w) > 0.0001f)
			output.TexCoord1 = texCoord1.xy / texCoord1.w;
		else
			output.TexCoord1 = texCoord1.xy;
	}
	else
	{
		float tileScaleX = vMaterialParams.x;
		float tileScaleY = vMaterialParams.y;

		// If no scale set, use default terrain tiling (1/640)
		if (abs(tileScaleX) < 0.0001f) tileScaleX = 0.0015625f;
		if (abs(tileScaleY) < 0.0001f) tileScaleY = -0.0015625f;

		output.TexCoord0 = float2(worldPos.x * tileScaleX, worldPos.y * tileScaleY);

		// Splat alpha coords: transform position by matTexture1
		float4 texCoord1 = mul(worldPos, matTexture1);
		output.TexCoord1 = texCoord1.xy;
	}

	output.Diffuse = max(globalAmbient.rgb, float3(0.4f, 0.4f, 0.4f));

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
