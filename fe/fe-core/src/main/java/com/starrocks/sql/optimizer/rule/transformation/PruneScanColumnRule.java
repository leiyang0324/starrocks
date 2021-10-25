// This file is licensed under the Elastic License 2.0. Copyright 2021 StarRocks Limited.

package com.starrocks.sql.optimizer.rule.transformation;

import avro.shaded.com.google.common.base.Preconditions;
import com.google.common.collect.Lists;
import com.starrocks.catalog.Column;
import com.starrocks.catalog.Table;
import com.starrocks.catalog.Type;
import com.starrocks.sql.common.ErrorType;
import com.starrocks.sql.common.StarRocksPlannerException;
import com.starrocks.sql.optimizer.OptExpression;
import com.starrocks.sql.optimizer.OptimizerContext;
import com.starrocks.sql.optimizer.Utils;
import com.starrocks.sql.optimizer.base.ColumnRefSet;
import com.starrocks.sql.optimizer.operator.OperatorType;
import com.starrocks.sql.optimizer.operator.Projection;
import com.starrocks.sql.optimizer.operator.logical.LogicalOlapScanOperator;
import com.starrocks.sql.optimizer.operator.logical.LogicalScanOperator;
import com.starrocks.sql.optimizer.operator.pattern.Pattern;
import com.starrocks.sql.optimizer.operator.scalar.ColumnRefOperator;
import com.starrocks.sql.optimizer.operator.scalar.ScalarOperator;
import com.starrocks.sql.optimizer.rule.RuleType;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;

import static java.util.function.Function.identity;

public class PruneScanColumnRule extends TransformationRule {
    public static final PruneScanColumnRule OLAP_SCAN = new PruneScanColumnRule(OperatorType.LOGICAL_OLAP_SCAN);
    public static final PruneScanColumnRule SCHEMA_SCAN = new PruneScanColumnRule(OperatorType.LOGICAL_SCHEMA_SCAN);
    public static final PruneScanColumnRule MYSQL_SCAN = new PruneScanColumnRule(OperatorType.LOGICAL_MYSQL_SCAN);
    public static final PruneScanColumnRule ES_SCAN = new PruneScanColumnRule(OperatorType.LOGICAL_ES_SCAN);

    public PruneScanColumnRule(OperatorType logicalOperatorType) {
        super(RuleType.TF_PRUNE_OLAP_SCAN_COLUMNS, Pattern.create(logicalOperatorType));
    }

    @Override
    public List<OptExpression> transform(OptExpression input, OptimizerContext context) {
        LogicalScanOperator scanOperator = (LogicalScanOperator) input.getOp();
        ColumnRefSet requiredOutputColumns = context.getTaskContext().get(0).getRequiredColumns();

        // The `outputColumns`s are some columns required but not specified by `requiredOutputColumns`.
        // including columns in predicate or some specialized columns defined by scan operator.
        Set<ColumnRefOperator> outputColumns =
                scanOperator.getColRefToColumnMetaMap().keySet().stream().filter(requiredOutputColumns::contains)
                        .collect(Collectors.toSet());
        outputColumns.addAll(Utils.extractColumnRef(scanOperator.getPredicate()));

        if (outputColumns.size() == 0) {
            List<ColumnRefOperator> columnRefOperatorList =
                    new ArrayList<>(scanOperator.getColRefToColumnMetaMap().keySet());

            int smallestIndex = -1;
            int smallestColumnLength = Integer.MAX_VALUE;
            for (int index = 0; index < columnRefOperatorList.size(); ++index) {
                if (smallestIndex == -1) {
                    smallestIndex = index;
                }
                Type columnType = columnRefOperatorList.get(index).getType();
                if (columnType.isScalarType()) {
                    int columnLength = columnType.getSlotSize();
                    if (columnLength < smallestColumnLength) {
                        smallestIndex = index;
                        smallestColumnLength = columnLength;
                    }
                }
            }
            Preconditions.checkArgument(smallestIndex != -1);
            outputColumns.add(columnRefOperatorList.get(smallestIndex));
        }

        if (scanOperator.getColRefToColumnMetaMap().keySet().equals(outputColumns)) {
            return Collections.emptyList();
        } else {
            Map<ColumnRefOperator, Column> newColumnRefMap = outputColumns.stream()
                    .collect(Collectors.toMap(identity(), scanOperator.getColRefToColumnMetaMap()::get));
            if (scanOperator instanceof LogicalOlapScanOperator) {
                LogicalOlapScanOperator olapScanOperator = (LogicalOlapScanOperator) scanOperator;
                LogicalOlapScanOperator newScanOperator = new LogicalOlapScanOperator(
                        olapScanOperator.getTable(),
                        newColumnRefMap,
                        olapScanOperator.getColumnMetaToColRefMap(),
                        olapScanOperator.getDistributionSpec(),
                        olapScanOperator.getLimit(),
                        olapScanOperator.getPredicate(),
                        olapScanOperator.getSelectedIndexId(),
                        olapScanOperator.getSelectedPartitionId(),
                        olapScanOperator.getPartitionNames(),
                        olapScanOperator.getSelectedTabletId(),
                        olapScanOperator.getHintsTabletIds());

                return Lists.newArrayList(new OptExpression(newScanOperator));
            } else {
                try {
                    Class<? extends LogicalScanOperator> classType = scanOperator.getClass();
                    LogicalScanOperator newScanOperator =
                            classType.getConstructor(Table.class, Map.class, Map.class, long.class,
                                    ScalarOperator.class, Projection.class).newInstance(
                                    scanOperator.getTable(),
                                    newColumnRefMap,
                                    scanOperator.getColumnMetaToColRefMap(),
                                    scanOperator.getLimit(),
                                    scanOperator.getPredicate(),
                                    scanOperator.getProjection());

                    return Lists.newArrayList(new OptExpression(newScanOperator));
                } catch (Exception e) {
                    throw new StarRocksPlannerException(e.getMessage(), ErrorType.INTERNAL_ERROR);
                }
            }
        }
    }
}