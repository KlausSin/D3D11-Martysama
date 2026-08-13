
cbuffer CBPerFrame : register(b0)
{
	matrix matView;
	matrix matProjection;
	float4 vCameraPos;      // w = viewport width
	float4 vFogParams;      // z = viewport height
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
	float4 Color     : COLOR0;
	float2 TexCoord  : TEXCOORD0;
	float2 MaskCoord : TEXCOORD1;  // For minimap circular mask
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	float4 worldPos = mul(float4(input.Position, 1.0f), matWorld);
	float4 viewPos = mul(worldPos, matView);
	float4 projPos = mul(viewPos, matProjection);

	bool usePixelTransform = (abs(matProjection._11 - 1.0f) < 0.001f && abs(matProjection._22 - 1.0f) < 0.001f);

	if (usePixelTransform)
	{
		float2 screenSize = float2(vCameraPos.w, vFogParams.z);
		if (screenSize.x < 1.0f) screenSize.x = 1024.0f;
		if (screenSize.y < 1.0f) screenSize.y = 768.0f;

		// Use world-transformed position, not raw input position
		output.Position.x = (worldPos.x / screenSize.x) * 2.0f - 1.0f;
		output.Position.y = 1.0f - (worldPos.y / screenSize.y) * 2.0f;
		output.Position.z = worldPos.z;
		output.Position.w = 1.0f;
	}
	else
	{
		output.Position = projPos;
	}

	output.Color = input.Color;

	output.TexCoord = input.TexCoord;

	output.MaskCoord = mul(float4(viewPos.xyz, 1.0f), matTexture1).xy;

	return output;
}
