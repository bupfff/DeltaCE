// cachesim.c - the TI-84 Plus CE's flash cache (as CEmu models it: 128 sets of 2 ways of 32-byte
// lines, LRU, a miss ~197 cycles) replayed on a line trace (scripts/emu_linetrace.py), the code
// as it was or moved: each function's lines at its address in another build's map (same names).
// Also a fully associative LRU cache of the same 256 lines: what no placement can avoid.
// -c 3: a trace of 8-byte chunk visits (emu_linetrace.py ... 3: the first and last address read
// in each), for code moved by any number of bytes: each visit moved with its code, split into the
// lines it is in now.
//   cc -O2 -o cachesim scripts/cachesim.c
//   ./cachesim [-c bits] trace.bin [old.map new.map]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { uint32_t a; char * n; } sym;
static int cmpa(const void * x, const void * y) { const sym * p = x, * q = y; return p->a < q->a ? -1 : p->a > q->a; }
static int cmpn(const void * x, const void * y) { const sym * p = x, * q = y; return strcmp(p->n, q->n); }
// the map's code symbols (name = hex address), sorted by address
static sym * load(const char * path, int * n, uint32_t * start) {
  FILE * f = fopen(path, "r");
  if (!f) { perror(path); exit(1); }
  int cap = 1 << 16; sym * s = malloc(cap * sizeof(sym)); *n = 0; *start = 0;
  char line[1024], name[1000]; unsigned a;
  while (fgets(line, sizeof line, f)) {
    char eq[4];
    if (sscanf(line, "%999s %3s %x", name, eq, &a) != 3 || strcmp(eq, "=")) continue;
    if (!strcmp(name, "___app_start")) *start = a;
    if (a >= 0xD00000 || !strncmp(name, "___", 3)) continue;
    if (*n == cap) s = realloc(s, (cap *= 2) * sizeof(sym));
    s[*n].a = a; s[*n].n = strdup(name); ++*n;
  }
  fclose(f);
  qsort(s, *n, sizeof(sym), cmpa);
  return s;
}

