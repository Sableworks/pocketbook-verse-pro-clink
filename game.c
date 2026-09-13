#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int meter_for_level(int level) {
  if (level < 1) level = 1;
  return 280 + 220 * level;
}

static void grant_start_powers(Game *g) {
  if (g->mode == MODE_ENDLESS) {
    g->hints = 5;
    g->smashes = 2;
    g->bolts = 2;
    g->shuffles = 3;
    g->free_reshuffle = 99;
    g->time_left = 0;
  } else if (g->mode == MODE_TIMED) {
    g->hints = 2;
    g->smashes = 1;
    g->bolts = 1;
    g->shuffles = 1;
    g->free_reshuffle = 1;
    g->time_left = 75;
  } else {
    g->hints = 3;
    g->smashes = 1;
    g->bolts = 1;
    g->shuffles = 1;
    g->free_reshuffle = 1;
    g->time_left = 0;
  }
}

static void level_up(Game *g) {
  g->level++;
  g->meter_max = meter_for_level(g->level);
  g->hints++;
  if (g->level % 2 == 0) g->smashes++;
  if (g->level % 3 == 0) g->bolts++;
  if (g->level % 4 == 0) g->shuffles++;
  if (g->mode == MODE_TIMED) {
    g->time_left += 8;
    if (g->time_left > 150) g->time_left = 150;
  }
}

void game_init(Game *g, int mode, unsigned seed) {
  ScoreEntry saved[N_MODES][N_SCORES];
  int saved_n[N_MODES];
  memcpy(saved, g->scores, sizeof(saved));
  memcpy(saved_n, g->score_n, sizeof(saved_n));

  memset(g, 0, sizeof(*g));
  memcpy(g->scores, saved, sizeof(saved));
  memcpy(g->score_n, saved_n, sizeof(saved_n));

  if (mode < 0 || mode >= N_MODES) mode = MODE_CLASSIC;
  g->mode = mode;
  g->status = ST_PLAYING;
  g->level = 1;
  g->meter_max = meter_for_level(1);
  grant_start_powers(g);
  board_seed(&g->board, seed ? seed : (unsigned)time(0));
  board_fill_stable(&g->board);
}

void game_reset_combo(Game *g) {
  g->combo = 0;
}

long game_apply_wave(Game *g, const WaveResult *w) {
  long pts;
  if (!w || w->cleared <= 0) return 0;
  g->combo++;
  if (g->combo > g->best_combo) g->best_combo = g->combo;
  pts = (long)w->cleared * 10L * (long)g->combo;
  if (w->created == KIND_BURST) pts += 50;
  if (w->created == KIND_STAR) pts += 100;
  if (w->created == KIND_PRISM) pts += 250;
  pts += 20L * w->bursts_popped + 40L * w->stars_popped + 80L * w->prisms_popped;
  pts = pts * (10 + g->level) / 10;
  g->score += pts;
  g->stones_cleared += w->cleared;
  g->meter += w->cleared * 10 + (int)(pts / 8);
  if (w->created == KIND_PRISM) g->cascade_prism = 1;
  while (g->meter >= g->meter_max) {
    g->meter -= g->meter_max;
    level_up(g);
  }
  return pts;
}

void game_end(Game *g, int reason) {
  g->status = ST_OVER;
  g->over_reason = reason;
}

void game_finish_cascade(Game *g) {
  if (g->combo >= 5) g->smashes++;
  if (g->cascade_prism) g->bolts++;
  if (g->mode == MODE_TIMED && g->combo >= 2) {
    g->time_left += 2;
    if (g->time_left > 150) g->time_left = 150;
  }
  g->combo = 0;
  g->cascade_prism = 0;
  g->armed = PWR_NONE;
  game_ensure_moves(g);
}

