#include "send_data_protocol.h"

#include <ydb/core/base/appdata.h>
#include <util/system/error.h>
#include <util/system/yassert.h>

namespace NActors {

void TSendDataProtocol::ProtocolFunc(
        TAutoPtr<NActors::IEventHandle>& ev,
        const TActorContext& ctx) noexcept
{
    if (Cancelled) {
        return;
    }

    switch (ev->GetTypeRewrite()) {
    case TEvSocketReadyWrite::EventType:
        TryAgain(ctx);
        break;

    default:
        Y_FAIL("Unknown message type dispatched");
    }
}


static TDelegate NotifyReadyWrite(
        const TIntrusivePtr<TSharedDescriptor>& FDPtr,
        const TActorContext& ctx)
{
    Y_UNUSED(FDPtr);
    return [=]() { ctx.Send(ctx.SelfID, new TEvSocketReadyWrite); };
}


void TSendDataProtocol::TryAgain(const TActorContext& ctx) noexcept {
    int sendResult;
    for (;;) {
        sendResult = Socket->Send(Data, Len);

        if (sendResult > 0) {
            Y_VERIFY(Len >= (size_t)sendResult);
            MemLogPrintF("TSendDataProtocol::TryAgain, sent %d bytes",
                         sendResult);

            Data += sendResult;
            Len -= sendResult;
            if (Len == 0) {
                CatchSendDataComplete(ctx);
                return;
            }
        } else {
            if (sendResult < 0 && sendResult == -EINTR)
                continue;
            else
                break;
        }
    }

    if (-sendResult == EAGAIN || -sendResult == EWOULDBLOCK) {
        IPoller* poller = NKikimr::AppData(ctx)->PollerThreads.Get();
        poller->StartWrite(Socket,
            std::bind(NotifyReadyWrite, std::placeholders::_1, ctx));
        return;
    }

    switch (-sendResult) {
    case ECONNRESET:
        CatchSendDataError("Connection reset by peer");
        return;

    case EPIPE:
        CatchSendDataError("Connection is closed");
        return;

    case 0:
        /* Not realy sure what to do with 0 result, assume socket is closed */
        CatchSendDataError("Connection is closed");
        return;

    default:
        {
            char buf[1024];
            LastSystemErrorText(buf, 1024, -sendResult);
            CatchSendDataError(TString("Socker error: ") + buf);
            return;
        }

    case EBADF:
    case EFAULT:
    case EINVAL:
    case ENOTCONN:
    case ENOTSOCK:
    case EOPNOTSUPP:
        {
            Y_FAIL("Very bad socket error");
        }
    }
}

void TSendDataProtocol::CancelSendData(const TActorContext& /*ctx*/) noexcept {
    Cancelled = true;
//    IPoller* poller = NKikimr::AppData(ctx)->PollerThreads.Get();
//    poller->CancelWrite(Socket);
}

}
