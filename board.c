#include "board.h"

#include <string.h>

int board_in_bounds(int r, int c) {
  return r >= 0 && r < BOARD_N && c >= 0 && c < BOARD_N;
}

int cell_alive(Cell c) {
  return c.kind == KIND_PRISM || c.color != 0;
}

int cells_same_color(Cell a, Cell b) {
  if (a.kind == KIND_PRISM || b.kind == KIND_PRISM) return 0;
  if (a.color == 0 || b.color == 0) return 0;
  return a.color == b.color;
}

void board_seed(Board *b, unsigned seed) {
  b->rng = seed ? seed : 1u;
}

int board_rand(Board *b) {
  b->rng = b->rng * 1103515245u + 12345u;
  return (int)((b->rng >> 16) & 32767);
}

int board_rand_n(Board *b, int n) {
  if (n <= 1) return 0;
  return board_rand(b) % n;
}

unsigned char board_rand_color(Board *b) {
  return (unsigned char)(1 + board_rand_n(b, N_COLORS));
}

void board_clear(Board *b) {
  memset(b->cells, 0, sizeof(b->cells));
}

static Cell make_normal(unsigned char color) {
  Cell c;
  c.color = color;
  c.kind = KIND_NORMAL;
  return c;
}

static Cell make_empty(void) {
  Cell c;
  c.color = 0;
  c.kind = KIND_NORMAL;
  return c;
}

/* Horizontal / vertical run length through (r,c). 0 if empty or prism. */
static void run_lens(const Board *b, int r, int c, int *hlen, int *vlen) {
  int i;
  Cell cur;
  *hlen = 0;
  *vlen = 0;
  if (!board_in_bounds(r, c)) return;
  cur = b->cells[r][c];
  if (!cell_alive(cur) || cur.kind == KIND_PRISM) return;

  *hlen = 1;
  for (i = c - 1; i >= 0 && cells_same_color(cur, b->cells[r][i]); i--) (*hlen)++;
  for (i = c + 1; i < BOARD_N && cells_same_color(cur, b->cells[r][i]); i++) (*hlen)++;

  *vlen = 1;
  for (i = r - 1; i >= 0 && cells_same_color(cur, b->cells[i][c]); i--) (*vlen)++;
  for (i = r + 1; i < BOARD_N && cells_same_color(cur, b->cells[i][c]); i++) (*vlen)++;
}

int board_mark_matches(const Board *b, int marked[BOARD_N][BOARD_N]) {
  int r, c, n = 0;
  int hlen, vlen;
  memset(marked, 0, sizeof(int) * BOARD_N * BOARD_N);
  for (r = 0; r < BOARD_N; r++) {
    for (c = 0; c < BOARD_N; c++) {
      run_lens(b, r, c, &hlen, &vlen);
      if (hlen >= 3 || vlen >= 3) {
        marked[r][c] = 1;
        n++;
      }
    }
  }
  return n;
}

int board_has_match(const Board *b) {
  int marked[BOARD_N][BOARD_N];
  return board_mark_matches(b, marked) > 0;
}

int board_adjacent(int r0, int c0, int r1, int c1) {
  int dr, dc;
  if (!board_in_bounds(r0, c0) || !board_in_bounds(r1, c1)) return 0;
  dr = r0 - r1;
  dc = c0 - c1;
  if (dr < 0) dr = -dr;
  if (dc < 0) dc = -dc;
  return (dr + dc) == 1;
}

void board_swap(Board *b, int r0, int c0, int r1, int c1) {
  Cell t;
  if (!board_in_bounds(r0, c0) || !board_in_bounds(r1, c1)) return;
  t = b->cells[r0][c0];
  b->cells[r0][c0] = b->cells[r1][c1];
  b->cells[r1][c1] = t;
}

