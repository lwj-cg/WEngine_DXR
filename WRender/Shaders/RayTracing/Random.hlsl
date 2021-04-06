#ifndef RANDOM_H_
#define RANDOM_H_
// Some useful functions for random number generation

inline unsigned int tea16(unsigned int val0, unsigned int val1)
{
	unsigned int v0 = val0;
	unsigned int v1 = val1;
	unsigned int s0 = 0;
	int N = 16;

	for (unsigned int n = 0; n < N; n++)
	{
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}

	return v0;
}

// Generate random unsigned int in [0, 2^24)
inline unsigned int lcg(inout unsigned int prev)
{
	const unsigned int LCG_A = 1664525u;
	const unsigned int LCG_C = 1013904223u;
	prev = (LCG_A * prev + LCG_C);
	return prev & 0x00FFFFFF;
}

inline unsigned int lcg2(inout unsigned int prev)
{
	prev = (prev * 8121 + 28411) % 134456;
	return prev;
}

// Generate random float in [0, 1)
inline float rnd(inout unsigned int prev)
{
	return ((float) lcg(prev) / (float) 0x01000000);
}

inline unsigned int rot_seed(unsigned int seed, unsigned int frame)
{
	return seed ^ frame;
}

#endif