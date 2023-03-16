//
// Created by jinhai on 23-3-16.
//

#pragma once

#include "storage/table_def.h"
#include "executor/physical_operator.h"

#include <memory>

namespace infinity {

class PhysicalCreateCollection : public PhysicalOperator {
public:
    explicit
    PhysicalCreateCollection(SharedPtr<String> schema_name,
                             SharedPtr<String> collection_name,
                             ConflictType conflict_type,
                             u64 table_index,
                             u64 id);

    ~PhysicalCreateCollection() override = default;

    void
    Init() override;

    void
    Execute(SharedPtr<QueryContext>& query_context) override;

    inline u64
    table_index() const {
        return table_index_;
    }

private:
    SharedPtr<String> schema_name_{};
    SharedPtr<String> collection_name_{};
    ConflictType conflict_type_{};
    u64 table_index_{};

};

}
