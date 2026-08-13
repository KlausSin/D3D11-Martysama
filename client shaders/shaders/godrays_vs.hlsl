
struct VS_INPUT
{
	float3 Position : POSITION;
	float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	// Full-screen quad: position is already in NDC
	output.Position = float4(input.Position.xy, 0.0f, 1.0f);
	output.TexCoord = input.TexCoord;
	return output;
}
