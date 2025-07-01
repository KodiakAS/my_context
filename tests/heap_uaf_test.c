#include "ma_context.h"
#include <stdlib.h>

static struct my_context ctx;
static char *dangling;

static void coroutine(void *arg) {
  (void)arg;
  char *p = malloc(8);
  dangling = p;
  free(p);
  my_context_yield(&ctx);
}

int main(void) {
  my_context_init(&ctx, 1 << 16);
  my_context_spawn(&ctx, coroutine, NULL);
  /* Use after free */
  dangling[0] = 'B';
  my_context_continue(&ctx);
  my_context_destroy(&ctx);
  return 0;
}
