
cbuffer CBPerFrame : register(b0)
{
	matrix matView;
	matrix matProjection;
	float4 vCameraPos;
	float4 vFogParams;
	float4 vFogColor;
	float4 vTime;           // x = total time, y = delta, z = layer2 speed mult
	float4 vSunDirection;   // xyz = sun dir, w = intensity
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
	float2 TexCoord2 : TEXCOORD1;  // Second cloud layer UV
	float3 WorldPos  : TEXCOORD2;  // For sun lighting calculation
	float2 ScreenPos : TEXCOORD3;  // For sun glow effect
	float  Height    : TEXCOORD4;  // Normalized gradient height [0=top, 1=bottom]
	float2 LocalPos  : TEXCOORD5;  // Object-space XY for cloud quad edge fade
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	float4 worldPos = mul(float4(input.Position, 1.0f), matWorld);
	output.Position = mul(float4(input.Position, 1.0f), matWorldViewProj);
	output.WorldPos = worldPos.xyz;
	output.Color = input.Color * vDiffuseColor * vSkyTint;

	output.Height = (1.0f - input.Position.z) * 0.5f;

	// Primary cloud layer UV with texture matrix scrolling
	float4 transformedTC = mul(float4(input.TexCoord, 0.0f, 1.0f), matTexture0);
	output.TexCoord = transformedTC.xy;

	float layer2SpeedMult = vTime.z > 0.01f ? vTime.z : 0.5f;
	float4 tc2 = float4(input.TexCoord * 0.7f, 0.0f, 1.0f);  // Smaller scale
	tc2.x += vTime.x * 0.02f * layer2SpeedMult;  // Slower scroll
	tc2.y += vTime.x * 0.015f * layer2SpeedMult;
	output.TexCoord2 = tc2.xy;

	// Screen position for sun glow
	output.ScreenPos = output.Position.xy / output.Position.w;

	output.LocalPos = input.Position.xy;

	return output;
}
