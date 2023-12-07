#include <ydb/core/tablet_flat/flat_part_dump.h>
#include <ydb/core/tablet_flat/test/libs/rows/cook.h>
#include <ydb/core/tablet_flat/test/libs/rows/layout.h>
#include <ydb/core/tablet_flat/test/libs/table/model/large.h>
#include <ydb/core/tablet_flat/test/libs/table/wrap_part.h>
#include <ydb/core/tablet_flat/test/libs/table/test_writer.h>
#include <ydb/core/tablet_flat/test/libs/table/test_wreck.h>
#include <ydb/core/tablet_flat/test/libs/table/test_comp.h>

#include <library/cpp/testing/unittest/registar.h>

namespace NKikimr {
namespace NTable {

namespace {
    NPage::TConf PageConf(size_t groups = 1) noexcept
    {
        NPage::TConf conf{ true, 2 * 1024 };

        conf.Groups.resize(groups);
        for (size_t group : xrange(groups)) {
            conf.Group(group).IndexMin = 1024; /* Should cover index buffer grow code */
        }
        conf.SmallEdge = 19;  /* Packed to page collection large cell values */
        conf.LargeEdge = 29;  /* Large values placed to single blobs */
        conf.SliceSize = conf.Group(0).PageSize * 4;

        return conf;
    }

    const NTest::TMass& Mass0()
    {
        static const NTest::TMass mass0(new NTest::TModelStd(false), 24000);
        return mass0;
    }

    const NTest::TPartEggs& Eggs0()
    {
        static const NTest::TPartEggs eggs0 = NTest::TPartCook::Make(Mass0(), PageConf());
        return eggs0;
    }

    const NTest::TMass& Mass1()
    {
        static const NTest::TMass mass1(new NTest::TModelStd(true), 24000);
        return mass1;
    }

