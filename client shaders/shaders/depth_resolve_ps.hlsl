
Texture2DMS<float> texDepthMS : register(t0);

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	int2 coord = int2(input.Position.xy);
	float depth = texDepthMS.Load(coord, 0).r;
	return float4(depth, 0, 0, 0);
}
