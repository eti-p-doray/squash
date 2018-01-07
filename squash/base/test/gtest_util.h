// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQUASH_BASE_TEST_GTEST_UTIL_H_
#define SQUASH_BASE_TEST_GTEST_UTIL_H_

#include "squash/base/logging.h"
#include "gtest/gtest.h"

// EXPECT/ASSERT_DCHECK_DEATH is intended to replace EXPECT/ASSERT_DEBUG_DEATH
// when the death is expected to be caused by a DCHECK. Contrary to
// EXPECT/ASSERT_DEBUG_DEATH however, it doesn't execute the statement in non-
// dcheck builds as DCHECKs are intended to catch things that should never
// happen and as such executing the statement results in undefined behavior
// (|statement| is compiled in unsupported configurations nonetheless).
// Death tests misbehave on Android.
#if ELPP_DEBUG_LOG && defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)

// EXPECT/ASSERT_DCHECK_DEATH tests verify that a DCHECK is hit ("Check failed"
// is part of the error message), but intentionally do not expose the gtest
// death test's full |regex| parameter to avoid users having to verify the exact
// syntax of the error message produced by the DCHECK.
#define EXPECT_DCHECK_DEATH(statement) EXPECT_DEATH(statement, "")
#define ASSERT_DCHECK_DEATH(statement) ASSERT_DEATH(statement, "")

#else
// ELPP_DEBUG_LOG && defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)

#define EXPECT_DCHECK_DEATH(statement)
#define ASSERT_DCHECK_DEATH(statement)

#endif
// ELPP_DEBUG_LOG && defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)

#endif  // SQUASH_BASE_TEST_GTEST_UTIL_H_