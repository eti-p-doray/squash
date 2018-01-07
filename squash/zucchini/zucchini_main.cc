// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "squash/base/command_line.h"
#include "squash/base/logging.h"
#include "squash/zucchini/main_utils.h"

int main(int argc, const char* argv[]) {
  // Initialize infrastructure from base.
  base::CommandLine command_line(argc, argv);
  zucchini::status::Code status =
      RunZucchiniCommand(command_line, std::cout, std::cerr);
  if (status != zucchini::status::kStatusSuccess)
    std::cerr << "Failed with code " << static_cast<int>(status) << std::endl;
  return static_cast<int>(status);
}
