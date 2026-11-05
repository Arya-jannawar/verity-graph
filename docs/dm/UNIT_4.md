# DM UNIT 4 — Number theory and cryptography (Phase 1: core section)

> Phase 4 adds Miller–Rabin, CRT, Fermat demonstrations and the ledger chain.

## 1. Core number theory (implemented in Phase 1)

### Definitions
- **gcd(a,b)**: largest common divisor; Euclid: `gcd(a,b) = gcd(b, a mod b)`.
- **Bézout identity:** ∃ integers x,y with `a·x + b·y = gcd(a,b)` (computed by
  the extended Euclid algorithm).
- **lcm(a,b) = a·b / gcd(a,b)** (overflow-guarded here: throws instead of
  wrapping).
- **Modular exponentiation** `a^e mod m` by binary exponentiation.
- **Modular inverse:** `a·x ≡ 1 (mod m)` exists iff `gcd(a,m) = 1`.
- **Primes:** trial division in O(√n); **sieve of Eratosthenes** O(n log log n).
- **Euler's totient φ(n):** count of 1 ≤ k ≤ n with gcd(k,n) = 1.

### Theorem (Fermat's little theorem)
If p is prime and gcd(a,p) = 1, then `a^(p−1) ≡ 1 (mod p)`.
*Proof sketch:* the map x ↦ a·x permutes the nonzero residues mod p, so the
products ∏x and ∏(a·x) are congruent; cancel a^(p−1).

### Use in Verity
- `nextPrime` powers HashTable prime sizing (table capacities are always prime,
  which keeps chain lengths balanced for the polynomial string hash).
- `modPow`/`modInverse` are the foundation of the Phase-4 Miller–Rabin witness
  test and CRT recombination; the portable overflow-safe multiply (no __int128)
  was chosen precisely so those later phases stay MSVC-portable.
- φ appears in the Phase-4 RSA sanity checks (φ(p·q) = (p−1)(q−1)).

### Worked example
gcd(240,46): 240 = 5·46 + 10 → 46 = 4·10 + 6 → 10 = 1·6 + 4 → 6 = 1·4 + 2 →
4 = 2·2 → **gcd = 2**; extended Euclid gives 240·(−9) + 46·47 = 2.
`2^10 mod 1000 = 24`; `3·4 = 12 ≡ 1 (mod 11)` so `3^{-1} ≡ 4 (mod 11)`.
Run: `verity dm numtheory`.

### Tests
`tests/dm/test_numtheory.cpp`: "extendedGcd: Bezout identity", "lcm: known
values and overflow guard", "modPow: known values and edge cases", "modInverse:
known values and failure paths", "sieve agrees with isPrime for all n in
[0, 5000]", "nextPrime: strictly greater", "eulerTotient: known values and
multiplicative law".
