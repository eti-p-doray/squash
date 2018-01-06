// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQUASH_BASE_OPTIONAL_H_
#define SQUASH_BASE_OPTIONAL_H_

#include "absl/types/optional.h"

namespace base {

template <class T>
using Optional = absl::optional<T>;

using absl::bad_optional_access;
using absl::make_optional;
using absl::nullopt_t;
using absl::nullopt;

}

#endif  // SQUASH_BASE_OPTIONAL_H_