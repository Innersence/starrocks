// This file is licensed under the Elastic License 2.0. Copyright 2021 StarRocks Limited.
package com.starrocks.sql.analyzer.relation;

import com.starrocks.sql.optimizer.base.SetQualifier;

import java.util.List;

public class ExceptRelation extends SetOperationRelation {

    public ExceptRelation(List<QueryRelation> relations, SetQualifier qualifier) {
        super(relations, qualifier);
    }

    public <R, C> R accept(RelationVisitor<R, C> visitor, C context) {
        return visitor.visitExcept(this, context);
    }
}

