/*
 * Copyright (C) 2026 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <errno.h>
#include <stddef.h>
#include <sys/mman.h>

static inline bool HardenedMallocSupportsPlatform(size_t page_size) {
#if defined(__LP64__)
  if (page_size != 4096) return false;

  // The upstream configuration reserves over 3 TiB before application allocations.
  // Probe without allocating memory through libc, including on release builds.
  constexpr size_t kAddressSpaceProbeSize = 1ULL << 43;
  int saved_errno = errno;
  void* addr = mmap(nullptr, kAddressSpaceProbeSize, PROT_NONE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  bool supported = addr != MAP_FAILED && munmap(addr, kAddressSpaceProbeSize) == 0;
  errno = saved_errno;
  return supported;
#else
  (void)page_size;
  return false;
#endif
}
