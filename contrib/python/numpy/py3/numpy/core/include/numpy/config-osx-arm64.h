#pragma once

#include "config-osx-x86_64.h"

#undef HAVE_XMMINTRIN_H
#undef HAVE_EMMINTRIN_H
#undef HAVE_IMMINTRIN_H

#undef HAVE_LDOUBLE_INTEL_EXTENDED_16_BYTES_LE
#define HAVE_LDOUBLE_IEEE_DOUBLE_LE 1
