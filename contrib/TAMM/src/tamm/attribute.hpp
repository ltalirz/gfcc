#pragma once

#include "tamm/range.hpp"
#include "tamm/types.hpp"
#include <algorithm>
#include <map>
#include <vector>
#include <functional>

namespace tamm {
/**
 * @class Attribute
 * @brief Attribute definition which will be used for representing
 *        Spin, Spatial and any other attributes required.
 *
 * @todo Possibly move to a separate header file
 *
 * @tparam T is an Attribute type (e.g. Spin, Spatial)
 */
template<typename T>
class Attribute {
public:
    /**
     * @brief Construct a new Attribute object using default implementation
     *
     */
    Attribute() = default;

    /**
     * @brief Construct a new Attribute object using the map from attribute
     * values to set of ranges
     *
     * @param [in] attr_map a map from attribute values to vector of Ranges
     */
    Attribute(const AttributeToRangeMap<T>& attr_map) : attr_map_{attr_map} {}

    Attribute(const Attribute&) = default;
    Attribute(Attribute&&)      = default;
    Attribute& operator=(const Attribute&) = default;
    Attribute& operator=(Attribute&&) = default;

    ~Attribute() = default;

    /**
     * @brief Given an Index, ind, user can query
     *        associated Attribute value
     *
     * @param [in] idx Index argument
     * @returns the associated Attribute value
     */
    T operator()(Index idx) const {
        for(const auto& kv : attr_map_) {
            for(const auto& range : kv.second) {
                if(range.contains(idx)) { return kv.first; }
            }
        }

        // return default attribute value if not found
        // assumption for default attribute -> {0}
        return T{0};
    }

    /**
     * @brief Accessor to the map from attribute values to set of ranges
     *
     * @param [in] att input Attribute value being searched
     * @returns Range vector for the corresponding attribute value
     *          (empty vector if it doesn't exist)
     */
    const std::vector<Range>& attribute_range(T att) const {
        return ((attr_map_.find(att) == attr_map_.end()) ? empty_range_ :
                                                           attr_map_.at(att));
    }

    /**
     * @brief Getting the iterators for Index vectors
     *        for a given Attribute value (e.g. Spin{1})
     *
     * @param [in] val an Attribute value as an argument
     * @returns std::vector<Range>::const_iterator returns beginning
     * iterator for associated ranges for the attribute
     */
    std::vector<Range>::const_iterator attribute_begin(const T& val) const {
        return attr_map_[val].begin();
    }

    /**
     * @brief Getting the iterators for Index vectors
     *        for a given Attribute value (e.g. Spin{1})
     *
     * @param [in] val an Attribute value as an argument
     * @returns std::vector<Range>::const_iterator returns end
     * iterator for associated ranges for the attribute
     */
    std::vector<Range>::const_iterator attribute_end(const T& val) const {
        return attr_map_[val].end();
    }

    const AttributeToRangeMap<T>& get_map() const { return attr_map_; }

    /**
     * @brief Check if the attribute relations are empty
     *
     * @returns empty
     */
    bool empty() const { return attr_map_.find(T{0}) != attr_map_.end(); }

protected:
    AttributeToRangeMap<T> attr_map_; //
    std::vector<Range> empty_range_;
}; // Attribute
using SpinAttribute    = Attribute<Spin>;
using SpatialAttribute = Attribute<Spatial>;

} // namespace tamm

namespace std {
template <>
struct hash<tamm::SpinAttribute> {
    typedef tamm::SpinAttribute argument_type;
    typedef std::size_t result_type;
    result_type operator()(argument_type const& attribute) const noexcept {
        using tamm::internal::hash_combine;
        const auto& attr_map = attribute.get_map();
        result_type result = attr_map.size();
        
        for(const auto& att_ranges : attr_map) {
            hash_combine(result, att_ranges.first.value());
            hash_combine(result, att_ranges.second.size());
            for(const auto& range : att_ranges.second) {
                hash_combine(result, range);
            }
        }

        return result;
    }
};

template <>
struct hash<tamm::SpatialAttribute> {
    typedef tamm::SpatialAttribute argument_type;
    typedef std::size_t result_type;
    result_type operator()(argument_type const& attribute) const noexcept {
        using tamm::internal::hash_combine;
        const auto& attr_map = attribute.get_map();
        result_type result = attr_map.size();
        
        for(const auto& att_ranges : attr_map) {
            hash_combine(result, att_ranges.first.value());
            hash_combine(result, att_ranges.second.size());
            for(const auto& range : att_ranges.second) {
                hash_combine(result, range);
            }
        }

        return result;
    }
};

} // namespace std

