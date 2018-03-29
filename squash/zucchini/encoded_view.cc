// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "squash/zucchini/encoded_view.h"

#include <algorithm>
#include <utility>

#include "squash/base/logging.h"

namespace zucchini {

EncodedView::EncodedView(const ImageIndex& image_index, const TargetPool* target_pool)
    : image_index_(image_index),
      target_pool_(target_pool) {}
EncodedView::~EncodedView() = default;

EncodedView::value_type EncodedView::Projection(offset_t location) const {
  DCHECK_LT(location, image_index_.size());

  // Find out what lies at |location|.
  TypeTag type = image_index_.LookupType(location);

  // |location| points into raw data.
  if (type == kNoTypeTag) {
    // The projection is the identity function on raw content.
    return image_index_.GetRawValue(location);
  }

  // |location| points into a Reference.
  const ReferenceSet& ref_set = image_index_.refs(type);
  Reference ref = ref_set.at(location);
  DCHECK_GE(location, ref.location);
  DCHECK_LT(location, ref.location + ref_set.width());

  // |location| is not the first byte of the reference.
  if (location != ref.location) {
    // Trailing bytes of a reference are all projected to the same value.
    return kReferencePaddingProjection;
  }

  PoolTag pool_tag = ref_set.pool_tag();

  key_t target_key = target_pool_->KeyForOffset(ref.target);

  // Targets with an associated Label will use its Label index in projection.
  DCHECK_EQ(target_pool_->size(), labels_.size());
  uint32_t label = labels_[target_key];

  // Projection is done on (|target|, |type|), shifted by a constant value to
  // avoid collisions with raw content.
  value_type projection = label;
  projection *= image_index_.TypeCount();
  projection += type.value();
  return projection + kBaseReferenceProjection;
}

size_t EncodedView::Cardinality() const {
  return bound_ * image_index_.TypeCount() + kBaseReferenceProjection;
}

void EncodedView::SetLabels(std::vector<uint32_t>&& labels,
                            size_t bound) {
  DCHECK_EQ(labels.size(), target_pool_->size());
  DCHECK(labels.empty() || *max_element(labels.begin(), labels.end()) < bound);
  labels_ = std::move(labels);
  bound_ = bound;
}

}  // namespace zucchini
