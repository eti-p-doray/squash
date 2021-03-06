// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_DISASSEMBLER_NO_OP_H_
#define CHROME_INSTALLER_ZUCCHINI_DISASSEMBLER_NO_OP_H_

#include <memory>
#include <string>
#include <vector>

#include "squash/base/macros.h"
#include "squash/zucchini/buffer_view.h"
#include "squash/zucchini/disassembler.h"
#include "squash/zucchini/image_utils.h"

namespace zucchini {

// This disassembler works on any file and does not look for reference.
class DisassemblerNoOp : public Disassembler {
 public:
  DisassemblerNoOp();
  ~DisassemblerNoOp() override;

  // Disassembler:
  ExecutableType GetExeType() const override;
  std::string GetExeTypeString() const override;
  std::vector<ReferenceGroup> MakeReferenceGroups() const override;

 private:
  friend Disassembler;

  bool Parse(ConstBufferView image) override;

  DISALLOW_COPY_AND_ASSIGN(DisassemblerNoOp);
};

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_DISASSEMBLER_NO_OP_H_
