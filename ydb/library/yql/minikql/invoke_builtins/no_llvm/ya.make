LIBRARY()

CXXFLAGS(-DMKQL_DISABLE_CODEGEN)

ADDINCL(GLOBAL ydb/library/yql/minikql/codegen/llvm_stub)

INCLUDE(../ya.make.inc)

PEERDIR(ydb/library/yql/minikql/computation/no_llvm)

END()

