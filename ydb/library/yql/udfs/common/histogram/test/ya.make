YQL_UDF_TEST_CONTRIB()

TIMEOUT(600)

SIZE(MEDIUM)

IF (SANITIZER_TYPE == "memory")
    TAG(ya:not_autocheck) # YQL-15385
ENDIF()

DEPENDS(ydb/library/yql/udfs/common/histogram)

END()
