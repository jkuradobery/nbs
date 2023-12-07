#include "mkql_computation_node_ut.h"

#include <ydb/library/yql/minikql/computation/mkql_computation_node_holders.h>

namespace NKikimr {
namespace NMiniKQL {
Y_UNIT_TEST_SUITE(TMiniKQLBlocksTest) {
Y_UNIT_TEST(TestEmpty) {
    TSetup<false> setup;
    auto& pb = *setup.PgmBuilder;

    const auto type = pb.NewDataType(NUdf::TDataType<ui64>::Id);
    const auto list = pb.NewEmptyList(type);
    const auto sourceFlow = pb.ToFlow(list);
    const auto flowAfterBlocks = pb.FromBlocks(pb.ToBlocks(sourceFlow));
    const auto pgmReturn = pb.ForwardList(flowAfterBlocks);

    const auto graph = setup.BuildGraph(pgmReturn);
    const auto iterator = graph->GetValue().GetListIterator();
    NUdf::TUnboxedValue item;
    UNIT_ASSERT(!iterator.Next(item));
}

Y_UNIT_TEST(TestSimple) {
    static const size_t dataCount = 1000;
    TSetup<false> setup;
    auto& pb = *setup.PgmBuilder;

    auto data = TVector<TRuntimeNode>(Reserve(dataCount));
    for (size_t i = 0; i < dataCount; ++i) {
        data.push_back(pb.NewDataLiteral<ui64>(i));
    }
    const auto type = pb.NewDataType(NUdf::TDataType<ui64>::Id);
    const auto list = pb.NewList(type, data);
    const auto sourceFlow = pb.ToFlow(list);
    const auto flowAfterBlocks = pb.FromBlocks(pb.ToBlocks(sourceFlow));
    const auto pgmReturn = pb.ForwardList(flowAfterBlocks);

    const auto graph = setup.BuildGraph(pgmReturn);
    const auto iterator = graph->GetValue().GetListIterator();

    for (size_t i = 0; i < dataCount; ++i) {
        NUdf::TUnboxedValue item;
        UNIT_ASSERT(iterator.Next(item));
        UNIT_ASSERT_VALUES_EQUAL(item.Get<ui64>(), i);
    }
    NUdf::TUnboxedValue item;
    UNIT_ASSERT(!iterator.Next(item));
}

Y_UNIT_TEST(TestWideToBlocks) {
    TSetup<false> setup;
    auto& pb = *setup.PgmBuilder;

    const auto ui64Type = pb.NewDataType(NUdf::TDataType<ui64>::Id);
    const auto tupleType = pb.NewTupleType({ui64Type, ui64Type});

    const auto data1 = pb.NewTuple(tupleType, {pb.NewDataLiteral<ui64>(1), pb.NewDataLiteral<ui64>(10)});
    const auto data2 = pb.NewTuple(tupleType, {pb.NewDataLiteral<ui64>(2), pb.NewDataLiteral<ui64>(20)});
    const auto data3 = pb.NewTuple(tupleType, {pb.NewDataLiteral<ui64>(3), pb.NewDataLiteral<ui64>(30)});

    const auto list = pb.NewList(tupleType, {data1, data2, data3});
    const auto flow = pb.ToFlow(list);

    const auto wideFlow = pb.ExpandMap(flow, [&](TRuntimeNode item) -> TRuntimeNode::TList {
        return {pb.Nth(item, 0U), pb.Nth(item, 1U)};
    });
    const auto wideBlocksFlow = pb.WideToBlocks(wideFlow);
    const auto narrowBlocksFlow = pb.NarrowMap(wideBlocksFlow, [&](TRuntimeNode::TList items) -> TRuntimeNode {
        return items[1];
    });
    const auto narrowFlow = pb.FromBlocks(narrowBlocksFlow);
    const auto pgmReturn = pb.ForwardList(narrowFlow);

    const auto graph = setup.BuildGraph(pgmReturn);
    const auto iterator = graph->GetValue().GetListIterator();

    NUdf::TUnboxedValue item;
    UNIT_ASSERT(iterator.Next(item));
    UNIT_ASSERT_VALUES_EQUAL(item.Get<ui64>(), 10);

    UNIT_ASSERT(iterator.Next(item));
    UNIT_ASSERT_VALUES_EQUAL(item.Get<ui64>(), 20);

    UNIT_ASSERT(iterator.Next(item));
    UNIT_ASSERT_VALUES_EQUAL(item.Get<ui64>(), 30);
}

namespace {
void TestChunked(bool withBlockExpand) {
    TSetup<false> setup;
    auto& pb = *setup.PgmBuilder;

    const auto ui64Type   = pb.NewDataType(NUdf::TDataType<ui64>::Id);
    const auto boolType   = pb.NewDataType(NUdf::TDataType<bool>::Id);
    const auto stringType = pb.NewDataType(NUdf::EDataSlot::String);
    const auto utf8Type   = pb.NewDataType(NUdf::EDataSlot::Utf8);

    const auto tupleType = pb.NewTupleType({ui64Type, boolType, stringType, utf8Type});

    TVector<TRuntimeNode> items;
    const size_t bigStrSize = 1024 * 1024 + 100;
    const size_t smallStrSize = 256 * 1024;
    for (size_t i = 0; i < 20; ++i) {

        if (i % 2 == 0) {
            std::string big(bigStrSize, '0' + i);
            std::string small(smallStrSize, 'A' + i);

            items.push_back(pb.NewTuple(tupleType, { pb.NewDataLiteral<ui64>(i), pb.NewDataLiteral<bool>(true),
                                                     pb.NewDataLiteral<NUdf::EDataSlot::String>(big),
                                                     pb.NewDataLiteral<NUdf::EDataSlot::Utf8>(small),
                                                     }));
        } else {
            items.push_back(pb.NewTuple(tupleType, { pb.NewDataLiteral<ui64>(i), pb.NewDataLiteral<bool>(false),
                                                     pb.NewDataLiteral<NUdf::EDataSlot::String>(""),
                                                     pb.NewDataLiteral<NUdf::EDataSlot::Utf8>(""),
                                                     }));

        }
    }

    const auto list = pb.NewList(tupleType, std::move(items));

    auto node = pb.ToFlow(list);
    node = pb.ExpandMap(node, [&](TRuntimeNode item) -> TRuntimeNode::TList {
        return {pb.Nth(item, 0U), pb.Nth(item, 1U), pb.Nth(item, 2U), pb.Nth(item, 3U)};
    });
    node = pb.WideToBlocks(node);
    if (withBlockExpand) {
        node = pb.BlockExpandChunked(node);
        // WideTakeBlocks won't work on chunked blocks
        node = pb.WideTakeBlocks(node, pb.NewDataLiteral<ui64>(19));
        node = pb.WideFromBlocks(node);
    } else {
        // WideFromBlocks should support chunked blocks
        node = pb.WideFromBlocks(node);
        node = pb.Take(node, pb.NewDataLiteral<ui64>(19));
    }
    node = pb.NarrowMap(node, [&](TRuntimeNode::TList items) -> TRuntimeNode {
        return pb.NewTuple(tupleType, {items[0], items[1], items[2], items[3]});
    });

    const auto pgmReturn = pb.ForwardList(node);
    const auto graph = setup.BuildGraph(pgmReturn);
    const auto iterator = graph->GetValue().GetListIterator();

    for (size_t i = 0; i < 19; ++i) {
        NUdf::TUnboxedValue item;
        UNIT_ASSERT(iterator.Next(item));
        ui64 num = item.GetElement(0).Get<ui64>();
        bool bl = item.GetElement(1).Get<bool>();
        auto strVal = item.GetElement(2);
        auto utf8Val = item.GetElement(3);
        std::string_view str = strVal.AsStringRef();
        std::string_view utf8 = utf8Val.AsStringRef();

        UNIT_ASSERT_VALUES_EQUAL(num, i);
        UNIT_ASSERT_VALUES_EQUAL(bl, i % 2 == 0);
        if (i % 2 == 0) {
            std::string big(bigStrSize, '0' + i);
            std::string small(smallStrSize, 'A' + i);
            UNIT_ASSERT_VALUES_EQUAL(str, big);
            UNIT_ASSERT_VALUES_EQUAL(utf8, small);
        } else {
            UNIT_ASSERT_VALUES_EQUAL(str.size(), 0);
            UNIT_ASSERT_VALUES_EQUAL(utf8.size(), 0);
        }
    }

    NUdf::TUnboxedValue item;
    UNIT_ASSERT(!iterator.Next(item));
    UNIT_ASSERT(!iterator.Next(item));

}

} // namespace

Y_UNIT_TEST(TestBlockExpandChunked) {
    TestChunked(true);
}

Y_UNIT_TEST(TestWideFromBlocksForChunked) {
    TestChunked(false);
}

Y_UNIT_TEST(TestScalar) {
    const ui64 testValue = 42;

    TSetup<false> setup;
    auto& pb = *setup.PgmBuilder;

    auto dataLiteral = pb.NewDataLiteral<ui64>(testValue);
    const auto dataAfterBlocks = pb.AsScalar(dataLiteral);

    const auto graph = setup.BuildGraph(dataAfterBlocks);
    const auto value = graph->GetValue();
    UNIT_ASSERT(value.HasValue() && value.IsBoxed());
    UNIT_ASSERT_VALUES_EQUAL(TArrowBlock::From(value).GetDatum().scalar_as<arrow::UInt64Scalar>().value, testValue);
}

Y_UNIT_TEST(TestBlockFunc) {
    TSetup<false> setup;
    TProgramBuilder& pb = *setup.PgmBuilder;

    const auto ui64Type = pb.NewDataType(NUdf::TDataType<ui64>::Id);
    const auto tupleType = pb.NewTupleType({ui64Type, ui64Type});
    const auto ui64BlockType = pb.NewBlockType(ui64Type, TBlockType::EShape::Many);

    const auto data1 = pb.NewTuple(tupleType, {pb.NewDataLiteral<ui64>(1), pb.NewDataLiteral<ui64>(10)});
    const auto data2 = pb.NewTuple(tupleType, {pb.NewDataLiteral<ui64>(2), pb.NewDataLiteral<ui64>(20)});
    const auto data3 = pb.NewTuple(tupleType, {pb.NewDataLiteral<ui64>(3), pb.NewDataLiteral<ui64>(30)});

    const auto list = pb.NewList(tupleType, {data1, data2, data3});
    const auto flow = pb.ToFlow(list);

    const auto wideFlow = pb.ExpandMap(flow, [&](TRuntimeNode item) -> TRuntimeNode::TList {
        return {pb.Nth(item, 0U), pb.Nth(item, 1U)};
    });
    const auto wideBlocksFlow = pb.WideToBlocks(wideFlow);
    const auto sumWideFlow = pb.WideMap(wideBlocksFlow, [&](TRuntimeNode::TList items) -> TRuntimeNode::TList {
        return {pb.BlockFunc("Add", ui64BlockType, {items[0], items[1]})};
    });
    const auto sumNarrowFlow = pb.NarrowMap(sumWideFlow, [&](TRuntimeNode::TList items) -> TRuntimeNode {
        return items[0];
    });
    const auto pgmReturn = pb.Collect(pb.FromBlocks(sumNarrowFlow));

    const auto graph = setup.BuildGraph(pgmReturn);
    const auto iterator = graph->GetValue().GetListIterator();

    NUdf::TUnboxedValue item;
    UNIT_ASSERT(iterator.Next(item));
    UNIT_ASSERT_VALUES_EQUAL(item.Get<ui64>(), 11);

    UNIT_ASSERT(iterator.Next(item));
    UNIT_ASSERT_VALUES_EQUAL(item.Get<ui64>(), 22);

    UNIT_ASSERT(iterator.Next(item));
    UNIT_ASSERT_VALUES_EQUAL(item.Get<ui64>(), 33);
    UNIT_ASSERT(!iterator.Next(item));
    UNIT_ASSERT(!iterator.Next(item));
}

Y_UNIT_TEST(TestBlockFuncWithNullables) {
    TSetup<false> setup;
    TProgramBuilder& pb = *setup.PgmBuilder;

    const auto optionalUi64Type = pb.NewDataType(NUdf::TDataType<ui64>::Id, true);
    const auto tupleType = pb.NewTupleType({optionalUi64Type, optionalUi64Type});
    const auto emptyOptionalUi64 = pb.NewEmptyOptional(optionalUi64Type);
    const auto ui64OptBlockType = pb.NewBlockType(optionalUi64Type, TBlockType::EShape::Many);

    const auto data1 = pb.NewTuple(tupleType, {
        pb.NewOptional(pb.NewDataLiteral<ui64>(1)),
        emptyOptionalUi64
    });
    const auto data2 = pb.NewTuple(tupleType, {
        emptyOptionalUi64,
        pb.NewOptional(pb.NewDataLiteral<ui64>(20))
    });
    const auto data3 = pb.NewTuple(tupleType, {
        emptyOptionalUi64,
        emptyOptionalUi64
    });
    const auto data4 = pb.NewTuple(tupleType, {
        pb.NewOptional(pb.NewDataLiteral<ui64>(10)),
        pb.NewOptional(pb.NewDataLiteral<ui64>(20))
    });

    const auto list = pb.NewList(tupleType, {data1, data2, data3, data4});
    const auto flow = pb.ToFlow(list);

    const auto wideFlow = pb.ExpandMap(flow, [&](TRuntimeNode item) -> TRuntimeNode::TList {
        return {pb.Nth(item, 0U), pb.Nth(item, 1U)};
    });
    const auto wideBlocksFlow = pb.WideToBlocks(wideFlow);
    const auto sumWideFlow = pb.WideMap(wideBlocksFlow, [&](TRuntimeNode::TList items) -> TRuntimeNode::TList {
        return {pb.BlockFunc("Add", ui64OptBlockType, {items[0], items[1]})};
    });
    const auto sumNarrowFlow = pb.NarrowMap(sumWideFlow, [&](TRuntimeNode::TList items) -> TRuntimeNode {
        return items[0];
    });
    const auto pgmReturn = pb.Collect(pb.FromBlocks(sumNarrowFlow));

    const auto graph = setup.BuildGraph(pgmReturn);
    const auto iterator = graph->GetValue().GetListIterator();

    NUdf::TUnboxedValue item;
    UNIT_ASSERT(iterator.Next(item));
    UNIT_ASSERT(!item);

    UNIT_ASSERT(iterator.Next(item));
    UNIT_ASSERT(!item);

    UNIT_ASSERT(iterator.Next(item));
    UNIT_ASSERT(!item);

    UNIT_ASSERT(iterator.Next(item));
    UNIT_ASSERT_VALUES_EQUAL(item.Get<ui64>(), 30);
    UNIT_ASSERT(!iterator.Next(item));
}

Y_UNIT_TEST(TestBlockFuncWithNullableScalar) {
    TSetup<false> setup;
    TProgramBuilder& pb = *setup.PgmBuilder;

    const auto optionalUi64Type = pb.NewDataType(NUdf::TDataType<ui64>::Id, true);
    const auto ui64OptBlockType = pb.NewBlockType(optionalUi64Type, TBlockType::EShape::Many);
    const auto emptyOptionalUi64 = pb.NewEmptyOptional(optionalUi64Type);

    const auto list = pb.NewList(optionalUi64Type, {
        pb.NewOptional(pb.NewDataLiteral<ui64>(10)),
        pb.NewOptional(pb.NewDataLiteral<ui64>(20)),
        pb.NewOptional(pb.NewDataLiteral<ui64>(30))
    });
    const auto flow = pb.ToFlow(list);
    const auto blocksFlow = pb.ToBlocks(flow);

    THolder<IComputationGraph> graph;
    auto map = [&](const TProgramBuilder::TUnaryLambda& func) {
        const auto pgmReturn = pb.Collect(pb.FromBlocks(pb.Map(blocksFlow, func)));
        graph = setup.BuildGraph(pgmReturn);
        return graph->GetValue().GetListIterator();
    };

    {
        const auto scalar = pb.AsScalar(emptyOptionalUi64);
        auto iterator = map([&](TRuntimeNode item) -> TRuntimeNode {
            return {pb.BlockFunc("Add", ui64OptBlockType, {scalar, item})};
        });

        NUdf::TUnboxedValue item;
        UNIT_ASSERT(iterator.Next(item));
        UNIT_ASSERT(!item);

        UNIT_ASSERT(iterator.Next(item));
        UNIT_ASSERT(!item);

        UNIT_ASSERT(iterator.Next(item));
        UNIT_ASSERT(!item);

        UNIT_ASSERT(!iterator.Next(item));
    }

    {
        const auto scalar = pb.AsScalar(emptyOptionalUi64);
        auto iterator = map([&](TRuntimeNode item) -> TRuntimeNode {
            return {pb.BlockFunc("Add", ui64OptBlockType, {item, scalar})};
        });

        NUdf::TUnboxedValue item;
        UNIT_ASSERT(iterator.Next(item));
        UNIT_ASSERT(!item);

        UNIT_ASSERT(iterator.Next(item));
        UNIT_ASSERT(!item);

        UNIT_ASSERT(iterator.Next(item));
        UNIT_ASSERT(!item);

        UNIT_ASSERT(!iterator.Next(item));
    }

    {
        const auto scalar = pb.AsScalar(pb.NewDataLiteral<ui64>(100));
        auto iterator = map([&](TRuntimeNode item) -> TRuntimeNode {
            return {pb.BlockFunc("Add", ui64OptBlockType, {item, scalar})};
        });

        NUdf::TUnboxedValue item;
        UNIT_ASSERT(iterator.Next(item));
        UNIT_ASSERT_VALUES_EQUAL(item.Get<ui64>(), 110);

        UNIT_ASSERT(iterator.Next(item));
        UNIT_ASSERT_VALUES_EQUAL(item.Get<ui64>(), 120);

        UNIT_ASSERT(iterator.Next(item));
        UNIT_ASSERT_VALUES_EQUAL(item.Get<ui64>(), 130);

        UNIT_ASSERT(!iterator.Next(item));
    }
}

Y_UNIT_TEST(TestBlockFuncWithScalar) {
    TSetup<false> setup;
    TProgramBuilder& pb = *setup.PgmBuilder;

    const auto ui64Type = pb.NewDataType(NUdf::TDataType<ui64>::Id);
    const auto ui64BlockType = pb.NewBlockType(ui64Type, TBlockType::EShape::Many);

    const auto data1 = pb.NewDataLiteral<ui64>(10);
    const auto data2 = pb.NewDataLiteral<ui64>(20);
    const auto data3 = pb.NewDataLiteral<ui64>(30);
    const auto rightScalar = pb.AsScalar(pb.NewDataLiteral<ui64>(100));
    const auto leftScalar = pb.AsScalar(pb.NewDataLiteral<ui64>(1000));

    const auto list = pb.NewList(ui64Type, {data1, data2, data3});
    const auto flow = pb.ToFlow(list);
    const auto blocksFlow = pb.ToBlocks(flow);
    const auto sumBlocksFlow = pb.Map(blocksFlow, [&](TRuntimeNode item) -> TRuntimeNode {
        return {pb.BlockFunc("Add", ui64BlockType, { leftScalar, {pb.BlockFunc("Add", ui64BlockType, { item, rightScalar } )}})};
    });
    const auto pgmReturn = pb.Collect(pb.FromBlocks(sumBlocksFlow));

    const auto graph = setup.BuildGraph(pgmReturn);
    const auto iterator = graph->GetValue().GetListIterator();

    NUdf::TUnboxedValue item;
    UNIT_ASSERT(iterator.Next(item));
    UNIT_ASSERT_VALUES_EQUAL(item.Get<ui64>(), 1110);

    UNIT_ASSERT(iterator.Next(item));
    UNIT_ASSERT_VALUES_EQUAL(item.Get<ui64>(), 1120);

    UNIT_ASSERT(iterator.Next(item));
    UNIT_ASSERT_VALUES_EQUAL(item.Get<ui64>(), 1130);
    UNIT_ASSERT(!iterator.Next(item));
    UNIT_ASSERT(!iterator.Next(item));
}

Y_UNIT_TEST(TestWideFromBlocks) {
    TSetup<false> setup;
    auto& pb = *setup.PgmBuilder;

    const auto ui64Type = pb.NewDataType(NUdf::TDataType<ui64>::Id);
    const auto tupleType = pb.NewTupleType({ui64Type, ui64Type});

    const auto data1 = pb.NewTuple(tupleType, {pb.NewDataLiteral<ui64>(1), pb.NewDataLiteral<ui64>(10)});
    const auto data2 = pb.NewTuple(tupleType, {pb.NewDataLiteral<ui64>(2), pb.NewDataLiteral<ui64>(20)});
    const auto data3 = pb.NewTuple(tupleType, {pb.NewDataLiteral<ui64>(3), pb.NewDataLiteral<ui64>(30)});

    const auto list = pb.NewList(tupleType, {data1, data2, data3});
    const auto flow = pb.ToFlow(list);

    const auto wideFlow = pb.ExpandMap(flow, [&](TRuntimeNode item) -> TRuntimeNode::TList {
        return {pb.Nth(item, 0U), pb.Nth(item, 1U)};
    });
    const auto wideBlocksFlow = pb.WideToBlocks(wideFlow);
    const auto wideFlow2 = pb.WideFromBlocks(wideBlocksFlow);
    const auto narrowFlow = pb.NarrowMap(wideFlow2, [&](TRuntimeNode::TList items) -> TRuntimeNode {
        return items[1];
    });

    const auto pgmReturn = pb.ForwardList(narrowFlow);

    const auto graph = setup.BuildGraph(pgmReturn);
    const auto iterator = graph->GetValue().GetListIterator();

    NUdf::TUnboxedValue item;
    UNIT_ASSERT(iterator.Next(item));
    UNIT_ASSERT_VALUES_EQUAL(item.Get<ui64>(), 10);

    UNIT_ASSERT(iterator.Next(item));
    UNIT_ASSERT_VALUES_EQUAL(item.Get<ui64>(), 20);

    UNIT_ASSERT(iterator.Next(item));
    UNIT_ASSERT_VALUES_EQUAL(item.Get<ui64>(), 30);
}


}

}
}
