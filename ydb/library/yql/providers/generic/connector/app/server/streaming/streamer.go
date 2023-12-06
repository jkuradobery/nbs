package streaming

import (
	"context"
	"fmt"
	"sync"

	data_source "github.com/ydb-platform/nbs/ydb/library/yql/providers/generic/connector/app/server/datasource"
	"github.com/ydb-platform/nbs/ydb/library/yql/providers/generic/connector/app/server/paging"
	"github.com/ydb-platform/nbs/ydb/library/yql/providers/generic/connector/app/server/utils"
	api_service "github.com/ydb-platform/nbs/ydb/library/yql/providers/generic/connector/libgo/service"
	api_service_protos "github.com/ydb-platform/nbs/ydb/library/yql/providers/generic/connector/libgo/service/protos"
	"github.com/ydb-platform/nbs/library/go/core/log"
)

type Streamer struct {
	stream     api_service.Connector_ReadSplitsServer
	request    *api_service_protos.TReadSplitsRequest
	dataSource data_source.DataSource
	split      *api_service_protos.TSplit
	sink       paging.Sink
	logger     log.Logger
	ctx        context.Context // clone of a stream context
	cancel     context.CancelFunc
}

func (s *Streamer) writeDataToStream() error {
	// exit from this function will cause publisher's goroutine termination as well
	defer s.cancel()

	for {
		select {
		case result, ok := <-s.sink.ResultQueue():
			if !ok {
				// correct termination
				return nil
			}

			if result.Error != nil {
				return fmt.Errorf("read result: %w", result.Error)
			}

			// handle next data block
			if err := s.sendResultToStream(result); err != nil {
				return fmt.Errorf("send buffer to stream: %w", err)
			}
		case <-s.stream.Context().Done():
			// handle request termination
			return s.stream.Context().Err()
		}
	}
}

func (s *Streamer) sendResultToStream(result *paging.ReadResult) error {
	// buffer must be explicitly marked as unused,
	// otherwise memory will leak
	defer result.ColumnarBuffer.Release()

	resp, err := result.ColumnarBuffer.ToResponse()
	if err != nil {
		return fmt.Errorf("buffer to response: %w", err)
	}

	resp.Stats = result.Stats

	utils.DumpReadSplitsResponse(s.logger, resp)

	if err := s.stream.Send(resp); err != nil {
		return fmt.Errorf("stream send: %w", err)
	}

	return nil
}

func (s *Streamer) Run() error {
	wg := &sync.WaitGroup{}
	wg.Add(1)

	defer wg.Wait()

	// Launch reading from the data source.
	// Subsriber goroutine controls publisher goroutine lifetime.
	go func() {
		defer wg.Done()

		s.dataSource.ReadSplit(s.ctx, s.logger, s.split, s.sink)
	}()

	// pass received blocks into the GRPC channel
	if err := s.writeDataToStream(); err != nil {
		return fmt.Errorf("write data to stream: %w", err)
	}

	return nil
}

func NewStreamer(
	logger log.Logger,
	stream api_service.Connector_ReadSplitsServer,
	request *api_service_protos.TReadSplitsRequest,
	split *api_service_protos.TSplit,
	sink paging.Sink,
	dataSource data_source.DataSource,
) *Streamer {
	ctx, cancel := context.WithCancel(stream.Context())

	return &Streamer{
		logger:     logger,
		stream:     stream,
		split:      split,
		request:    request,
		dataSource: dataSource,
		sink:       sink,
		ctx:        ctx,
		cancel:     cancel,
	}
}
