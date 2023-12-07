#pragma once

#include "defs.h"
#include "blobstorage_replctx.h"
#include <ydb/core/blobstorage/base/transparent.h>

namespace NKikimr {

    namespace NRepl {

        ////////////////////////////////////////////////////////////////////////////
        // TVDiskProxy -- forward declarations
        ////////////////////////////////////////////////////////////////////////////
        class TVDiskProxy;
        typedef TIntrusivePtr<TVDiskProxy> TVDiskProxyPtr; // TODO(alexvru): maybe std::unique_ptr?


        ////////////////////////////////////////////////////////////////////////////
        // Data Structures
        ////////////////////////////////////////////////////////////////////////////

        // A portion of data elements
        class TDataPortion {
            // Data element received from a vdisk or rebuilt by ourselves
            struct TDataElement {
                TLogoBlobID LogoBlobId;
                NKikimrProto::EReplyStatus Status;
                TTrackableString Data;

                TDataElement(TMemoryConsumer&& consumer, const TLogoBlobID& logoBlobId, NKikimrProto::EReplyStatus status,
                        TString&& data)
                    : LogoBlobId(logoBlobId)
                    , Status(status)
                    , Data(std::move(consumer), std::move(data))
                {}

                void Reset() {
                    Data.clear();
                }

                size_t GetDataSize() const {
                    return Data.capacity();
                }
            };

        public:
            TDataPortion(TMemoryConsumer&& consumer)
                : Consumer(std::move(consumer))
                , Items(TMemoryConsumer(Consumer))
                , FrontPos(0)
            {}

            TDataPortion(const TDataPortion& other) = delete;
            TDataPortion& operator =(const TDataPortion& other) = delete;

            TDataPortion(TDataPortion&& other)
                : Consumer(other.Consumer)
                , Items(std::move(other.Items))
                , FrontPos(other.FrontPos)
            {}

            TDataPortion& operator =(TDataPortion&& other) {
                Items = std::move(other.Items);
                FrontPos = other.FrontPos;
                return *this;
            }

            void Reset() {
                Items.clear();
                Items.shrink_to_fit();
                FrontPos = 0;
            }

            ////////////////////////////////////////////////////////////////////////////////
            // WRITE PART
            ////////////////////////////////////////////////////////////////////////////////

            void Add(const TLogoBlobID& logoBlobId, TString&& data) {
                Y_VERIFY_DEBUG(FrontPos == 0);
                Items.emplace_back(TMemoryConsumer(Consumer), logoBlobId, NKikimrProto::OK, std::move(data));
            }

            void AddError(const TLogoBlobID& logoBlobId, NKikimrProto::EReplyStatus status) {
                Y_VERIFY_DEBUG(FrontPos == 0);
                Items.emplace_back(TMemoryConsumer(Consumer), logoBlobId, status, TString());
            }

            void Append(TDataPortion&& from) {
                Y_VERIFY_DEBUG(FrontPos == 0);
                if (Items.empty()) {
                    Items = std::move(from.Items);
                } else {
                    Items.insert(Items.end(), from.Items.begin(), from.Items.end());
                    from.Items.clear();
                    from.Items.shrink_to_fit();
                }
            }

            ////////////////////////////////////////////////////////////////////////////////
            // READ PART
            ////////////////////////////////////////////////////////////////////////////////

            size_t GetNumItems() const {
                return Items.size();
            }

            size_t GetItemsDataTotalSize() const {
                size_t bytes = 0;
                for (const TDataElement& item : Items) {
                    bytes += item.GetDataSize();
                }
                return bytes;
            }

            void GetFrontItem(TLogoBlobID *logoBlobId, NKikimrProto::EReplyStatus *status, TTrackableString *data) const {
                Y_VERIFY_DEBUG(FrontPos < Items.size());
                const TDataElement& elem = Items[FrontPos];
                *logoBlobId = elem.LogoBlobId;
                *status = elem.Status;
                *data = elem.Data;
            }

            void GetFrontItem(TLogoBlobID *logoBlobId) const {
                Y_VERIFY_DEBUG(FrontPos < Items.size());
                *logoBlobId = Items[FrontPos].LogoBlobId;
            }

            bool Valid() const {
                Y_VERIFY_DEBUG(FrontPos <= Items.size());
                return FrontPos != Items.size();
            }

            void Next() {
                Y_VERIFY_DEBUG(FrontPos < Items.size());
                Items[FrontPos].Reset();
                ++FrontPos;
            }

        private:
            TMemoryConsumer Consumer;
            TTrackableVector<TDataElement> Items;
            size_t FrontPos;
        };

        // A portion of data from proxy
        struct TNextPortion {
            enum EStatus {
                Ok = 0,
                Eof = 1,
                Error = 2,
                Unknown = 3
            };

            EStatus Status;
            TDataPortion DataPortion;

            TNextPortion(EStatus status, TMemoryConsumer&& consumer)
                : Status(status)
                , DataPortion(std::move(consumer))
            {}

            // AppendDataPortion(): add some data from another DataPortion to the end of this one
            void AppendDataPortion(TDataPortion&& from) {
                DataPortion.Append(std::move(from));
            }

            // Valid(): returns true if this next portion contains any sensitive data that can be
            // sent to VDiskProxy; this case includes ERROR/EOF replies or OK reply with non-empty
            // DataPortion
            bool Valid() const {
                return Status == Eof || Status == Error || (Status == Ok && DataPortion.Valid());
            }

            void Reset() {
                Status = Unknown;
                DataPortion.Reset();
            }
        };

