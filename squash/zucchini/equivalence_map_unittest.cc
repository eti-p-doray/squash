// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "squash/zucchini/equivalence_map.h"

#include <cstring>
#include <utility>
#include <vector>

#include "squash/zucchini/encoded_view.h"
#include "squash/zucchini/image_index.h"
#include "squash/zucchini/suffix_array.h"
#include "squash/zucchini/targets_affinity.h"
#include "squash/zucchini/test_disassembler.h"
#include "gtest/gtest.h"

namespace zucchini {

namespace {

using OffsetVector = std::vector<offset_t>;

// Make all references 2 bytes long.
constexpr offset_t kReferenceSize = 2;

// Creates and initialize an ImageIndex from |a| and with 2 types of references.
// The result is populated with |refs0| and |refs1|. |a| is expected to be a
// string literal valid for the lifetime of the object.
ImageIndex MakeImageIndexForTesting(const char* a,
                                    std::vector<Reference>&& refs0,
                                    std::vector<Reference>&& refs1) {
  TestDisassembler disasm(
      {kReferenceSize, TypeTag(0), PoolTag(0)}, std::move(refs0),
      {kReferenceSize, TypeTag(1), PoolTag(0)}, std::move(refs1),
      {kReferenceSize, TypeTag(2), PoolTag(1)}, {});

  ImageIndex image_index(
      ConstBufferView(reinterpret_cast<const uint8_t*>(a), std::strlen(a)));

  EXPECT_TRUE(image_index.Initialize(&disasm));
  return image_index;
}

TargetsAffinity MakeTargetsAffinitiesForTesting(
    const ImageIndex& old_image_index,
    const ImageIndex& new_image_index,
    const EquivalenceMap& equivalence_map) {
  std::vector<TargetsAffinity> target_affinities(old_image_index.PoolCount());
  for (const auto& old_target_pool : old_image_index.target_pools()) {
    target_affinities[old_target_pool.first.value()].InferFromSimilarities(
        equivalence_map, old_target_pool.second.targets(),
        new_image_index.pool(old_target_pool.first).targets());
  }
  return target_affinities;
}

}  // namespace

TEST(EquivalenceMapTest, GetTokenSimilarity) {
  ImageIndex old_index = MakeImageIndexForTesting(
      "ab1122334455", {{2, 0}, {4, 1}, {6, 2}, {8, 2}}, {{10, 3}});
  ImageIndex new_index = MakeImageIndexForTesting(
      "a11b22334455", {{1, 0}, {4, 1}, {6, 2}, {8, 2}}, {{10, 3}});
  std::vector<TargetsAffinity> affinities = MakeTargetsAffinitiesForTesting(
      old_index, new_index,
      EquivalenceMap({{{0, 0, 1}, 1.0}, {{1, 2, 1}, 1.0}}));

  // Raw match.
  EXPECT_LT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 0, 0));
  // Raw mismatch.
  EXPECT_GT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 0, 1));
  EXPECT_GT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 1, 0));

  // Type mismatch.
  EXPECT_EQ(kMismatchFatal,
            GetTokenSimilarity(old_index, new_index, affinities, 0, 1));
  EXPECT_EQ(kMismatchFatal,
            GetTokenSimilarity(old_index, new_index, affinities, 2, 0));
  EXPECT_EQ(kMismatchFatal,
            GetTokenSimilarity(old_index, new_index, affinities, 2, 10));
  EXPECT_EQ(kMismatchFatal,
            GetTokenSimilarity(old_index, new_index, affinities, 10, 1));

  // Reference strong match.
  EXPECT_LT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 2, 1));
  EXPECT_LT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 4, 6));

  // Reference weak match.
  EXPECT_LT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 6, 4));
  EXPECT_LT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 4, 8));
  EXPECT_LT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 8, 4));

  // Weak match is not greater than strong match.
  EXPECT_LE(GetTokenSimilarity(old_index, new_index, affinities, 6, 4),
            GetTokenSimilarity(old_index, new_index, affinities, 2, 1));

  // Reference mismatch.
  EXPECT_GT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 2, 4));
  EXPECT_GT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 2, 6));
}

TEST(EquivalenceMapTest, GetEquivalenceSimilarity) {
  ImageIndex image_index =
      MakeImageIndexForTesting("abcdef1122", {{6, 0}}, {{8, 1}});
  std::vector<TargetsAffinity> affinities =
      MakeTargetsAffinitiesForTesting(image_index, image_index, {});

  // Sanity check. This is a no-op with length-0 equivalences.
  EXPECT_EQ(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {0, 0, 0}));
  EXPECT_EQ(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {0, 3, 0}));
  EXPECT_EQ(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {3, 0, 0}));

  EXPECT_LT(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {0, 0, 3}));
  EXPECT_GE(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {0, 3, 3}));
  EXPECT_GE(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {3, 0, 3}));

  EXPECT_LT(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {6, 6, 4}));
}