int board_can_swap(Board *b, int r0, int c0, int r1, int c1) {
  int ok;
  if (!board_adjacent(r0, c0, r1, c1)) return 0;
  if (!cell_alive(b->cells[r0][c0]) || !cell_alive(b->cells[r1][c1])) return 0;
  if (b->cells[r0][c0].kind == KIND_PRISM || b->cells[r1][c1].kind == KIND_PRISM)
    return 1;
  board_swap(b, r0, c0, r1, c1);
  ok = board_has_match(b);
  board_swap(b, r0, c0, r1, c1);
  return ok;
}

int board_find_hint(Board *b, int *r0, int *c0, int *r1, int *c1) {
  int r, c;
  static const int dr[2] = {0, 1};
  static const int dc[2] = {1, 0};
  int d;
  for (r = 0; r < BOARD_N; r++) {
    for (c = 0; c < BOARD_N; c++) {
      for (d = 0; d < 2; d++) {
        int nr = r + dr[d], nc = c + dc[d];
        if (!board_in_bounds(nr, nc)) continue;
        if (board_can_swap(b, r, c, nr, nc)) {
          if (r0) *r0 = r;
          if (c0) *c0 = c;
          if (r1) *r1 = nr;
          if (c1) *c1 = nc;
          return 1;
        }
      }
    }
  }
  return 0;
}

int board_has_move(const Board *b) {
  Board tmp = *b;
  return board_find_hint(&tmp, 0, 0, 0, 0);
}

void board_gravity(Board *b) {
  int c, r, w;
  for (c = 0; c < BOARD_N; c++) {
    w = BOARD_N - 1;
    for (r = BOARD_N - 1; r >= 0; r--) {
      if (cell_alive(b->cells[r][c])) {
        if (w != r) {
          b->cells[w][c] = b->cells[r][c];
          b->cells[r][c] = make_empty();
        }
        w--;
      }
    }
    while (w >= 0) {
      b->cells[w][c] = make_empty();
      w--;
    }
  }
}

void board_refill(Board *b) {
  int r, c;
  for (r = 0; r < BOARD_N; r++) {
    for (c = 0; c < BOARD_N; c++) {
      if (!cell_alive(b->cells[r][c]))
        b->cells[r][c] = make_normal(board_rand_color(b));
    }
  }
}

static void mark_if(int marked[BOARD_N][BOARD_N], int r, int c, int *changed) {
  if (!board_in_bounds(r, c)) return;
  if (!marked[r][c]) {
    marked[r][c] = 1;
    if (changed) *changed = 1;
  }
}

static void expand_specials(const Board *b, int marked[BOARD_N][BOARD_N]) {
  int detonated[BOARD_N][BOARD_N];
  int again = 1;
  memset(detonated, 0, sizeof(detonated));
  while (again) {
    int r, c;
    again = 0;
    for (r = 0; r < BOARD_N; r++) {
      for (c = 0; c < BOARD_N; c++) {
        int k, i, dr, dc;
        if (!marked[r][c] || detonated[r][c]) continue;
        k = b->cells[r][c].kind;
        if (k == KIND_NORMAL) continue;
        detonated[r][c] = 1;
        again = 1;
        if (k == KIND_BURST) {
          for (dr = -1; dr <= 1; dr++)
            for (dc = -1; dc <= 1; dc++)
              mark_if(marked, r + dr, c + dc, 0);
        } else if (k == KIND_STAR) {
          for (i = 0; i < BOARD_N; i++) {
            mark_if(marked, r, i, 0);
            mark_if(marked, i, c, 0);
          }
        } else if (k == KIND_PRISM) {
          unsigned char pick = 0;
          int rr, cc;
          for (rr = 0; rr < BOARD_N && !pick; rr++)
            for (cc = 0; cc < BOARD_N && !pick; cc++) {
              if (marked[rr][cc] && b->cells[rr][cc].color)
                pick = b->cells[rr][cc].color;
            }
          if (!pick) {
            for (rr = 0; rr < BOARD_N && !pick; rr++)
              for (cc = 0; cc < BOARD_N && !pick; cc++)
                if (b->cells[rr][cc].color) pick = b->cells[rr][cc].color;
          }
          if (pick) {
            for (rr = 0; rr < BOARD_N; rr++)
              for (cc = 0; cc < BOARD_N; cc++)
                if (b->cells[rr][cc].color == pick) marked[rr][cc] = 1;
          }
        }
      }
    }
  }
}

