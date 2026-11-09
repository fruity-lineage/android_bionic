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

#include <gtest/gtest.h>

#include "utils.h"

#if defined(__BIONIC__)
#include <errno.h>
#include <malloc.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>

#include "SignalUtils.h"
#include "platform/bionic/malloc.h"
#include "private/hardened_malloc_platform.h"

extern "C" void malloc_disable();
extern "C" void malloc_enable();
extern "C" int malloc_iterate(uintptr_t, size_t,
                              void (*)(uintptr_t, size_t, void*), void*);
#endif

TEST(hardened_malloc, rejects_unsupported_page_sizes) {
#if defined(__BIONIC__)
  EXPECT_FALSE(HardenedMallocSupportsPlatform(16384));
  EXPECT_FALSE(HardenedMallocSupportsPlatform(65536));
#else
  GTEST_SKIP() << "bionic-only test";
#endif
}

TEST(hardened_malloc, rejects_insufficient_address_space) {
#if defined(__BIONIC__)
  // Exercise the real mmap failure path without changing the parent's limit.
  ASSERT_EXIT({
    struct rlimit limit = {1ULL << 30, 1ULL << 30};
    if (setrlimit(RLIMIT_AS, &limit) != 0) _exit(1);
    errno = EBUSY;
    bool supported = HardenedMallocSupportsPlatform(4096);
    _exit(!supported && errno == EBUSY ? 0 : 2);
  }, ::testing::ExitedWithCode(0), "");
#else
  GTEST_SKIP() << "bionic-only test";
#endif
}

TEST(hardened_malloc, startup_selection) {
#if defined(__BIONIC__)
  SKIP_WITH_HWASAN;
  bool iterate_supported;
  ASSERT_TRUE(android_mallopt(M_GET_MALLOC_ITERATE_SUPPORTED, &iterate_supported,
                              sizeof(iterate_supported)));
#if defined(EXPECT_HARDENED_MALLOC_OPT_IN) && !defined(EXPECT_JEMALLOC)
  bool expected_hardened = getenv("DISABLE_HARDENED_MALLOC") == nullptr &&
                           HardenedMallocSupportsPlatform(getpagesize());
  EXPECT_EQ(!expected_hardened, iterate_supported);
#else
  EXPECT_TRUE(iterate_supported);
#endif
#else
  GTEST_SKIP() << "bionic-only test";
#endif
}

TEST(hardened_malloc, iterate_capability_argument_validation) {
#if defined(__BIONIC__)
  bool value;
  EXPECT_ERRNO_FAILURE(EINVAL, false,
                       android_mallopt(M_GET_MALLOC_ITERATE_SUPPORTED, nullptr, sizeof(value)));
  EXPECT_ERRNO_FAILURE(EINVAL, false,
                       android_mallopt(M_GET_MALLOC_ITERATE_SUPPORTED, &value, 0));
#else
  GTEST_SKIP() << "bionic-only test";
#endif
}

TEST(hardened_malloc, environment_opt_out) {
#if defined(__BIONIC__)
  std::string executable(testing::internal::GetArgvs()[0]);
  ExecTestHelper helper;
  helper.SetArgs({executable.c_str(), "--gtest_filter=hardened_malloc.startup_selection", nullptr});
  helper.SetEnv({"DISABLE_HARDENED_MALLOC=1", nullptr});
  helper.Run([&]() { execve(executable.c_str(), helper.GetArgs(), helper.GetEnv()); }, 0,
             R"(\[  PASSED  \] 1 test)");
#else
  GTEST_SKIP() << "bionic-only test";
#endif
}

TEST(hardened_malloc, iterate_reports_unsupported) {
#if defined(__BIONIC__)
  SKIP_WITH_HWASAN;
  bool iterate_supported;
  ASSERT_TRUE(android_mallopt(M_GET_MALLOC_ITERATE_SUPPORTED, &iterate_supported,
                              sizeof(iterate_supported)));
  if (iterate_supported) GTEST_SKIP() << "native allocator supports iteration";
  bool called = false;
  malloc_disable();
  int result = malloc_iterate(0, 4096,
                              [](uintptr_t, size_t, void* arg) {
                                *static_cast<bool*>(arg) = true;
                              }, &called);
  int error = errno;
  malloc_enable();
  EXPECT_EQ(-1, result);
  EXPECT_EQ(ENOTSUP, error);
  EXPECT_FALSE(called);
#else
  GTEST_SKIP() << "bionic-only test";
#endif
}