TEST(EquivalenceMapTest, ExtendEquivalenceForward) {
  auto test_extend_forward =
      [](const ImageIndex old_index, const ImageIndex new_index,
         const EquivalenceCandidate& equivalence, double base_similarity) {
        return ExtendEquivalenceForward(
                   old_index, new_index,
                   MakeTargetsAffinitiesForTesting(old_index, new_index, {}),
                   equivalence, base_similarity)
            .eq;
      };

  EXPECT_EQ(Equivalence({0, 0, 0}),
            test_extend_forward(MakeImageIndexForTesting("", {}, {}),
                                MakeImageIndexForTesting("", {}, {}),
                                {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({0, 0, 0}),
            test_extend_forward(MakeImageIndexForTesting("banana", {}, {}),
                                MakeImageIndexForTesting("zzzz", {}, {}),
                                {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({0, 0, 6}),
            test_extend_forward(MakeImageIndexForTesting("banana", {}, {}),
                                MakeImageIndexForTesting("banana", {}, {}),
                                {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({2, 2, 4}),
            test_extend_forward(MakeImageIndexForTesting("banana", {}, {}),
                                MakeImageIndexForTesting("banana", {}, {}),
                                {{2, 2, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({0, 0, 6}),
            test_extend_forward(MakeImageIndexForTesting("bananaxx", {}, {}),
                                MakeImageIndexForTesting("bananayy", {}, {}),
                                {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(
      Equivalence({0, 0, 8}),
      test_extend_forward(MakeImageIndexForTesting("banana11", {{6, 0}}, {}),
                          MakeImageIndexForTesting("banana11", {{6, 0}}, {}),
                          {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(
      Equivalence({0, 0, 6}),
      test_extend_forward(MakeImageIndexForTesting("banana11", {{6, 0}}, {}),
                          MakeImageIndexForTesting("banana22", {}, {{6, 0}}),
                          {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(
      Equivalence({0, 0, 17}),
      test_extend_forward(MakeImageIndexForTesting("bananaxxpineapple", {}, {}),
                          MakeImageIndexForTesting("bananayypineapple", {}, {}),
                          {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(
      Equivalence({3, 0, 19}),
      test_extend_forward(
          MakeImageIndexForTesting("foobanana11xxpineapplexx", {{9, 0}}, {}),
          MakeImageIndexForTesting("banana11yypineappleyy", {{6, 0}}, {}),
          {{3, 0, 0}, 0.0}, 8.0));
}

TEST(EquivalenceMapTest, ExtendEquivalenceBackward) {
  auto test_extend_backward =
      [](const ImageIndex old_index, const ImageIndex new_index,
         const EquivalenceCandidate& equivalence, double base_similarity) {
        return ExtendEquivalenceBackward(
                   old_index, new_index,
                   MakeTargetsAffinitiesForTesting(old_index, new_index, {}),
                   equivalence, base_similarity)
            .eq;
      };

  EXPECT_EQ(Equivalence({0, 0, 0}),
            test_extend_backward(MakeImageIndexForTesting("", {}, {}),
                                 MakeImageIndexForTesting("", {}, {}),
                                 {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({6, 4, 0}),
            test_extend_backward(MakeImageIndexForTesting("banana", {}, {}),
                                 MakeImageIndexForTesting("zzzz", {}, {}),
                                 {{6, 4, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({0, 0, 6}),
            test_extend_backward(MakeImageIndexForTesting("banana", {}, {}),
                                 MakeImageIndexForTesting("banana", {}, {}),
                                 {{6, 6, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({2, 2, 6}),
            test_extend_backward(MakeImageIndexForTesting("xxbanana", {}, {}),
                                 MakeImageIndexForTesting("yybanana", {}, {}),
                                 {{8, 8, 0}, 0.0}, 8.0));

  EXPECT_EQ(
      Equivalence({0, 0, 8}),
      test_extend_backward(MakeImageIndexForTesting("11banana", {{0, 0}}, {}),
                           MakeImageIndexForTesting("11banana", {{0, 0}}, {}),
                           {{8, 8, 0}, 0.0}, 8.0));

  EXPECT_EQ(
      Equivalence({2, 2, 6}),
      test_extend_backward(MakeImageIndexForTesting("11banana", {{0, 0}}, {}),
                           MakeImageIndexForTesting("22banana", {}, {{0, 0}}),
                           {{8, 8, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({0, 0, 17}),
            test_extend_backward(
                MakeImageIndexForTesting("bananaxxpineapple", {}, {}),
                MakeImageIndexForTesting("bananayypineapple", {}, {}),
                {{8, 8, 9}, 9.0}, 8.0));

  EXPECT_EQ(
      Equivalence({3, 0, 19}),
      test_extend_backward(
          MakeImageIndexForTesting("foobanana11xxpineapplexx", {{9, 0}}, {}),
          MakeImageIndexForTesting("banana11yypineappleyy", {{6, 0}}, {}),
          {{22, 19, 0}, 0.0}, 8.0));
}

TEST(EquivalenceMapTest, Build) {
  auto test_build_equivalence = [](const ImageIndex old_index,
                                   const ImageIndex new_index,
                                   double minimum_similarity) {
    auto affinities = MakeTargetsAffinitiesForTesting(old_index, new_index, {});

    EncodedView old_view(old_index);
    EncodedView new_view(new_index);

    for (const auto& old_target_pool : old_index.target_pools()) {
      std::vector<uint32_t> old_labels;
      std::vector<uint32_t> new_labels;
      size_t label_bound =
          affinities[old_target_pool.first.value()].AssignLabels(
              1.0, &old_labels, &new_labels);
      old_view.SetLabels(old_target_pool.first, std::move(old_labels),
                         label_bound);
      new_view.SetLabels(old_target_pool.first, std::move(new_labels),
                         label_bound);
    }

    std::vector<offset_t> old_sa =
        MakeSuffixArray<InducedSuffixSort>(old_view, old_view.Cardinality());

    EquivalenceMap equivalence_map;
    equivalence_map.Build(old_sa, old_view, new_view, affinities,
                          minimum_similarity);

    offset_t current_dst_offset = 0;
    offset_t coverage = 0;
    for (const auto& candidate : equivalence_map) {
      EXPECT_GE(candidate.eq.dst_offset, current_dst_offset);
      EXPECT_GT(candidate.eq.length, offset_t(0));
      EXPECT_LE(candidate.eq.src_offset + candidate.eq.length,
                old_index.size());
      EXPECT_LE(candidate.eq.dst_offset + candidate.eq.length,
                new_index.size());
      EXPECT_GE(candidate.similarity, minimum_similarity);
      current_dst_offset = candidate.eq.dst_offset;
      coverage += candidate.eq.length;
    }
    return coverage;
  };

  EXPECT_EQ(0U,
            test_build_equivalence(MakeImageIndexForTesting("", {}, {}),
                                   MakeImageIndexForTesting("", {}, {}), 4.0));

  EXPECT_EQ(0U, test_build_equivalence(
                    MakeImageIndexForTesting("", {}, {}),
                    MakeImageIndexForTesting("banana", {}, {}), 4.0));

  EXPECT_EQ(0U,
            test_build_equivalence(MakeImageIndexForTesting("banana", {}, {}),
                                   MakeImageIndexForTesting("", {}, {}), 4.0));

  EXPECT_EQ(0U, test_build_equivalence(
                    MakeImageIndexForTesting("banana", {}, {}),
                    MakeImageIndexForTesting("zzzz", {}, {}), 4.0));

  EXPECT_EQ(6U, test_build_equivalence(
                    MakeImageIndexForTesting("banana", {}, {}),
                    MakeImageIndexForTesting("banana", {}, {}), 4.0));

  EXPECT_EQ(6U, test_build_equivalence(
                    MakeImageIndexForTesting("bananaxx", {}, {}),
                    MakeImageIndexForTesting("bananayy", {}, {}), 4.0));

  EXPECT_EQ(8U, test_build_equivalence(
                    MakeImageIndexForTesting("banana11", {{6, 0}}, {}),
                    MakeImageIndexForTesting("banana11", {{6, 0}}, {}), 4.0));

  EXPECT_EQ(6U, test_build_equivalence(
                    MakeImageIndexForTesting("banana11", {{6, 0}}, {}),
                    MakeImageIndexForTesting("banana22", {}, {{6, 0}}), 4.0));

  EXPECT_EQ(
      15U,
      test_build_equivalence(
          MakeImageIndexForTesting("banana11pineapple", {{6, 0}}, {}),
          MakeImageIndexForTesting("banana22pineapple", {}, {{6, 0}}), 4.0));

  EXPECT_EQ(
      15U,
      test_build_equivalence(
          MakeImageIndexForTesting("bananaxxxxxxxxpineapple", {}, {}),
          MakeImageIndexForTesting("bananayyyyyyyypineapple", {}, {}), 4.0));

  EXPECT_EQ(
      19U,
      test_build_equivalence(
          MakeImageIndexForTesting("foobanana11xxpineapplexx", {{9, 0}}, {}),
          MakeImageIndexForTesting("banana11yypineappleyy", {{6, 0}}, {}),
          4.0));
}

TEST(EquivalenceMapTest, OffsetMapper) {}

TEST(EquivalenceMapTest, ProjectOffsets) {
  auto ProjectOffsetsTest = [](const OffsetMapper& offset_mapper,
                               std::initializer_list<offset_t> offsets) {
    OffsetVector offsets_vec(offsets);
    offset_mapper.ProjectOffsets(&offsets_vec);
    return offsets_vec;
  };

  OffsetMapper offset_mapper1({{0, 10, 2}, {2, 13, 1}, {4, 16, 2}});
  EXPECT_EQ(OffsetVector({10}), ProjectOffsetsTest(offset_mapper1, {0}));
  EXPECT_EQ(OffsetVector({13}), ProjectOffsetsTest(offset_mapper1, {2}));
  EXPECT_EQ(OffsetVector({}), ProjectOffsetsTest(offset_mapper1, {3}));
  EXPECT_EQ(OffsetVector({10, 13}), ProjectOffsetsTest(offset_mapper1, {0, 2}));
  EXPECT_EQ(OffsetVector({11, 13, 17}),
            ProjectOffsetsTest(offset_mapper1, {1, 2, 5}));
  EXPECT_EQ(OffsetVector({11, 17}),
            ProjectOffsetsTest(offset_mapper1, {1, 3, 5}));
  EXPECT_EQ(OffsetVector({10, 11, 13, 16, 17}),
            ProjectOffsetsTest(offset_mapper1, {0, 1, 2, 3, 4, 5, 6}));

  OffsetMapper offset_mapper2({{0, 10, 2}, {13, 2, 1}, {16, 4, 2}});
  EXPECT_EQ(OffsetVector({2}), ProjectOffsetsTest(offset_mapper2, {13}));
  EXPECT_EQ(OffsetVector({10, 2}), ProjectOffsetsTest(offset_mapper2, {0, 13}));
  EXPECT_EQ(OffsetVector({11, 2, 5}),
            ProjectOffsetsTest(offset_mapper2, {1, 13, 17}));
  EXPECT_EQ(OffsetVector({11, 5}),
            ProjectOffsetsTest(offset_mapper2, {1, 14, 17}));
  EXPECT_EQ(OffsetVector({10, 11, 2, 4, 5}),
            ProjectOffsetsTest(offset_mapper2, {0, 1, 13, 14, 16, 17, 18}));
}

TEST(EquivalenceMapTest, ProjectOffset) {
  OffsetMapper offset_mapper1({{0, 10, 2}, {2, 13, 1}, {4, 16, 2}});
  EXPECT_EQ(10U, offset_mapper1.ProjectOffset(0));
  EXPECT_EQ(11U, offset_mapper1.ProjectOffset(1));
  EXPECT_EQ(13U, offset_mapper1.ProjectOffset(2));
  EXPECT_EQ(14U, offset_mapper1.ProjectOffset(3));  // Previous equivalence.
  EXPECT_EQ(16U, offset_mapper1.ProjectOffset(4));
  EXPECT_EQ(17U, offset_mapper1.ProjectOffset(5));
  EXPECT_EQ(18U, offset_mapper1.ProjectOffset(6));  // Previous equivalence.

  OffsetMapper offset_mapper2({{0, 10, 2}, {13, 2, 1}, {16, 4, 2}});
  EXPECT_EQ(10U, offset_mapper2.ProjectOffset(0));
  EXPECT_EQ(11U, offset_mapper2.ProjectOffset(1));
  EXPECT_EQ(2U, offset_mapper2.ProjectOffset(13));
  EXPECT_EQ(3U, offset_mapper2.ProjectOffset(14));  // Previous equivalence.
  EXPECT_EQ(4U, offset_mapper2.ProjectOffset(16));
  EXPECT_EQ(5U, offset_mapper2.ProjectOffset(17));
  EXPECT_EQ(6U, offset_mapper2.ProjectOffset(18));  // Previous equivalence.
}

}  // namespace zucchini
