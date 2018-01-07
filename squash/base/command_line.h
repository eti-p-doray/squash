// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQUASH_BASE_COMMAND_LINE_H_
#define SQUASH_BASE_COMMAND_LINE_H_

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace base {

class CommandLine {
 public:
  using StringType = std::string;
  using CharType = StringType::value_type;
  using StringVector = std::vector<StringType>;
  using SwitchMap = std::map<std::string, StringType, std::less<>>;

  CommandLine(int argc, const CharType* const* argv);
  CommandLine(const StringVector& argv);

  // Initialize from an argv vector.
  void InitFromArgv(int argc, const CharType* const* argv);
  void InitFromArgv(const StringVector& argv);

  // Returns true if this command line contains the given switch.
  // Switch names must be lowercase.
  // The second override provides an optimized version to avoid inlining codegen
  // at every callsite to find the length of the constant and construct a
  // StringPiece.
  bool HasSwitch(const char switch_constant[]) const;

  // Get the remaining arguments to the command.
  StringVector GetArgs() const;

  // Append a switch [with optional value] to the command line.
  // Note: Switches will precede arguments regardless of appending order.
  void AppendSwitchNative(const std::string& switch_string,
                          const StringType& value);

  // Append an argument to the command line. Note that the argument is quoted
  // properly such that it is interpreted as one argument to the target command.
  // AppendArg is primarily for ASCII; non-ASCII input is interpreted as UTF-8.
  // Note: Switches will precede arguments regardless of appending order.
  void AppendArgNative(const StringType& value);

 private:
  // The argv array: { program, [(--|-|/)switch[=value]]*, [--], [argument]* }
  StringVector argv_;

  // Parsed-out switch keys and values.
  SwitchMap switches_;

  // The index after the program and switches, any arguments start here.
  size_t begin_args_;
};

}  // namespace base

#endif  // SQUASH_BASE_COMMAND_LINE_H_