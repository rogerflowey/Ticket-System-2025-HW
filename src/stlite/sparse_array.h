#include <limits>
#include <cstddef>

template <typename T>
struct DefaultCompareTriviallyCopyable {
    constexpr bool operator()(const T& a, const T& b) const {
        return a < b;
    }
};

template <
    typename IndexType,
    typename ValueType,
    ValueType DefaultVal,
    typename CompareFunc,
    size_t MaxNumSegments
>
class SparseArray {
public:
    struct Segment {
        IndexType end_idx;
        ValueType value;

        constexpr Segment() : end_idx{}, value{} {}
        constexpr Segment(IndexType e, ValueType v) : end_idx(e), value(v) {}

        bool operator==(const Segment& other) const {
            return end_idx == other.end_idx && value == other.value;
        }
    };

private:
    std::array<Segment, MaxNumSegments> segments_array_;
    size_t current_num_segments_;
    CompareFunc compare_;

    static void manual_sort_points(IndexType* begin, IndexType* end) {
        if (begin == end || begin + 1 == end) return;
        for (IndexType* i = begin + 1; i != end; ++i) {
            IndexType key = *i;
            IndexType* j = i - 1;
            while (j >= begin && *j > key) {
                *(j + 1) = *j;
                if (j == begin) {
                    j = nullptr;
                    break;
                }
                --j;
            }
            if (j) {
                 *(j + 1) = key;
            } else {
                 *begin = key;
            }
        }
    }

    static IndexType* manual_unique_points(IndexType* begin, IndexType* end) {
        if (begin == end) return begin;
        IndexType* result = begin;
        for (IndexType* current = begin + 1; current != end; ++current) {
            if (!(*result == *current)) {
                ++result;
                if (result != current) {
                    *result = *current;
                }
            }
        }
        return result + 1;
    }

public:
    constexpr SparseArray()
        : segments_array_{}, current_num_segments_(0), compare_() {}

    ValueType get_value_at(IndexType idx) const {
        for (size_t i = 0; i < current_num_segments_; ++i) {
            const auto& seg = segments_array_[i];
            IndexType actual_segment_start = (i == 0) ? IndexType{0} : (segments_array_[i-1].end_idx + 1);

            if (idx >= actual_segment_start && idx <= seg.end_idx) {
                return seg.value;
            }
            if (idx < actual_segment_start) {
                return DefaultVal;
            }
        }
        return DefaultVal;
    }

    bool add_range(IndexType range_start, IndexType range_end, ValueType new_value) {
        if (range_start > range_end) {
            return true;
        }

        constexpr size_t MaxCriticalPointsArraySize = 2 * MaxNumSegments + 4;
        std::array<IndexType, MaxCriticalPointsArraySize> critical_points_storage;
        IndexType* critical_points = critical_points_storage.data();
        size_t num_critical_points = 0;

        auto add_point = [&](IndexType p) {
            if (num_critical_points < MaxCriticalPointsArraySize) {
                critical_points[num_critical_points++] = p;
            }
        };

        add_point(IndexType{0});
        add_point(range_start);
        if (range_end < std::numeric_limits<IndexType>::max()) {
            add_point(range_end + 1);
        } else {
            add_point(range_end);
        }

        for (size_t i = 0; i < current_num_segments_; ++i) {
            const auto& seg = segments_array_[i];
            IndexType current_segment_actual_start = (i == 0) ? IndexType{0} : (segments_array_[i-1].end_idx + 1);

            add_point(current_segment_actual_start);
            if (seg.end_idx < std::numeric_limits<IndexType>::max()) {
                add_point(seg.end_idx + 1);
            } else {
                add_point(seg.end_idx);
            }
        }

        manual_sort_points(critical_points, critical_points + num_critical_points);
        IndexType* last_unique_ptr = manual_unique_points(critical_points, critical_points + num_critical_points);
        num_critical_points = std::distance(critical_points, last_unique_ptr);

        std::array<Segment, MaxNumSegments> next_segments_array_temp;
        size_t num_next_segments = 0;

        for (size_t i = 0; i + 1 < num_critical_points; ++i) {
            IndexType p_start = critical_points[i];
            IndexType p_end = critical_points[i+1] - 1;

            if (p_start > p_end) {
                continue;
            }

            IndexType query_point = p_start;
            ValueType val_from_old_segments = get_value_at(query_point);

            ValueType final_value_for_interval;
            bool is_in_new_range = (p_start <= range_end && p_end >= range_start);

            if (is_in_new_range) {
                final_value_for_interval = compare_(new_value, val_from_old_segments) ? new_value : val_from_old_segments;
            } else {
                final_value_for_interval = val_from_old_segments;
            }

            if (final_value_for_interval != DefaultVal) {
                if (num_next_segments > 0 && next_segments_array_temp[num_next_segments - 1].value == final_value_for_interval) {
                    next_segments_array_temp[num_next_segments - 1].end_idx = p_end;
                } else {
                    if (num_next_segments < MaxNumSegments) {
                        next_segments_array_temp[num_next_segments++] = Segment{p_end, final_value_for_interval};
                    } else {
                        return false;
                    }
                }
            }
        }

        segments_array_ = next_segments_array_temp;
        current_num_segments_ = num_next_segments;
        return true;
    }

    const Segment* get_segments_data() const {
        return segments_array_.data();
    }

    size_t get_num_segments() const {
        return current_num_segments_;
    }
};