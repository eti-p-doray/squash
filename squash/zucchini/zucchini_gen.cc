// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "squash/zucchini/zucchini_gen.h"

#include <memory>
#include <utility>

#include "squash/base/logging.h"
#include "squash/zucchini/disassembler.h"
#include "squash/zucchini/element_detection.h"
#include "squash/zucchini/encoded_view.h"
#include "squash/zucchini/equivalence_map.h"
#include "squash/zucchini/image_index.h"
#include "squash/zucchini/patch_writer.h"
#include "squash/zucchini/suffix_array.h"
#include "squash/zucchini/targets_affinity.h"

namespace zucchini {

namespace {

// Parameters for patch generation.
constexpr double kMinEquivalenceSimilarity = 12.0;
constexpr double kLargeEquivalenceSimilarity = 64.0;
constexpr size_t kNumIterations = 2;

}  // namespace

std::vector<offset_t> FindExtraTargets(const TargetPool& projected_old_targets,
                                       const TargetPool& new_targets) {
  std::vector<offset_t> extra_targets;
  std::set_difference(
      new_targets.begin(), new_targets.end(), projected_old_targets.begin(),
      projected_old_targets.end(), std::back_inserter(extra_targets));
  return extra_targets;
}

EquivalenceMap CreateEquivalenceMap(const ImageIndex& old_image_index,
                                    const ImageIndex& new_image_index) {
  // Label matching (between "old" and "new") can guide EquivalenceMap
  // construction; but EquivalenceMap induces Label matching. This apparent
  // "chick and egg" problem is solved by multiple iterations alternating 2
  // steps:
  // - Association of targets based on previous EquivalenceMap. Note that the
  //   EquivalenceMap is empty on first iteration, so this is a no-op.
  // - Construction of refined EquivalenceMap based on new targets associations.
  TargetPool old_targets;
  TargetPool new_targets;
  for (const auto& old_target_pool : old_image_index.target_pools())
    old_targets.InsertTargets(old_target_pool.second.targets());
  for (const auto& new_target_pool : new_image_index.target_pools())
    new_targets.InsertTargets(new_target_pool.second.targets());

  TargetsAffinity targets_affinity(&old_targets, &new_targets);

  EquivalenceMap equivalence_map;
  for (size_t i = 0; i < kNumIterations; ++i) {
    EncodedView old_view(old_image_index, &old_targets);
    EncodedView new_view(new_image_index, &new_targets);

    // Associate targets from "old" to "new" image based on |equivalence_map|
    // for each reference pool.
    targets_affinity.InferFromSimilarities(equivalence_map);

    // Creates labels for strongly associated targets.
    std::vector<uint32_t> old_labels;
    std::vector<uint32_t> new_labels;
    size_t label_bound = targets_affinity.AssignLabels(
        kLargeEquivalenceSimilarity, &old_labels, &new_labels);
    old_view.SetLabels(std::move(old_labels), label_bound);
    new_view.SetLabels(std::move(new_labels), label_bound);

    // Build equivalence map, where references in "old" and "new" that
    // share common semantics (i.e., their respective targets were associated
    // earlier on) are considered equivalent.
    equivalence_map.Build(
        MakeSuffixArray<InducedSuffixSort>(old_view, old_view.Cardinality()),
        old_view, new_view, targets_affinity, kMinEquivalenceSimilarity);
  }

  return equivalence_map;
}

bool GenerateEquivalencesAndExtraData(ConstBufferView new_image,
                                      const EquivalenceMap& equivalence_map,
                                      PatchElementWriter* patch_writer) {
  // Make 2 passes through |equivalence_map| to reduce write churn.
  // Pass 1: Write all equivalences.
  EquivalenceSink equivalences_sink;
  for (const EquivalenceCandidate& candidate : equivalence_map)
    equivalences_sink.PutNext(candidate.eq);
  patch_writer->SetEquivalenceSink(std::move(equivalences_sink));

  // Pass 2: Write data in gaps in |new_image| before / between  after
  // |equivalence_map| as "extra data".
  ExtraDataSink extra_data_sink;
  offset_t dst_offset = 0;
  for (const EquivalenceCandidate& candidate : equivalence_map) {
    extra_data_sink.PutNext(
        new_image[{dst_offset, candidate.eq.dst_offset - dst_offset}]);
    dst_offset = candidate.eq.dst_end();
    DCHECK_LE(dst_offset, new_image.size());
  }
  extra_data_sink.PutNext(
      new_image[{dst_offset, new_image.size() - dst_offset}]);
  patch_writer->SetExtraDataSink(std::move(extra_data_sink));
  return true;
}

bool GenerateRawDelta(ConstBufferView old_image,
                      ConstBufferView new_image,
                      const EquivalenceMap& equivalence_map,
                      const ImageIndex& new_image_index,
                      PatchElementWriter* patch_writer) {
  RawDeltaSink raw_delta_sink;

  // Visit |equivalence_map| blocks in |new_image| order. Find and emit all
  // bytewise differences.
  offset_t base_copy_offset = 0;
  for (const EquivalenceCandidate& candidate : equivalence_map) {
    Equivalence equivalence = candidate.eq;
    // For each bytewise delta from |old_image| to |new_image|, compute "copy
    // offset" and pass it along with delta to the sink.
    for (offset_t i = 0; i < equivalence.length; ++i) {
      if (new_image_index.IsReference(equivalence.dst_offset + i))
        continue;  // Skip references since they're handled elsewhere.

      int8_t diff = new_image[equivalence.dst_offset + i] -
                    old_image[equivalence.src_offset + i];
      if (diff)
        raw_delta_sink.PutNext({base_copy_offset + i, diff});
    }
    base_copy_offset += equivalence.length;
  }
  patch_writer->SetRawDeltaSink(std::move(raw_delta_sink));
  return true;
}

bool GenerateReferencesDelta(const ReferenceSet& src_refs,
                             const ReferenceSet& dst_refs,
                             const TargetPool& projected_target_pool,
                             const OffsetMapper& offset_mapper,
                             const EquivalenceMap& equivalence_map,
                             ReferenceDeltaSink* reference_delta_sink) {
  size_t ref_width = src_refs.width();
  auto dst_ref = dst_refs.begin();

  // For each equivalence, for each covered |dst_ref| and the matching
  // |src_ref|, emit the delta between the respective target labels. Note: By
  // construction, each reference location (with |ref_width|) lies either
  // completely inside an equivalence or completely outside. We perform
  // "straddle checks" throughout to verify this assertion.
  for (const auto& candidate : equivalence_map) {
    const Equivalence equiv = candidate.eq;
    // Increment |dst_ref| until it catches up to |equiv|.
    while (dst_ref != dst_refs.end() && dst_ref->location < equiv.dst_offset)
      ++dst_ref;
    if (dst_ref == dst_refs.end())
      break;
    if (dst_ref->location >= equiv.dst_end())
      continue;
    // Straddle check.
    DCHECK_LE(dst_ref->location + ref_width, equiv.dst_end());

    offset_t src_loc =
        equiv.src_offset + (dst_ref->location - equiv.dst_offset);
    auto src_ref = std::lower_bound(
        src_refs.begin(), src_refs.end(), src_loc,
        [](const Reference& a, offset_t b) { return a.location < b; });
    for (; dst_ref != dst_refs.end() &&
           dst_ref->location + ref_width <= equiv.dst_end();
         ++dst_ref, ++src_ref) {
      // Local offset of |src_ref| should match that of |dst_ref|.
      DCHECK_EQ(src_ref->location - equiv.src_offset,
                dst_ref->location - equiv.dst_offset);
      //key_t src_target_key = sr
      offset_t old_offset = src_ref->target;
      offset_t new_expected_offset = offset_mapper.ProjectOffset(old_offset);
      offset_t new_expected_key =
          projected_target_pool.KeyForOffset(new_expected_offset);
      offset_t new_offset = dst_ref->target;
      offset_t new_key = projected_target_pool.KeyForOffset(new_offset);

      reference_delta_sink->PutNext(
          static_cast<int32_t>(new_key - new_expected_key));
    }
    if (dst_ref == dst_refs.end())
      break;  // Done.
    // Straddle check.
    DCHECK_GE(dst_ref->location, equiv.dst_end());
  }
  return true;
}

bool GenerateExtraTargets(const std::vector<offset_t>& extra_targets,
                          PoolTag pool_tag,
                          PatchElementWriter* patch_writer) {
  TargetSink target_sink;
  for (offset_t target : extra_targets)
    target_sink.PutNext(target);
  patch_writer->SetTargetSink(pool_tag, std::move(target_sink));
  return true;
}

bool GenerateRawElement(const std::vector<offset_t>& old_sa,
                        ConstBufferView old_image,
                        ConstBufferView new_image,
                        PatchElementWriter* patch_writer) {
  ImageIndex old_image_index(old_image);
  ImageIndex new_image_index(new_image);

  EquivalenceMap equivalences;
  equivalences.Build(old_sa, EncodedView(old_image_index, nullptr),
                     EncodedView(new_image_index, nullptr), {},
                     kMinEquivalenceSimilarity);

  patch_writer->SetReferenceDeltaSink({});
  return GenerateEquivalencesAndExtraData(new_image, equivalences,
                                          patch_writer) &&
         GenerateRawDelta(old_image, new_image, equivalences, new_image_index,
                          patch_writer);
}

bool GenerateExecutableElement(ExecutableType exe_type,
                               ConstBufferView old_image,
                               ConstBufferView new_image,
                               PatchElementWriter* patch_writer) {
  // Initialize Disassemblers.
  std::unique_ptr<Disassembler> old_disasm =
      MakeDisassemblerOfType(old_image, exe_type);
  std::unique_ptr<Disassembler> new_disasm =
      MakeDisassemblerOfType(new_image, exe_type);
  if (!old_disasm || !new_disasm) {
    LOG(ERROR) << "Failed to create Disassembler.";
    return false;
  }
  DCHECK_EQ(old_disasm->GetExeType(), new_disasm->GetExeType());

  // Initialize ImageIndexes.
  ImageIndex old_image_index(old_image);
  ImageIndex new_image_index(new_image);
  if (!old_image_index.Initialize(old_disasm.get()) ||
      !new_image_index.Initialize(new_disasm.get())) {
    LOG(ERROR) << "Failed to create ImageIndex: Overlapping references found?";
    return false;
  }

  DCHECK_EQ(old_image_index.PoolCount(), new_image_index.PoolCount());

  EquivalenceMap equivalence_map =
      CreateEquivalenceMap(old_image_index, new_image_index);
  OffsetMapper offset_mapper(equivalence_map);

  ReferenceDeltaSink reference_delta_sink;
  for (const auto& old_targets : old_image_index.target_pools()) {
    PoolTag pool_tag = old_targets.first;
    TargetPool projected_old_targets = old_targets.second;
    projected_old_targets.Project(offset_mapper);
    std::vector<offset_t> extra_target =
        FindExtraTargets(projected_old_targets, new_image_index.pool(pool_tag));
    projected_old_targets.InsertTargets(extra_target);

    if (!GenerateExtraTargets(extra_target, pool_tag, patch_writer))
      return false;
    for (TypeTag type_tag : old_targets.second.types()) {
      if (!GenerateReferencesDelta(old_image_index.refs(type_tag),
                                   new_image_index.refs(type_tag),
                                   projected_old_targets, offset_mapper,
                                   equivalence_map, &reference_delta_sink))
        return false;
    }
  }
  patch_writer->SetReferenceDeltaSink(std::move(reference_delta_sink));

  return GenerateEquivalencesAndExtraData(new_image, equivalence_map,
                                          patch_writer) &&
         GenerateRawDelta(old_image, new_image, equivalence_map,
                          new_image_index, patch_writer);
}

/******** Exported Functions ********/

status::Code GenerateEnsemble(ConstBufferView old_image,
                              ConstBufferView new_image,
                              EnsemblePatchWriter* patch_writer) {
  patch_writer->SetPatchType(PatchType::kEnsemblePatch);

  base::Optional<Element> old_element =
      DetectElementFromDisassembler(old_image);
  base::Optional<Element> new_element =
      DetectElementFromDisassembler(new_image);

  // If images do not match known executable formats, or if detected formats
  // don't match, fall back to generating a raw patch.
  if (!old_element.has_value() || !new_element.has_value() ||
      old_element->exe_type != new_element->exe_type) {
    LOG(WARNING) << "Fall back to raw mode.";
    return GenerateRaw(old_image, new_image, patch_writer);
  }

  if (old_element->region() != old_image.region() ||
      new_element->region() != new_image.region()) {
    LOG(ERROR) << "Ensemble patching is currently unsupported.";
    return status::kStatusFatal;
  }

  PatchElementWriter patch_element(ElementMatch{*old_element, *new_element});

  // TODO(etiennep): Fallback to raw mode with proper logging.
  if (!GenerateExecutableElement(old_element->exe_type, old_image, new_image,
                                 &patch_element))
    return status::kStatusFatal;
  patch_writer->AddElement(std::move(patch_element));
  return status::kStatusSuccess;
}

status::Code GenerateRaw(ConstBufferView old_image,
                         ConstBufferView new_image,
                         EnsemblePatchWriter* patch_writer) {
  patch_writer->SetPatchType(PatchType::kRawPatch);

  ImageIndex old_image_index(old_image);
  EncodedView old_view(old_image_index, nullptr);
  std::vector<offset_t> old_sa =
      MakeSuffixArray<InducedSuffixSort>(old_view, old_view.Cardinality());

  PatchElementWriter patch_element(
      {Element(old_image.region()), Element(new_image.region())});
  if (!GenerateRawElement(old_sa, old_image, new_image, &patch_element))
    return status::kStatusFatal;
  patch_writer->AddElement(std::move(patch_element));
  return status::kStatusSuccess;
}

}  // namespace zucchini