int game_ensure_moves(Game *g) {
  g->note = NOTE_NONE;
  if (g->status != ST_PLAYING) return 0;
  if (board_has_move(&g->board)) return 1;
  if (g->mode == MODE_ENDLESS) {
    board_shuffle(&g->board);
    g->note = NOTE_SHUFFLED;
    return 1;
  }
  if (g->shuffles > 0 || g->free_reshuffle > 0) {
    if (g->shuffles > 0) g->shuffles--;
    else g->free_reshuffle--;
    board_shuffle(&g->board);
    if (board_has_move(&g->board)) {
      g->note = NOTE_SHUFFLED;
      return 1;
    }
  }
  game_end(g, OVER_NOMOVES);
  return 0;
}

int game_try_swap(Game *g, int r0, int c0, int r1, int c1) {
  int prism;
  if (g->status != ST_PLAYING) return SWAP_NONE;
  if (!board_can_swap(&g->board, r0, c0, r1, c1)) return SWAP_NONE;
  g->moves++;
  prism = g->board.cells[r0][c0].kind == KIND_PRISM ||
          g->board.cells[r1][c1].kind == KIND_PRISM;
  if (prism) return SWAP_PRISM;
  board_swap(&g->board, r0, c0, r1, c1);
  return SWAP_MATCH;
}

int game_arm_ok(const Game *g, int pwr) {
  if (pwr == PWR_HINT) return g->hints > 0;
  if (pwr == PWR_SMASH) return g->smashes > 0;
  if (pwr == PWR_BOLT) return g->bolts > 0;
  if (pwr == PWR_SHUFFLE) return g->shuffles > 0;
  return 0;
}

int game_use_hint(Game *g, int *r0, int *c0, int *r1, int *c1) {
  if (g->status != ST_PLAYING) return 0;
  if (g->hints <= 0) return 0;
  if (!board_find_hint(&g->board, r0, c0, r1, c1)) return 0;
  g->hints--;
  return 1;
}

int game_use_shuffle(Game *g) {
  if (g->status != ST_PLAYING) return 0;
  if (g->shuffles <= 0) return 0;
  g->shuffles--;
  board_shuffle(&g->board);
  return 1;
}

int game_meter_pct(const Game *g) {
  if (g->meter_max <= 0) return 0;
  if (g->meter >= g->meter_max) return 100;
  return (g->meter * 100) / g->meter_max;
}

const char *game_mode_name(int mode) {
  if (mode == MODE_ENDLESS) return "Endless";
  if (mode == MODE_TIMED) return "Timed";
  return "Classic";
}

const char *game_over_text(const Game *g) {
  if (g->over_reason == OVER_TIME) return "Time is up";
  return "No moves left";
}

void game_add_score(Game *g, long when) {
  ScoreEntry e;
  int i, n, mode;
  if (g->score <= 0) return;
  mode = g->mode;
  e.score = g->score;
  e.level = g->level;
  e.mode = mode;
  e.when = when;
  n = g->score_n[mode];
  for (i = 0; i < n; i++) {
    if (e.score > g->scores[mode][i].score) break;
  }
  if (i >= N_SCORES) return;
  if (n < N_SCORES) n++;
  if (i < n - 1) {
    memmove(&g->scores[mode][i + 1], &g->scores[mode][i],
            sizeof(ScoreEntry) * (size_t)(n - 1 - i));
  }
  g->scores[mode][i] = e;
  g->score_n[mode] = n;
}

