package server

import (
	"fmt"

	api_common "github.com/ydb-platform/nbs/ydb/library/yql/providers/generic/connector/api/common"
	data_source "github.com/ydb-platform/nbs/ydb/library/yql/providers/generic/connector/app/server/datasource"
	"github.com/ydb-platform/nbs/ydb/library/yql/providers/generic/connector/app/server/rdbms"
	"github.com/ydb-platform/nbs/ydb/library/yql/providers/generic/connector/app/server/rdbms/clickhouse"
	"github.com/ydb-platform/nbs/ydb/library/yql/providers/generic/connector/app/server/rdbms/postgresql"
	"github.com/ydb-platform/nbs/ydb/library/yql/providers/generic/connector/app/server/utils"
	"github.com/ydb-platform/nbs/library/go/core/log"
)

type dataSourceFactory struct {
	clickhouse rdbms.Preset
	postgresql rdbms.Preset
}

func (dsf *dataSourceFactory) Make(
	logger log.Logger,
	dataSourceType api_common.EDataSourceKind,
) (data_source.DataSource, error) {
	switch dataSourceType {
	case api_common.EDataSourceKind_CLICKHOUSE:
		return rdbms.NewDataSource(logger, &dsf.clickhouse), nil
	case api_common.EDataSourceKind_POSTGRESQL:
		return rdbms.NewDataSource(logger, &dsf.postgresql), nil
	default:
		return nil, fmt.Errorf("pick handler for data source type '%v': %w", dataSourceType, utils.ErrDataSourceNotSupported)
	}
}

func newDataSourceFacotry(qlf utils.QueryLoggerFactory) *dataSourceFactory {
	connManagerCfg := utils.ConnectionManagerBase{
		QueryLoggerFactory: qlf,
	}

	return &dataSourceFactory{
		clickhouse: rdbms.Preset{
			SQLFormatter:      clickhouse.NewSQLFormatter(),
			ConnectionManager: clickhouse.NewConnectionManager(connManagerCfg),
			TypeMapper:        clickhouse.NewTypeMapper(),
		},
		postgresql: rdbms.Preset{
			SQLFormatter:      postgresql.NewSQLFormatter(),
			ConnectionManager: postgresql.NewConnectionManager(connManagerCfg),
			TypeMapper:        postgresql.NewTypeMapper(),
		},
	}
}