static int decide_special(const Board *b, const int marked[BOARD_N][BOARD_N],
                          int pref_r, int pref_c, int *sr, int *sc) {
  int r, c, hlen, vlen;
  int has5 = 0, has_lt = 0, has4 = 0;
  int kind;

  for (r = 0; r < BOARD_N; r++) {
    for (c = 0; c < BOARD_N; c++) {
      if (!marked[r][c]) continue;
      run_lens(b, r, c, &hlen, &vlen);
      if (hlen >= 5 || vlen >= 5) has5 = 1;
      if (hlen >= 3 && vlen >= 3) has_lt = 1;
      if (hlen >= 4 || vlen >= 4) has4 = 1;
    }
  }

  if (has5) kind = KIND_PRISM;
  else if (has_lt) kind = KIND_STAR;
  else if (has4) kind = KIND_BURST;
  else return 0;

  if (board_in_bounds(pref_r, pref_c) && marked[pref_r][pref_c]) {
    run_lens(b, pref_r, pref_c, &hlen, &vlen);
    if ((kind == KIND_PRISM && (hlen >= 5 || vlen >= 5)) ||
        (kind == KIND_STAR && hlen >= 3 && vlen >= 3) ||
        (kind == KIND_BURST && (hlen >= 4 || vlen >= 4))) {
      *sr = pref_r;
      *sc = pref_c;
      return kind;
    }
  }

  for (r = 0; r < BOARD_N; r++) {
    for (c = 0; c < BOARD_N; c++) {
      if (!marked[r][c]) continue;
      run_lens(b, r, c, &hlen, &vlen);
      if (kind == KIND_PRISM && (hlen >= 5 || vlen >= 5)) {
        *sr = r; *sc = c; return kind;
      }
      if (kind == KIND_STAR && hlen >= 3 && vlen >= 3) {
        *sr = r; *sc = c; return kind;
      }
      if (kind == KIND_BURST && (hlen >= 4 || vlen >= 4)) {
        *sr = r; *sc = c; return kind;
      }
    }
  }
  *sr = pref_r;
  *sc = pref_c;
  return kind;
}

void board_clear_cells(Board *b, const int marked[BOARD_N][BOARD_N], WaveResult *out) {
  int r, c;
  for (r = 0; r < BOARD_N; r++) {
    for (c = 0; c < BOARD_N; c++) {
      int k;
      if (!marked[r][c] || !cell_alive(b->cells[r][c])) continue;
      k = b->cells[r][c].kind;
      if (out) {
        out->cleared++;
        if (k == KIND_BURST) out->bursts_popped++;
        if (k == KIND_STAR) out->stars_popped++;
        if (k == KIND_PRISM) out->prisms_popped++;
      }
      b->cells[r][c] = make_empty();
    }
  }
}

static void tally_pops(const Board *b, const int marked[BOARD_N][BOARD_N],
                       int skip_r, int skip_c, WaveResult *out) {
  int r, c;
  for (r = 0; r < BOARD_N; r++) {
    for (c = 0; c < BOARD_N; c++) {
      int k;
      if (!marked[r][c] || (r == skip_r && c == skip_c)) continue;
      if (!cell_alive(b->cells[r][c])) continue;
      k = b->cells[r][c].kind;
      out->cleared++;
      if (k == KIND_BURST) out->bursts_popped++;
      if (k == KIND_STAR) out->stars_popped++;
      if (k == KIND_PRISM) out->prisms_popped++;
    }
  }
}

