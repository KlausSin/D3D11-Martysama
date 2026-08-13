
struct ParticleInput
{
	float3 pos;
	float3 lastPos;
	float2 halfSize;
	float2 scale;
	float  rotation;
	uint   color;
	uint   flags;
	float3 _pad;
};

StructuredBuffer<ParticleInput> g_Particles : register(t0);
RWByteAddressBuffer g_OutputVB : register(u0);

cbuffer CBParticleCS : register(b0)
{
	float3 camUp;      float _pad0;
	float3 camCross;   float _pad1;
	float3 camView;    float _pad2;
	float4x4 attachMatrix;
	uint   particleCount;
	uint   facesPerParticle;
	uint   hasAttachMatrix;
	uint   _pad3;
	float4 faceRotations;
};

[numthreads(64, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	uint idx = DTid.x;
	if (idx >= particleCount) return;

	ParticleInput p = g_Particles[idx];
	uint bbType = p.flags & 0xF;
	bool bStretch = (p.flags & 0x10) != 0;

	float3 center = p.pos;
	if (hasAttachMatrix)
		center = mul(float4(center, 1), attachMatrix).xyz;

	for (uint face = 0; face < facesPerParticle; face++)
	{
		float3 v3Up, v3Cross;
		float totalRot = p.rotation;

		if (bStretch)
		{
			v3Up = p.pos - p.lastPos;
			if (hasAttachMatrix)
				v3Up = mul(float4(v3Up, 0), attachMatrix).xyz;
			float len = length(v3Up);
			if (len == 0) v3Up = float3(0, 0, 1);
			else v3Up *= (1.0f + log(1.0f + len)) / len;
			v3Cross = normalize(cross(v3Up, camView));
		}
		else
		{
			switch (bbType)
			{
				case 3: // BILLBOARD_TYPE_LIE
				{
					float c = cos(totalRot), s = sin(totalRot);
					v3Up = float3(c, -s, 0);
					v3Cross = float3(s, c, 0);
					break;
				}
				case 2: // BILLBOARD_TYPE_Y
				case 4: // BILLBOARD_TYPE_2FACE
				case 5: // BILLBOARD_TYPE_3FACE
				{
					v3Up = float3(0, 0, 1);
					if (v3Up.x * camView.y - v3Up.y * camView.x < 0)
						v3Up = -v3Up;
					float3 viewXY = float3(camView.x, camView.y, 0);
					v3Cross = normalize(cross(v3Up, viewXY));
					if (totalRot != 0)
					{
						float c = -sin(totalRot), s = cos(totalRot);
						float3 tmpUp = v3Up * c - v3Cross * s;
						v3Cross = v3Cross * c + v3Up * s;
						v3Up = tmpUp;
					}
					break;
				}
				default: // BILLBOARD_TYPE_ALL (1) and BILLBOARD_TYPE_NONE (0)
				{
					if (totalRot == 0)
					{
						v3Up = -camCross;
						v3Cross = camUp;
					}
					else
					{
						// Rodrigues' rotation around camView
						float c = cos(totalRot), s = sin(totalRot), omc = 1 - c;
						float3 k = camView;
						float3 v = -camCross;
						float kDotV = dot(k, v);
						v3Up = v * c + cross(k, v) * s + k * kDotV * omc;

						float kDotU = dot(k, camUp);
						v3Cross = camUp * c + cross(k, camUp) * s + k * kDotU * omc;
					}
					break;
				}
			}
		}

		// Apply face Z-rotation (for 2FACE/3FACE extra faces)
		float faceRot = (face == 0) ? faceRotations.x : ((face == 1) ? faceRotations.y : faceRotations.z);
		if (faceRot != 0)
		{
			float fc = cos(faceRot), fs = sin(faceRot);
			float ux = v3Up.x, uy = v3Up.y;
			v3Up.x = ux * fc - uy * fs;
			v3Up.y = uy * fc + ux * fs;
			float cx = v3Cross.x, cy = v3Cross.y;
			v3Cross.x = cx * fc - cy * fs;
			v3Cross.y = cy * fc + cx * fs;
		}

		// Scale
		v3Cross = -(p.halfSize.x * p.scale.x) * v3Cross;
		v3Up = (p.halfSize.y * p.scale.y) * v3Up;

		// Generate 4 vertices for this quad
		uint quadIdx = idx * facesPerParticle + face;
		uint baseOffset = quadIdx * 4 * 24; // 4 verts x 24 bytes each

		float3 positions[4];
		positions[0] = center - v3Up + v3Cross;
		positions[1] = center - v3Up - v3Cross;
		positions[2] = center + v3Up + v3Cross;
		positions[3] = center + v3Up - v3Cross;

		// UV coords match CPU particle mesh initialization
		float2 uvs[4] = { float2(0,1), float2(0,0), float2(1,1), float2(1,0) };

		[unroll]
		for (uint v = 0; v < 4; v++)
		{
			uint off = baseOffset + v * 24;
			g_OutputVB.Store3(off,      asuint(positions[v]));
			g_OutputVB.Store (off + 12, p.color);
			g_OutputVB.Store2(off + 16, asuint(uvs[v]));
		}
	}
}
