// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "squash/zucchini/reference_set.h"

#include <algorithm>
#include <iterator>

#include "squash/base/logging.h"
#include "squash/base/macros.h"
#include "squash/zucchini/target_pool.h"

namespace zucchini {

namespace {

// Returns true is |refs| is sorted.
bool IsReferenceListSorted(const std::vector<Reference>& refs) {
  return std::is_sorted(
      refs.begin(), refs.end(),
      [](const Reference& a, const Reference& b) {
        return a.location < b.location;
      });
}

}  // namespace

ReferenceSet::ReferenceSet(const ReferenceTypeTraits& traits,
                           const TargetPool& target_pool)
    : traits_(traits), target_pool_(target_pool) {}
ReferenceSet::ReferenceSet(ReferenceSet&&) = default;
ReferenceSet::~ReferenceSet() = default;

void ReferenceSet::InitReferences(ReferenceReader&& ref_reader) {
  DCHECK(references_.empty());
  for (auto ref = ref_reader.GetNext(); ref.has_value();
       ref = ref_reader.GetNext()) {
    references_.push_back(
        {ref->location, ref->target});
  }
  DCHECK(IsReferenceListSorted(references_));
}

void ReferenceSet::InitReferences(const std::vector<Reference>& refs) {
  DCHECK(references_.empty());
  references_.reserve(refs.size());
  std::transform(refs.begin(), refs.end(), std::back_inserter(references_),
                 [&](const Reference& ref) -> Reference {
                   return {ref.location, target_pool_.KeyForOffset(ref.target)};
                 });
  DCHECK(IsReferenceListSorted(references_));
}

Reference ReferenceSet::at(offset_t offset) const {
  auto pos = std::upper_bound(references_.begin(), references_.end(), offset,
                              [](offset_t a, const Reference& ref) {
                                return a < ref.location;
                              });

  DCHECK(pos != references_.begin());  // Iterators.
  --pos;
  DCHECK_LT(offset, pos->location + width());
  return *pos;
}

}  // namespace zucchini
