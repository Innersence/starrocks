// This file is licensed under the Elastic License 2.0. Copyright 2021 StarRocks Limited.

#pragma once

#include "column/nullable_column.h"
#include "storage/vectorized/column_predicate.h"

namespace starrocks::vectorized {
// Implementing a complete ColumnPredicate is very difficult, most of the ColumnPredicate logic is similar,
// we just need to implement similar apply operation can be more convenient to implement a new ColumnPredicate.

template <FieldType field_type, class ColumnType, template <FieldType> class ColumnOperator, typename... Args>
class ColumnOperatorPredicate final : public ColumnPredicate {
public:
    using SpecColumnOperator = ColumnOperator<field_type>;
    ColumnOperatorPredicate(const ColumnOperatorPredicate&) = delete;
    ColumnOperatorPredicate(const TypeInfoPtr& type_info, ColumnId id, Args... args)
            : ColumnPredicate(type_info, id), _predicate_operator(std::forward<Args>(args)...) {}

    void evaluate(const Column* column, uint8_t* sel, uint16_t from, uint16_t to) const override {
        // get raw column
        const ColumnType* lowcard_column;
        if (column->is_nullable()) {
            // This is NullableColumn, get its data_column
            lowcard_column =
                    down_cast<const ColumnType*>(down_cast<const NullableColumn*>(column)->data_column().get());
        } else {
            lowcard_column = down_cast<const ColumnType*>(column);
        }
        if (!column->has_null()) {
            for (size_t i = from; i < to; i++) {
                sel[i] = _predicate_operator.eval_at(lowcard_column, i);
            }
        } else {
            /* must use uint8_t* to make vectorized effect */
            const uint8_t* null_data = down_cast<const NullableColumn*>(column)->immutable_null_column_data().data();
            for (size_t i = from; i < to; i++) {
                sel[i] = !null_data[i] && _predicate_operator.eval_at(lowcard_column, i);
            }
        }
    }

    void evaluate_and(const Column* column, uint8_t* sel, uint16_t from, uint16_t to) const override {
        // get raw column
        const ColumnType* lowcard_column;
        if (column->is_nullable()) {
            // This is NullableColumn, get its data_column
            lowcard_column =
                    down_cast<const ColumnType*>(down_cast<const NullableColumn*>(column)->data_column().get());
        } else {
            lowcard_column = down_cast<const ColumnType*>(column);
        }
        if (!column->has_null()) {
            for (size_t i = from; i < to; i++) {
                sel[i] = (sel[i] && _predicate_operator.eval_at(lowcard_column, i));
            }
        } else {
            /* must use uint8_t* to make vectorized effect */
            const uint8_t* null_data = down_cast<const NullableColumn*>(column)->immutable_null_column_data().data();
            for (size_t i = from; i < to; i++) {
                sel[i] = (sel[i] && !null_data[i] && _predicate_operator.eval_at(lowcard_column, i));
            }
        }
    }

    void evaluate_or(const Column* column, uint8_t* sel, uint16_t from, uint16_t to) const override {
        // get raw column
        const ColumnType* lowcard_column;
        if (column->is_nullable()) {
            // This is NullableColumn, get its data_column
            lowcard_column =
                    down_cast<const ColumnType*>(down_cast<const NullableColumn*>(column)->data_column().get());
        } else {
            lowcard_column = down_cast<const ColumnType*>(column);
        }
        if (!column->has_null()) {
            for (size_t i = from; i < to; i++) {
                sel[i] = (sel[i] || _predicate_operator.eval_at(lowcard_column, i));
            }
        } else {
            /* must use uint8_t* to make vectorized effect */
            const uint8_t* null_data = down_cast<const NullableColumn*>(column)->immutable_null_column_data().data();
            for (size_t i = from; i < to; i++) {
                sel[i] = (sel[i] || (!null_data[i] && _predicate_operator.eval_at(lowcard_column, i)));
            }
        }
    }

    uint16_t evaluate_branchless(const Column* column, uint16_t* sel, uint16_t sel_size) const override {
        // Get BinaryColumn
        const ColumnType* lowcard_column;
        if (column->is_nullable()) {
            // This is NullableColumn, get its data_column
            lowcard_column =
                    down_cast<const ColumnType*>(down_cast<const NullableColumn*>(column)->data_column().get());
        } else {
            lowcard_column = down_cast<const ColumnType*>(column);
        }

        uint16_t new_size = 0;
        if (!column->has_null()) {
            for (uint16_t i = 0; i < sel_size; ++i) {
                uint16_t data_idx = sel[i];
                sel[new_size] = data_idx;
                new_size += _predicate_operator.eval_at(lowcard_column, i);
            }
        } else {
            /* must use uint8_t* to make vectorized effect */
            const uint8_t* null_data = down_cast<const NullableColumn*>(column)->immutable_null_column_data().data();
            for (uint16_t i = 0; i < sel_size; ++i) {
                uint16_t data_idx = sel[i];
                sel[new_size] = data_idx;
                new_size += !null_data[data_idx] && _predicate_operator.eval_at(lowcard_column, i);
            }
        }
        return new_size;
    }

    bool zone_map_filter(const ZoneMapDetail& detail) const override {
        return _predicate_operator.zone_map_filter(detail);
    }

    PredicateType type() const override { return SpecColumnOperator::type(); }

    Datum value() const override { return _predicate_operator.value(); }

    std::vector<Datum> values() const override { return _predicate_operator.values(); }

    bool can_vectorized() const override { return SpecColumnOperator::can_vectorized(); }

    Status seek_bitmap_dictionary(BitmapIndexIterator* iter, SparseRange* range) const override {
        return _predicate_operator.seek_bitmap_dictionary(iter, range);
    }

    bool support_bloom_filter() const override { return SpecColumnOperator::support_bloom_filter(); }

    bool bloom_filter(const BloomFilter* bf) const override {
        DCHECK(support_bloom_filter()) << "Not support bloom filter";
        if constexpr (SpecColumnOperator::support_bloom_filter()) {
            return _predicate_operator.bloom_filter(bf);
        }
        return true;
    }

    Status convert_to(const ColumnPredicate** output, const TypeInfoPtr& target_type_info,
                      ObjectPool* obj_pool) const override {
        return _predicate_operator.convert_to(output, target_type_info, obj_pool);
    }

    std::string debug_string() const override { return _predicate_operator.debug_string(); }

    bool padding_zeros(size_t len) override { return _predicate_operator.padding_zeros(len); }

private:
    SpecColumnOperator _predicate_operator;
};
} // namespace starrocks::vectorized