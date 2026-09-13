/* Host smoke tests for the match-3 engine (no InkView).
 *   cc -O2 -Wall -o tests/host_smoke board.c game.c tests/host_smoke.c && ./tests/host_smoke
 */
#include "../board.h"
#include "../game.h"

#include <stdio.h>
#include <string.h>

static int fails;

static void expect(int cond, const char *name) {
  if (!cond) {
    fprintf(stderr, "FAIL %s\n", name);
    fails++;
  }
}

static void set_row(Board *b, int r, const char *colors) {
  int c;
  for (c = 0; c < BOARD_N && colors[c]; c++) {
    char ch = colors[c];
    b->cells[r][c].kind = KIND_NORMAL;
    if (ch == '.') b->cells[r][c].color = 0;
    else b->cells[r][c].color = (unsigned char)(ch - '0');
  }
}

static void fill_unique(Board *b) {
  int r, c;
  board_clear(b);
  board_seed(b, 1);
  for (r = 0; r < BOARD_N; r++)
    for (c = 0; c < BOARD_N; c++) {
      b->cells[r][c].color = (unsigned char)(1 + (r + 2 * c) % N_COLORS);
      b->cells[r][c].kind = KIND_NORMAL;
    }
}

static int test_match3(void) {
  Board b;
  int marked[BOARD_N][BOARD_N];
  fill_unique(&b);
  set_row(&b, 0, "11123456");
  expect(board_mark_matches(&b, marked) >= 3, "h3 count");
  expect(marked[0][0] && marked[0][1] && marked[0][2], "h3 cells");
  expect(!marked[0][3], "h3 stop");
  return 0;
}

static int test_no_match(void) {
  Board b;
  fill_unique(&b);
  expect(!board_has_match(&b), "unique no match");
  return 0;
}

static int test_vertical(void) {
  Board b;
  int r;
  fill_unique(&b);
  for (r = 0; r < 3; r++) {
    b.cells[r][0].color = 2;
    b.cells[r][0].kind = KIND_NORMAL;
  }
  expect(board_has_match(&b), "v3 match");
  return 0;
}

static int test_burst_on_4(void) {
  Board b;
  WaveResult w;
  fill_unique(&b);
  set_row(&b, 7, "11112345");
  memset(&w, 0, sizeof(w));
  expect(board_resolve_wave(&b, 7, 0, &w), "burst wave");
  expect(w.created == KIND_BURST, "burst created");
  expect(w.cleared >= 3, "burst rest cleared");
  expect(b.cells[7][0].kind == KIND_BURST ||
         b.cells[w.spawn_r][w.spawn_c].kind == KIND_BURST, "burst stays");
  return 0;
}

static int test_prism_on_5(void) {
  Board b;
  WaveResult w;
  fill_unique(&b);
  set_row(&b, 7, "11111234");
  memset(&w, 0, sizeof(w));
  expect(board_resolve_wave(&b, 7, 2, &w), "prism wave");
  expect(w.created == KIND_PRISM, "prism created");
  expect(b.cells[w.spawn_r][w.spawn_c].kind == KIND_PRISM, "prism cell");
  return 0;
}

static int test_star_on_lt(void) {
  Board b;
  WaveResult w;
  int r;
  fill_unique(&b);
  set_row(&b, 5, "11145672");
  for (r = 3; r <= 5; r++) {
    b.cells[r][1].color = 1;
    b.cells[r][1].kind = KIND_NORMAL;
  }
  memset(&w, 0, sizeof(w));
  expect(board_resolve_wave(&b, 5, 1, &w), "star wave");
  expect(w.created == KIND_STAR, "star created");
  return 0;
}

static int test_invalid_swap(void) {
  Board b;
  fill_unique(&b);
  expect(!board_can_swap(&b, 0, 0, 0, 1), "no-match swap rejected");
  expect(!board_adjacent(0, 0, 1, 1), "diagonal not adjacent");
  expect(board_adjacent(0, 0, 0, 1), "ortho adjacent");
  return 0;
}

static int test_valid_swap(void) {
  Board b;
  fill_unique(&b);
  /* Make a swap create three 1s on the bottom row. */
  set_row(&b, 7, "11213456");
  b.cells[6][2].color = 1;
  b.cells[6][2].kind = KIND_NORMAL;
  expect(board_can_swap(&b, 6, 2, 7, 2), "swap makes 3");
  board_swap(&b, 6, 2, 7, 2);
  expect(board_has_match(&b), "match after swap");
  return 0;
}

static int test_gravity(void) {
  Board b;
  board_clear(&b);
  b.cells[0][0].color = 3;
  b.cells[0][0].kind = KIND_NORMAL;
  board_gravity(&b);
  expect(b.cells[7][0].color == 3, "stone fell");
  expect(!cell_alive(b.cells[0][0]), "top empty");
  return 0;
}

static int test_prism_swap(void) {
  Board b;
  WaveResult w;
  int r;
  fill_unique(&b);
  b.cells[0][0].kind = KIND_PRISM;
  b.cells[0][0].color = 0;
  b.cells[0][1].color = 2;
  b.cells[0][1].kind = KIND_NORMAL;
  for (r = 1; r < BOARD_N; r++) {
    b.cells[r][3].color = 2;
    b.cells[r][3].kind = KIND_NORMAL;
  }
  expect(board_can_swap(&b, 0, 0, 0, 1), "prism always swappable");
  board_activate_prism(&b, 0, 0, 0, 1, &w);
  expect(w.cleared >= 2, "prism cleared color");
  return 0;
}

