/*
 * fnv128.c
 *
 *  Created on: 14. 5. 2018
 *      Author: ondra
 */


#include "fnv128.h"

void fnvInitArith128from64(Arith128 *kOut, uint64_t val) {
  kOut->d[0] = val;
  kOut->d[1] = 0;
}

void fnvArith128add(Arith128 *kOut, const Arith128 *kIn) {
  *((__uint128_t *) kOut) += *((__uint128_t *) kIn);
}

void fnvArith128mul(Arith128 *kOut, const Arith128 *kIn) {
  *((__uint128_t *) kOut) *= *((__uint128_t *) kIn);
}

void fnvArith128xor8(Arith128 *kOut, unsigned char c) {
  kOut->d[0] ^= c;
}

void fnvArith128DecIn(Arith128 *kOut, const char *str) {
  kOut->d[0] = 0;
  kOut->d[1] = 0;
  Arith128 ten;
  fnvInitArith128from64(&ten, 10);
  while (*str) {
    unsigned char c = *str;
    ++str;
    Arith128 cur;
    fnvInitArith128from64(&cur, c - '0');
    fnvArith128mul(kOut, &ten);
    fnvArith128add(kOut, &cur);
  }
}

Arith128::Arith128(const char *str) {
	fnvArith128DecIn(this, str);

}

Arith128::Arith128(uint64_t val) {
	d[0] = val;
	d[1] = 0;
}

Arith128 &Arith128::operator+=(const Arith128 &other) {
	fnvArith128add(this, &other);
	return *this;
}
Arith128 &Arith128::operator*=(const Arith128 &other){
	fnvArith128mul(this, &other);
	return *this;
}
Arith128 &Arith128::operator^=(const Arith128 &other){
	this->d[0] ^= other.d[0];
	this->d[1] ^= other.d[1];
	return *this;
}

static const Arith128 fnv128_offset("144066263297769815596495629667062367629")
					, fnv128_prime("309485009821345068724781371");


FNV128::FNV128(Arith128 &curVal):curVal(curVal) {
	curVal = fnv128_offset;
}

void FNV128::operator()(unsigned char ch) const {
	curVal ^= ch;
	curVal *= fnv128_prime;

}
