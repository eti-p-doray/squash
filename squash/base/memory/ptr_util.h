// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQUASH_BASE_MEMORY_PTR_UTIL_H_
#define SQUASH_BASE_MEMORY_PTR_UTIL_H_

namespace base {

template <typename T, typename... Args>
auto MakeUnique(Args&&... args)
    -> decltype(std::make_unique<T>(std::forward<Args>(args)...)) {
  return std::make_unique<T>(std::forward<Args>(args)...);
}

}

#endif  // SQUASH_BASE_MEMORY_PTR_UTIL_H_
