// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQUASH_BASE_MACROS_H_
#define SQUASH_BASE_MACROS_H_

// Distinguish mips32.
#if defined(__mips__) && (_MIPS_SIM == _ABIO32) && !defined(__mips32__)
#define __mips32__
#endif

// Distinguish mips64.
#if defined(__mips__) && (_MIPS_SIM == _ABI64) && !defined(__mips64__)
#define __mips64__
#endif

// Put this in the declarations for a class to be uncopyable.
#define DISALLOW_COPY(TypeName) \
  TypeName(const TypeName&) = delete

// Put this in the declarations for a class to be unassignable.
#define DISALLOW_ASSIGN(TypeName) TypeName& operator=(const TypeName&) = delete

// Put this in the declarations for a class to be uncopyable and unassignable.
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  DISALLOW_COPY(TypeName);                 \
  DISALLOW_ASSIGN(TypeName)

// A macro to disallow all the implicit constructors, namely the
// default constructor, copy constructor and operator= functions.
// This is especially useful for classes containing only static methods.
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                           \
  DISALLOW_COPY_AND_ASSIGN(TypeName)

#endif  // SQUASH_BASE_MACROS_H_