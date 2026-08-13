
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

// VTF instance data texture (bound to VS t8)
Texture2D<float4> texInstanceData : register(t8);

struct VS_INPUT
{
	float3 Position  : POSITION;
	float3 Normal    : NORMAL;
	float2 TexCoord0 : TEXCOORD0;
	float2 TexCoord1 : TEXCOORD1;
	uint   InstanceID : SV_InstanceID;
};

struct VS_OUTPUT
{
	float4 Position   : SV_POSITION;
	float3 WorldPos   : TEXCOORD0;
	float3 WorldNorm  : TEXCOORD1;
	float2 TexCoord0  : TEXCOORD2;
	float2 TexCoord1  : TEXCOORD3;
	float  FogFactor  : TEXCOORD4;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	// Fetch instance world matrix from VTF texture
	uint baseTexel = input.InstanceID * 4;
	float4 row0 = texInstanceData.Load(int3(baseTexel + 0, 0, 0));
	float4 row1 = texInstanceData.Load(int3(baseTexel + 1, 0, 0));
	float4 row2 = texInstanceData.Load(int3(baseTexel + 2, 0, 0));
	float4 row3 = texInstanceData.Load(int3(baseTexel + 3, 0, 0));

	matrix instWorld = matrix(row0, row1, row2, row3);

	float4 worldPos = mul(float4(input.Position, 1.0f), instWorld);
	float4 viewPos = mul(worldPos, matView);
	output.Position = mul(viewPos, matProjection);
	output.WorldPos = worldPos.xyz;
	output.WorldNorm = normalize(mul((float3x3)instWorld, input.Normal));
	output.TexCoord0 = input.TexCoord0;
	output.TexCoord1 = input.TexCoord1;

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
