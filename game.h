#ifndef CLINK_GAME_H
#define CLINK_GAME_H

#include "board.h"

#define N_MODES 3
#define N_SCORES 10
#define N_PWR 4

enum {
  MODE_CLASSIC = 0,
  MODE_ENDLESS = 1,
  MODE_TIMED = 2
};

enum {
  ST_PLAYING = 0,
  ST_OVER = 1
};

enum {
  OVER_NONE = 0,
  OVER_NOMOVES = 1,
  OVER_TIME = 2
};

enum {
  NOTE_NONE = 0,
  NOTE_SHUFFLED = 1
};

enum {
  PWR_NONE = 0,
  PWR_HINT = 1,
  PWR_SMASH = 2,
  PWR_BOLT = 3,
  PWR_SHUFFLE = 4
};

enum {
  SWAP_NONE = 0,
  SWAP_MATCH = 1,
  SWAP_PRISM = 2
};

typedef struct {
  long score;
  int level;
  int mode;
  long when;
} ScoreEntry;

typedef struct {
  int mode;
  int status;
  int over_reason;
  long score;
  int level;
  int meter;
  int meter_max;
  int combo;
  int best_combo;
  int stones_cleared;
  int moves;
  int hints;
  int smashes;
  int bolts;
  int shuffles;
  int free_reshuffle;
  int time_left;
  int armed;
  int cascade_prism;
  int note;
  Board board;
  ScoreEntry scores[N_MODES][N_SCORES];
  int score_n[N_MODES];
} Game;

void game_init(Game *g, int mode, unsigned seed);
void game_reset_combo(Game *g);
long game_apply_wave(Game *g, const WaveResult *w);
void game_finish_cascade(Game *g);
int game_ensure_moves(Game *g);

int game_try_swap(Game *g, int r0, int c0, int r1, int c1);
int game_use_hint(Game *g, int *r0, int *c0, int *r1, int *c1);
int game_use_shuffle(Game *g);
int game_arm_ok(const Game *g, int pwr);

int game_meter_pct(const Game *g);
const char *game_mode_name(int mode);
const char *game_over_text(const Game *g);
void game_add_score(Game *g, long when);
void game_end(Game *g, int reason);

int game_save(const Game *g, const char *path);
int game_load(Game *g, const char *path);
int game_save_scores(const Game *g, const char *path);
int game_load_scores(Game *g, const char *path);
int game_save_prefs(int coach_done, const char *path);
int game_load_prefs(int *coach_done, const char *path);

#endif
