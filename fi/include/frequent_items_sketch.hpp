/*
 * Copyright 2019, Verizon Media.
 * Licensed under the terms of the Apache License 2.0. See LICENSE file at the project root for terms.
 */

#ifndef FREQUENT_ITEMS_SKETCH_HPP_
#define FREQUENT_ITEMS_SKETCH_HPP_

#include <memory>
#include <vector>
#include <iostream>
#include <streambuf>
#include <cstring>
#include <functional>

#include "reverse_purge_hash_map.hpp"

namespace datasketches {

/*
 * Based on Java implementation here:
 * https://github.com/DataSketches/sketches-core/blob/master/src/main/java/com/yahoo/sketches/frequencies/ItemsSketch.java
 * author Alexander Saydakov
 */

// serialize and deserialize
template<typename T> struct serde {
  // stream
  void serialize(std::ostream& os, const T* items, unsigned num);
  void deserialize(std::istream& is, T* items, unsigned num); // items are not initialized
  // raw bytes
  size_t size_of_item(const T& item);
  size_t serialize(char* ptr, const T* items, unsigned num);
  size_t deserialize(const char* ptr, T* items, unsigned num); // items are not initialized
};

enum frequent_items_error_type { NO_FALSE_POSITIVES, NO_FALSE_NEGATIVES };

// for serialization as raw bytes
typedef std::unique_ptr<void, std::function<void(void*)>> void_ptr_with_deleter;

template<typename T, typename H = std::hash<T>, typename E = std::equal_to<T>, typename S = serde<T>, typename A = std::allocator<T>>
class frequent_items_sketch {
public:
  static const uint64_t USE_MAX_ERROR = 0; // used in get_frequent_items

