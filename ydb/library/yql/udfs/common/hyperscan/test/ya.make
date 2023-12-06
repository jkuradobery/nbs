IF (OS_LINUX AND CLANG)

YQL_UDF_TEST_CONTRIB()

DEPENDS(ydb/library/yql/udfs/common/hyperscan)

TIMEOUT(300)

SIZE(MEDIUM)

IF (SANITIZER_TYPE == "memory")
    TAG(ya:not_autocheck) # YQL-15385
ENDIF()

END()

ENDIF()
