
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

Texture2D<float4> texInstanceData : register(t8);

struct VS_INPUT
{
	float3 Position    : POSITION;
	float4 Color       : COLOR0;
	float2 TexCoord    : TEXCOORD0;
	float2 ShadowCoord : TEXCOORD1;
	float2 WindData    : TEXCOORD2;
	uint   InstanceID  : SV_InstanceID;
};

struct VS_OUTPUT
{
	float4 Position  : SV_POSITION;
	float4 Color     : COLOR0;
	float2 TexCoord  : TEXCOORD0;
	float  FogFactor : TEXCOORD1;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	// Fetch per-instance tree position from VTF
	uint baseTexel = input.InstanceID * 4;
	float4 row0 = texInstanceData.Load(int3(baseTexel + 0, 0, 0));
	float4 row1 = texInstanceData.Load(int3(baseTexel + 1, 0, 0));
	float4 row2 = texInstanceData.Load(int3(baseTexel + 2, 0, 0));
	float4 row3 = texInstanceData.Load(int3(baseTexel + 3, 0, 0));

	// Tree world matrix (row 3 = translation)
	matrix instWorld = matrix(row0, row1, row2, row3);

	float3 localPos = input.Position;

	// Wind animation (same as non-VTF version)
	float heightFactor = saturate(localPos.z * 0.01f);
	float time = vTime.x;
	float3 treeWorldPos = row3.xyz;
	float primaryWave = sin(time * 1.5f + treeWorldPos.x * 0.05f + treeWorldPos.y * 0.05f);
	float secondaryWave = sin(time * 3.7f + treeWorldPos.x * 0.15f + treeWorldPos.y * 0.12f) * 0.3f;
	float swayAmount = (primaryWave + secondaryWave) * heightFactor * 3.0f;
	localPos += float3(swayAmount, swayAmount * 0.7f, 0.0f);

	float4 worldPos = mul(float4(localPos, 1.0f), instWorld);
	float4 viewPos = mul(worldPos, matView);
	output.Position = mul(viewPos, matProjection);
	output.Color = input.Color;
	output.TexCoord = input.TexCoord;

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
