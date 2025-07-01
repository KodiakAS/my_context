/*
 * Minimal context switching interface extracted from MariaDB Connector/C.
 * Provides cooperative multitasking using POSIX ucontext.
 */
#ifndef MA_CONTEXT_H
#define MA_CONTEXT_H

#include <stddef.h>
#include <ucontext.h>

#ifdef __cplusplus
extern "C" {
#endif

struct my_context {
  void (*user_func)(void *);
  void *user_data;
  void *stack;
  size_t stack_size;
  ucontext_t base_context;
  ucontext_t spawned_context;
  int active;
};

int my_context_init(struct my_context *c, size_t stack_size);
void my_context_destroy(struct my_context *c);
int my_context_spawn(struct my_context *c, void (*f)(void *), void *d);
int my_context_yield(struct my_context *c);
int my_context_continue(struct my_context *c);

#ifdef __cplusplus
}
#endif

#endif /* MA_CONTEXT_H */
