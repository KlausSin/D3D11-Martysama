#pragma once

struct SParticleVertex
{
	Vector3 v3Pos;
	DWORD dwColor;
	float u, v;
};

struct BlurVertex
{
	Vector3 pos;      // Screen-space position (x, y, z)
	DWORD   color;    // Vertex color (BGRA)
	FLOAT   tu, tv;   // Texture coordinates

	BlurVertex(Vector3 p, DWORD c, float u, float v) : pos(p), color(c), tu(u), tv(v) {}
	~BlurVertex() {}
};

class CSnowParticle
{
	public:
		CSnowParticle();
		~CSnowParticle();

		static CSnowParticle * New();
		static void Delete(CSnowParticle * pSnowParticle);
		static void DestroyPool();

		void Init(const Vector3 & c_rv3Pos);

		void SetCameraVertex(const Vector3 & rv3Up, const Vector3 & rv3Cross);
		bool IsActivate();

		// Accessors for GPU path
		const Vector3& GetPosition() const { return m_v3Position; }
		float GetHalfWidth() const { return m_fHalfWidth; }
		float GetHalfHeight() const { return m_fHalfHeight; }

		void Update(float fElapsedTime, const Vector3 & c_rv3Pos);
		void GetVerticies(SParticleVertex & rv3Vertex1, SParticleVertex & rv3Vertex2,
						  SParticleVertex & rv3Vertex3, SParticleVertex & rv3Vertex4);

	protected:
		bool m_bActivate;
		bool m_bChangedSize;
		float m_fHalfWidth;
		float m_fHalfHeight;

		Vector3 m_v3Velocity;
		Vector3 m_v3Position;

		Vector3 m_v3Up;
		Vector3 m_v3Cross;

		float m_fPeriod;
		float m_fcurRadian;
		float m_fAmplitude;

	public:
		static std::vector<CSnowParticle*> ms_kVct_SnowParticlePool;
};
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