int main(int argc, char ** argv) {
  int bits = 5; // -c <bits>: a chunk trace (the first and last address of each visit, patch_cemu.py 10)
  if (argc > 2 && !strcmp(argv[1], "-c")) { bits = atoi(argv[2]); argv += 2; argc -= 2; }
  if (argc < 2) { fprintf(stderr, "usage: cachesim [-c bits] trace.bin [old.map new.map]\n"); return 2; }
  FILE * f = fopen(argv[1], "rb");
  if (!f) { perror(argv[1]); return 1; }
  fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
  size_t n = sz / 4; uint32_t * t = malloc(sz);
  if (fread(t, 4, n, f) != n) return 1;
  fclose(f);
  uint32_t * na = 0; sym * o = 0; int no = 0; uint32_t so = 0;
  if (argc >= 4) { // the code moved: each old symbol's address in the new map (a line: by its middle)
    int nn; uint32_t sn;
    o = load(argv[2], &no, &so); sym * w = load(argv[3], &nn, &sn);
    sym * wn = malloc(nn * sizeof(sym)); memcpy(wn, w, nn * sizeof(sym));
    qsort(wn, nn, sizeof(sym), cmpn);
    // per old symbol: its new address (by name), or none
    na = malloc(no * sizeof(uint32_t));
    for (int i = 0; i < no; ++i) {
      sym key = {0, o[i].n};
      sym * h = bsearch(&key, wn, nn, sizeof(sym), cmpn);
      na[i] = h ? h->a : 0xFFFFFFFF;
    }
    if (bits == 5) {
      size_t moved = 0;
      for (size_t i = 0; i < n; ++i) {
        uint32_t a = t[i] * 32 + 16;
        if (a < so) continue; // the OS
        int lo = 0, hi = no - 1, k = -1;
        while (lo <= hi) { int m = (lo + hi) / 2; if (o[m].a <= a) { k = m; lo = m + 1; } else hi = m - 1; }
        if (k < 0 || na[k] == 0xFFFFFFFF) continue;
        t[i] = (na[k] + (a - o[k].a)) >> 5; ++moved;
      }
      fprintf(stderr, "moved %zu of %zu entries\n", moved, n);
    }
  }
  if (bits != 5) { // chunk visits (the first and the last address read): the lines they are in, the
    // code moved with its section; a read just after the last visit's (a prefetch past a ret or a
    // jp, a fall-through) moved with the code before it
    uint32_t * u = malloc((n + 1) * 4); size_t m = 0; uint32_t last = 0xFFFFFFFF, phi = 0xFFFFFFFF;
    int k = -1; int64_t pd = 0;
    for (size_t i = 0; i + 1 < n; i += 2) {
      uint32_t x = t[i], y = t[i + 1], hi = y > x ? y : x, lo = y < x ? y : x;
      int64_t d = 0;
      if (na && x >= so) {
        if (x == phi + 1) d = pd;
        else {
          if (k < 0 || x < o[k].a || (k + 1 < no && x >= o[k + 1].a)) { // (mostly the last visit's)
            int lo2 = 0, hi2 = no - 1; k = -1;
            while (lo2 <= hi2) { int mm = (lo2 + hi2) / 2; if (o[mm].a <= x) { k = mm; lo2 = mm + 1; } else hi2 = mm - 1; }
          }
          if (k >= 0 && na[k] != 0xFFFFFFFF) d = (int64_t)na[k] - o[k].a;
        }
      }
      uint32_t lx = (uint32_t)(x + d) >> 5, ll = (uint32_t)(lo + d) >> 5, lh = (uint32_t)(hi + d) >> 5;
      if (lx != last) u[m++] = last = lx;
      if (ll != lx && ll != last) u[m++] = last = ll;
      if (lh != lx && lh != last) u[m++] = last = lh;
      phi = hi; pd = d;
    }
    free(t); t = u; n = m;
  }
  { const char * dp = getenv("CACHESIM_DUMP"); // (the line stream simulated, u32 line numbers)
    if (dp) { FILE * o = fopen(dp, "wb"); fwrite(t, 4, n, o); fclose(o); } }
  // 2 ways x 128 sets, LRU
  // (CACHESIM_LINES=<path>: the misses per line, u32 per line number up to 1 MB of lines)
  uint32_t mru[128], lru[128]; size_t miss = 0;
  const char * lp = getenv("CACHESIM_LINES");
  uint32_t * per = lp ? calloc(1 << 17, 4) : 0;
  memset(mru, 0xFF, sizeof mru); memset(lru, 0xFF, sizeof lru);
  for (size_t i = 0; i < n; ++i) {
    uint32_t l = t[i], s = l & 127, tag = l >> 7;
    if (mru[s] == tag) continue;
    if (lru[s] == tag) { lru[s] = mru[s]; mru[s] = tag; continue; }
    lru[s] = mru[s]; mru[s] = tag; ++miss;
    if (per && l < (1 << 17)) ++per[l];
  }
  if (per) { FILE * o = fopen(lp, "wb"); fwrite(per, 4, 1 << 17, o); fclose(o); }
  // fully associative, 256 lines, LRU (a hash of line -> slot, a list by use)
  enum { C = 256, H = 1 << 12 };
  int32_t hk[H]; uint32_t hl[H]; int prev[C], next[C], head = -1, tail = -1, used = 0; uint32_t line_of[C];
  for (int i = 0; i < H; ++i) hk[i] = -1;
  size_t fmiss = 0;
  #define HASH(x) (((x) * 2654435761u) >> 20 & (H - 1))
  for (size_t i = 0; i < n; ++i) {
    uint32_t l = t[i]; int s = -1;
    for (uint32_t h = HASH(l);; h = (h + 1) & (H - 1)) { if (hk[h] < 0) break; if (hl[h] == l) { s = hk[h]; break; } }
    if (s >= 0) { // to the front
      if (s != head) { next[prev[s]] = next[s]; if (next[s] >= 0) prev[next[s]] = prev[s]; else tail = prev[s]; prev[s] = -1; next[s] = head; prev[head] = s; head = s; }
      continue;
    }
    ++fmiss;
    if (used < C) s = used++;
    else { // evict the tail: its hash entry removed (rehash the cluster after it)
      s = tail; tail = prev[s]; next[tail] = -1;
      uint32_t old = line_of[s], h = HASH(old);
      while (!(hk[h] >= 0 && hl[h] == old)) h = (h + 1) & (H - 1);
      hk[h] = -1;
      for (uint32_t j = (h + 1) & (H - 1); hk[j] >= 0; j = (j + 1) & (H - 1)) {
        int32_t k2 = hk[j]; uint32_t l2 = hl[j]; hk[j] = -1;
        uint32_t h2 = HASH(l2); while (hk[h2] >= 0) h2 = (h2 + 1) & (H - 1);
        hk[h2] = k2; hl[h2] = l2;
      }
    }
    line_of[s] = l;
    { uint32_t h = HASH(l); while (hk[h] >= 0) h = (h + 1) & (H - 1); hk[h] = s; hl[h] = l; }
    prev[s] = -1; next[s] = head; if (head >= 0) prev[head] = s; head = s; if (tail < 0) tail = s;
  }
  printf("entries %zu  misses %zu (2-way)  %zu (fully associative)  conflict share %.1f%%\n", n, miss, fmiss, 100.0 * (miss - (double)fmiss) / miss);
  return 0;
}