  explicit frequent_items_sketch(uint8_t lg_max_map_size);
  class row;
  void update(const T& item, uint64_t weight = 1);
  void update(T&& item, uint64_t weight = 1);
  void merge(const frequent_items_sketch& other);
  bool is_empty() const;
  uint32_t get_num_active_items() const;
  uint64_t get_total_weight() const;
  uint64_t get_estimate(const T& item) const;
  uint64_t get_lower_bound(const T& item) const;
  uint64_t get_upper_bound(const T& item) const;
  uint64_t get_maximum_error() const;
  double get_epsilon() const;
  static double get_epsilon(uint8_t lg_max_map_size);
  static double get_apriori_error(uint8_t lg_max_map_size, uint64_t estimated_total_weight);
  typedef typename std::allocator_traits<A>::template rebind_alloc<row> AllocRow;
  std::vector<row, AllocRow> get_frequent_items(frequent_items_error_type err_type, uint64_t threshold = USE_MAX_ERROR) const;
  size_t get_serialized_size_bytes() const;
  void serialize(std::ostream& os) const;
  std::pair<void_ptr_with_deleter, const size_t> serialize(unsigned header_size_bytes = 0) const;
  static frequent_items_sketch deserialize(std::istream& is);
  static frequent_items_sketch deserialize(const void* bytes, size_t size);
  void to_stream(std::ostream& os, bool print_items = false) const;
private:
  static const uint8_t LG_MIN_MAP_SIZE = 3;
  static const uint8_t SERIAL_VERSION = 1;
  static const uint8_t FAMILY_ID = 10;
  static const uint8_t PREAMBLE_LONGS_EMPTY = 1;
  static const uint8_t PREAMBLE_LONGS_NONEMPTY = 4;
  static constexpr double EPSILON_FACTOR = 3.5;
  enum flags { IS_EMPTY };
  uint64_t total_weight;
  uint64_t offset;
  reverse_purge_hash_map<T, H, E, A> map;
  frequent_items_sketch(uint8_t lg_cur_map_size, uint8_t lg_max_map_size);
  static void check_preamble_longs(uint8_t preamble_longs, bool is_empty);
  static void check_serial_version(uint8_t serial_version);
  static void check_family_id(uint8_t family_id);
  static void check_size(uint8_t lg_cur_size, uint8_t lg_max_size);
};

// clang++ seems to require this declaration for CMAKE_BUILD_TYPE='Debug"
template<typename T, typename H, typename E, typename S, typename A>
const uint8_t frequent_items_sketch<T,H,E,S,A>::LG_MIN_MAP_SIZE;

template<typename T, typename H, typename E, typename S, typename A>
frequent_items_sketch<T, H, E, S, A>::frequent_items_sketch(uint8_t lg_max_map_size):
total_weight(0),
offset(0),
map(frequent_items_sketch::LG_MIN_MAP_SIZE, std::max(lg_max_map_size, frequent_items_sketch::LG_MIN_MAP_SIZE))
{
}

template<typename T, typename H, typename E, typename S, typename A>
frequent_items_sketch<T, H, E, S, A>::frequent_items_sketch(uint8_t lg_cur_map_size, uint8_t lg_max_map_size):
total_weight(0),
offset(0),
map(lg_cur_map_size, lg_max_map_size)
{
}

template<typename T, typename H, typename E, typename S, typename A>
void frequent_items_sketch<T, H, E, S, A>::update(const T& item, uint64_t weight) {
  if (weight == 0) return;
  total_weight += weight;
  offset += map.adjust_or_insert(item, weight);
}

template<typename T, typename H, typename E, typename S, typename A>
void frequent_items_sketch<T, H, E, S, A>::update(T&& item, uint64_t weight) {
  if (weight == 0) return;
  total_weight += weight;
  offset += map.adjust_or_insert(std::move(item), weight);
}

template<typename T, typename H, typename E, typename S, typename A>
void frequent_items_sketch<T, H, E, S, A>::merge(const frequent_items_sketch& other) {
  if (other.is_empty()) return;
  const uint64_t merged_total_weight = total_weight + other.get_total_weight(); // for correction at the end
  for (auto &it: other.map) {
    update(it.first, it.second);
  }
  offset += other.offset;
  total_weight = merged_total_weight;
}

template<typename T, typename H, typename E, typename S, typename A>
bool frequent_items_sketch<T, H, E, S, A>::is_empty() const {
  return map.get_num_active() == 0;
}

template<typename T, typename H, typename E, typename S, typename A>
uint32_t frequent_items_sketch<T, H, E, S, A>::get_num_active_items() const {
  return map.get_num_active();
}

template<typename T, typename H, typename E, typename S, typename A>
uint64_t frequent_items_sketch<T, H, E, S, A>::get_total_weight() const {
  return total_weight;
}

template<typename T, typename H, typename E, typename S, typename A>
uint64_t frequent_items_sketch<T, H, E, S, A>::get_estimate(const T& item) const {
  // if item is tracked estimate = weight + offset, otherwise 0
  const uint64_t weight = map.get(item);
  if (weight > 0) return weight + offset;
  return 0;
}

template<typename T, typename H, typename E, typename S, typename A>
uint64_t frequent_items_sketch<T, H, E, S, A>::get_lower_bound(const T& item) const {
  return map.get(item);
}

template<typename T, typename H, typename E, typename S, typename A>
uint64_t frequent_items_sketch<T, H, E, S, A>::get_upper_bound(const T& item) const {
  return map.get(item) + offset;
}

template<typename T, typename H, typename E, typename S, typename A>
uint64_t frequent_items_sketch<T, H, E, S, A>::get_maximum_error() const {
  return offset;
}

template<typename T, typename H, typename E, typename S, typename A>
double frequent_items_sketch<T, H, E, S, A>::get_epsilon() const {
  return EPSILON_FACTOR / (1 << map.get_lg_max_size());
}

template<typename T, typename H, typename E, typename S, typename A>
double frequent_items_sketch<T, H, E, S, A>::get_epsilon(uint8_t lg_max_map_size) {
  return EPSILON_FACTOR / (1 << lg_max_map_size);
}

template<typename T, typename H, typename E, typename S, typename A>
double frequent_items_sketch<T, H, E, S, A>::get_apriori_error(uint8_t lg_max_map_size, uint64_t estimated_total_weight) {
  return get_epsilon(lg_max_map_size) * estimated_total_weight;
}

template<typename T, typename H, typename E, typename S, typename A>
class frequent_items_sketch<T, H, E, S, A>::row {
public:
  row(const T& item, uint64_t estimate, uint64_t lower_bound, uint64_t upper_bound):
    item(item), estimate(estimate), lower_bound(lower_bound), upper_bound(upper_bound) {}
  const T& get_item() const { return item; }
  uint64_t get_estimate() const { return estimate; }
  uint64_t get_lower_bound() const { return lower_bound; }
  uint64_t get_upper_bound() const { return upper_bound; }
private:
  T item;
  uint64_t estimate;
  uint64_t lower_bound;
  uint64_t upper_bound;
};

template<typename T, typename H, typename E, typename S, typename A>
std::vector<typename frequent_items_sketch<T, H, E, S, A>::row, typename frequent_items_sketch<T, H, E, S, A>::AllocRow>
frequent_items_sketch<T, H, E, S, A>::get_frequent_items(frequent_items_error_type err_type, uint64_t threshold) const {
  if (threshold == USE_MAX_ERROR) {
    threshold = get_maximum_error();
  }

  std::vector<row, AllocRow> items;
  for (auto &it: map) {
    const uint64_t est = it.second + offset;
    const uint64_t lb = it.second;
    const uint64_t ub = est;
    if ((err_type == NO_FALSE_NEGATIVES and ub > threshold) or (err_type == NO_FALSE_POSITIVES and lb > threshold)) {
      items.push_back(row(it.first, est, lb, ub));
    }
  }
  // sort by estimate in descending order
  std::sort(items.begin(), items.end(), [](row a, row b){ return a.get_estimate() > b.get_estimate(); });
  return items;
}

template<typename T, typename H, typename E, typename S, typename A>
void frequent_items_sketch<T, H, E, S, A>::serialize(std::ostream& os) const {
  const uint8_t preamble_longs = is_empty() ? PREAMBLE_LONGS_EMPTY : PREAMBLE_LONGS_NONEMPTY;
  os.write((char*)&preamble_longs, sizeof(preamble_longs));
  const uint8_t serial_version = SERIAL_VERSION;
  os.write((char*)&serial_version, sizeof(serial_version));
  const uint8_t family = FAMILY_ID;
  os.write((char*)&family, sizeof(family));
  const uint8_t lg_max_size = map.get_lg_max_size();
  os.write((char*)&lg_max_size, sizeof(lg_max_size));
  const uint8_t lg_cur_size = map.get_lg_cur_size();
  os.write((char*)&lg_cur_size, sizeof(lg_cur_size));
  const uint8_t flags_byte(
    (is_empty() ? 1 << flags::IS_EMPTY : 0)
  );
  os.write((char*)&flags_byte, sizeof(flags_byte));
  const uint16_t unused16 = 0;
  os.write((char*)&unused16, sizeof(unused16));
  if (!is_empty()) {
    const uint32_t num_items = map.get_num_active();
    os.write((char*)&num_items, sizeof(num_items));
    const uint32_t unused32 = 0;
    os.write((char*)&unused32, sizeof(unused32));
    os.write((char*)&total_weight, sizeof(total_weight));
    os.write((char*)&offset, sizeof(offset));

    // copy active items and their weights to use batch serialization
    typedef typename std::allocator_traits<A>::template rebind_alloc<uint64_t> AllocU64;
    uint64_t* weights = AllocU64().allocate(num_items);
    T* items = A().allocate(num_items);
    uint32_t i = 0;
    for (auto &it: map) {
      new (&items[i]) T(it.first);
      weights[i++] = it.second;
    }
    os.write((char*)weights, sizeof(uint64_t) * num_items);
    AllocU64().deallocate(weights, num_items);
    S().serialize(os, items, num_items);
    for (unsigned i = 0; i < num_items; i++) items[i].~T();
    A().deallocate(items, num_items);
  }
}

static inline void copy_from_mem(const char** src, void* dst, size_t size) {
  memcpy(dst, *src, size);
  *src += size;
}

static inline void copy_to_mem(const void* src, char** dst, size_t size) {
  memcpy(*dst, src, size);
  *dst += size;
}

template<typename T, typename H, typename E, typename S, typename A>
size_t frequent_items_sketch<T, H, E, S, A>::get_serialized_size_bytes() const {
  if (is_empty()) return PREAMBLE_LONGS_EMPTY * sizeof(uint64_t);
  size_t size = (PREAMBLE_LONGS_NONEMPTY + map.get_num_active()) * sizeof(uint64_t);
  for (auto &it: map) size += S().size_of_item(it.first);
  return size;
}

template<typename T, typename H, typename E, typename S, typename A>
std::pair<void_ptr_with_deleter, const size_t> frequent_items_sketch<T, H, E, S, A>::serialize(unsigned header_size_bytes) const {
  const size_t size = header_size_bytes + get_serialized_size_bytes();
  typedef typename std::allocator_traits<A>::template rebind_alloc<char> AllocChar;
  void_ptr_with_deleter data_ptr(
    static_cast<void*>(AllocChar().allocate(size)),
    [size](void* ptr) { AllocChar().deallocate(static_cast<char*>(ptr), size); }
  );
  char* ptr = static_cast<char*>(data_ptr.get()) + header_size_bytes;

  const uint8_t preamble_longs = is_empty() ? PREAMBLE_LONGS_EMPTY : PREAMBLE_LONGS_NONEMPTY;
  copy_to_mem(&preamble_longs, &ptr, sizeof(uint8_t));
  const uint8_t serial_version = SERIAL_VERSION;
  copy_to_mem(&serial_version, &ptr, sizeof(uint8_t));
  const uint8_t family = FAMILY_ID;
  copy_to_mem(&family, &ptr, sizeof(uint8_t));
  const uint8_t lg_max_size = map.get_lg_max_size();
  copy_to_mem(&lg_max_size, &ptr, sizeof(uint8_t));
  const uint8_t lg_cur_size = map.get_lg_cur_size();
  copy_to_mem(&lg_cur_size, &ptr, sizeof(uint8_t));
  const uint8_t flags_byte(
    (is_empty() ? 1 << flags::IS_EMPTY : 0)
  );
  copy_to_mem(&flags_byte, &ptr, sizeof(uint8_t));
  const uint16_t unused16 = 0;
  copy_to_mem(&unused16, &ptr, sizeof(uint16_t));
  if (!is_empty()) {
    const uint32_t num_items = map.get_num_active();
    copy_to_mem(&num_items, &ptr, sizeof(uint32_t));
    const uint32_t unused32 = 0;
    copy_to_mem(&unused32, &ptr, sizeof(uint32_t));
    copy_to_mem(&total_weight, &ptr, sizeof(uint64_t));
    copy_to_mem(&offset, &ptr, sizeof(uint64_t));

    // copy active items and their weights to use batch serialization
    typedef typename std::allocator_traits<A>::template rebind_alloc<uint64_t> AllocU64;
    uint64_t* weights = AllocU64().allocate(num_items);
    T* items = A().allocate(num_items);
    uint32_t i = 0;
    for (auto &it: map) {
      new (&items[i]) T(it.first);
      weights[i++] = it.second;
    }
    copy_to_mem(weights, &ptr, sizeof(uint64_t) * num_items);
    AllocU64().deallocate(weights, num_items);
    ptr += S().serialize(ptr, items, num_items);
    for (unsigned i = 0; i < num_items; i++) items[i].~T();
    A().deallocate(items, num_items);
  }
  return std::make_pair(std::move(data_ptr), size);
}

template<typename T, typename H, typename E, typename S, typename A>
frequent_items_sketch<T, H, E, S, A> frequent_items_sketch<T, H, E, S, A>::deserialize(std::istream& is) {
  uint8_t preamble_longs;
  is.read((char*)&preamble_longs, sizeof(preamble_longs));
  uint8_t serial_version;
  is.read((char*)&serial_version, sizeof(serial_version));
  uint8_t family_id;
  is.read((char*)&family_id, sizeof(family_id));
  uint8_t lg_max_size;
  is.read((char*)&lg_max_size, sizeof(lg_max_size));
  uint8_t lg_cur_size;
  is.read((char*)&lg_cur_size, sizeof(lg_cur_size));
  uint8_t flags_byte;
  is.read((char*)&flags_byte, sizeof(flags_byte));
  uint16_t unused16;
  is.read((char*)&unused16, sizeof(unused16));

  const bool is_empty = flags_byte & (1 << flags::IS_EMPTY);

  check_preamble_longs(preamble_longs, is_empty);
  check_serial_version(serial_version);
  check_family_id(family_id);
  check_size(lg_cur_size, lg_max_size);

  frequent_items_sketch<T, H, E, S, A> sketch(lg_cur_size, lg_max_size);
  if (!is_empty) {
    uint32_t num_items;
    is.read((char*)&num_items, sizeof(num_items));
    uint32_t unused32;
    is.read((char*)&unused32, sizeof(unused32));
    uint64_t total_weight;
    is.read((char*)&total_weight, sizeof(total_weight));
    uint64_t offset;
    is.read((char*)&offset, sizeof(offset));

    // batch deserialization with intermediate array of items and weights
    typedef typename std::allocator_traits<A>::template rebind_alloc<uint64_t> AllocU64;
    uint64_t* weights = AllocU64().allocate(num_items);
    is.read((char*)weights, sizeof(uint64_t) * num_items);
    T* items = A().allocate(num_items); // rely on serde to construct items
    S().deserialize(is, items, num_items);
    for (uint32_t i = 0; i < num_items; i++) {
      sketch.update(std::move(items[i]), weights[i]);
      items[i].~T();
    }
    AllocU64().deallocate(weights, num_items);
    A().deallocate(items, num_items);

    sketch.total_weight = total_weight;
    sketch.offset = offset;
  }
  return sketch;
}

template<typename T, typename H, typename E, typename S, typename A>
frequent_items_sketch<T, H, E, S, A> frequent_items_sketch<T, H, E, S, A>::deserialize(const void* bytes, size_t size) {
  const char* ptr = static_cast<const char*>(bytes);
  uint8_t preamble_longs;
  copy_from_mem(&ptr, &preamble_longs, sizeof(uint8_t));
  uint8_t serial_version;
  copy_from_mem(&ptr, &serial_version, sizeof(uint8_t));
  uint8_t family_id;
  copy_from_mem(&ptr, &family_id, sizeof(uint8_t));
  uint8_t lg_max_size;
  copy_from_mem(&ptr, &lg_max_size, sizeof(uint8_t));
  uint8_t lg_cur_size;
  copy_from_mem(&ptr, &lg_cur_size, sizeof(uint8_t));
  uint8_t flags_byte;
  copy_from_mem(&ptr, &flags_byte, sizeof(uint8_t));
  uint16_t unused16;
  copy_from_mem(&ptr, &unused16, sizeof(uint16_t));

  const bool is_empty = flags_byte & (1 << flags::IS_EMPTY);

  check_preamble_longs(preamble_longs, is_empty);
  check_serial_version(serial_version);
  check_family_id(family_id);
  check_size(lg_cur_size, lg_max_size);

  frequent_items_sketch<T, H, E, S, A> sketch(lg_cur_size, lg_max_size);
  if (!is_empty) {
    uint32_t num_items;
    copy_from_mem(&ptr, &num_items, sizeof(uint32_t));
    uint32_t unused32;
    copy_from_mem(&ptr, &unused32, sizeof(uint32_t));
    uint64_t total_weight;
    copy_from_mem(&ptr, &total_weight, sizeof(uint64_t));
    uint64_t offset;
    copy_from_mem(&ptr, &offset, sizeof(uint64_t));

    // batch deserialization with intermediate array of items and weights
    typedef typename std::allocator_traits<A>::template rebind_alloc<uint64_t> AllocU64;
    uint64_t* weights = AllocU64().allocate(num_items);
    copy_from_mem(&ptr, weights, sizeof(uint64_t) * num_items);
    T* items = A().allocate(num_items);
    ptr += S().deserialize(ptr, items, num_items);
    for (uint32_t i = 0; i < num_items; i++) {
      sketch.update(std::move(items[i]), weights[i]);
      items[i].~T();
    }
    AllocU64().deallocate(weights, num_items);
    A().deallocate(items, num_items);

    sketch.total_weight = total_weight;
    sketch.offset = offset;
  }
  return sketch;
}

template<typename T, typename H, typename E, typename S, typename A>
void frequent_items_sketch<T, H, E, S, A>::check_preamble_longs(uint8_t preamble_longs, bool is_empty) {
  if (is_empty) {
    if (preamble_longs != PREAMBLE_LONGS_EMPTY) {
      throw std::invalid_argument("Possible corruption: preamble longs of an empty sketch must be " + std::to_string(PREAMBLE_LONGS_EMPTY) + ": " + std::to_string(preamble_longs));
    }
  } else {
    if (preamble_longs != PREAMBLE_LONGS_NONEMPTY) {
      throw std::invalid_argument("Possible corruption: preamble longs of an non-empty sketch must be " + std::to_string(PREAMBLE_LONGS_NONEMPTY) + ": " + std::to_string(preamble_longs));
    }
  }
}

template<typename T, typename H, typename E, typename S, typename A>
void frequent_items_sketch<T, H, E, S, A>::check_serial_version(uint8_t serial_version) {
  if (serial_version != SERIAL_VERSION) {
    throw std::invalid_argument("Possible corruption: serial version must be " + std::to_string(SERIAL_VERSION) + ": " + std::to_string(serial_version));
  }
}

template<typename T, typename H, typename E, typename S, typename A>
void frequent_items_sketch<T, H, E, S, A>::check_family_id(uint8_t family_id) {
  if (family_id != FAMILY_ID) {
    throw std::invalid_argument("Possible corruption: family ID must be " + std::to_string(FAMILY_ID) + ": " + std::to_string(family_id));
  }
}

template<typename T, typename H, typename E, typename S, typename A>
void frequent_items_sketch<T, H, E, S, A>::check_size(uint8_t lg_cur_size, uint8_t lg_max_size) {
  if (lg_cur_size > lg_max_size) {
    throw std::invalid_argument("Possible corruption: expected lg_cur_size <= lg_max_size: " + std::to_string(lg_cur_size) + " <= " + std::to_string(lg_max_size));
  }
  if (lg_cur_size < LG_MIN_MAP_SIZE) {
    throw std::invalid_argument("Possible corruption: lg_cur_size must not be less than " + std::to_string(LG_MIN_MAP_SIZE) + ": " + std::to_string(lg_cur_size));
  }
}

template <typename T, typename H, typename E, typename S, typename A>
void frequent_items_sketch<T, H, E, S, A>::to_stream(std::ostream& os, bool print_items) const {
  os << "### Frequent items sketch summary:" << std::endl;
  os << "   lg cur map size  : " << (int) map.get_lg_cur_size() << std::endl;
  os << "   lg max map size  : " << (int) map.get_lg_max_size() << std::endl;
  os << "   num active items : " << get_num_active_items() << std::endl;
  os << "   total weight     : " << get_total_weight() << std::endl;
  os << "   max error        : " << get_maximum_error() << std::endl;
  os << "### End sketch summary" << std::endl;
  if (print_items) {
    std::vector<row, AllocRow> items;
    for (auto &it: map) {
      const uint64_t est = it.second + offset;
      const uint64_t lb = it.second;
      const uint64_t ub = est;
      items.push_back(row(it.first, est, lb, ub));
    }
    // sort by estimate in descending order
    std::sort(items.begin(), items.end(), [](row a, row b){ return a.get_estimate() > b.get_estimate(); });
    os << "### Items in descending order by estimate" << std::endl;
    os << "   item, estimate, lower bound, upper bound" << std::endl;
    for (auto &it: items) {
      os << "   " << it.get_item() << ", " << it.get_estimate() << ", "
         << it.get_lower_bound() << ", " << it.get_upper_bound() << std::endl;
    }
    os << "### End items" << std::endl;
  }
}


// serde for signed 64-bit integers
// this should produce sketches binary-compatible with LongsSketch
// and ItemsSketch<Long> with ArrayOfLongsSerDe in Java
template<>
struct serde<int64_t> {
  void serialize(std::ostream& os, const int64_t* items, unsigned num) {
    os.write((char*)items, sizeof(int64_t) * num);
  }
  void deserialize(std::istream& is, int64_t* items, unsigned num) {
    is.read((char*)items, sizeof(int64_t) * num);
  }
  size_t size_of_item(int64_t item) {
    return sizeof(int64_t);
  }
  size_t serialize(char* ptr, const int64_t* items, unsigned num) {
    memcpy(ptr, items, sizeof(int64_t) * num);
    return sizeof(int64_t) * num;
  }
  size_t deserialize(const char* ptr, int64_t* items, unsigned num) {
    memcpy(items, ptr, sizeof(int64_t) * num);
    return sizeof(int64_t) * num;
  }
};

// serde for std::string items
// This should produce sketches binary-compatible with
// ItemsSketch<String> with ArrayOfStringsSerDe in Java.
// The length of each string is stored as a 32-bit integer (historically),
// which may be too wasteful. Treat this as an example.
template<>
struct serde<std::string> {
  void serialize(std::ostream& os, const std::string* items, unsigned num) {
    for (unsigned i = 0; i < num; i++) {
      uint32_t length = items[i].size();
      os.write((char*)&length, sizeof(length));
      os.write(items[i].c_str(), length);
    }
  }
  void deserialize(std::istream& is, std::string* items, unsigned num) {
    for (unsigned i = 0; i < num; i++) {
      uint32_t length;
      is.read((char*)&length, sizeof(length));
      new (&items[i]) std::string;
      items[i].reserve(length);
      auto it = std::istreambuf_iterator<char>(is);
      for (uint32_t j = 0; j < length; j++) {
        items[i].push_back(*it);
        ++it;
      }
    }
  }
  size_t size_of_item(const std::string& item) {
    return sizeof(uint32_t) + item.size();
  }
  size_t serialize(char* ptr, const std::string* items, unsigned num) {
    size_t size = sizeof(uint32_t) * num;
    for (unsigned i = 0; i < num; i++) {
      uint32_t length = items[i].size();
      memcpy(ptr, &length, sizeof(length));
      ptr += sizeof(uint32_t);
      memcpy(ptr, items[i].c_str(), length);
      ptr += length;
      size += length;
    }
    return size;
  }
  size_t deserialize(const char* ptr, std::string* items, unsigned num) {
    size_t size = sizeof(uint32_t) * num;
    for (unsigned i = 0; i < num; i++) {
      uint32_t length;
      memcpy(&length, ptr, sizeof(length));
      ptr += sizeof(uint32_t);
      new (&items[i]) std::string(ptr, length);
      ptr += length;
      size += length;
    }
    return size;
  }
};

} /* namespace datasketches */

# endif
