// trg.c - the temporal relationship graph of a line trace (scripts/emu_linetrace.py), for code
// placement (scripts/codelayout.py color): for each reuse of a flash line that a fully associative
// cache of D lines would hit (its LRU stack depth <= D), each other line used since gets a weight
// of 1 with it: the misses the pair costs if the two share a set of the 2-way cache. With -2, the
// chance that another of the k lines used since is in that set too (1 - (127/128)^(k-1)): a 2-way
// set misses on the second line of the set used in between, not the first. Several traces are
// added. Output: records u32 line, u32 line (the first < the second), f32 weight >= min.
//   cc -O2 -o trg scripts/trg.c
//   ./trg [-2] out.bin D min trace.bin [trace.bin...]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static uint64_t * key, * val; static size_t cap, used; // (the weights x1024)
static size_t slot(uint64_t k) {
  size_t h = (size_t)((k * 0x9E3779B97F4A7C15ull) >> 20) & (cap - 1);
  while (key[h] && key[h] != k) h = (h + 1) & (cap - 1);
  return h;
}
static void grow(void) {
  uint64_t * ok = key, * ov = val; size_t oc = cap;
  cap *= 2; key = calloc(cap, 8); val = calloc(cap, 8);
  for (size_t i = 0; i < oc; ++i) if (ok[i]) { size_t h = slot(ok[i]); key[h] = ok[i]; val[h] = ov[i]; }
  free(ok); free(ov);
}
static void add(uint32_t a, uint32_t b, uint32_t w) {
  uint64_t k = a < b ? ((uint64_t)a << 32 | b) + 1 : ((uint64_t)b << 32 | a) + 1; // (+1: 0 is empty)
  size_t h = slot(k);
  if (!key[h]) { key[h] = k; if (++used * 10 > cap * 7) { grow(); h = slot(k); } }
  val[h] += w;
}

int main(int argc, char ** argv) {
  int two = argc > 1 && !strcmp(argv[1], "-2");
  if (two) { ++argv; --argc; }
  if (argc < 5) { fprintf(stderr, "usage: trg [-2] out.bin D min trace.bin...\n"); return 2; }
  int D = atoi(argv[2]); double mn = atof(argv[3]);
  cap = 1 << 22; key = calloc(cap, 8); val = calloc(cap, 8);
  uint32_t * st = malloc((D + 1) * 4), * kw = malloc((D + 2) * 4);
  for (int k = 0; k <= D + 1; ++k) { double p = 1; for (int j = 1; j < k; ++j) p *= 127.0 / 128; kw[k] = two ? (uint32_t)(1024 * (1 - p) + 0.5) : 1024; }
  size_t hits = 0, depth = 0;
  for (int ai = 4; ai < argc; ++ai) {
    FILE * f = fopen(argv[ai], "rb");
    if (!f) { perror(argv[ai]); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    size_t n = sz / 4; uint32_t * t = malloc(sz);
    if (fread(t, 4, n, f) != n) return 1;
    fclose(f);
    int top = 0; // the stack st[0..top), most recent first
    for (size_t i = 0; i < n; ++i) {
      uint32_t l = t[i]; int k = 0;
      while (k < top && st[k] != l) ++k;
      if (k < top) { // a hit at depth k: the k lines above
        if (kw[k]) for (int j = 0; j < k; ++j) add(l, st[j], kw[k]);
        ++hits; depth += k;
        memmove(st + 1, st, k * 4);
      } else {
        if (top <= D) ++top;
        memmove(st + 1, st, (top - 1) * 4);
      }
      st[0] = l;
    }
    free(t);
  }
  FILE * o = fopen(argv[1], "wb"); size_t out = 0;
  for (size_t i = 0; i < cap; ++i) if (key[i] && val[i] / 1024.0 >= mn) {
    float wf = (float)(val[i] / 1024.0);
    uint32_t r[3] = { (uint32_t)((key[i] - 1) >> 32), (uint32_t)(key[i] - 1), 0 };
    memcpy(&r[2], &wf, 4);
    fwrite(r, 4, 3, o); ++out;
  }
  fclose(o);
  fprintf(stderr, "hits %zu (mean depth %.1f), pairs %zu, written %zu\n", hits, hits ? (double)depth / hits : 0, used, out);
  return 0;
}
