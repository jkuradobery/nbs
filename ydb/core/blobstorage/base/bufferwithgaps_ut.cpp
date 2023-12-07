#include "bufferwithgaps.h"
#include <library/cpp/testing/unittest/registar.h>

using NKikimr::TBufferWithGaps;

Y_UNIT_TEST_SUITE(BufferWithGaps) {

    Y_UNIT_TEST(Basic) {
        TBufferWithGaps buffer(0);
        TString data = "Hello!";
        buffer.SetData(TString(data));
        UNIT_ASSERT_STRINGS_EQUAL(data, buffer.Substr(0, buffer.Size()));
    }

    Y_UNIT_TEST(IsReadable) {
        TBufferWithGaps buffer(0);
        TString data = "Hello! How are you? I'm fine, and you? Me too, thanks!";
        TString gaps = "G           GGGG           GG              GGG G     G";
        buffer.SetData(TString(data));
        for (size_t k = 0; k < gaps.size(); ++k) {
            if (gaps[k] != ' ') {
                buffer.AddGap(k, k + 1);
            }
        }
        UNIT_ASSERT_EQUAL(buffer.Size(), data.size());
        for (size_t k = 0; k < buffer.Size(); ++k) {
            for (size_t len = 1; len <= buffer.Size() - k; ++len) {
                bool haveGaps = false;
                for (size_t i = k; i < k + len; ++i) {
                    if (gaps[i] != ' ') {
                        haveGaps = true;
                        break;
                    }
                }
                UNIT_ASSERT_EQUAL(!haveGaps, buffer.IsReadable(k, len));
                if (!haveGaps) {
                    UNIT_ASSERT_STRINGS_EQUAL(buffer.Substr(k, len), data.substr(k, len));
                }
            }
        }
    }

}