TEST(hardened_malloc, rejects_incompatible_gwp_asan) {
#if defined(__BIONIC__)
  SKIP_WITH_HWASAN;
  bool iterate_supported;
  ASSERT_TRUE(android_mallopt(M_GET_MALLOC_ITERATE_SUPPORTED, &iterate_supported,
                              sizeof(iterate_supported)));
  if (iterate_supported) GTEST_SKIP() << "hardened allocator is not selected";
  android_mallopt_gwp_asan_options_t options;
  options.program_name = "";
  options.mode = android_mallopt_gwp_asan_options_t::Mode::APP_MANIFEST_ALWAYS;
  EXPECT_FALSE(android_mallopt(M_INITIALIZE_GWP_ASAN, &options, sizeof(options)));
  ASSERT_TRUE(android_mallopt(M_GET_MALLOC_ITERATE_SUPPORTED, &iterate_supported,
                              sizeof(iterate_supported)));
  EXPECT_FALSE(iterate_supported);
  void* ptr = aligned_alloc(64, 128);
  ASSERT_NE(nullptr, ptr);
  free(ptr);
#else
  GTEST_SKIP() << "bionic-only test";
#endif
}

TEST(hardened_malloc, reports_unsupported_allocator_options) {
#if defined(__BIONIC__)
  SKIP_WITH_HWASAN;
  bool iterate_supported;
  ASSERT_TRUE(android_mallopt(M_GET_MALLOC_ITERATE_SUPPORTED, &iterate_supported,
                              sizeof(iterate_supported)));
  if (iterate_supported) GTEST_SKIP() << "hardened allocator is not selected";
  for (int option : {M_DECAY_TIME, M_PURGE_ALL, M_PURGE_FAST, M_LOG_STATS}) {
    EXPECT_EQ(0, mallopt(option, 0));
  }
#else
  GTEST_SKIP() << "bionic-only test";
#endif
}

TEST(hardened_malloc, DISABLED_restore_sigabrt_handler) {
#if defined(__BIONIC__)
  struct sigaction saved;
  ASSERT_EQ(0, sigaction(SIGABRT, nullptr, &saved));
  ASSERT_NE(SIG_ERR, signal(SIGABRT, saved.sa_handler == SIG_IGN ? SIG_DFL : SIG_IGN));
  ASSERT_EQ(1, mallopt(M_BIONIC_RESTORE_DEFAULT_SIGABRT_HANDLER, 0));
  struct sigaction restored;
  ASSERT_EQ(0, sigaction(SIGABRT, nullptr, &restored));
  EXPECT_EQ(saved.sa_handler, restored.sa_handler);
  EXPECT_EQ(saved.sa_flags, restored.sa_flags);
  for (int signo = 1; signo < NSIG; ++signo) {
    EXPECT_EQ(sigismember(&saved.sa_mask, signo), sigismember(&restored.sa_mask, signo));
  }
#else
  GTEST_SKIP() << "bionic-only test";
#endif
}

TEST(hardened_malloc, restores_startup_sigabrt_dispositions) {
#if defined(__BIONIC__)
  std::string executable(testing::internal::GetArgvs()[0]);
  for (bool ignored : {false, true}) {
    ScopedSignalHandler handler(SIGABRT, ignored ? SIG_IGN : SIG_DFL);
    ExecTestHelper helper;
    const char* filter = "--gtest_filter=hardened_malloc.DISABLED_restore_sigabrt_handler";
    helper.SetArgs({executable.c_str(), "--gtest_also_run_disabled_tests", filter, nullptr});
    helper.Run([&]() { execv(executable.c_str(), helper.GetArgs()); }, 0,
               R"(\[  PASSED  \] 1 test)");
  }
#else
  GTEST_SKIP() << "bionic-only test";
#endif
}
