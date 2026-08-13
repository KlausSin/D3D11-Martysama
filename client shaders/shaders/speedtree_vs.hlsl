
#define NUM_WIND_MATRICES 4

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

// SpeedTree-specific constant buffer
cbuffer CBSpeedTree : register(b3)
{
	row_major matrix matWindMatrices[NUM_WIND_MATRICES];  // Wind rotation matrices (row-major)
	float4 vTreePos;                             // Tree world position
	float4 vLeafTables[48];                      // Leaf billboard tables
	float4 vLeafLightingAdj;
	float4 vLightDir;
	float4 vLightDiffuse;
	float4 vLightAmbient;
	float4 vMaterialDiffuse;
	float4 vMaterialAmbient;
	float4 vSpeedTreeFog;
	int nNumLeafTables;
	int3 _padST;
};

struct VS_INPUT
{
	float3 Position   : POSITION;
	float4 Color      : COLOR0;       // Vertex color (static lighting)
	float2 TexCoord   : TEXCOORD0;
	float2 ShadowCoord: TEXCOORD1;    // Self-shadow texture coords
	float2 WindData   : TEXCOORD2;    // x = wind matrix index, y = wind weight
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

	float3 localPos = input.Position;

	int   nWindMat = clamp(int(input.WindData.x * 0.25f), 0, NUM_WIND_MATRICES - 1);
	float fWindAmt = saturate(input.WindData.y);
	float3 windPos = mul(float4(localPos, 1.0f), matWindMatrices[nWindMat]).xyz;
	float3 animatedPos = lerp(localPos, windPos, fWindAmt);

	float4 worldPos = mul(float4(animatedPos, 1.0f), matWorld);
	float4 viewPos = mul(worldPos, matView);
	output.Position = mul(viewPos, matProjection);

	output.Color = input.Color;

	// Pass texture coordinates
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