        // Per proxy statistics for VDisk, we can sum up it to obtain total statistics for all proxies
        struct TProxyStat {
            ui64 VDiskReqs = 0;
            ui64 VDiskRespOK = 0;
            ui64 VDiskRespRACE = 0;
            ui64 VDiskRespERROR = 0;
            ui64 VDiskRespDEADLINE = 0;
            ui64 VDiskRespOther = 0;
            ui64 LogoBlobGotIt = 0;
            ui64 LogoBlobNoData = 0;
            ui64 LogoBlobNotOK = 0;
            ui64 LogoBlobDataSize = 0;
            ui64 OverflowedMsgs = 0;

            TProxyStat &operator +=(const TProxyStat &stat) {
                VDiskReqs += stat.VDiskReqs;
                VDiskRespOK += stat.VDiskRespOK;
                VDiskRespRACE += stat.VDiskRespRACE;
                VDiskRespERROR += stat.VDiskRespERROR;
                VDiskRespDEADLINE += stat.VDiskRespDEADLINE;
                VDiskRespOther += stat.VDiskRespOther;
                LogoBlobGotIt += stat.LogoBlobGotIt;
                LogoBlobNoData += stat.LogoBlobNoData;
                LogoBlobNotOK += stat.LogoBlobNotOK;
                LogoBlobDataSize += stat.LogoBlobDataSize;
                OverflowedMsgs += stat.OverflowedMsgs;
                return *this;
            }
        };

    } // NRepl


    ////////////////////////////////////////////////////////////////////////////
    // Internal Messages
    ////////////////////////////////////////////////////////////////////////////
    struct TEvReplProxyNext : public TEventLocal<TEvReplProxyNext, TEvBlobStorage::EvReplProxyNext> {
        TEvReplProxyNext()
        {}
    };

    struct TEvReplProxyNextResult : public TEventLocal<TEvReplProxyNextResult, TEvBlobStorage::EvReplProxyNextResult> {
        TVDiskID VDiskId;
        NRepl::TNextPortion Portion;
        NRepl::TProxyStat Stat;
        bool HasTransientErrors;

        TEvReplProxyNextResult(TVDiskID vdiskId, NRepl::TNextPortion&& portion, const NRepl::TProxyStat &stat,
                bool hasTransientErrors)
            : VDiskId(vdiskId)
            , Portion(std::move(portion))
            , Stat(stat)
            , HasTransientErrors(hasTransientErrors)
        {}
    };

    namespace NRepl {

        ////////////////////////////////////////////////////////////////////////////
        // TVDiskProxy
        ////////////////////////////////////////////////////////////////////////////
        class TVDiskProxy : public TThrRefBase {
        public:
            struct TScheduledBlob {
                TLogoBlobID Id;
                ui32 ExpectedReplySize;

                TScheduledBlob(const TLogoBlobID& id, ui32 expectedReplySize)
                    : Id(id)
                    , ExpectedReplySize(expectedReplySize)
                {}
            };

            enum EState {
                Initial = 0,
                RunProxy = 1,
                Ok = 2,
                Eof = 3,
                Error = 4
            };

            TVDiskProxy(
                    std::shared_ptr<TReplCtx> replCtx,
                    const TVDiskID &vdisk,
                    const TActorId &serviceID);

            TActorId Run(const TActorId& parentId);
            void SendNextRequest();
            void HandleNext(TEvReplProxyNextResult::TPtr &ev);

        private:
            void HandlePortion(TNextPortion &portion);

        public:
            void Put(const TLogoBlobID &id, ui32 expectedReplySize) {
                Y_VERIFY_DEBUG(State == Initial);
                Ids.emplace_back(id, expectedReplySize);
            }

            // Next(): advance to next data item
            // Precondition: Valid()
            void Next() {
                DataPortion.Next();
            }

            void GetData(TLogoBlobID *logoBlobId, NKikimrProto::EReplyStatus *status, TTrackableString *data) const {
                DataPortion.GetFrontItem(logoBlobId, status, data);
            }

            TLogoBlobID GenLogoBlobId() const {
                TLogoBlobID logoBlobId;
                DataPortion.GetFrontItem(&logoBlobId);
                return TLogoBlobID(logoBlobId, 0);
            }

            // IsEof(): returns true on EOF condition, i.e. when there is no more data exists in buffer and
            // no more data is expected in future
            bool IsEof() const {
                Y_VERIFY_DEBUG(State == Ok || State == Eof || State == Error);
                return (State == Error || State == Eof) && !DataPortion.Valid();
            }

            // Valid(): returns true when there is data in buffer to read using LogoBlobID() / GetData()
            // methods and to advance using Next() method
            bool Valid() const {
                return DataPortion.Valid();
            }

            // returns true if there were no transient errors during query execution
            bool NoTransientErrors() const {
                return !HasTransientErrors;
            }

            std::shared_ptr<TReplCtx> ReplCtx;
            const TVDiskID VDiskId;
            const TActorId ServiceId;
            TProxyStat Stat;

        private:
            TActorId ParentId;
            TActorId ProxyId;
            TTrackableVector<TScheduledBlob> Ids;
            EState State = Initial;
            TDataPortion DataPortion;
            bool HasTransientErrors = false;

        public:
            struct TPtrGreater {
                bool operator() (const TVDiskProxyPtr &x, const TVDiskProxyPtr &y) const {
                    Y_VERIFY_DEBUG(x->Valid() && y->Valid());
                    return x->GenLogoBlobId() > y->GenLogoBlobId();
                }
            };
        };

    } // NRepl

} // NKikimr
