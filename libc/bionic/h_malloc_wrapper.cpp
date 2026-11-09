/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include <errno.h>
#include <malloc.h>
#include <sys/param.h>
#include <unistd.h>

#include <private/MallocXmlElem.h>

#include "malloc_common.h"

#if defined(HAVE_HARDENED_MALLOC)

int h_malloc_info(int options, FILE* fp) {
  if (options != 0) {
    errno = EINVAL;
    return -1;
  }

  if (fflush(fp) != 0) return -1;
  int fd = fileno(fp);
  if (fd == -1) return -1;
  MallocXmlElem root(fd, "malloc", "version=\"jemalloc-1\"");

  // Dump all of the large allocations in the arenas.
  for (size_t i = 0; i < h_mallinfo_narenas(); i++) {
    struct mallinfo mi = h_mallinfo_arena_info(i);
    if (mi.hblkhd != 0) {
      MallocXmlElem arena_elem(fd, "heap", "nr=\"%zu\"", i);
      {
        MallocXmlElem(fd, "allocated-large").Contents("%zu", mi.ordblks);
        MallocXmlElem(fd, "allocated-huge").Contents("%zu", mi.uordblks);
        MallocXmlElem(fd, "allocated-bins").Contents("%zu", mi.fsmblks);

        size_t total = 0;
        for (size_t j = 0; j < h_mallinfo_nbins(); j++) {
          struct mallinfo mi = h_mallinfo_bin_info(i, j);
          if (mi.ordblks != 0) {
            MallocXmlElem bin_elem(fd, "bin", "nr=\"%zu\"", j);
            MallocXmlElem(fd, "allocated").Contents("%zu", mi.ordblks);
            MallocXmlElem(fd, "nmalloc").Contents("%zu", mi.uordblks);
            MallocXmlElem(fd, "ndalloc").Contents("%zu", mi.fordblks);
            total += mi.ordblks;
          }
        }
        MallocXmlElem(fd, "bins-total").Contents("%zu", total);
      }
    }
  }

  return 0;
}
#endif
