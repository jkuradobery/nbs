LIBRARY()

SRCS(
    mkql_addmember.cpp
    mkql_addmember.h
    mkql_aggrcount.cpp
    mkql_aggrcount.h
    mkql_append.cpp
    mkql_append.h
    mkql_apply.cpp
    mkql_apply.h
    mkql_block_agg.cpp
    mkql_block_agg.h
    mkql_block_agg_count.cpp
    mkql_block_agg_count.h
    mkql_block_agg_factory.cpp
    mkql_block_agg_factory.h
    mkql_block_agg_minmax.cpp
    mkql_block_agg_minmax.h
    mkql_block_agg_sum.cpp
    mkql_block_agg_sum.h
    mkql_block_builder.cpp
    mkql_block_builder.h
    mkql_block_coalesce.cpp
    mkql_block_coalesce.h
    mkql_block_if.cpp
    mkql_block_if.h
    mkql_block_impl.cpp
    mkql_block_impl.h
    mkql_block_logical.cpp
    mkql_block_logical.h
    mkql_block_compress.cpp
    mkql_block_compress.h
    mkql_block_func.cpp
    mkql_block_func.h
    mkql_block_reader.cpp
    mkql_block_reader.h
    mkql_block_skiptake.cpp
    mkql_block_skiptake.h
    mkql_blocks.cpp
    mkql_blocks.h
    mkql_callable.cpp
    mkql_callable.h
    mkql_chain_map.cpp
    mkql_chain_map.h
    mkql_chain1_map.cpp
    mkql_chain1_map.h
    mkql_check_args.cpp
    mkql_check_args.h
    mkql_chopper.cpp
    mkql_chopper.h
    mkql_coalesce.cpp
    mkql_coalesce.h
    mkql_collect.cpp
    mkql_collect.h
    mkql_combine.cpp
    mkql_combine.h
    mkql_contains.cpp
    mkql_contains.h
    mkql_decimal_div.cpp
    mkql_decimal_div.h
    mkql_decimal_mod.cpp
    mkql_decimal_mod.h
    mkql_decimal_mul.cpp
    mkql_decimal_mul.h
    mkql_dictitems.cpp
    mkql_dictitems.h
    mkql_discard.cpp
    mkql_discard.h
    mkql_element.cpp
    mkql_element.h
    mkql_ensure.h
    mkql_ensure.cpp
    mkql_enumerate.cpp
    mkql_enumerate.h
    mkql_exists.cpp
    mkql_exists.h
    mkql_extend.cpp
    mkql_extend.h
    mkql_factories.h
    mkql_factory.cpp
    mkql_filter.cpp
    mkql_filter.h
    mkql_flatmap.cpp
    mkql_flatmap.h
    mkql_flow.cpp
    mkql_flow.h
    mkql_fold.cpp
    mkql_fold.h
    mkql_fold1.cpp
    mkql_fold1.h
    mkql_frombytes.cpp
    mkql_frombytes.h
    mkql_fromstring.cpp
    mkql_fromstring.h
    mkql_fromyson.cpp
    mkql_fromyson.h
    mkql_group.cpp
    mkql_group.h
    mkql_grace_join.cpp
    mkql_grace_join.h
    mkql_grace_join_imp.cpp
    mkql_grace_join_imp.h
    mkql_guess.cpp
    mkql_guess.h
    mkql_hasitems.cpp
    mkql_hasitems.h
    mkql_heap.cpp
    mkql_heap.h
    mkql_hopping.cpp
    mkql_hopping.h
    mkql_if.cpp
    mkql_if.h
    mkql_ifpresent.cpp
    mkql_ifpresent.h
    mkql_invoke.cpp
    mkql_invoke.h
    mkql_iterable.cpp
    mkql_iterable.h
    mkql_iterator.cpp
    mkql_iterator.h
    mkql_join.cpp
    mkql_join.h
    mkql_join_dict.cpp
    mkql_join_dict.h
    mkql_lazy_list.cpp
    mkql_lazy_list.h
    mkql_length.cpp
    mkql_length.h
    mkql_listfromrange.cpp
    mkql_listfromrange.h
    mkql_logical.cpp
    mkql_logical.h
    mkql_lookup.cpp
    mkql_lookup.h
    mkql_map.cpp
    mkql_map.h
    mkql_mapnext.cpp
    mkql_mapnext.h
    mkql_map_join.cpp
    mkql_map_join.h
    mkql_multihopping.cpp
    mkql_multihopping.h
    mkql_multimap.cpp
    mkql_multimap.h
    mkql_next_value.cpp
    mkql_next_value.h
    mkql_now.cpp
    mkql_now.h
    mkql_null.cpp
    mkql_null.h
    mkql_pickle.cpp
    mkql_pickle.h
    mkql_prepend.cpp
    mkql_prepend.h
    mkql_queue.cpp
    mkql_queue.h
    mkql_random.cpp
    mkql_random.h
    mkql_range.cpp
    mkql_range.h
    mkql_reduce.cpp
    mkql_reduce.h
    mkql_removemember.cpp
    mkql_removemember.h
    mkql_replicate.cpp
    mkql_replicate.h
    mkql_reverse.cpp
    mkql_reverse.h
    mkql_rh_hash.cpp
    mkql_rh_hash.h
    mkql_round.cpp
    mkql_round.h
    mkql_safe_circular_buffer.cpp
    mkql_safe_circular_buffer.h
    mkql_saveload.h
    mkql_seq.cpp
    mkql_seq.h
    mkql_size.cpp
    mkql_size.h
    mkql_skip.cpp
    mkql_skip.h
    mkql_sort.cpp
    mkql_sort.h
    mkql_source.cpp
    mkql_source.h
    mkql_squeeze_state.cpp
    mkql_squeeze_state.h
    mkql_squeeze_to_list.cpp
    mkql_squeeze_to_list.h
    mkql_condense.cpp
    mkql_condense.h
    mkql_condense1.cpp
    mkql_condense1.h
    mkql_switch.cpp
    mkql_switch.h
    mkql_take.cpp
    mkql_take.h
    mkql_timezone.cpp
    mkql_timezone.h
    mkql_tobytes.cpp
    mkql_tobytes.h
    mkql_todict.cpp
    mkql_todict.h
    mkql_toindexdict.cpp
    mkql_toindexdict.h
    mkql_tooptional.cpp
    mkql_tooptional.h
    mkql_tostring.cpp
    mkql_tostring.h
    mkql_udf.cpp
    mkql_udf.h
    mkql_unwrap.cpp
    mkql_unwrap.h
    mkql_varitem.cpp
    mkql_varitem.h
    mkql_visitall.cpp
    mkql_visitall.h
    mkql_way.cpp
    mkql_way.h
    mkql_weakmember.cpp
    mkql_weakmember.h
    mkql_while.cpp
    mkql_while.h
    mkql_wide_chain_map.cpp
    mkql_wide_chain_map.h
    mkql_wide_chopper.cpp
    mkql_wide_chopper.h
    mkql_wide_combine.cpp
    mkql_wide_combine.h
    mkql_wide_condense.cpp
    mkql_wide_condense.h
    mkql_wide_filter.cpp
    mkql_wide_filter.h
    mkql_wide_map.cpp
    mkql_wide_map.h
    mkql_withcontext.cpp
    mkql_withcontext.h
    mkql_zip.cpp
    mkql_zip.h
)

PEERDIR(
    contrib/libs/apache/arrow
    ydb/library/binary_json
    ydb/library/yql/minikql
    ydb/library/yql/minikql/arrow
    ydb/library/yql/minikql/invoke_builtins
    ydb/library/yql/parser/pg_wrapper/interface
    ydb/library/yql/utils
)

NO_COMPILER_WARNINGS()

IF (NOT MKQL_DISABLE_CODEGEN)
    PEERDIR(
        ydb/library/yql/minikql/codegen
        contrib/libs/llvm12/lib/IR
        contrib/libs/llvm12/lib/ExecutionEngine/MCJIT
        contrib/libs/llvm12/lib/Linker
        contrib/libs/llvm12/lib/Target/X86
        contrib/libs/llvm12/lib/Target/X86/AsmParser
        contrib/libs/llvm12/lib/Transforms/IPO
    )
ELSE()
    CFLAGS(
        -DMKQL_DISABLE_CODEGEN
    )
ENDIF()

YQL_LAST_ABI_VERSION()

END()

RECURSE_FOR_TESTS(
    ut
)
