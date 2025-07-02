#include "ma_context.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
#include <sanitizer/asan_interface.h>
#define MA_ASAN 1
#else
#define MA_ASAN 0
#endif

#if MA_ASAN
static __thread void *tls_fake_main = NULL;
static __thread const void *tls_main_bottom = NULL;
static __thread size_t tls_main_size = 0;
#endif

typedef void (*uc_func_t)(void);
union pass_void_ptr_as_2_int {
  int a[2];
  void *p;
};

static void my_context_spawn_internal(int i0, int i1) {
  union pass_void_ptr_as_2_int u;
  u.a[0] = i0;
  u.a[1] = i1;
  struct my_context *c = (struct my_context *)u.p;
#if MA_ASAN
  const void *old_bottom = NULL;
  size_t old_size = 0;
  __sanitizer_finish_switch_fiber(tls_fake_main, &old_bottom, &old_size);
  if (!tls_main_bottom) {
    tls_main_bottom = old_bottom;
    tls_main_size = old_size;
  }
#endif

  c->user_func(c->user_data);
#if MA_ASAN
  __sanitizer_start_switch_fiber(&c->asan_fake_stack, NULL, 0);
#endif
  c->active = 0;
  if (setcontext(&c->base_context) == -1)
    fprintf(stderr, "setcontext failed: %d\n", errno);
}

int my_context_continue(struct my_context *c) {
  if (!c->active)
    return 0;
#if MA_ASAN
  __sanitizer_start_switch_fiber(&tls_fake_main, c->stack, c->stack_size);
#endif
  if (swapcontext(&c->base_context, &c->spawned_context) == -1) {
    fprintf(stderr, "swapcontext failed: %d\n", errno);
    return -1;
  }
#if MA_ASAN
  __sanitizer_finish_switch_fiber(c->asan_fake_stack, NULL, NULL);
#endif
  return c->active;
}

int my_context_spawn(struct my_context *c, void (*f)(void *), void *d) {
  union pass_void_ptr_as_2_int u;
  if (getcontext(&c->spawned_context) == -1)
    return -1;
  c->spawned_context.uc_stack.ss_sp = c->stack;
  c->spawned_context.uc_stack.ss_size = c->stack_size;
  c->spawned_context.uc_link = NULL;
  c->user_func = f;
  c->user_data = d;
  c->active = 1;
  u.a[1] = 0;
  u.p = c;
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type-strict"
#endif
  makecontext(&c->spawned_context, (uc_func_t)my_context_spawn_internal, 2,
              u.a[0], u.a[1]);
#ifdef __clang__
#pragma clang diagnostic pop
#endif
  return my_context_continue(c);
}

int my_context_yield(struct my_context *c) {
  if (!c->active)
    return -1;
#if MA_ASAN
  __sanitizer_start_switch_fiber(&c->asan_fake_stack, tls_main_bottom,
                                 tls_main_size);
#endif
  if (swapcontext(&c->spawned_context, &c->base_context) == -1)
    return -1;
#if MA_ASAN
  __sanitizer_finish_switch_fiber(c->asan_fake_stack, NULL, NULL);
#endif
  return 0;
}

int my_context_init(struct my_context *c, size_t stack_size) {
  memset(c, 0, sizeof(*c));
  c->stack = malloc(stack_size);
  if (!c->stack)
    return -1;
  c->stack_size = stack_size;
#if MA_ASAN
  c->asan_fake_stack = NULL;
#endif
  return 0;
}

void my_context_destroy(struct my_context *c) {
  if (c->stack) {
    free(c->stack);
    c->stack = NULL;
  }
}
