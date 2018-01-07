// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "squash/zucchini/mapped_file.h"

#include <utility>
#include <fstream>

#include "squash/base/logging.h"

namespace zucchini {

MappedFileReader::MappedFileReader(const boost::filesystem::path& file_path)
    : file_mapping_(file_path.c_str(), boost::interprocess::read_only),
      buffer_(file_mapping_, boost::interprocess::read_only) {}

MappedFileWriter::MappedFileWriter(const boost::filesystem::path& file_path,
                                   size_t length)
    : file_path_(file_path),
      delete_behavior_(kManualDeleteOnClose) {
  {
    std::filebuf fbuf;
    if (!fbuf.open(file_path.c_str(), std::ios_base::in | std::ios_base::out
                         | std::ios_base::trunc | std::ios_base::binary))
      return;
    //Set the size
    fbuf.pubseekoff(length-1, std::ios_base::beg);
    fbuf.sputc(0);
  }

  file_mapping_ =
      boost::interprocess::file_mapping(file_path.c_str(),
                                        boost::interprocess::read_write);
  buffer_ = boost::interprocess::mapped_region(file_mapping_,
                                               boost::interprocess::read_write);

}

MappedFileWriter::~MappedFileWriter() {
  if (IsValid() && delete_behavior_ == kManualDeleteOnClose &&
      !boost::interprocess::file_mapping::remove(file_path_.c_str())) {
    LOG(WARNING) << "Failed to delete file: " << file_path_.c_str();
  }
}

bool MappedFileWriter::Keep() {
  delete_behavior_ = kKeep;
  return true;
}

}  // namespace zucchini
