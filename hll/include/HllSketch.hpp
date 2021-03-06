/*
 * Copyright 2018, Yahoo! Inc. Licensed under the terms of the
 * Apache License 2.0. See LICENSE file at the project root for terms.
 */

#ifndef _HLLSKETCH_H_
#define _HLLSKETCH_H_

#include "hll.hpp"
#include "PairIterator.hpp"
#include "HllUtil.hpp"
#include "HllSketchImpl.hpp"

#include <memory>
#include <iostream>

namespace datasketches {

class HllSketchImpl;

// Contains the non-public API for HllSketch
class HllSketchPvt final : public HllSketch {
  public:
    explicit HllSketchPvt(int lgConfigK, TgtHllType tgtHllType = HLL_4);
    static std::unique_ptr<HllSketchPvt> deserialize(std::istream& is);
    static std::unique_ptr<HllSketchPvt> deserialize(const void* bytes, size_t len);

    virtual ~HllSketchPvt();

    HllSketchPvt(const HllSketch& that);
    HllSketchPvt(HllSketchImpl* that);
    
    HllSketchPvt& operator=(HllSketchPvt other);

    virtual std::unique_ptr<HllSketchPvt> copy() const;
    virtual std::unique_ptr<HllSketchPvt> copyAs(TgtHllType tgtHllType) const;

    virtual void reset();

    virtual void update(const std::string& datum);
    virtual void update(uint64_t datum);
    virtual void update(uint32_t datum);
    virtual void update(uint16_t datum);
    virtual void update(uint8_t datum);
    virtual void update(int64_t datum);
    virtual void update(int32_t datum);
    virtual void update(int16_t datum);
    virtual void update(int8_t datum);
    virtual void update(double datum);
    virtual void update(float datum);
    virtual void update(const void* data, size_t lengthBytes);
    
    virtual std::pair<std::unique_ptr<uint8_t[]>, const size_t> serializeCompact() const;
    virtual std::pair<std::unique_ptr<uint8_t[]>, const size_t> serializeUpdatable() const;
    virtual void serializeCompact(std::ostream& os) const;
    virtual void serializeUpdatable(std::ostream& os) const;

    virtual std::ostream& to_string(std::ostream& os,
                                    bool summary = true,
                                    bool detail = false,
                                    bool auxDetail = false,
                                    bool all = false) const;
    virtual std::string to_string(bool summary = true,
                                  bool detail = false,
                                  bool auxDetail = false,
                                  bool all = false) const;

    virtual double getEstimate() const;
    virtual double getCompositeEstimate() const;
    virtual double getLowerBound(int numStdDev) const;
    virtual double getUpperBound(int numStdDev) const;

    virtual int getLgConfigK() const;
    virtual TgtHllType getTgtHllType() const;
    
    virtual bool isCompact() const;
    virtual bool isEmpty() const;

    virtual int getUpdatableSerializationBytes() const;
    virtual int getCompactSerializationBytes() const;

    virtual std::unique_ptr<PairIterator> getIterator() const;

    virtual void couponUpdate(int coupon);

    virtual std::string typeAsString() const;
    virtual std::string modeAsString() const;

    CurMode getCurrentMode() const;
    int getSerializationVersion() const;
    bool isOutOfOrderFlag() const;
    bool isEstimationMode() const;

    HllSketchImpl* hllSketchImpl;
};

}

#endif // _HLLSKETCH_H_
