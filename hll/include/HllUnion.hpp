/*
 * Copyright 2018, Yahoo! Inc. Licensed under the terms of the
 * Apache License 2.0. See LICENSE file at the project root for terms.
 */

#ifndef _HLLUNION_H_
#define _HLLUNION_H_

#include "hll.hpp"
#include "HllUtil.hpp"
#include "HllSketch.hpp"

namespace datasketches {

/**
 * This performs union operations for HLL sketches. This union operator is configured with a
 * <i>lgMaxK</i> instead of the normal <i>lgConfigK</i>.
 *
 * <p>This union operator does permit the unioning of sketches with different values of
 * <i>lgConfigK</i>.  The user should be aware that the resulting accuracy of a sketch returned
 * at the end of the unioning process will be a function of the smallest of <i>lgMaxK</i> and
 * <i>lgConfigK</i> that the union operator has seen.
 *
 * <p>This union operator also permits unioning of any of the three different target HllSketch
 * types.
 *
 * <p>Although the API for this union operator parallels many of the methods of the
 * <i>HllSketch</i>, the behavior of the union operator has some fundamental differences.
 *
 * <p>First, the user cannot specify the {@link TgtHllType} as an input parameter.
 * Instead, it is specified for the sketch returned with {@link #getResult(TgtHllType)}.
 *
 * <p>Second, the internal effective value of log-base-2 of <i>k</i> for the union operation can
 * change dynamically based on the smallest <i>lgConfigK</i> that the union operation has seen.
 *
 * @author Lee Rhodes
 * @author Kevin Lang
 */
class HllUnionPvt final : public HllUnion {
  public:
    explicit HllUnionPvt(int lgMaxK);
    explicit HllUnionPvt(std::unique_ptr<HllSketchPvt> sketch);

    HllUnionPvt(const HllUnion& other);
    HllUnionPvt& operator=(HllUnionPvt& other);

    static std::unique_ptr<HllUnionPvt> deserialize(std::istream& is);
    static std::unique_ptr<HllUnionPvt> deserialize(const void* bytes, size_t len);

    virtual ~HllUnionPvt();

    virtual double getEstimate() const;
    virtual double getCompositeEstimate() const;
    virtual double getLowerBound(int numStdDev) const;
    virtual double getUpperBound(int numStdDev) const;

    virtual int getCompactSerializationBytes() const;
    virtual int getUpdatableSerializationBytes() const;
    virtual int getLgConfigK() const;

    virtual TgtHllType getTgtHllType() const;
    virtual bool isCompact() const;
    virtual bool isEmpty() const;

    virtual void reset();

    virtual hll_sketch getResult(TgtHllType tgtHllType = HLL_4) const;

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

    virtual void update(const HllSketch& sketch);
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

    void couponUpdate(int coupon);

    CurMode getCurrentMode() const;
    int getSerializationVersion() const;
    bool isOutOfOrderFlag() const;
    bool isEstimationMode() const;


   /**
    * Union the given source and destination sketches. This static method examines the state of
    * the current internal gadget and the incoming sketch and determines the optimum way to
    * perform the union. This may involve swapping, down-sampling, transforming, and / or
    * copying one of the arguments and may completely replace the internals of the union.
    *
    * @param incomingImpl the given incoming sketch, which may not be modified.
    * @param gadgetImpl the given gadget sketch, which must have a target of HLL_8 and may be
    * modified.
    * @param lgMaxK the maximum value of log2 K for this union.
    * //@return the union of the two sketches in the form of the internal HllSketchImpl, which for
    * //the union is always in HLL_8 form.
    */
    void unionImpl(HllSketchImpl* incomingImpl, int lgMaxK);

    static HllSketchImpl* copyOrDownsampleHll(HllSketchImpl* srcImpl, int tgtLgK);

    // calls couponUpdate on sketch, freeing the old sketch upon changes in CurMode
    static HllSketchImpl* leakFreeCouponUpdate(HllSketchImpl* impl, int coupon);

    int lgMaxK;
    std::unique_ptr<HllSketchPvt> gadget;
};

}

#endif // _HLLUNION_H_