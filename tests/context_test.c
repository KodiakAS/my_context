#include "ma_context.h"
#include <assert.h>
#include <stdio.h>

static int counter = 0;
static struct my_context ctx;

static void coroutine(void *arg) {
  (void)arg;
  counter++; // first increment
  my_context_yield(&ctx);
  counter++; // second increment
  my_context_yield(&ctx);
}

int main(void) {
  assert(my_context_init(&ctx, 1 << 16) == 0);
  assert(my_context_spawn(&ctx, coroutine, NULL) == 1);
  // after first yield
  assert(counter == 1);
  assert(my_context_continue(&ctx) == 1);
  // after second yield
  assert(counter == 2);
  assert(my_context_continue(&ctx) == 0);
  // coroutine finished
  assert(counter == 2);
  my_context_destroy(&ctx);
  printf("ok\n");
  return 0;
}
