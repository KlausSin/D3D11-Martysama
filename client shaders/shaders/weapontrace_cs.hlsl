
struct SplineSegment
{
	float3 a; float timeStart;
	float3 b; float timeEnd;
	float3 c; float _pad0;
	float3 d; float _pad1;
};

StructuredBuffer<SplineSegment> g_Segments : register(t0);
RWByteAddressBuffer g_OutputVB : register(u0);

cbuffer CBWeaponTraceCS : register(b0)
{
	uint numSegments;
	uint numSamples;
	float lifetime;
	float samplingTime;
	float firstPointTime;
	float totalLength;
	uint2 _pad;
};

[numthreads(64, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	uint sampleIdx = DTid.x;
	if (sampleIdx >= numSamples) return;

	float t = sampleIdx * samplingTime;
	if (t > totalLength) t = totalLength;

	uint seg = 0;
	for (uint i = 0; i < numSegments; i++)
	{
		if (t <= g_Segments[i].timeEnd) { seg = i; break; }
		seg = i;
	}

	SplineSegment ss = g_Segments[seg];
	float cc = t - ss.timeStart;
	float3 shortPos = ss.a + cc * (ss.b + cc * (ss.c + cc * ss.d));

	SplineSegment ls = g_Segments[seg + numSegments];
	cc = t - ls.timeStart;
	float3 longPos = ls.a + cc * (ls.b + cc * (ls.c + cc * ls.d));

	// Alpha fade for long vertex (short is always alpha=0)
	float ttt = saturate((t + firstPointTime) / lifetime);
	float alpha = saturate((1.0 - ttt) * (1.0 - ttt) / 2.5 - 0.1);

	// Pack color as ARGB DWORD: Color(0.3, 0.8, 1.0, alpha)
	uint R = (uint)(0.3 * 255.0 + 0.5);
	uint G = (uint)(0.8 * 255.0 + 0.5);
	uint B = 255;
	uint A = (uint)(alpha * 255.0 + 0.5);
	uint longColor = (A << 24) | (R << 16) | (G << 8) | B;

	float texU = t / lifetime;

	uint baseOff = sampleIdx * 2 * 24;

	// Long vertex
	g_OutputVB.Store3(baseOff,      asuint(longPos));
	g_OutputVB.Store (baseOff + 12, longColor);
	g_OutputVB.Store2(baseOff + 16, asuint(float2(texU, 0.0)));

	// Short vertex (alpha = 0, fully transparent)
	g_OutputVB.Store3(baseOff + 24, asuint(shortPos));
	g_OutputVB.Store (baseOff + 36, 0);
	g_OutputVB.Store2(baseOff + 40, asuint(float2(texU, 1.0)));
}
