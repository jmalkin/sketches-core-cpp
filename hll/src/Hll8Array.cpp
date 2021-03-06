/*
 * Copyright 2018, Yahoo! Inc. Licensed under the terms of the
 * Apache License 2.0. See LICENSE file at the project root for terms.
 */

#include "Hll8Array.hpp"

#include <cstring>

namespace datasketches {

Hll8Iterator::Hll8Iterator(const Hll8Array& hllArray, const int lengthPairs)
  : HllPairIterator(lengthPairs),
    hllArray(hllArray)
{}

Hll8Iterator::~Hll8Iterator() { }

int Hll8Iterator::value() {
  return hllArray.hllByteArr[index] & HllUtil::VAL_MASK_6;
}

Hll8Array::Hll8Array(const int lgConfigK) :
    HllArray(lgConfigK, TgtHllType::HLL_8) {
  const int numBytes = hll8ArrBytes(lgConfigK);
  hllByteArr = new uint8_t[numBytes];
  std::fill(hllByteArr, hllByteArr + numBytes, 0);
}

Hll8Array::Hll8Array(const Hll8Array& that) :
  HllArray(that)
{
  // can determine hllByteArr size in parent class, no need to allocate here
}

Hll8Array::~Hll8Array() {
  // hllByteArr deleted in parent
}

Hll8Array* Hll8Array::copy() const {
  return new Hll8Array(*this);
}

std::unique_ptr<PairIterator> Hll8Array::getIterator() const {
  PairIterator* itr = new Hll8Iterator(*this, 1 << lgConfigK);
  return std::unique_ptr<PairIterator>(itr);
}

int Hll8Array::getSlot(const int slotNo) const {
  return (int) hllByteArr[slotNo] & HllUtil::VAL_MASK_6;
}

void Hll8Array::putSlot(const int slotNo, const int value) {
  hllByteArr[slotNo] = value & HllUtil::VAL_MASK_6;
}

int Hll8Array::getHllByteArrBytes() const {
  return hll8ArrBytes(lgConfigK);
}

HllSketchImpl* Hll8Array::couponUpdate(const int coupon) { // used by HLL_8 and HLL_6
  const int configKmask = (1 << getLgConfigK()) - 1;
  const int slotNo = HllUtil::getLow26(coupon) & configKmask;
  const int newVal = HllUtil::getValue(coupon);
  if (newVal <= 0) {
    throw std::logic_error("newVal must be a positive integer: " + std::to_string(newVal));
  }

  const int curVal = getSlot(slotNo);
  if (newVal > curVal) {
    putSlot(slotNo, newVal);
    hipAndKxQIncrementalUpdate(*this, curVal, newVal);
    if (curVal == 0) {
      decNumAtCurMin(); // interpret numAtCurMin as num zeros
      if (getNumAtCurMin() < 0) { 
        throw std::logic_error("getNumAtCurMin() must return a nonnegative integer: " + std::to_string(getNumAtCurMin()));
      }
    }
  }
  return this;
}

}

