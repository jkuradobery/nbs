# Generated by devtools/yamaker from nixpkgs 3322db8e36d0b32700737d8de7315bd9e9c2b21a.

LIBRARY()

LICENSE(
    Apache-2.0 AND
    BSD-2-Clause AND
    BSD-3-Clause AND
    BSL-1.0 AND
    CC0-1.0 AND
    MIT AND
    NCSA AND
    Protobuf-License AND
    Public-Domain AND
    ZPL-2.1 AND
    Zlib
)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

VERSION(5.0.0)

ORIGINAL_SOURCE(https://github.com/apache/arrow/archive/apache-arrow-5.0.0.tar.gz)

PEERDIR(
    contrib/libs/apache/orc
    contrib/libs/brotli/dec
    contrib/libs/brotli/enc
    contrib/libs/double-conversion
    contrib/libs/lz4
    contrib/libs/rapidjson
    contrib/libs/re2
    contrib/libs/snappy
    contrib/libs/utf8proc
    contrib/libs/xxhash
    contrib/libs/zlib
    contrib/libs/zstd
    contrib/restricted/fast_float
    contrib/restricted/thrift
    contrib/restricted/uriparser
)

ADDINCL(
    GLOBAL contrib/libs/apache/arrow/cpp/src
    GLOBAL contrib/libs/apache/arrow/src
    contrib/libs/apache/arrow/cpp/src/generated
    contrib/libs/apache/orc/c++/include
    contrib/libs/flatbuffers/include
    contrib/libs/lz4
    contrib/libs/rapidjson/include
    contrib/libs/utf8proc
    contrib/libs/zstd/include
)

NO_COMPILER_WARNINGS()

NO_UTIL()

CFLAGS(
    GLOBAL -DARROW_STATIC
    GLOBAL -DPARQUET_STATIC
    -DARROW_EXPORTING
    -DARROW_WITH_BROTLI
    -DARROW_WITH_LZ4
    -DARROW_WITH_RE2
    -DARROW_WITH_SNAPPY
    -DARROW_WITH_TIMING_TESTS
    -DARROW_WITH_UTF8PROC
    -DARROW_WITH_ZLIB
    -DARROW_WITH_ZSTD
    -DHAVE_INTTYPES_H
    -DHAVE_NETDB_H
    -DPARQUET_EXPORTING
    -DURI_STATIC_BUILD
)

IF (NOT OS_WINDOWS)
    CFLAGS(
        -DHAVE_NETINET_IN_H
    )
ENDIF()

SRCS(
    cpp/src/arrow/adapters/orc/adapter.cc
    cpp/src/arrow/adapters/orc/adapter_util.cc
    cpp/src/arrow/array/array_base.cc
    cpp/src/arrow/array/array_binary.cc
    cpp/src/arrow/array/array_decimal.cc
    cpp/src/arrow/array/array_dict.cc
    cpp/src/arrow/array/array_nested.cc
    cpp/src/arrow/array/array_primitive.cc
    cpp/src/arrow/array/builder_adaptive.cc
    cpp/src/arrow/array/builder_base.cc
    cpp/src/arrow/array/builder_binary.cc
    cpp/src/arrow/array/builder_decimal.cc
    cpp/src/arrow/array/builder_dict.cc
    cpp/src/arrow/array/builder_nested.cc
    cpp/src/arrow/array/builder_primitive.cc
    cpp/src/arrow/array/builder_union.cc
    cpp/src/arrow/array/concatenate.cc
    cpp/src/arrow/array/data.cc
    cpp/src/arrow/array/diff.cc
    cpp/src/arrow/array/util.cc
    cpp/src/arrow/array/validate.cc
    cpp/src/arrow/buffer.cc
    cpp/src/arrow/builder.cc
    cpp/src/arrow/c/bridge.cc
    cpp/src/arrow/chunked_array.cc
    cpp/src/arrow/compare.cc
    cpp/src/arrow/compute/api_aggregate.cc
    cpp/src/arrow/compute/api_scalar.cc
    cpp/src/arrow/compute/api_vector.cc
    cpp/src/arrow/compute/cast.cc
    cpp/src/arrow/compute/exec.cc
    cpp/src/arrow/compute/exec/exec_plan.cc
    cpp/src/arrow/compute/exec/expression.cc
    cpp/src/arrow/compute/exec/key_compare.cc
    cpp/src/arrow/compute/exec/key_encode.cc
    cpp/src/arrow/compute/exec/key_hash.cc
    cpp/src/arrow/compute/exec/key_map.cc
    cpp/src/arrow/compute/exec/util.cc
    cpp/src/arrow/compute/function.cc
    cpp/src/arrow/compute/function_internal.cc
    cpp/src/arrow/compute/kernel.cc
    cpp/src/arrow/compute/kernels/aggregate_basic.cc
    cpp/src/arrow/compute/kernels/aggregate_mode.cc
    cpp/src/arrow/compute/kernels/aggregate_quantile.cc
    cpp/src/arrow/compute/kernels/aggregate_tdigest.cc
    cpp/src/arrow/compute/kernels/aggregate_var_std.cc
    cpp/src/arrow/compute/kernels/codegen_internal.cc
    cpp/src/arrow/compute/kernels/hash_aggregate.cc
    cpp/src/arrow/compute/kernels/scalar_arithmetic.cc
    cpp/src/arrow/compute/kernels/scalar_boolean.cc
    cpp/src/arrow/compute/kernels/scalar_cast_boolean.cc
    cpp/src/arrow/compute/kernels/scalar_cast_dictionary.cc
    cpp/src/arrow/compute/kernels/scalar_cast_internal.cc
    cpp/src/arrow/compute/kernels/scalar_cast_nested.cc
    cpp/src/arrow/compute/kernels/scalar_cast_numeric.cc
    cpp/src/arrow/compute/kernels/scalar_cast_string.cc
    cpp/src/arrow/compute/kernels/scalar_cast_temporal.cc
    cpp/src/arrow/compute/kernels/scalar_compare.cc
    cpp/src/arrow/compute/kernels/scalar_fill_null.cc
    cpp/src/arrow/compute/kernels/scalar_if_else.cc
    cpp/src/arrow/compute/kernels/scalar_nested.cc
    cpp/src/arrow/compute/kernels/scalar_set_lookup.cc
    cpp/src/arrow/compute/kernels/scalar_string.cc
    cpp/src/arrow/compute/kernels/scalar_temporal.cc
    cpp/src/arrow/compute/kernels/scalar_validity.cc
    cpp/src/arrow/compute/kernels/util_internal.cc
    cpp/src/arrow/compute/kernels/vector_hash.cc
    cpp/src/arrow/compute/kernels/vector_nested.cc
    cpp/src/arrow/compute/kernels/vector_replace.cc
    cpp/src/arrow/compute/kernels/vector_selection.cc
    cpp/src/arrow/compute/kernels/vector_sort.cc
    cpp/src/arrow/compute/registry.cc
    cpp/src/arrow/config.cc
    cpp/src/arrow/csv/chunker.cc
    cpp/src/arrow/csv/column_builder.cc
    cpp/src/arrow/csv/column_decoder.cc
    cpp/src/arrow/csv/converter.cc
    cpp/src/arrow/csv/options.cc
    cpp/src/arrow/csv/parser.cc
    cpp/src/arrow/csv/reader.cc
    cpp/src/arrow/csv/writer.cc
    cpp/src/arrow/datum.cc
    cpp/src/arrow/device.cc
    cpp/src/arrow/extension_type.cc
    cpp/src/arrow/filesystem/filesystem.cc
    cpp/src/arrow/filesystem/localfs.cc
    cpp/src/arrow/filesystem/mockfs.cc
    cpp/src/arrow/filesystem/path_util.cc
    cpp/src/arrow/filesystem/util_internal.cc
    cpp/src/arrow/io/buffered.cc
    cpp/src/arrow/io/caching.cc
    cpp/src/arrow/io/compressed.cc
    cpp/src/arrow/io/file.cc
    cpp/src/arrow/io/interfaces.cc
    cpp/src/arrow/io/memory.cc
    cpp/src/arrow/io/slow.cc
    cpp/src/arrow/io/stdio.cc
    cpp/src/arrow/io/transform.cc
    cpp/src/arrow/ipc/dictionary.cc
    cpp/src/arrow/ipc/feather.cc
    cpp/src/arrow/ipc/json_simple.cc
    cpp/src/arrow/ipc/message.cc
    cpp/src/arrow/ipc/metadata_internal.cc
    cpp/src/arrow/ipc/options.cc
    cpp/src/arrow/ipc/reader.cc
    cpp/src/arrow/ipc/writer.cc
    cpp/src/arrow/json/chunked_builder.cc
    cpp/src/arrow/json/chunker.cc
    cpp/src/arrow/json/converter.cc
    cpp/src/arrow/json/object_parser.cc
    cpp/src/arrow/json/object_writer.cc
    cpp/src/arrow/json/options.cc
    cpp/src/arrow/json/parser.cc
    cpp/src/arrow/json/reader.cc
    cpp/src/arrow/memory_pool.cc
    cpp/src/arrow/pretty_print.cc
    cpp/src/arrow/record_batch.cc
    cpp/src/arrow/result.cc
    cpp/src/arrow/scalar.cc
    cpp/src/arrow/sparse_tensor.cc
    cpp/src/arrow/status.cc
    cpp/src/arrow/table.cc
    cpp/src/arrow/table_builder.cc
    cpp/src/arrow/tensor.cc
    cpp/src/arrow/tensor/coo_converter.cc
    cpp/src/arrow/tensor/csf_converter.cc
    cpp/src/arrow/tensor/csx_converter.cc
    cpp/src/arrow/type.cc
    cpp/src/arrow/util/basic_decimal.cc
    cpp/src/arrow/util/bit_block_counter.cc
    cpp/src/arrow/util/bit_run_reader.cc
    cpp/src/arrow/util/bit_util.cc
    cpp/src/arrow/util/bitmap.cc
    cpp/src/arrow/util/bitmap_builders.cc
    cpp/src/arrow/util/bitmap_ops.cc
    cpp/src/arrow/util/bpacking.cc
    cpp/src/arrow/util/cancel.cc
    cpp/src/arrow/util/compression.cc
    cpp/src/arrow/util/compression_brotli.cc
    cpp/src/arrow/util/compression_lz4.cc
    cpp/src/arrow/util/compression_snappy.cc
    cpp/src/arrow/util/compression_zlib.cc
    cpp/src/arrow/util/compression_zstd.cc
    cpp/src/arrow/util/cpu_info.cc
    cpp/src/arrow/util/decimal.cc
    cpp/src/arrow/util/delimiting.cc
    cpp/src/arrow/util/formatting.cc
    cpp/src/arrow/util/future.cc
    cpp/src/arrow/util/int_util.cc
    cpp/src/arrow/util/io_util.cc
    cpp/src/arrow/util/key_value_metadata.cc
    cpp/src/arrow/util/logging.cc
    cpp/src/arrow/util/memory.cc
    cpp/src/arrow/util/mutex.cc
    cpp/src/arrow/util/string.cc
    cpp/src/arrow/util/string_builder.cc
    cpp/src/arrow/util/task_group.cc
    cpp/src/arrow/util/tdigest.cc
    cpp/src/arrow/util/thread_pool.cc
    cpp/src/arrow/util/time.cc
    cpp/src/arrow/util/trie.cc
    cpp/src/arrow/util/uri.cc
    cpp/src/arrow/util/utf8.cc
    cpp/src/arrow/util/value_parsing.cc
    cpp/src/arrow/vendored/base64.cpp
    cpp/src/arrow/vendored/datetime/tz.cpp
    cpp/src/arrow/vendored/musl/strptime.c
    cpp/src/arrow/visitor.cc
    cpp/src/generated/parquet_constants.cpp
    cpp/src/generated/parquet_types.cpp
    cpp/src/parquet/arrow/path_internal.cc
    cpp/src/parquet/arrow/reader.cc
    cpp/src/parquet/arrow/reader_internal.cc
    cpp/src/parquet/arrow/schema.cc
    cpp/src/parquet/arrow/schema_internal.cc
    cpp/src/parquet/arrow/writer.cc
    cpp/src/parquet/bloom_filter.cc
    cpp/src/parquet/column_reader.cc
    cpp/src/parquet/column_scanner.cc
    cpp/src/parquet/column_writer.cc
    cpp/src/parquet/encoding.cc
    cpp/src/parquet/encryption/encryption.cc
    cpp/src/parquet/encryption/encryption_internal_nossl.cc
    cpp/src/parquet/encryption/internal_file_decryptor.cc
    cpp/src/parquet/encryption/internal_file_encryptor.cc
    cpp/src/parquet/exception.cc
    cpp/src/parquet/file_reader.cc
    cpp/src/parquet/file_writer.cc
    cpp/src/parquet/level_comparison.cc
    cpp/src/parquet/level_conversion.cc
    cpp/src/parquet/metadata.cc
    cpp/src/parquet/murmur3.cc
    cpp/src/parquet/platform.cc
    cpp/src/parquet/printer.cc
    cpp/src/parquet/properties.cc
    cpp/src/parquet/schema.cc
    cpp/src/parquet/statistics.cc
    cpp/src/parquet/stream_reader.cc
    cpp/src/parquet/stream_writer.cc
    cpp/src/parquet/types.cc
)

END()

RECURSE(
    cpp/src/arrow/python
)