int game_save(const Game *g, const char *path) {
  FILE *f;
  int r, c;
  if (!path) return 0;
  f = fopen(path, "w");
  if (!f) return 0;
  fprintf(f, "magic=CLNK1\n");
  fprintf(f, "over=%d\n", g->over_reason);
  fprintf(f, "mode=%d\n", g->mode);
  fprintf(f, "status=%d\n", g->status);
  fprintf(f, "score=%ld\n", g->score);
  fprintf(f, "level=%d\n", g->level);
  fprintf(f, "meter=%d\n", g->meter);
  fprintf(f, "meter_max=%d\n", g->meter_max);
  fprintf(f, "combo=%d\n", g->combo);
  fprintf(f, "best_combo=%d\n", g->best_combo);
  fprintf(f, "stones=%d\n", g->stones_cleared);
  fprintf(f, "moves=%d\n", g->moves);
  fprintf(f, "hints=%d\n", g->hints);
  fprintf(f, "smashes=%d\n", g->smashes);
  fprintf(f, "bolts=%d\n", g->bolts);
  fprintf(f, "shuffles=%d\n", g->shuffles);
  fprintf(f, "free=%d\n", g->free_reshuffle);
  fprintf(f, "time=%d\n", g->time_left);
  fprintf(f, "rng=%u\n", g->board.rng);
  for (r = 0; r < BOARD_N; r++) {
    for (c = 0; c < BOARD_N; c++) {
      fprintf(f, "c%d%d=%u,%u\n", r, c,
              (unsigned)g->board.cells[r][c].color,
              (unsigned)g->board.cells[r][c].kind);
    }
  }
  fclose(f);
  return 1;
}

int game_load(Game *g, const char *path) {
  FILE *f;
  char line[128];
  int got_magic = 0;
  ScoreEntry saved[N_MODES][N_SCORES];
  int saved_n[N_MODES];
  if (!path) return 0;
  f = fopen(path, "r");
  if (!f) return 0;
  memcpy(saved, g->scores, sizeof(saved));
  memcpy(saved_n, g->score_n, sizeof(saved_n));
  memset(g, 0, sizeof(*g));
  memcpy(g->scores, saved, sizeof(saved));
  memcpy(g->score_n, saved_n, sizeof(saved_n));
  g->level = 1;
  g->meter_max = meter_for_level(1);
  while (fgets(line, sizeof(line), f)) {
    char *eq = strchr(line, '=');
    char *val;
    size_t L;
    if (!eq) continue;
    *eq = '\0';
    val = eq + 1;
    L = strlen(val);
    while (L > 0 && (val[L - 1] == '\n' || val[L - 1] == '\r')) val[--L] = '\0';
    if (strcmp(line, "magic") == 0) got_magic = (strcmp(val, "CLNK1") == 0);
    else if (strcmp(line, "over") == 0) g->over_reason = atoi(val);
    else if (strcmp(line, "mode") == 0) g->mode = atoi(val);
    else if (strcmp(line, "status") == 0) g->status = atoi(val);
    else if (strcmp(line, "score") == 0) g->score = atol(val);
    else if (strcmp(line, "level") == 0) g->level = atoi(val);
    else if (strcmp(line, "meter") == 0) g->meter = atoi(val);
    else if (strcmp(line, "meter_max") == 0) g->meter_max = atoi(val);
    else if (strcmp(line, "combo") == 0) g->combo = atoi(val);
    else if (strcmp(line, "best_combo") == 0) g->best_combo = atoi(val);
    else if (strcmp(line, "stones") == 0) g->stones_cleared = atoi(val);
    else if (strcmp(line, "moves") == 0) g->moves = atoi(val);
    else if (strcmp(line, "hints") == 0) g->hints = atoi(val);
    else if (strcmp(line, "smashes") == 0) g->smashes = atoi(val);
    else if (strcmp(line, "bolts") == 0) g->bolts = atoi(val);
    else if (strcmp(line, "shuffles") == 0) g->shuffles = atoi(val);
    else if (strcmp(line, "free") == 0) g->free_reshuffle = atoi(val);
    else if (strcmp(line, "time") == 0) g->time_left = atoi(val);
    else if (strcmp(line, "rng") == 0) g->board.rng = (unsigned)strtoul(val, 0, 10);
    else if (line[0] == 'c' && strlen(line) == 3) {
      int r = line[1] - '0';
      int c = line[2] - '0';
      unsigned col = 0, kind = 0;
      if (r >= 0 && r < BOARD_N && c >= 0 && c < BOARD_N &&
          sscanf(val, "%u,%u", &col, &kind) == 2) {
        g->board.cells[r][c].color = (unsigned char)col;
        g->board.cells[r][c].kind = (unsigned char)kind;
      }
    }
  }
  fclose(f);
  if (!got_magic) return 0;
  if (g->mode < 0 || g->mode >= N_MODES) g->mode = MODE_CLASSIC;
  if (g->level < 1) g->level = 1;
  if (g->meter_max < 1) g->meter_max = meter_for_level(g->level);
  g->armed = PWR_NONE;
  g->combo = 0;
  return 1;
}

