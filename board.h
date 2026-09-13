#ifndef CLINK_BOARD_H
#define CLINK_BOARD_H

#define BOARD_N 8
#define N_COLORS 7

enum {
  KIND_NORMAL = 0,
  KIND_BURST = 1,
  KIND_STAR = 2,
  KIND_PRISM = 3
};

typedef struct {
  unsigned char color; /* 0 empty (unless PRISM), 1..N_COLORS */
  unsigned char kind;
} Cell;

typedef struct {
  Cell cells[BOARD_N][BOARD_N];
  unsigned rng;
} Board;

typedef struct {
  int cleared;
  int created; /* KIND_* or 0 */
  int spawn_r, spawn_c;
  int bursts_popped;
  int stars_popped;
  int prisms_popped;
} WaveResult;

typedef struct {
  int marked[BOARD_N][BOARD_N];
  WaveResult result;
  int spawn_kind;
  int spawn_r, spawn_c;
  unsigned char spawn_color;
} WavePlan;

void board_seed(Board *b, unsigned seed);
int board_rand(Board *b);
int board_rand_n(Board *b, int n);
unsigned char board_rand_color(Board *b);

int cell_alive(Cell c);
int cells_same_color(Cell a, Cell b);
int board_in_bounds(int r, int c);

void board_clear(Board *b);
void board_fill_stable(Board *b);
void board_shuffle(Board *b);

int board_mark_matches(const Board *b, int marked[BOARD_N][BOARD_N]);
int board_has_match(const Board *b);
int board_has_move(const Board *b);
int board_find_hint(Board *b, int *r0, int *c0, int *r1, int *c1);

int board_adjacent(int r0, int c0, int r1, int c1);
int board_can_swap(Board *b, int r0, int c0, int r1, int c1);
void board_swap(Board *b, int r0, int c0, int r1, int c1);

int board_plan_color(const Board *b, int pref_r, int pref_c, WavePlan *p);
int board_plan_prism(const Board *b, int hr, int hc, int tr, int tc, WavePlan *p);
int board_plan_smash(const Board *b, int r, int c, WavePlan *p);
int board_plan_bolt(const Board *b, int r, int c, WavePlan *p);
void board_apply_plan(Board *b, const WavePlan *p);
void board_settle(Board *b);

int board_resolve_wave(Board *b, int pref_r, int pref_c, WaveResult *out);
int board_activate_prism(Board *b, int hr, int hc, int tr, int tc, WaveResult *out);
int board_resolve_all(Board *b, int pref_r, int pref_c, WaveResult *total);
void board_clear_cells(Board *b, const int marked[BOARD_N][BOARD_N], WaveResult *out);
void board_gravity(Board *b);
void board_refill(Board *b);
int board_smash(Board *b, int r, int c, WaveResult *out);
int board_bolt(Board *b, int r, int c, WaveResult *out);

#endif
