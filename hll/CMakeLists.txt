add_library(hll OBJECT "")

add_library(${PROJECT_NAME}::HLL ALIAS hll)

if (BUILD_TESTS)
  add_subdirectory(test)
endif()

target_include_directories(hll
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>
  PRIVATE 
    ${COMMON_INCLUDE_DIR}
)

target_link_libraries(hll common)

set_target_properties(hll PROPERTIES
  PUBLIC_HEADER "include/hll.hpp"
  POSITION_INDEPENDENT_CODE ON
  CXX_STANDARD 11
  CXX_STANDARD_REQUIRED YES
)

install(TARGETS hll
  EXPORT ${PROJCT_NAME}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DataSketches
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DataSketches
)

target_sources(hll
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/hll.hpp
  PRIVATE
    src/AuxHashMap.cpp
    src/CompositeInterpolationXTable.cpp
    src/Conversions.cpp
    src/CouponHashSet.cpp
    src/CouponList.cpp
    src/CubicInterpolation.cpp
    src/HarmonicNumbers.cpp
    src/Hll4Array.cpp
    src/Hll6Array.cpp
    src/Hll8Array.cpp
    src/HllArray.cpp
    src/HllPairIterator.cpp
    src/HllSketch.cpp
    src/HllSketchImpl.cpp
    src/HllUnion.cpp
    src/HllUtil.cpp
    src/IntArrayPairIterator.cpp
    src/RelativeErrorTables.cpp
    # headers
    include/AuxHashMap.hpp
    include/CompositeInterpolationXTable.hpp
    include/Conversions.hpp
    include/CouponHashSet.hpp
    include/CouponList.hpp
    include/CubicInterpolation.hpp
    include/HarmonicNumbers.hpp
    include/Hll4Array.hpp
    include/Hll6Array.hpp
    include/Hll8Array.hpp
    include/HllArray.hpp
    include/HllPairIterator.hpp
    include/HllSketch.hpp
    include/HllSketchImpl.hpp
    include/HllUnion.hpp
    include/HllUtil.hpp
    include/IntArrayPairIterator.hpp
    include/PairIterator.hpp
    include/RelativeErrorTables.hpp
)
