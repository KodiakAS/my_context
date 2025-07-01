#include "ma_context.h"
#include <stdio.h>

static struct my_context ctx;
static char *dangling;

static void coroutine(void *arg) {
  (void)arg;
  char local[8];
  dangling = local;
  my_context_yield(&ctx);
}

int main(void) {
  my_context_init(&ctx, 1 << 16);
  my_context_spawn(&ctx, coroutine, NULL);
  my_context_continue(&ctx); /* finish coroutine */
  /* Stack use after return */
  dangling[0] = 'A';
  printf("%c\n", dangling[0]);
  my_context_destroy(&ctx);
  return 0;
}
