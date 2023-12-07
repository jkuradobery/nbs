#pragma once

#include "yql_s3_provider.h"

#include <ydb/library/yql/providers/dq/interface/yql_dq_integration.h>

#include <util/generic/ptr.h>

namespace NYql {

THolder<IDqIntegration> CreateS3DqIntegration(TS3State::TPtr state);

}
