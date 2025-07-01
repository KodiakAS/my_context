#include "ma_context.h"
#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

static struct my_context ctx;
static int counter;
static char *dangling;
static char *dangling_multi;
static struct my_context nested_ctx;
static int nested_counter;
static int nested_spawn_ret;
static int nested_continue_ret;
static char *nested_dangling;

static void no_yield_coroutine(void *arg) {
  (void)arg;
  counter++;
}

static void one_yield_coroutine(void *arg) {
  (void)arg;
  counter++;
  my_context_yield(&ctx);
  counter++;
}

static void multi_yield_coroutine(void *arg) {
  (void)arg;
  counter++; // first
  my_context_yield(&ctx);
  counter++; // second
  my_context_yield(&ctx);
}

static void leak_coroutine(void *arg) {
  (void)arg;
  malloc(20); // intentional leak of 20 bytes
  my_context_yield(&ctx);
}

static void leak_multi_coroutine(void *arg) {
  (void)arg;
  malloc(10); // leak before first yield
  my_context_yield(&ctx);
  malloc(10); // leak after resuming
  my_context_yield(&ctx);
}

static void heap_uaf_coroutine(void *arg) {
  (void)arg;
  char *p = (char *)malloc(8);
  dangling = p;
  free(p);
  my_context_yield(&ctx);
}

static void heap_uaf_multi_coroutine(void *arg) {
  (void)arg;
  char *p = (char *)malloc(8);
  dangling_multi = p;
  my_context_yield(&ctx); // first yield
  free(p);
  my_context_yield(&ctx); // second yield after free
}

static void stack_uaf_coroutine(void *arg) {
  (void)arg;
  char local[8];
  dangling = local;
  my_context_yield(&ctx);
}

static void inner_coroutine(void *arg) {
  (void)arg;
  nested_counter++;
  my_context_yield(&nested_ctx);
  nested_counter++;
}

static void inner_leak_coroutine(void *arg) {
  (void)arg;
  malloc(24);
  my_context_yield(&nested_ctx);
}

static void inner_heap_uaf_coroutine(void *arg) {
  (void)arg;
  char *p = (char *)malloc(8);
  nested_dangling = p;
  free(p);
  my_context_yield(&nested_ctx);
}

static void outer_leak_coroutine(void *arg) {
  (void)arg;
  my_context_init(&nested_ctx, 1 << 16);
  my_context_spawn(&nested_ctx, inner_leak_coroutine, NULL);
  my_context_continue(&nested_ctx);
  my_context_destroy(&nested_ctx);
}

static void outer_heap_uaf_coroutine(void *arg) {
  (void)arg;
  my_context_init(&nested_ctx, 1 << 16);
  my_context_spawn(&nested_ctx, inner_heap_uaf_coroutine, NULL);
  my_context_yield(&ctx);
  my_context_continue(&nested_ctx);
  my_context_destroy(&nested_ctx);
}

static void outer_coroutine(void *arg) {
  (void)arg;
  counter++;
  my_context_init(&nested_ctx, 1 << 16);
  nested_spawn_ret = my_context_spawn(&nested_ctx, inner_coroutine, NULL);
  nested_continue_ret = my_context_continue(&nested_ctx);
  my_context_destroy(&nested_ctx);
  counter++;
}

static void reset_context() {
  counter = 0;
  my_context_init(&ctx, 1 << 16);
}

static void destroy_context() { my_context_destroy(&ctx); }

TEST(Context, NoYield) {
  reset_context();
  ASSERT_EQ(my_context_spawn(&ctx, no_yield_coroutine, NULL), 0);
  EXPECT_EQ(counter, 1);
  EXPECT_EQ(my_context_continue(&ctx), 0);
  EXPECT_EQ(counter, 1);
  destroy_context();
}

TEST(Context, YieldOnce) {
  reset_context();
  ASSERT_EQ(my_context_spawn(&ctx, one_yield_coroutine, NULL), 1);
  EXPECT_EQ(counter, 1);
  ASSERT_EQ(my_context_continue(&ctx), 0);
  EXPECT_EQ(counter, 2);
  destroy_context();
}

TEST(Context, YieldMultiple) {
  reset_context();
  ASSERT_EQ(my_context_spawn(&ctx, multi_yield_coroutine, NULL), 1);
  EXPECT_EQ(counter, 1);
  ASSERT_EQ(my_context_continue(&ctx), 1);
  EXPECT_EQ(counter, 2);
  ASSERT_EQ(my_context_continue(&ctx), 0);
  EXPECT_EQ(counter, 2);
  destroy_context();
}

TEST(Context, NestedSwitch) {
  reset_context();
  nested_counter = 0;
  ASSERT_EQ(my_context_spawn(&ctx, outer_coroutine, NULL), 0);
  EXPECT_EQ(counter, 2);
  EXPECT_EQ(nested_spawn_ret, 1);
  EXPECT_EQ(nested_continue_ret, 0);
  EXPECT_EQ(nested_counter, 2);
  destroy_context();
}