static int test_burst_explode(void) {
  Board b;
  WaveResult w;
  fill_unique(&b);
  b.cells[4][4].kind = KIND_BURST;
  b.cells[4][4].color = 1;
  b.cells[4][3].color = 1;
  b.cells[4][5].color = 1;
  board_smash(&b, 4, 4, &w);
  expect(w.cleared >= 9 || w.bursts_popped >= 1, "burst blast");
  return 0;
}

static int test_hint_and_fill(void) {
  Board b;
  int r0, c0, r1, c1;
  board_seed(&b, 42);
  board_fill_stable(&b);
  expect(!board_has_match(&b), "stable start");
  expect(board_has_move(&b), "stable has move");
  expect(board_find_hint(&b, &r0, &c0, &r1, &c1), "hint found");
  expect(board_can_swap(&b, r0, c0, r1, c1), "hint is legal");
  return 0;
}

static int test_game_score(void) {
  Game g;
  WaveResult w;
  memset(&g, 0, sizeof(g));
  game_init(&g, MODE_CLASSIC, 7);
  memset(&w, 0, sizeof(w));
  w.cleared = 3;
  expect(game_apply_wave(&g, &w) > 0, "score > 0");
  expect(g.score > 0, "score stored");
  expect(g.combo == 1, "combo 1");
  game_apply_wave(&g, &w);
  expect(g.combo == 2, "combo 2");
  game_finish_cascade(&g);
  expect(g.combo == 0, "combo reset");
  return 0;
}

static int test_save_roundtrip(void) {
  Game a, b;
  const char *sp = "tests/_smoke_save";
  const char *sc = "tests/_smoke_scores";
  memset(&a, 0, sizeof(a));
  game_init(&a, MODE_TIMED, 99);
  a.score = 12345;
  a.level = 3;
  a.hints = 8;
  a.board.cells[2][3].color = 5;
  a.board.cells[2][3].kind = KIND_STAR;
  expect(game_save(&a, sp), "save game");
  memset(&b, 0, sizeof(b));
  expect(game_load(&b, sp), "load game");
  expect(b.score == 12345, "score rt");
  expect(b.level == 3, "level rt");
  expect(b.mode == MODE_TIMED, "mode rt");
  expect(b.board.cells[2][3].kind == KIND_STAR, "star rt");
  expect(b.board.cells[2][3].color == 5, "color rt");
  game_add_score(&a, 100);
  expect(game_save_scores(&a, sc), "save scores");
  memset(&b, 0, sizeof(b));
  expect(game_load_scores(&b, sc), "load scores");
  expect(b.score_n[MODE_TIMED] >= 1, "score n");
  expect(b.scores[MODE_TIMED][0].score == 12345, "top score");
  remove(sp);
  remove(sc);
  return 0;
}

static int test_play_swap(void) {
  Game g;
  WaveResult w;
  WavePlan p;
  memset(&g, 0, sizeof(g));
  game_init(&g, MODE_ENDLESS, 3);
  fill_unique(&g.board);
  set_row(&g.board, 7, "11213456");
  g.board.cells[6][2].color = 1;
  g.board.cells[6][2].kind = KIND_NORMAL;
  expect(game_try_swap(&g, 6, 2, 7, 2) == SWAP_MATCH, "try swap");
  expect(g.moves == 1, "move counted");
  expect(board_has_match(&g.board), "match ready");
  expect(board_plan_color(&g.board, 7, 2, &p), "plan");
  expect(p.result.cleared >= 3, "plan cleared");
  board_apply_plan(&g.board, &p);
  board_settle(&g.board);
  expect(board_resolve_wave(&g.board, 7, 2, &w) || !board_has_match(&g.board),
         "after settle");
  return 0;
}

static int test_economy_once(void) {
  Game g;
  WaveResult w;
  memset(&g, 0, sizeof(g));
  game_init(&g, MODE_CLASSIC, 1);
  memset(&w, 0, sizeof(w));
  w.cleared = 3;
  w.created = KIND_PRISM;
  game_apply_wave(&g, &w);
  game_apply_wave(&g, &w);
  game_apply_wave(&g, &w);
  game_apply_wave(&g, &w);
  game_apply_wave(&g, &w);
  {
    int smashes = g.smashes;
    int bolts = g.bolts;
    game_finish_cascade(&g);
    expect(g.smashes == smashes + 1, "smash once per cascade");
    expect(g.bolts == bolts + 1, "bolt once per prism cascade");
  }
  return 0;
}

int main(void) {
  test_match3();
  test_no_match();
  test_vertical();
  test_burst_on_4();
  test_prism_on_5();
  test_star_on_lt();
  test_invalid_swap();
  test_valid_swap();
  test_gravity();
  test_prism_swap();
  test_burst_explode();
  test_hint_and_fill();
  test_game_score();
  test_save_roundtrip();
  test_play_swap();
  test_economy_once();
  if (fails) {
    fprintf(stderr, "%d test(s) failed\n", fails);
    return 1;
  }
  printf("host_smoke: all tests passed\n");
  return 0;
}