int game_save_scores(const Game *g, const char *path) {
  FILE *f;
  int m, i;
  if (!path) return 0;
  f = fopen(path, "w");
  if (!f) return 0;
  fprintf(f, "magic=CLSC1\n");
  for (m = 0; m < N_MODES; m++) {
    fprintf(f, "n%d=%d\n", m, g->score_n[m]);
    for (i = 0; i < g->score_n[m] && i < N_SCORES; i++) {
      fprintf(f, "s%d%d=%ld,%d,%ld\n", m, i,
              g->scores[m][i].score,
              g->scores[m][i].level,
              g->scores[m][i].when);
    }
  }
  fclose(f);
  return 1;
}

int game_load_scores(Game *g, const char *path) {
  FILE *f;
  char line[128];
  int ok = 0;
  if (!path) return 0;
  f = fopen(path, "r");
  if (!f) return 0;
  memset(g->scores, 0, sizeof(g->scores));
  memset(g->score_n, 0, sizeof(g->score_n));
  while (fgets(line, sizeof(line), f)) {
    char *eq = strchr(line, '=');
    char *val;
    size_t L;
    if (!eq) continue;
    *eq = '\0';
    val = eq + 1;
    L = strlen(val);
    while (L > 0 && (val[L - 1] == '\n' || val[L - 1] == '\r')) val[--L] = '\0';
    if (strcmp(line, "magic") == 0) ok = (strcmp(val, "CLSC1") == 0);
    else if (line[0] == 'n' && line[1] >= '0' && line[1] < '0' + N_MODES) {
      int m = line[1] - '0';
      g->score_n[m] = atoi(val);
      if (g->score_n[m] > N_SCORES) g->score_n[m] = N_SCORES;
      if (g->score_n[m] < 0) g->score_n[m] = 0;
    } else if (line[0] == 's' && strlen(line) >= 3) {
      int m = line[1] - '0';
      int i = line[2] - '0';
      long sc = 0, when = 0;
      int lv = 0;
      if (m >= 0 && m < N_MODES && i >= 0 && i < N_SCORES &&
          sscanf(val, "%ld,%d,%ld", &sc, &lv, &when) == 3) {
        g->scores[m][i].score = sc;
        g->scores[m][i].level = lv;
        g->scores[m][i].mode = m;
        g->scores[m][i].when = when;
      }
    }
  }
  fclose(f);
  return ok;
}

int game_save_prefs(int coach_done, const char *path) {
  FILE *f;
  if (!path) return 0;
  f = fopen(path, "w");
  if (!f) return 0;
  fprintf(f, "magic=CLPF1\n");
  fprintf(f, "coach=%d\n", coach_done ? 1 : 0);
  fclose(f);
  return 1;
}

int game_load_prefs(int *coach_done, const char *path) {
  FILE *f;
  char line[128];
  int ok = 0;
  if (coach_done) *coach_done = 0;
  if (!path) return 0;
  f = fopen(path, "r");
  if (!f) return 0;
  while (fgets(line, sizeof(line), f)) {
    char *eq = strchr(line, '=');
    char *val;
    size_t L;
    if (!eq) continue;
    *eq = '\0';
    val = eq + 1;
    L = strlen(val);
    while (L > 0 && (val[L - 1] == '\n' || val[L - 1] == '\r')) val[--L] = '\0';
    if (strcmp(line, "magic") == 0) ok = (strcmp(val, "CLPF1") == 0);
    else if (strcmp(line, "coach") == 0 && coach_done)
      *coach_done = atoi(val) ? 1 : 0;
  }
  fclose(f);
  return ok;
}