static void run_leak() {
  reset_context();
  my_context_spawn(&ctx, leak_coroutine, NULL);
  my_context_continue(&ctx);
  destroy_context();
}

static void run_leak_multi() {
  reset_context();
  ASSERT_EQ(my_context_spawn(&ctx, leak_multi_coroutine, NULL), 1);
  ASSERT_EQ(my_context_continue(&ctx), 1);
  my_context_continue(&ctx);
  destroy_context();
}

static void run_heap_uaf() {
  reset_context();
  my_context_spawn(&ctx, heap_uaf_coroutine, NULL);
  dangling[0] = 'B';
  my_context_continue(&ctx);
  destroy_context();
}

static void run_heap_uaf_multi() {
  reset_context();
  my_context_spawn(&ctx, heap_uaf_multi_coroutine, NULL);
  ASSERT_EQ(my_context_continue(&ctx), 1); // first resume
  dangling_multi[0] = 'C';
  my_context_continue(&ctx); // finish coroutine after free
  destroy_context();
}

static void run_stack_uaf() {
  reset_context();
  my_context_spawn(&ctx, stack_uaf_coroutine, NULL);
  my_context_continue(&ctx); // finish coroutine
  dangling[0] = 'A';
  destroy_context();
}

static void run_nested_leak() {
  reset_context();
  my_context_spawn(&ctx, outer_leak_coroutine, NULL);
  destroy_context();
}

static void run_nested_heap_uaf() {
  reset_context();
  my_context_spawn(&ctx, outer_heap_uaf_coroutine, NULL);
  nested_dangling[0] = 'D';
  my_context_continue(&ctx);
  destroy_context();
}

#if defined(__SANITIZE_ADDRESS__)
static void expect_leak(void (*func)(), int expected_bytes) {
  int pipes[2];
  ASSERT_EQ(pipe(pipes), 0);
  pid_t pid = fork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    dup2(pipes[1], STDERR_FILENO);
    close(pipes[0]);
    close(pipes[1]);
    func();
    exit(0);
  }
  close(pipes[1]);
  std::string output;
  char buf[256];
  ssize_t n;
  while ((n = read(pipes[0], buf, sizeof(buf))) > 0)
    output.append(buf, n);
  close(pipes[0]);
  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 1);
  EXPECT_NE(output.find("LeakSanitizer: detected memory leaks"),
            std::string::npos);
  std::string needle = std::to_string(expected_bytes) + " byte";
  EXPECT_NE(output.find(needle), std::string::npos);
}

TEST(AddressSanitizer, DetectLeak) { expect_leak(run_leak, 20); }

TEST(AddressSanitizer, DetectLeakMultiYield) {
  expect_leak(run_leak_multi, 20);
}

TEST(AddressSanitizer, DetectHeapUseAfterFree) {
  EXPECT_EXIT(run_heap_uaf(), ::testing::ExitedWithCode(1),
              "AddressSanitizer: heap-use-after-free");
}

TEST(AddressSanitizer, DetectStackUseAfterReturn) {
  EXPECT_EXIT(run_stack_uaf(), ::testing::ExitedWithCode(1),
              "AddressSanitizer: stack-use-after-return");
}

TEST(AddressSanitizer, DetectHeapUAFMultiYield) {
  EXPECT_EXIT(run_heap_uaf_multi(), ::testing::ExitedWithCode(1),
              "AddressSanitizer: heap-use-after-free");
}

TEST(AddressSanitizer, NestedSwitch) {
  reset_context();
  nested_counter = 0;
  ASSERT_EQ(my_context_spawn(&ctx, outer_coroutine, NULL), 0);
  destroy_context();
}

TEST(AddressSanitizer, DetectNestedLeak) { expect_leak(run_nested_leak, 24); }

TEST(AddressSanitizer, DetectNestedHeapUseAfterFree) {
  EXPECT_EXIT(run_nested_heap_uaf(), ::testing::ExitedWithCode(1),
              "AddressSanitizer: heap-use-after-free");
}
#else
TEST(AddressSanitizer, DetectLeak) { GTEST_SKIP() << "ASAN required"; }
TEST(AddressSanitizer, DetectLeakMultiYield) {
  GTEST_SKIP() << "ASAN required";
}
TEST(AddressSanitizer, DetectHeapUseAfterFree) {
  GTEST_SKIP() << "ASAN required";
}
TEST(AddressSanitizer, DetectStackUseAfterReturn) {
  GTEST_SKIP() << "ASAN required";
}
TEST(AddressSanitizer, DetectHeapUAFMultiYield) {
  GTEST_SKIP() << "ASAN required";
}
TEST(AddressSanitizer, NestedSwitch) { GTEST_SKIP() << "ASAN required"; }
TEST(AddressSanitizer, DetectNestedLeak) { GTEST_SKIP() << "ASAN required"; }
TEST(AddressSanitizer, DetectNestedHeapUseAfterFree) {
  GTEST_SKIP() << "ASAN required";
}
#endif

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
