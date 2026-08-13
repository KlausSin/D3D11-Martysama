
struct FlyTraceSegment
{
	float3 pos1;
	float  size1;
	float3 pos2;
	float  size2;
	uint   color;
	uint3  _pad;
};

StructuredBuffer<FlyTraceSegment> g_Segments : register(t0);
RWByteAddressBuffer g_OutputVB : register(u0);

cbuffer CBFlyTraceCS : register(b0)
{
	float3 camEye;     float _pad0;
	float3 camFwd;     float _pad1;
	uint   segmentCount; uint3 _pad2;
};

[numthreads(64, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	uint idx = DTid.x;
	if (idx >= segmentCount) return;

	FlyTraceSegment seg = g_Segments[idx];
	float3 pos1 = seg.pos1;
	float3 pos2 = seg.pos2;

	float3 B = pos2 - pos1;           // segment direction
	float3 E = camEye - pos1;         // to camera
	float3 P = cross(B, E);           // perpendicular
	float3 rawU = cross(camFwd, P);
	float lenU = length(rawU);
	float3 U = (lenU > 0.0001f) ? (rawU / lenU) : float3(0, 0, 1);
	float3 R = cross(camFwd, U);

	float3 positions[6];
	positions[0] = pos1 + U * seg.size1;   // top
	positions[1] = pos1 - R * seg.size1;   // left
	positions[2] = pos1 + R * seg.size1;   // right
	positions[3] = pos2 - R * seg.size2;   // left
	positions[4] = pos2 + R * seg.size2;   // right
	positions[5] = pos2 - U * seg.size2;   // bottom

	float2 uvs[6] = {
		float2(0.0, 0.0), float2(0.0, 0.5), float2(0.5, 0.0),
		float2(0.5, 1.0), float2(1.0, 0.5), float2(1.0, 1.0)
	};

	uint baseOffset = idx * 6 * 24;  // 6 verts x 24 bytes
	[unroll] for (uint v = 0; v < 6; v++)
	{
		uint off = baseOffset + v * 24;
		g_OutputVB.Store3(off,      asuint(positions[v]));
		g_OutputVB.Store (off + 12, seg.color);
		g_OutputVB.Store2(off + 16, asuint(uvs[v]));
	}
}