static void plan_from_marks(const Board *b, int marked[BOARD_N][BOARD_N],
                            int spawn_kind, int sr, int sc, WavePlan *p) {
  unsigned char col = 0;
  memset(p, 0, sizeof(*p));
  memcpy(p->marked, marked, sizeof(p->marked));
  p->spawn_kind = spawn_kind;
  p->spawn_r = sr;
  p->spawn_c = sc;
  if (spawn_kind && board_in_bounds(sr, sc)) {
    col = b->cells[sr][sc].color;
    if (spawn_kind == KIND_PRISM) col = 0;
  }
  p->spawn_color = col;
  tally_pops(b, marked, spawn_kind ? sr : -1, spawn_kind ? sc : -1, &p->result);
  p->result.created = spawn_kind;
  p->result.spawn_r = sr;
  p->result.spawn_c = sc;
}

int board_plan_color(const Board *b, int pref_r, int pref_c, WavePlan *p) {
  int marked[BOARD_N][BOARD_N];
  int sr = -1, sc = -1, kind;
  if (!p) return 0;
  if (board_mark_matches(b, marked) == 0) {
    memset(p, 0, sizeof(*p));
    return 0;
  }
  kind = decide_special(b, marked, pref_r, pref_c, &sr, &sc);
  expand_specials(b, marked);
  plan_from_marks(b, marked, kind, sr, sc, p);
  return 1;
}

int board_plan_prism(const Board *b, int hr, int hc, int tr, int tc, WavePlan *p) {
  int marked[BOARD_N][BOARD_N];
  int r, c;
  Cell target;
  unsigned char col;
  if (!p) return 0;
  memset(p, 0, sizeof(*p));
  if (!board_in_bounds(hr, hc) || !board_in_bounds(tr, tc)) return 0;
  if (b->cells[hr][hc].kind != KIND_PRISM && b->cells[tr][tc].kind != KIND_PRISM)
    return 0;
  if (b->cells[hr][hc].kind != KIND_PRISM) {
    int tmp = hr; hr = tr; tr = tmp;
    tmp = hc; hc = tc; tc = tmp;
  }
  memset(marked, 0, sizeof(marked));
  marked[hr][hc] = 1;
  marked[tr][tc] = 1;
  target = b->cells[tr][tc];
  if (target.kind == KIND_PRISM) {
    for (r = 0; r < BOARD_N; r++)
      for (c = 0; c < BOARD_N; c++)
        marked[r][c] = 1;
  } else {
    col = target.color;
    if (col) {
      for (r = 0; r < BOARD_N; r++)
        for (c = 0; c < BOARD_N; c++)
          if (b->cells[r][c].color == col) marked[r][c] = 1;
    }
  }
  expand_specials(b, marked);
  plan_from_marks(b, marked, 0, -1, -1, p);
  return 1;
}

int board_plan_smash(const Board *b, int r, int c, WavePlan *p) {
  int marked[BOARD_N][BOARD_N];
  if (!p) return 0;
  memset(p, 0, sizeof(*p));
  if (!board_in_bounds(r, c) || !cell_alive(b->cells[r][c])) return 0;
  memset(marked, 0, sizeof(marked));
  marked[r][c] = 1;
  expand_specials(b, marked);
  plan_from_marks(b, marked, 0, -1, -1, p);
  return 1;
}

int board_plan_bolt(const Board *b, int r, int c, WavePlan *p) {
  int marked[BOARD_N][BOARD_N];
  int i;
  if (!p) return 0;
  memset(p, 0, sizeof(*p));
  if (!board_in_bounds(r, c)) return 0;
  memset(marked, 0, sizeof(marked));
  for (i = 0; i < BOARD_N; i++) marked[r][i] = 1;
  expand_specials(b, marked);
  plan_from_marks(b, marked, 0, -1, -1, p);
  return 1;
}

void board_apply_plan(Board *b, const WavePlan *p) {
  int r, c;
  if (!p) return;
  for (r = 0; r < BOARD_N; r++) {
    for (c = 0; c < BOARD_N; c++) {
      if (!p->marked[r][c]) continue;
      if (p->spawn_kind && r == p->spawn_r && c == p->spawn_c) continue;
      b->cells[r][c] = make_empty();
    }
  }
  if (p->spawn_kind && board_in_bounds(p->spawn_r, p->spawn_c)) {
    b->cells[p->spawn_r][p->spawn_c].color = p->spawn_color;
    b->cells[p->spawn_r][p->spawn_c].kind = (unsigned char)p->spawn_kind;
  }
}

