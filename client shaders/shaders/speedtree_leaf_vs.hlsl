

cbuffer CBPerFrame : register(b0)
{
	matrix matView;
	matrix matProjection;
	float4 vCameraPos;
	float4 vFogParams;
	float4 vFogColor;
	float4 vTime;
};

cbuffer CBSpeedTree : register(b3)
{
	row_major matrix matWindMatrices[4];
	float4 vTreePos;
	float4 vLeafTables[48];
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
	float3 Position  : POSITION;
	float4 Color     : COLOR0;
	float2 TexCoord  : TEXCOORD0;
	float4 LeafData  : TEXCOORD2;
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

	int   nWindMat = clamp(int(input.LeafData.x * 0.25f), 0, 3);
	float fWindAmt = saturate(input.LeafData.y);
	float3 windPos = mul(float4(localPos, 1.0f), matWindMatrices[nWindMat]).xyz;
	localPos = lerp(localPos, windPos, fWindAmt);

	// Transform to clip space
	float3 worldPos = localPos + vTreePos.xyz;
	float4 viewPos = mul(float4(worldPos, 1.0f), matView);
	output.Position = mul(viewPos, matProjection);

	output.Color = input.Color;
	output.TexCoord = input.TexCoord;

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
