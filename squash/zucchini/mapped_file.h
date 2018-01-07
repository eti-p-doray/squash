// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_MAPPED_FILE_H_
#define CHROME_INSTALLER_ZUCCHINI_MAPPED_FILE_H_

#include <stddef.h>
#include <stdint.h>

#include "boost/filesystem.hpp"
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include "squash/base/macros.h"
#include "squash/zucchini/buffer_view.h"

namespace zucchini {

// A file reader wrapper.
class MappedFileReader {
 public:
  // Opens |file_path| and maps it to memory for reading.
  explicit MappedFileReader(const boost::filesystem::path& file_path);

  bool IsValid() const { return buffer_.get_size() != 0; }
  const uint8_t* data() const {
    return reinterpret_cast<const uint8_t*>(buffer_.get_address());
  }
  size_t length() const { return buffer_.get_size(); }
  zucchini::ConstBufferView region() const { return {data(), length()}; }

 private:
  boost::interprocess::file_mapping file_mapping_;
  boost::interprocess::mapped_region buffer_;

  DISALLOW_COPY_AND_ASSIGN(MappedFileReader);
};

// A file writer wrapper. The target file is deleted on destruction unless
// Keep() is called.
class MappedFileWriter {
 public:
  // Creates |file_path| and maps it to memory for writing.
  MappedFileWriter(const boost::filesystem::path& file_path, size_t length);
  ~MappedFileWriter();

  bool IsValid() const { return buffer_.get_size() != 0; }
  uint8_t* data() {
    return reinterpret_cast<uint8_t*>(buffer_.get_address());
  }
  size_t length() const { return buffer_.get_size(); }
  zucchini::MutableBufferView region() { return {data(), length()}; }

  // Indicates that the file should not be deleted on destruction. Returns true
  // iff the operation succeeds.
  bool Keep();

 private:
  enum OnCloseDeleteBehavior {
    kKeep,
    kAutoDeleteOnClose,
    kManualDeleteOnClose
  };

  boost::filesystem::path file_path_;
  boost::interprocess::file_mapping file_mapping_;
  boost::interprocess::mapped_region buffer_;
  OnCloseDeleteBehavior delete_behavior_;

  DISALLOW_COPY_AND_ASSIGN(MappedFileWriter);
};

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_MAPPED_FILE_H_
