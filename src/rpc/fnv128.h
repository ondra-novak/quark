/*
 * fnv128.h
 *
 *  Created on: 14. 5. 2018
 *      Author: ondra
 */

#ifndef SRC_RPC_FNV128_H_
#define SRC_RPC_FNV128_H_

#include <stdint.h>

struct Arith128 {
  uint64_t d[2];

  Arith128() {}
  Arith128(const char *str);
  Arith128(uint64_t val);
  Arith128 &operator+=(const Arith128 &other);
  Arith128 &operator*=(const Arith128 &other);
  Arith128 &operator^=(const Arith128 &other);
};

class FNV128 {
public:

	FNV128(Arith128 &curVal);
	void operator()(unsigned char ch) const;
	Arith128 &curVal;
};




#endif /* SRC_RPC_FNV128_H_ */
