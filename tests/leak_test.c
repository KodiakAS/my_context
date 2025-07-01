#include "ma_context.h"
#include <stdlib.h>

static struct my_context ctx;

static void coroutine(void *arg) {
  (void)arg;
  /* Intentional memory leak */
  malloc(10);
}

int main(void) {
  my_context_init(&ctx, 1 << 16);
  my_context_spawn(&ctx, coroutine, NULL);
  my_context_continue(&ctx);
  my_context_destroy(&ctx);
  return 0;
}
