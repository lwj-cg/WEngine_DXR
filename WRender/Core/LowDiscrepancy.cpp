#include "../Include/LowDiscrepancy.h"
#include "../Include/LowDiscrepancyConsts.h"

std::vector<uint32_t> ComputeRadicalInversePermutations(RNG& rng)
{
    std::vector<uint32_t> perms;
    // Allocate space in _perms_ for radical inverse permutations
    int permArraySize = 0;
    for (int i = 0; i < PrimeTableSize; ++i) permArraySize += Primes[i];
    perms.resize(permArraySize);
    uint32_t* p = &perms[0];
    for (int i = 0; i < PrimeTableSize; ++i) {
        // Generate random permutation for $i$th prime base
        for (int j = 0; j < Primes[i]; ++j) p[j] = j;
        Shuffle(p, Primes[i], 1, rng);
        p += Primes[i];
    }
    return perms;
}