void board_settle(Board *b) {
  board_gravity(b);
  board_refill(b);
}

int board_resolve_wave(Board *b, int pref_r, int pref_c, WaveResult *out) {
  WavePlan p;
  if (!board_plan_color(b, pref_r, pref_c, &p)) return 0;
  if (out) *out = p.result;
  board_apply_plan(b, &p);
  board_settle(b);
  return 1;
}

int board_activate_prism(Board *b, int hr, int hc, int tr, int tc, WaveResult *out) {
  WavePlan p;
  if (!board_plan_prism(b, hr, hc, tr, tc, &p)) return 0;
  if (out) *out = p.result;
  board_apply_plan(b, &p);
  board_settle(b);
  return 1;
}

int board_resolve_all(Board *b, int pref_r, int pref_c, WaveResult *total) {
  WaveResult w;
  int waves = 0;
  if (total) memset(total, 0, sizeof(*total));
  while (board_resolve_wave(b, pref_r, pref_c, &w)) {
    waves++;
    if (total) {
      total->cleared += w.cleared;
      total->bursts_popped += w.bursts_popped;
      total->stars_popped += w.stars_popped;
      total->prisms_popped += w.prisms_popped;
      if (w.created) {
        total->created = w.created;
        total->spawn_r = w.spawn_r;
        total->spawn_c = w.spawn_c;
      }
    }
    pref_r = -1;
    pref_c = -1;
  }
  return waves;
}

int board_smash(Board *b, int r, int c, WaveResult *out) {
  WavePlan p;
  if (!board_plan_smash(b, r, c, &p)) return 0;
  if (out) *out = p.result;
  board_apply_plan(b, &p);
  board_settle(b);
  return 1;
}

int board_bolt(Board *b, int r, int c, WaveResult *out) {
  WavePlan p;
  if (!board_plan_bolt(b, r, c, &p)) return 0;
  if (out) *out = p.result;
  board_apply_plan(b, &p);
  board_settle(b);
  return 1;
}

void board_fill_stable(Board *b) {
  int tries, r, c;
  for (tries = 0; tries < 100; tries++) {
    for (r = 0; r < BOARD_N; r++)
      for (c = 0; c < BOARD_N; c++)
        b->cells[r][c] = make_normal(board_rand_color(b));
    if (!board_has_match(b) && board_has_move(b)) return;
  }
  /* Deterministic fallback: cycling 1..7, then tweak one swap-friendly pair. */
  for (r = 0; r < BOARD_N; r++) {
    for (c = 0; c < BOARD_N; c++)
      b->cells[r][c] = make_normal((unsigned char)(1 + (r + 2 * c) % N_COLORS));
  }
  if (board_has_match(b)) {
    for (r = 0; r < BOARD_N; r++) {
      for (c = 0; c < BOARD_N; c++) {
        if (board_has_match(b))
          b->cells[r][c] = make_normal(board_rand_color(b));
      }
    }
  }
}

void board_shuffle(Board *b) {
  Cell list[BOARD_N * BOARD_N];
  int n = 0, r, c, i, j, tries;
  for (r = 0; r < BOARD_N; r++)
    for (c = 0; c < BOARD_N; c++)
      list[n++] = b->cells[r][c];

  for (tries = 0; tries < 80; tries++) {
    for (i = n - 1; i > 0; i--) {
      Cell t;
      j = board_rand_n(b, i + 1);
      t = list[i];
      list[i] = list[j];
      list[j] = t;
    }
    i = 0;
    for (r = 0; r < BOARD_N; r++)
      for (c = 0; c < BOARD_N; c++)
        b->cells[r][c] = list[i++];
    if (!board_has_match(b) && board_has_move(b)) return;
  }
}
