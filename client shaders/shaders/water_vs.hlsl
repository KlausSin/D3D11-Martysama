
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

#ifdef WATER_REFLECTION_ENABLED
cbuffer CBWater : register(b5)
{
	float4 vWaterParams_VS;
	float4 vWaterColor_VS;
	float4 vScreenParams_VS;
	matrix matReflectVP;
};
#endif

struct VS_INPUT
{
	float3 Position : POSITION;
	float4 Color    : COLOR0;
};

struct VS_OUTPUT
{
	float4 Position  : SV_POSITION;
	float4 Color     : COLOR0;
	float2 TexCoord  : TEXCOORD0;
	float3 WorldPos  : TEXCOORD1;
	float4 ClipPos   : TEXCOORD2;
	float  FogFactor : TEXCOORD3;
#ifdef WATER_REFLECTION_ENABLED
	float4 ReflProj  : TEXCOORD4;
#endif
};

float waterNoise(float2 p)
{
	return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	float3 displacedPos = input.Position;

	float4 worldPos = mul(float4(displacedPos, 1.0f), matWorld);
	output.Position = mul(float4(displacedPos, 1.0f), matWorldViewProj);
	output.WorldPos = worldPos.xyz;
	output.ClipPos  = output.Position;

	output.Color = input.Color;

	// Water texture coordinates based on world position
	float scaleU = matTexture0._11;
	float scaleV = matTexture0._22;
	if (abs(scaleU) < 0.00001f) scaleU = 0.0003f;
	if (abs(scaleV) < 0.00001f) scaleV = -0.0003f;

	output.TexCoord = float2(worldPos.x * scaleU, worldPos.y * scaleV);

#ifdef WATER_REFLECTION_ENABLED
	output.ReflProj = mul(worldPos, matReflectVP);
#endif

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