    const NTest::TPartEggs& Eggs1()
    {
        static const auto& mass1 = Mass1();
        static const auto eggs1 = NTest::TPartCook::Make(mass1, PageConf(mass1.Model->Scheme->Families.size()));
        return eggs1;
    }
}

Y_UNIT_TEST_SUITE(TPart) {
    using namespace NTest;

    Y_UNIT_TEST(State)
    {
        TRowState row(2);

        const NPageCollection::TGlobId glob{ TLogoBlobID(10, 20, 30, 1, 0, 0), 7 };

        row.Set(1, { ECellOp::Set, ELargeObj::GlobId }, TCell::Make(glob));
        UNIT_ASSERT(row.Need() == 0 && row.Left() == 1);
        row.Set(0, { ECellOp::Null, ELargeObj::GlobId }, TCell::Make(glob));
        UNIT_ASSERT(row.Need() == 1 && row.Left() == 0);
    }

    Y_UNIT_TEST(Trivials)
    {
        TLayoutCook lay;

        lay
            .Col(0, 0,  NScheme::NTypeIds::String)
            .Col(0, 1,  NScheme::NTypeIds::Uint32)
            .Col(0, 2,  NScheme::NTypeIds::Uint64)
            .Key({0, 1});

        { /*_ Check that trivial part hasn't been produced by writer */
            auto eggs = TPartCook(lay, { true, 4096 }).Finish();

            if (eggs.Written->Rows > 0) {
                ythrow yexception() << "Unexpected trivial TWriteStats result";
            } else if (eggs.Parts) {
                ythrow yexception() << "Writer produced parts with trival data";
            }
        }
    }

    Y_UNIT_TEST(Basics)
    {
        TLayoutCook lay;

        lay
            .Col(0, 0,  NScheme::NTypeIds::Uint32)
            .Col(0, 1,  NScheme::NTypeIds::String)
            .Col(0, 2,  NScheme::NTypeIds::Double)
            .Col(0, 3,  NScheme::NTypeIds::Bool)
            .Key({0, 1});

        const auto foo = *TSchemedCookRow(*lay).Col(555_u32, "foo", 3.14, nullptr);
        const auto bar = *TSchemedCookRow(*lay).Col(777_u32, "bar", 2.72, true);

        TCheckIt wrap(TPartCook(lay, { }).Add(foo).Add(bar).Finish(), { });

        wrap.To(10).Has(foo).Has(bar);
        wrap.To(11).NoVal(*TSchemedCookRow(*lay).Col(555_u32, "foo", 10.));
        wrap.To(12).NoKey(*TSchemedCookRow(*lay).Col(888_u32, "foo", 3.14));

        /*_ Basic lower and upper bounds lookup semantic  */

        wrap.To(20).Seek(foo, ESeek::Lower).Is(foo);
        wrap.To(21).Seek(foo, ESeek::Upper).Is(bar);
        wrap.To(22).Seek(bar, ESeek::Upper).Is(EReady::Gone);

        /*_ The empty key is interpreted depending on ESeek mode... */

        wrap.To(30).Seek({ }, ESeek::Lower).Is(foo);
        wrap.To(31).Seek({ }, ESeek::Exact).Is(EReady::Gone);
        wrap.To(32).Seek({ }, ESeek::Upper).Is(EReady::Gone);

        /* ... but incomplete keys are padded with +inf instead of nulls
            on lookup. Check that it really happens for Seek()'s */

        wrap.To(33).Seek(*TSchemedCookRow(*lay).Col(555_u32), ESeek::Lower).Is(bar);
        wrap.To(34).Seek(*TSchemedCookRow(*lay).Col(555_u32), ESeek::Upper).Is(bar);

        /* Verify part has correct first and last key in the index */

        const auto part = (*wrap).Eggs.Lone();
        UNIT_ASSERT_VALUES_EQUAL(part->Index.Rows(), 2u);
        const NPage::TCompare<NPage::TIndex::TRecord> cmp(part->Scheme->Groups[0].ColsKeyIdx, *(*lay).Keys);
        UNIT_ASSERT_VALUES_EQUAL(cmp.Compare(*part->Index.GetFirstKeyRecord(), TRowTool(*lay).KeyCells(foo)), 0);
        UNIT_ASSERT_VALUES_EQUAL(cmp.Compare(*part->Index.GetLastKeyRecord(), TRowTool(*lay).KeyCells(bar)), 0);

        DumpPart(*(*wrap).Eggs.Lone(), 10);
    }

    Y_UNIT_TEST(BasicColumnGroups)
    {
        using namespace NTable::NTest;

        TLayoutCook lay;

        lay
            .Col(0, 0, NScheme::NTypeIds::Uint32)
            .Col(0, 1, NScheme::NTypeIds::String)
            .Col(1, 2, NScheme::NTypeIds::Double)
            .Col(2, 3, NScheme::NTypeIds::Bool)
            .Key({ 0 });

        const auto foo = *TSchemedCookRow(*lay).Col(555_u32, "foo", 3.14, nullptr);
        const auto bar = *TSchemedCookRow(*lay).Col(777_u32, "bar", 2.72, true);

        auto eggs = TPartCook(lay, PageConf(/* groups = */ 3)).Add(foo).Add(bar).Finish();
        auto part = eggs.Lone();

        UNIT_ASSERT_VALUES_EQUAL(part->Groups, 3u);

        TCheckIt wrap(eggs, { });

        wrap.To(10).Has(foo).Has(bar);
    }

    Y_UNIT_TEST(CellDefaults)
    {
        TLayoutCook lay, fake;

        lay
            .Col(0, 0,  NScheme::NTypeIds::Uint64)
            .Col(0, 8,  NScheme::NTypeIds::Uint32)
            .Key({ 0 });

        fake
            .Col(0, 0,  NScheme::NTypeIds::Uint64)
            .Col(0, 1,  NScheme::NTypeIds::Uint32, Cimple(3_u32))
            .Col(0, 2,  NScheme::NTypeIds::Uint32, Cimple(7_u32))
            .Col(0, 8,  NScheme::NTypeIds::Uint32, Cimple(99_u32))
            .Key({ 0, 1, 2 });

        auto eggs =
            TPartCook(lay, { true, 4096 })
                .AddN(10_u64, 77_u32)
                .AddN(12_u64, 44_u32)
                .AddN(17_u64, nullptr)  /* explicit null    */
                .AddN(19_u64)           /* default value    */
                .Finish();

        eggs.Scheme = fake.RowScheme();

        TCheckIt wrap(std::move(eggs), { });

        auto trA = *TSchemedCookRow(*fake).Col(10_u64, 3_u32, 7_u32, 77_u32);
        auto trB = *TSchemedCookRow(*fake).Col(12_u64, 3_u32, 7_u32, 44_u32);
        auto trC = *TSchemedCookRow(*fake).Col(17_u64, 3_u32, 7_u32, nullptr);
        auto trD = *TSchemedCookRow(*fake).Col(19_u64, 3_u32, 7_u32, 99_u32);

        auto low = *TSchemedCookRow(*fake).Col(10_u64, 2_u32, 2_u32);
        auto mid = *TSchemedCookRow(*fake).Col(10_u64, 3_u32, 5_u32);
        auto upp = *TSchemedCookRow(*fake).Col(10_u64, 3_u32, 8_u32);

        wrap.To(10).Has(trA);   /* default values in key    */
        wrap.To(11).Has(trB);   /* default values in key    */
        wrap.To(12).Has(trC);   /* explicit null in values  */
        wrap.To(13).Has(trD);   /* default value in values  */
        wrap.To(20).Seek(trA, ESeek::Lower).Is(trA);
        wrap.To(21).Seek(trA, ESeek::Upper).Is(trB);
        wrap.To(22).Seek(low, ESeek::Lower).Is(trA);
        wrap.To(23).Seek(low, ESeek::Exact).Is(EReady::Gone);
        wrap.To(24).Seek(mid, ESeek::Lower).Is(trA);
        wrap.To(25).Seek(mid, ESeek::Upper).Is(trA);
        wrap.To(26).Seek(mid, ESeek::Exact).Is(EReady::Gone);
        wrap.To(27).Seek(upp, ESeek::Lower).Is(trB);
        wrap.To(28).Seek(upp, ESeek::Upper).Is(trB);
        wrap.To(29).Seek(upp, ESeek::Exact).Is(EReady::Gone);
    }

    Y_UNIT_TEST(Matter)
    {
        TLayoutCook lay;

        lay.Col(0, 0, ETypes::Uint32).Col(0, 1, ETypes::String).Key({ 0 });

        const auto foo = *TSchemedCookRow(*lay).Col(7_u32, TString(128, 'x'));

        TCheckIt wrap(TPartCook(lay, { true, 99, 32 }).Add(foo).Finish(), { });

        wrap.To(10).Seek(foo, ESeek::Exact).To(11).Is(foo);

        auto &part = dynamic_cast<const NTest::TPartStore&>(*(*wrap).Eggs.Lone());

        const auto glob = part.Store->GlobForBlob(0);

        { /*_ Check that part has exactly one external blob */
            UNIT_ASSERT(part.Store->PageCollectionPagesCount(part.Store->GetExternRoom()) == 1);
            UNIT_ASSERT(part.Blobs->Total() == 1);
            UNIT_ASSERT(part.Blobs->Glob(0) == glob);
            UNIT_ASSERT(part.Large->Stats().Items == 1);
            UNIT_ASSERT(part.Large->Relation(0).Row == 0);
            UNIT_ASSERT(part.Large->Relation(0).Tag == 1);
        }

        { /*_ Check access to external blob out of cache */
            wrap.Displace<IPages>(new TNoEnv{ true, ELargeObjNeed::Yes });

            wrap.To(12).Seek(foo, ESeek::Exact).Is(EReady::Page);
        }

        { /*_ Check correctness of reference to TGlobId array */
            wrap.Displace<IPages>(new TNoEnv{ true, ELargeObjNeed::No });
            wrap.To(14).Seek(foo, ESeek::Exact);

            const auto cell = (*wrap).Apply().Get(1);

            UNIT_ASSERT(cell.Data() == (void*)&(**part.Blobs)[0]);
            UNIT_ASSERT(cell.Size() == sizeof(NPageCollection::TGlobId));
        }

        { /*_ Check marker of absent external blob in TCell */
            wrap.Displace<IPages>(new TNoEnv{ true, ELargeObjNeed::No });

            auto marked = *TSchemedCookRow(*lay).Col(7_u32, glob);

            wrap.To(16).Seek(foo, ESeek::Exact).Is(marked);
        }

        DumpPart(*(*wrap).Eggs.Lone(), 10);
    }

    Y_UNIT_TEST(External)
    {
        TLayoutCook lay;

        lay.Col(0, 0, ETypes::Uint32).Col(0, 1, ETypes::String).Key({ 0 });

        TPartCook cook(lay, { });

        const auto glob = cook.PutBlob(TString(128, 'x'), 0);
        const auto foo = *TSchemedCookRow(*lay).Col(7_u32,  glob);

        TCheckIt wrap(cook.Add(foo).Finish(), { });

        wrap.Displace<IPages>(new TNoEnv{ true, ELargeObjNeed::No });
        wrap.To(10).Seek(foo, ESeek::Exact).To(11).Is(foo);

        { /*_ Check that part has correct glob in its catalog  */
            auto &part = *(*wrap).Eggs.Lone();

            UNIT_ASSERT(part.Blobs->Total() == 1);
            UNIT_ASSERT(part.Large->Stats().Items == 1);
            UNIT_ASSERT(part.Blobs->Glob(0) == glob);
        }

        DumpPart(*(*wrap).Eggs.Lone(), 10);
    }

    Y_UNIT_TEST(Outer)
    {
        using namespace NTable::NTest;

        TLayoutCook lay;

        lay.Col(0, 0, ETypes::Uint32).Col(0, 1, ETypes::String).Key({ 0 });

        TPartCook cook(lay, PageConf());

        const auto foo = *TSchemedCookRow(*lay).Col(7_u32, TString(24, 'x'));
        const auto bar = *TSchemedCookRow(*lay).Col(8_u32, TString(10, 'x'));

        TCheckIt wrap(cook.Add(foo).Add(bar).Finish(), { });

        wrap.To(10).Has(foo, bar);

        { /*_ Check that part has correct glob in its catalog  */
            auto &part = *(*wrap).Eggs.Lone();

            UNIT_ASSERT_C(part.Small, "Writer didn't produced small blobs");
            UNIT_ASSERT(part.Small->Stats().Items == 1);
        }

        DumpPart(*(*wrap).Eggs.Lone(), 10);
    }

    Y_UNIT_TEST(MassCheck)
    {
        UNIT_ASSERT_C(Eggs0().Parts.size() == 1, "Eggs0 has " << Eggs0().Parts.size() << "p");

        auto &part = *Eggs0().Lone();
        auto pages = part.Index->End() - part.Index->Begin();
        auto minIndex = PageConf().Groups.at(0).IndexMin * 8;

        auto cWidth = [](const NPage::TFrames *frames, ui32 span) -> ui32 {
            if (nullptr == frames) return 0;

            ui32 page = 0, found = 0;

            while (auto rel = frames->Relation(page)) {
                UNIT_ASSERT(rel.Refer <= 0);

                found += bool(ui32(-rel.Refer) >= span);
                page = page - rel.Refer;
            }

            return found;
        };

        /*_ Ensure that produced part has enough pages for code coverage and
            index grow algorithm in data pages writer has been triggered. */

        UNIT_ASSERT(pages > 100 && part.Index.RawSize() >= minIndex);

        { /*_ Ensure that part has some external blobs written to room 1 */
            auto one = Eggs0().Lone()->Blobs->Total();
            auto two = Eggs0().Lone()->Large->Stats().Items;

            UNIT_ASSERT(one && one == two && one == part.Store->PageCollectionPagesCount(part.Store->GetExternRoom()));
        }

        { /*_ Ensure that part has some outer packed blobs in room 2 */
            auto one = Eggs0().Lone()->Small->Stats().Items;

            UNIT_ASSERT(one && one == part.Store->PageCollectionPagesCount(part.Store->GetOuterRoom()));
        }

        { /*_ Enusre there is some rows with two cells with references */

            ui32 large = cWidth(Eggs0().Lone()->Large.Get(), 2);
            ui32 small = cWidth(Eggs0().Lone()->Small.Get(), 2);

            UNIT_ASSERT_C(small > 10, "Eggs0 has trivial outer packed set");
            UNIT_ASSERT_C(large > 10, "Eggs0 has trivial external blobs set");
        }

        { /*_  Check that the last key matches in the index */
            UNIT_ASSERT_VALUES_EQUAL(part.Index.Rows(), Mass0().Saved.Size());
            const NPage::TCompare<NPage::TIndex::TRecord> cmp(part.Scheme->Groups[0].ColsKeyIdx, *Eggs0().Scheme->Keys);
            auto lastKey = TRowTool(*Eggs0().Scheme).KeyCells(Mass0().Saved[Mass0().Saved.Size()-1]);
            UNIT_ASSERT_VALUES_EQUAL(cmp.Compare(*part.Index.GetLastKeyRecord(), lastKey), 0);
        }

        { /*_  Check that part has correct number of slices */
            UNIT_ASSERT_C(part.Slices, "Part was generated without slices");
            UNIT_ASSERT_C(part.Slices->size() > 1, "Slice items " << +part.Slices->size());
        }

        DumpPart(*Eggs0().Lone(), 1);
    }

    Y_UNIT_TEST(WreckPart)
    {
        TWreck<TCheckIt, TPartEggs>(Mass0(), 666).Do(EWreck::Cached, Eggs0());
    }

    Y_UNIT_TEST(PageFailEnv)
    {
        TWreck<TCheckIt, TPartEggs>(Mass0(), 666).Do(EWreck::Evicted, Eggs0());
    }

    Y_UNIT_TEST(ForwardEnv)
    {
        TWreck<TCheckIt, TPartEggs>(Mass0(), 666).Do(EWreck::Forward, Eggs0());
    }

    Y_UNIT_TEST(WreckPartColumnGroups)
    {
        TWreck<TCheckIt, TPartEggs>(Mass1(), 666).Do(EWreck::Cached, Eggs1());
    }

    Y_UNIT_TEST(PageFailEnvColumnGroups)
    {
        TWreck<TCheckIt, TPartEggs>(Mass1(), 666).Do(EWreck::Evicted, Eggs1());
    }

    Y_UNIT_TEST(ForwardEnvColumnGroups)
    {
        TWreck<TCheckIt, TPartEggs>(Mass1(), 666).Do(EWreck::Forward, Eggs1());
    }

    Y_UNIT_TEST(Versions)
    {
        using namespace NTable::NTest;

        TLayoutCook lay;

        lay
            .Col(0, 0, NScheme::NTypeIds::Uint32)
            .Col(0, 1, NScheme::NTypeIds::String)
            .Col(1, 2, NScheme::NTypeIds::Double)
            .Col(2, 3, NScheme::NTypeIds::Bool)
            .Key({ 0 });

        const auto hey = *TSchemedCookRow(*lay).Col(333_u32, "hey", 0.25, true);
        const auto foo = *TSchemedCookRow(*lay).Col(555_u32);
        const auto foo1 = *TSchemedCookRow(*lay).Col(555_u32, "foo1", 3.14, nullptr);
        const auto foo2 = *TSchemedCookRow(*lay).Col(555_u32, "foo2", 4.25, false);
        const auto foo3 = *TSchemedCookRow(*lay).Col(555_u32, "foo3", 7.75, true);
        const auto bar = *TSchemedCookRow(*lay).Col(777_u32, "bar", 2.72, true);
        const auto baz = *TSchemedCookRow(*lay).Col(999_u32, "baz", 0.0, false);

        auto conf = PageConf(/* groups = */ 3);
        conf.MinRowVersion = TRowVersion(0, 50);

        auto eggs = TPartCook(lay, conf)
            .Ver({5, 50}).Add(hey)
            .Ver({4, 0}).Add(foo1, ERowOp::Erase)
            .Ver({3, 30}).Add(foo1)
            .Ver({2, 20}).Add(foo2, ERowOp::Erase)
            .Ver({1, 10}).Add(foo2)
            .Ver({0, 99}).Add(foo3)
            .Ver({4, 40}).Add(bar)
            .Ver({5, 50}).Add(baz)
            .Ver({5, 0}).Add(baz, ERowOp::Erase)
            .Finish();
        auto part = eggs.Lone();

        UNIT_ASSERT_VALUES_EQUAL(part->Groups, 3u);
        UNIT_ASSERT_VALUES_EQUAL(part->MinRowVersion, TRowVersion(0, 99));
        UNIT_ASSERT_VALUES_EQUAL(part->MaxRowVersion, TRowVersion(5, 50));

        TCheckIt wrap(eggs, { });

        wrap.To(10).Has(hey).NoKey(foo, false).Has(bar);

        wrap.To(20).Seek(foo, ESeek::Lower).IsOp(ERowOp::Erase, foo).IsVer({4, 0})
            .To(21).Ver({4, 0}).IsOp(ERowOp::Erase, foo).IsVer({4, 0})
            .To(22).Ver({3, 99}).Is(foo1).IsVer({3, 30})
            .To(23).Ver({3, 30}).Is(foo1).IsVer({3, 30})
            .To(24).Ver({2, 99}).IsOp(ERowOp::Erase, foo).IsVer({2, 20})
            .To(25).Ver({2, 20}).IsOp(ERowOp::Erase, foo).IsVer({2, 20})
            .To(26).Ver({1, 99}).Is(foo2).IsVer({1, 10})
            .To(27).Ver({1, 10}).Is(foo2).IsVer({1, 10})
            .To(28).Ver({1, 9}).Is(foo3).IsVer({0, 99});

        wrap.To(30).Seek(bar, ESeek::Lower).Is(bar).IsVer({4, 40})
            .To(31).Ver({4, 40}).Is(bar).IsVer({4, 40})
            .To(32).Ver({4, 39}).Is(EReady::Gone)
            .To(33).Next().Is(baz).IsVer({5, 50})
            .To(34).Ver({5, 49}).Is(EReady::Gone);

        // This verifies that using the same version multiple times is a no-op
        wrap.To(40).Seek(foo, ESeek::Exact).Is(EReady::Data)
            .To(41).Ver({1, 15}).Is(foo2).IsVer({1, 10})
            .To(42).Ver({1, 15}).Is(foo2).IsVer({1, 10});

        // This verifies that history data is correctly repositioned to an
        // earlier version after seeking backwards in part iterator.
        wrap.To(50).Seek(foo, ESeek::Exact).IsOp(ERowOp::Erase, foo).IsVer({4, 0})
            .To(51).Ver({1, 9}).Is(foo3).IsVer({0, 99})
            .To(52).SeekAgain(foo, ESeek::Exact).IsOp(ERowOp::Erase, foo).IsVer({4, 0})
            .To(53).Ver({1, 10}).Is(foo2).IsVer({1, 10});

        // This verifies that using a version that is too low returns EReady::Gone
        wrap.To(60).Seek(foo, ESeek::Exact).Is(EReady::Data)
            .To(61).Ver({0, 98}).Is(EReady::Gone);

        // This verifies that first unversioned row on a page works correctly
        wrap.To(70).Seek(hey, ESeek::Exact).Is(hey).IsVer({5, 50})
            .To(71).Ver({5, 50}).Is(hey).IsVer({5, 50})
            .To(72).Ver({5, 49}).Is(EReady::Gone)
            .To(73).Next().IsOp(ERowOp::Erase, foo).IsVer({4, 0});
    }

    Y_UNIT_TEST(ManyVersions)
    {
        using namespace NTable::NTest;

        TLayoutCook lay;

        lay
            .Col(0, 0, NScheme::NTypeIds::Uint32)
            .Col(0, 1, NScheme::NTypeIds::String)
            .Col(1, 2, NScheme::NTypeIds::Double)
            .Col(2, 3, NScheme::NTypeIds::Bool)
            .Key({ 0 });

        const auto foo = *TSchemedCookRow(*lay).Col(555_u32);
        const auto bar = *TSchemedCookRow(*lay).Col(777_u32);

        auto conf = PageConf(/* groups = */ 3);
        auto cook = TPartCook(lay, conf);

        TVector<TRow> foos(Reserve(1001));
        TVector<TRowVersion> foov(Reserve(1001));
        for (int i = 0; i <= 1000; ++i) {
            TString s = TStringBuilder() << "foo_xxxxxxxxxxxxxxxx" << i;
            foov.emplace_back(1, 1000-i);
            foos.emplace_back(*TSchemedCookRow(*lay).Col(555_u32, s.c_str(), i * 0.1, nullptr));
            cook.Ver(foov.back()).Add(foos.back());
        }

        TVector<TRow> bars(Reserve(1001));
        TVector<TRowVersion> barv(Reserve(1001));
        for (int i = 0; i <= 1000; ++i) {
            TString s = TStringBuilder() << "bar_xxxxxxxxxxxxxxxx" << i;
            barv.emplace_back(2, 1000-i);
            bars.emplace_back(*TSchemedCookRow(*lay).Col(777_u32, s.c_str(), i * 0.2, i % 2 == 0));
            cook.Ver(barv.back()).Add(bars.back());
        }

        // This row lowers min version, so we can trigger necessary search paths
        const auto baz = *TSchemedCookRow(*lay).Col(999_u32, "baz", 0.42, true);
        cook.Ver({0, 42}).Add(baz);

        auto eggs = cook.Finish();
        auto part = eggs.Lone();

        UNIT_ASSERT_VALUES_EQUAL(part->Groups, 3u);
        UNIT_ASSERT_VALUES_EQUAL(part->MinRowVersion, TRowVersion(0, 42));
        UNIT_ASSERT_VALUES_EQUAL(part->MaxRowVersion, TRowVersion(2, 1000));

        TCheckIt wrap(eggs, { });

        wrap.To(10).Has(foos[0]).Has(bars[0]);

        // Test that linear search falls back to binary search correctly
        wrap.To(20).Seek(foo, ESeek::Exact).Is(foos[0]).IsVer(foov[0])
            .To(21).Ver(foov[2]).Is(foos[2]).IsVer(foov[2])
            .To(21).Ver(foov[12]).Is(foos[12]).IsVer(foov[12])
            .To(22).Ver(foov[500]).Is(foos[500]).IsVer(foov[500]);

        // Test that binary search stops correctly when there is no such version
        wrap.To(30).Seek(foo, ESeek::Exact).Is(foos[0]).IsVer(foov[0])
            .To(31).Ver(foov[980]).Is(foos[980]).IsVer(foov[980])
            .To(32).Ver({0, 99}).Is(EReady::Gone)
            .To(33).Next().Is(bars[0]).IsVer(barv[0])
            .To(34).Ver(barv[1]).Is(bars[1]).IsVer(barv[1])
            .To(35).Ver({1, 1000}).Is(EReady::Gone);
    }

    Y_UNIT_TEST(ManyDeltas) {
        TLayoutCook lay;

        lay
            .Col(0, 0, NScheme::NTypeIds::Uint32)
            .Col(0, 1, NScheme::NTypeIds::String)
            .Col(0, 2, NScheme::NTypeIds::String)
            .Col(0, 3, NScheme::NTypeIds::String)
            .Key({ 0 });

        auto conf = PageConf();
        auto cook = TPartCook(lay, conf);

        const auto foo = *TSchemedCookRow(*lay).Col(555_u32);
        const auto bar = *TSchemedCookRow(*lay).Col(777_u32);

        for (int i = 1; i <= 1000; ++i) {
            TTag tag = 1 + (i - 1) % 3;
            TString s = TStringBuilder() << "foo_xxxxxxxxxxxxxxxx" << i;
            cook.Delta(i).Add(TRow().Do(0, 555_u32).Do(tag, s.c_str()));
        }

        auto cooked = cook.Finish();

        TSubset subset(TEpoch::Zero(), cooked.Scheme);
        for (const auto &part : cooked.Parts) {
            Y_VERIFY(part->Slices, "Missing part slices");
            subset.Flatten.push_back({ part, nullptr, part->Slices });
        }
        for (int i = 1; i <= 1000; ++i) {
            subset.CommittedTransactions.Add(i, TRowVersion(1, i));
        }

        // This just verifies we can compact that many deltas with small blobs without crashing
        auto cooked2 = TCompaction(new TForwardEnv(512, 1024), conf).Do(subset);
    }

}

}
}
