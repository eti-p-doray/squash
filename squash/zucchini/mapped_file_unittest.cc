// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "squash/zucchini/mapped_file.h"

#include "boost/filesystem.hpp"

#include "gtest/gtest.h"

namespace zucchini {

class MappedFileWriterTest : public testing::Test {
 protected:
  MappedFileWriterTest() {
    file_path_ = boost::filesystem::temp_directory_path();
    file_path_ /= "test-file";
  }
  ~MappedFileWriterTest() {
    boost::filesystem::remove_all(file_path_);
  }


  boost::filesystem::path file_path_;
};

TEST_F(MappedFileWriterTest, ErrorCreating) {
  // Create a directory |file_path_|, so |file_writer| fails when it tries to
  // open a file with the same name for write.
  ASSERT_TRUE(boost::filesystem::create_directory(file_path_));
  {
    MappedFileWriter file_writer(file_path_, 10);
    EXPECT_FALSE(file_writer.IsValid());
  }
  EXPECT_TRUE(boost::filesystem::exists(file_path_));
}

TEST_F(MappedFileWriterTest, Keep) {
  EXPECT_FALSE(boost::filesystem::exists(file_path_));
  {
    MappedFileWriter file_writer(file_path_, 10);
    EXPECT_TRUE(file_writer.Keep());
  }
  EXPECT_TRUE(boost::filesystem::exists(file_path_));
}

TEST_F(MappedFileWriterTest, DeleteOnClose) {
  EXPECT_FALSE(boost::filesystem::exists(file_path_));
  { MappedFileWriter file_writer(file_path_, 10); }
  EXPECT_FALSE(boost::filesystem::exists(file_path_));
}

}  // namespace zucchini
