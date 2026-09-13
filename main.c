#include "inkview.h"
#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef KEY_HOME
#define KEY_HOME 0x1a
#endif

#define APP_NAME "Clink"
#define SAVE_PATH FLASHDIR "/.clink_save"
#define SCORE_PATH FLASHDIR "/.clink_scores"
#define PREF_PATH FLASHDIR "/.clink_prefs"

#define BEAT_TIMER "cl_beat"
#define TICK_TIMER "cl_tick"
#define BEAT_MS 300
#define BAD_MS 320

#define COL_BG 0xE8E4D8
#define COL_HUD 0xFFFFFF
#define COL_RED 0xC41E1E
#define COL_ORANGE 0xD86A10
#define COL_YELLOW 0xD4B000
#define COL_GREEN 0x1E7A28
#define COL_BLUE 0x1A5CB0
#define COL_PURPLE 0x7A20A0
#define COL_TEAL 0x1A8A8A

enum {
  SCR_TITLE = 0,
  SCR_PLAY,
  SCR_PAUSE,
  SCR_OVER,
  SCR_SCORES,
  SCR_HELP
};

enum {
  R_IDLE = 0,
  R_FLASH,
  R_HOLES,
  R_SETTLE,
  R_BAD
};

enum {
  ACT_NONE = 0,
  ACT_NEW,
  ACT_QUIT
};

typedef struct {
  int x, y, w, h;
} Rect;

static struct {
  Game game;
  int sw, sh;
  int screen;
  int busy;
  int phase;
  WavePlan plan;
  int pref_r, pref_c;
  int sel_r, sel_c;
  int cur_r, cur_c;
  int show_cur;
  int hint_r0, hint_c0, hint_r1, hint_c1;
  int hint_on;
  int bad_r0, bad_c0, bad_r1, bad_c1;
  int last_pts;
  int help_page;
  int score_mode;
  int coach;
  int coach_done;
  int pending;
  int pending_mode;
  int has_save;
  Game save_peek;
  char toast[96];
  ifont *font_title;
  ifont *font_hud;
  ifont *font_ui;
  ifont *font_small;
  int tile;
  int board_x, board_y;
  Rect hud, pwr[N_PWR], menu_btn, title_btn[7], clock;
  int touch_on;
  int touch_x0, touch_y0;
  int touch_r, touch_c;
  int swipe_used;
} ui;

static const int GEM_COL[N_COLORS + 1] = {
  0,
  COL_RED, COL_ORANGE, COL_YELLOW, COL_GREEN,
  COL_BLUE, COL_PURPLE, COL_TEAL
};

static void persist_game(void);
static void persist_scores(void);
static void persist_prefs(void);
static void draw_screen(int full);
static void refresh_board(void);
static void beat_timer(void);
static void start_from_plan(void);
static void finish_busy(void);

static int hit_rect(Rect r, int x, int y) {
  return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

static void set_font(ifont *f, int color) {
  if (f) SetFont(f, color);
}

static void text_left(ifont *f, int color, int x, int y, const char *s) {
  set_font(f, color);
  DrawString(x, y, s);
}

static void text_center(ifont *f, int color, int cx, int y, const char *s) {
  int w;
  set_font(f, color);
  w = StringWidth(s);
  DrawString(cx - w / 2, y, s);
}

static void format_num(char *buf, int n, long v) {
  char tmp[32];
  int i, len, out = 0;
  snprintf(tmp, sizeof(tmp), "%ld", v);
  len = (int)strlen(tmp);
  for (i = 0; i < len && out + 1 < n; i++) {
    if (i > 0 && (len - i) % 3 == 0) buf[out++] = ',';
    buf[out++] = tmp[i];
  }
  buf[out] = '\0';
}

static void draw_btn(Rect r, const char *label, int inverted, int enabled) {
  int fg = enabled ? (inverted ? WHITE : BLACK) : DGRAY;
  int bg = !enabled ? LGRAY : (inverted ? BLACK : WHITE);
  FillArea(r.x, r.y, r.w, r.h, bg);
  DrawRect(r.x, r.y, r.w, r.h, enabled ? BLACK : DGRAY);
  if (enabled)
    DrawRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, inverted ? WHITE : BLACK);
  text_center(ui.font_ui, fg, r.x + r.w / 2, r.y + r.h / 2 - 12, label);
}

static int isqrt_i(int n) {
  int x, y;
  if (n <= 0) return 0;
  x = n;
  y = (x + 1) / 2;
  while (y < x) {
    x = y;
    y = (x + n / x) / 2;
  }
  return x;
}

static void hline(int x, int y, int w, int color) {
  if (w > 0) FillArea(x, y, w, 1, color);
}

static void fill_circle(int cx, int cy, int rad, int color) {
  int y, span;
  for (y = -rad; y <= rad; y++) {
    span = isqrt_i(rad * rad - y * y);
    hline(cx - span, cy + y, span * 2 + 1, color);
  }
}

static void fill_diamond(int cx, int cy, int rad, int color) {
  int y, span;
  for (y = -rad; y <= rad; y++) {
    span = rad - (y < 0 ? -y : y);
    hline(cx - span, cy + y, span * 2 + 1, color);
  }
}

static void fill_triangle(int cx, int cy, int rad, int color) {
  int y, span;
  for (y = -rad; y <= rad; y++) {
    span = ((y + rad) * rad) / (2 * rad);
    hline(cx - span, cy + y, span * 2 + 1, color);
  }
}

static void fill_hex(int cx, int cy, int rad, int color) {
  int y, span, half = rad / 2;
  for (y = -rad; y <= rad; y++) {
    int ay = y < 0 ? -y : y;
    if (ay <= half) span = rad;
    else span = rad - ((ay - half) * rad) / (rad - half + 1);
    hline(cx - span, cy + y, span * 2 + 1, color);
  }
}

static void fill_plus(int cx, int cy, int rad, int color) {
  int t = rad / 3;
  if (t < 3) t = 3;
  FillArea(cx - t, cy - rad, t * 2, rad * 2, color);
  FillArea(cx - rad, cy - t, rad * 2, t * 2, color);
}

static void outline_rect(int x, int y, int w, int h, int color, int thick) {
  int i;
  for (i = 0; i < thick; i++)
    DrawRect(x + i, y + i, w - 2 * i, h - 2 * i, color);
}

static void draw_stone_shape(int cx, int cy, int rad, int color_id, int fill) {
  int col = (color_id >= 1 && color_id <= N_COLORS) ? GEM_COL[color_id] : DGRAY;
  switch (color_id) {
    case 1: fill_circle(cx, cy, rad, fill); fill_circle(cx, cy, rad - 4, col); break;
    case 2: fill_diamond(cx, cy, rad, fill); fill_diamond(cx, cy, rad - 4, col); break;
    case 3: fill_triangle(cx, cy, rad, fill); fill_triangle(cx, cy, rad - 5, col); break;
    case 4:
      FillArea(cx - rad, cy - rad, rad * 2, rad * 2, fill);
      FillArea(cx - rad + 4, cy - rad + 4, rad * 2 - 8, rad * 2 - 8, col);
      break;
    case 5: fill_hex(cx, cy, rad, fill); fill_hex(cx, cy, rad - 4, col); break;
    case 6: fill_plus(cx, cy, rad, fill); fill_plus(cx, cy, rad - 4, col); break;
    default:
      fill_circle(cx, cy, rad, fill);
      fill_circle(cx, cy, rad - 3, col);
      fill_circle(cx, cy, rad / 2, WHITE);
      fill_circle(cx, cy, rad / 2 - 3, col);
      break;
  }
}

static void draw_special_badge(int x, int y, int kind) {
  int s = 18;
  if (kind == KIND_BURST) {
    FillArea(x, y, s, s, WHITE);
    DrawRect(x, y, s, s, BLACK);
    fill_triangle(x + s / 2, y + s / 2 + 1, s / 2 - 2, COL_ORANGE);
  } else if (kind == KIND_STAR) {
    FillArea(x, y, s, s, WHITE);
    DrawRect(x, y, s, s, BLACK);
    fill_diamond(x + s / 2, y + s / 2, s / 2 - 2, BLACK);
  } else if (kind == KIND_PRISM) {
    FillArea(x, y, s, s, BLACK);
    FillArea(x + 2, y + 2, s - 4, 4, COL_RED);
    FillArea(x + 2, y + 7, s - 4, 4, COL_GREEN);
    FillArea(x + 2, y + 12, s - 4, 4, COL_BLUE);
  }
}

static void layout(void) {
  int hud_h, pwr_h, foot_h, avail, i, gap, bw;
  ui.sw = ScreenWidth();
  ui.sh = ScreenHeight();
  hud_h = 168;
  pwr_h = 118;
  foot_h = 64;
  avail = ui.sh - hud_h - pwr_h - foot_h;
  ui.tile = (ui.sw - 24) / BOARD_N;
  if (ui.tile * BOARD_N > avail - 8)
    ui.tile = (avail - 8) / BOARD_N;
  if (ui.tile < 48) ui.tile = 48;
  ui.board_x = (ui.sw - ui.tile * BOARD_N) / 2;
  ui.board_y = hud_h + (avail - ui.tile * BOARD_N) / 2;
  ui.hud.x = 0;
  ui.hud.y = 0;
  ui.hud.w = ui.sw;
  ui.hud.h = hud_h;
  ui.clock.x = ui.sw - 168;
  ui.clock.y = 10;
  ui.clock.w = 156;
  ui.clock.h = 44;
  gap = 10;
  bw = (ui.sw - 20 - 3 * gap) / 4;
  for (i = 0; i < N_PWR; i++) {
    ui.pwr[i].x = 10 + i * (bw + gap);
    ui.pwr[i].y = ui.sh - foot_h - pwr_h + 12;
    ui.pwr[i].w = bw;
    ui.pwr[i].h = 88;
  }
  ui.menu_btn.x = 12;
  ui.menu_btn.y = ui.sh - foot_h + 8;
  ui.menu_btn.w = 160;
  ui.menu_btn.h = 48;
  bw = ui.sw - 80;
  for (i = 0; i < 7; i++) {
    ui.title_btn[i].x = 40;
    ui.title_btn[i].y = 320 + i * 84;
    ui.title_btn[i].w = bw;
    ui.title_btn[i].h = 70;
  }
}

static int cell_at(int x, int y, int *r, int *c) {
  int rr, cc;
  if (x < ui.board_x || y < ui.board_y) return 0;
  cc = (x - ui.board_x) / ui.tile;
  rr = (y - ui.board_y) / ui.tile;
  if (!board_in_bounds(rr, cc)) return 0;
  *r = rr;
  *c = cc;
  return 1;
}

static void persist_game(void) {
  if (ui.game.status == ST_PLAYING)
    game_save(&ui.game, SAVE_PATH);
}

static void persist_scores(void) {
  game_save_scores(&ui.game, SCORE_PATH);
}

static void persist_prefs(void) {
  game_save_prefs(ui.coach_done, PREF_PATH);
}

static void refresh_save_peek(void) {
  Game tmp;
  memcpy(&tmp, &ui.game, sizeof(tmp));
  ui.has_save = game_load(&tmp, SAVE_PATH) && tmp.status == ST_PLAYING;
  if (ui.has_save) ui.save_peek = tmp;
}

static void set_toast(const char *s) {
  snprintf(ui.toast, sizeof(ui.toast), "%s", s);
}

static void toast_wave(const WaveResult *w, long pts) {
  char num[32];
  format_num(num, sizeof(num), pts);
  ui.last_pts = (int)pts;
  if (w && w->created == KIND_PRISM)
    snprintf(ui.toast, sizeof(ui.toast), "Prism! +%s", num);
  else if (w && w->created == KIND_STAR)
    snprintf(ui.toast, sizeof(ui.toast), "Star! +%s", num);
  else if (w && w->created == KIND_BURST)
    snprintf(ui.toast, sizeof(ui.toast), "Burst! +%s", num);
  else if (ui.game.combo >= 2)
    snprintf(ui.toast, sizeof(ui.toast), "Cascade x%d  +%s", ui.game.combo, num);
  else
    snprintf(ui.toast, sizeof(ui.toast), "+%s", num);
}

static void finish_busy(void) {
  ui.busy = 0;
  ui.phase = R_IDLE;
  memset(&ui.plan, 0, sizeof(ui.plan));
  game_finish_cascade(&ui.game);
  if (ui.game.note == NOTE_SHUFFLED)
    set_toast("No moves — board shuffled");
  persist_game();
  if (ui.game.status == ST_OVER) {
    game_add_score(&ui.game, (long)time(0));
    persist_scores();
    remove(SAVE_PATH);
    ui.has_save = 0;
    ui.screen = SCR_OVER;
    draw_screen(1);
    return;
  }
  draw_screen(1);
}

static void start_from_plan(void) {
  ui.busy = 1;
  ui.sel_r = ui.sel_c = -1;
  ui.hint_on = 0;
  ui.phase = R_FLASH;
  refresh_board();
  SetHardTimer(BEAT_TIMER, beat_timer, BEAT_MS);
}

static void beat_timer(void) {
  if (ui.phase == R_BAD) {
    board_swap(&ui.game.board, ui.bad_r0, ui.bad_c0, ui.bad_r1, ui.bad_c1);
    ui.phase = R_IDLE;
    ui.busy = 0;
    ui.sel_r = ui.sel_c = -1;
    set_toast("That swap does not match");
    refresh_board();
    return;
  }
  if (!ui.busy) return;
  if (ui.phase == R_FLASH) {
    long pts;
    board_apply_plan(&ui.game.board, &ui.plan);
    pts = game_apply_wave(&ui.game, &ui.plan.result);
    toast_wave(&ui.plan.result, pts);
    ui.phase = R_HOLES;
    refresh_board();
    SetHardTimer(BEAT_TIMER, beat_timer, BEAT_MS);
    return;
  }
  if (ui.phase == R_HOLES) {
    board_settle(&ui.game.board);
    ui.phase = R_SETTLE;
    memset(&ui.plan, 0, sizeof(ui.plan));
    refresh_board();
    SetHardTimer(BEAT_TIMER, beat_timer, BEAT_MS);
    return;
  }
  if (ui.phase == R_SETTLE) {
    ui.pref_r = -1;
    ui.pref_c = -1;
    if (board_plan_color(&ui.game.board, ui.pref_r, ui.pref_c, &ui.plan)) {
      ui.phase = R_FLASH;
      refresh_board();
      SetHardTimer(BEAT_TIMER, beat_timer, BEAT_MS);
    } else {
      finish_busy();
    }
  }
}

static void start_bad_swap(int r0, int c0, int r1, int c1) {
  board_swap(&ui.game.board, r0, c0, r1, c1);
  ui.bad_r0 = r0;
  ui.bad_c0 = c0;
  ui.bad_r1 = r1;
  ui.bad_c1 = c1;
  ui.busy = 1;
  ui.phase = R_BAD;
  ui.sel_r = ui.sel_c = -1;
  refresh_board();
  SetHardTimer(BEAT_TIMER, beat_timer, BAD_MS);
}

static void tick_timer(void) {
  if (ui.screen != SCR_PLAY || ui.game.mode != MODE_TIMED) return;
  if (ui.game.status != ST_PLAYING) return;
  ui.game.time_left--;
  if (ui.game.time_left <= 0) {
    ui.game.time_left = 0;
    game_end(&ui.game, OVER_TIME);
    game_add_score(&ui.game, (long)time(0));
    persist_scores();
    remove(SAVE_PATH);
    ui.has_save = 0;
    ui.screen = SCR_OVER;
    draw_screen(1);
    return;
  }
  if (ui.screen == SCR_PLAY) {
    char line[32];
    FillArea(ui.clock.x, ui.clock.y, ui.clock.w, ui.clock.h, COL_HUD);
    snprintf(line, sizeof(line), "%d:%02d", ui.game.time_left / 60, ui.game.time_left % 60);
    text_left(ui.font_hud, ui.game.time_left <= 10 ? COL_RED : BLACK,
              ui.clock.x + 8, ui.clock.y + 6, line);
    PartialUpdate(ui.clock.x, ui.clock.y, ui.clock.w, ui.clock.h);
  }
  SetHardTimer(TICK_TIMER, tick_timer, 1000);
}

static void start_mode(int mode) {
  game_init(&ui.game, mode, (unsigned)time(0));
  ui.sel_r = ui.sel_c = -1;
  ui.cur_r = ui.cur_c = 0;
  ui.show_cur = 0;
  ui.hint_on = 0;
  ui.busy = 0;
  ui.phase = R_IDLE;
  ui.toast[0] = '\0';
  ui.coach = !ui.coach_done;
  ui.screen = SCR_PLAY;
  persist_game();
  ui.has_save = 1;
  ClearTimer(tick_timer);
  if (mode == MODE_TIMED)
    SetHardTimer(TICK_TIMER, tick_timer, 1000);
  draw_screen(1);
}

static void try_continue(void) {
  Game tmp;
  memcpy(&tmp, &ui.game, sizeof(tmp));
  if (!game_load(&tmp, SAVE_PATH) || tmp.status != ST_PLAYING) {
    set_toast("No saved game");
    draw_screen(1);
    return;
  }
  memcpy(tmp.scores, ui.game.scores, sizeof(tmp.scores));
  memcpy(tmp.score_n, ui.game.score_n, sizeof(tmp.score_n));
  ui.game = tmp;
  ui.sel_r = ui.sel_c = -1;
  ui.cur_r = ui.cur_c = 0;
  ui.show_cur = 0;
  ui.hint_on = 0;
  ui.busy = 0;
  ui.phase = R_IDLE;
  ui.toast[0] = '\0';
  ui.screen = SCR_PLAY;
  ClearTimer(tick_timer);
  if (ui.game.mode == MODE_TIMED)
    SetHardTimer(TICK_TIMER, tick_timer, 1000);
  draw_screen(1);
}

static void draw_hint_arrow(void) {
  int x0, y0, x1, y1, mx, my;
  if (!ui.hint_on) return;
  x0 = ui.board_x + ui.hint_c0 * ui.tile + ui.tile / 2;
  y0 = ui.board_y + ui.hint_r0 * ui.tile + ui.tile / 2;
  x1 = ui.board_x + ui.hint_c1 * ui.tile + ui.tile / 2;
  y1 = ui.board_y + ui.hint_r1 * ui.tile + ui.tile / 2;
  DrawLine(x0, y0, x1, y1, BLACK);
  mx = (x0 + x1) / 2;
  my = (y0 + y1) / 2;
  fill_diamond(mx, my, 8, BLACK);
}

static void draw_cell(int r, int c) {
  int x = ui.board_x + c * ui.tile;
  int y = ui.board_y + r * ui.tile;
  int pad = ui.tile / 10;
  int cx = x + ui.tile / 2;
  int cy = y + ui.tile / 2;
  int rad = ui.tile / 2 - pad;
  Cell cell = ui.game.board.cells[r][c];
  int sel = (ui.sel_r == r && ui.sel_c == c);
  int cur = ui.show_cur && (ui.cur_r == r && ui.cur_c == c);
  int hint = ui.hint_on &&
             ((r == ui.hint_r0 && c == ui.hint_c0) ||
              (r == ui.hint_r1 && c == ui.hint_c1));
  int marked = (ui.phase == R_FLASH && ui.plan.marked[r][c]);
  int hole = (ui.phase == R_HOLES && !cell_alive(cell));
  int bad = ui.phase == R_BAD &&
            ((r == ui.bad_r0 && c == ui.bad_c0) ||
             (r == ui.bad_r1 && c == ui.bad_c1));

  FillArea(x + 1, y + 1, ui.tile - 2, ui.tile - 2,
           marked || bad ? LGRAY : WHITE);
  if (hole) {
    DrawRect(x + 8, y + 8, ui.tile - 16, ui.tile - 16, LGRAY);
  } else if (cell.kind == KIND_PRISM) {
    draw_special_badge(x + 6, y + 6, KIND_PRISM);
    fill_circle(cx, cy, rad / 3, WHITE);
  } else if (cell_alive(cell)) {
    draw_stone_shape(cx, cy, rad, cell.color, BLACK);
    if (cell.kind != KIND_NORMAL)
      draw_special_badge(x + ui.tile - 22, y + 4, cell.kind);
  }
  DrawRect(x, y, ui.tile, ui.tile, LGRAY);
  if (sel) outline_rect(x + 2, y + 2, ui.tile - 4, ui.tile - 4, BLACK, 3);
  else if (hint) outline_rect(x + 3, y + 3, ui.tile - 6, ui.tile - 6, BLACK, 2);
  else if (cur) DrawRect(x + 2, y + 2, ui.tile - 4, ui.tile - 4, DGRAY);
  if (bad) InvertArea(x + 4, y + 4, ui.tile - 8, ui.tile - 8);
}

static void draw_board(void) {
  int r, c;
  FillArea(ui.board_x - 4, ui.board_y - 4,
           ui.tile * BOARD_N + 8, ui.tile * BOARD_N + 8, BLACK);
  for (r = 0; r < BOARD_N; r++)
    for (c = 0; c < BOARD_N; c++)
      draw_cell(r, c);
  draw_hint_arrow();
}

static void pwr_label(int i, char *buf, int n) {
  int qty = 0;
  const char *name = "";
  if (i == 0) { name = "Hint"; qty = ui.game.hints; }
  else if (i == 1) { name = "Smash"; qty = ui.game.smashes; }
  else if (i == 2) { name = "Bolt"; qty = ui.game.bolts; }
  else { name = "Mix"; qty = ui.game.shuffles; }
  snprintf(buf, (size_t)n, "%s  %d", name, qty);
}

static void draw_coach(void) {
  int w = ui.sw - 80;
  int h = 220;
  int x = 40;
  int y = ui.board_y + 40;
  FillArea(x, y, w, h, WHITE);
  DrawRect(x, y, w, h, BLACK);
  DrawRect(x + 2, y + 2, w - 4, h - 4, BLACK);
  text_center(ui.font_hud, BLACK, ui.sw / 2, y + 28, "Swipe to play");
  set_font(ui.font_ui, BLACK);
  DrawTextRect(x + 24, y + 80, w - 48, 90,
               "Swipe a stone toward a neighbor to make a line of 3. "
               "Tap this card to start.",
               ALIGN_LEFT);
}

static void draw_play(int full) {
  char line[96], num[32], num2[32];
  int i, pct, mx, my, mw, mh, fill;
  int pwr_kind[N_PWR] = {PWR_HINT, PWR_SMASH, PWR_BOLT, PWR_SHUFFLE};

  if (full) {
    ClearScreen();
    FillArea(0, 0, ui.sw, ui.sh, COL_BG);
  }

  FillArea(0, 0, ui.sw, ui.hud.h, COL_HUD);
  DrawLine(0, ui.hud.h - 1, ui.sw, ui.hud.h - 1, BLACK);
  format_num(num, sizeof(num), ui.game.score);
  snprintf(line, sizeof(line), "Score  %s", num);
  text_left(ui.font_hud, BLACK, 16, 14, line);
  snprintf(line, sizeof(line), "Lv %d", ui.game.level);
  text_left(ui.font_hud, BLACK, ui.sw / 2 - 30, 14, line);
  if (ui.game.mode == MODE_TIMED)
    snprintf(line, sizeof(line), "%d:%02d", ui.game.time_left / 60, ui.game.time_left % 60);
  else
    snprintf(line, sizeof(line), "%s", game_mode_name(ui.game.mode));
  text_left(ui.font_hud, ui.game.mode == MODE_TIMED && ui.game.time_left <= 10 ? COL_RED : BLACK,
            ui.clock.x + 8, 14, line);

  format_num(num2, sizeof(num2), ui.game.stones_cleared);
  snprintf(line, sizeof(line), "Cleared %s   Best x%d", num2, ui.game.best_combo);
  text_left(ui.font_small, DGRAY, 16, 58, line);

  mx = 16;
  my = 96;
  mw = ui.sw - 32;
  mh = 36;
  FillArea(mx, my, mw, mh, WHITE);
  DrawRect(mx, my, mw, mh, BLACK);
  pct = game_meter_pct(&ui.game);
  fill = (mw - 4) * pct / 100;
  if (fill > 0) FillArea(mx + 2, my + 2, fill, mh - 4, BLACK);
  snprintf(line, sizeof(line), "Level meter  %d%%", pct);
  text_center(ui.font_small, fill > mw / 2 ? WHITE : BLACK, ui.sw / 2, my + 6, line);

  if (ui.toast[0])
    text_center(ui.font_ui, BLACK, ui.sw / 2, ui.hud.h - 36, ui.toast);

  draw_board();
  if (ui.coach) draw_coach();

  FillArea(0, ui.pwr[0].y - 12, ui.sw, ui.sh - (ui.pwr[0].y - 12), COL_HUD);
  DrawLine(0, ui.pwr[0].y - 12, ui.sw, ui.pwr[0].y - 12, BLACK);
  for (i = 0; i < N_PWR; i++) {
    pwr_label(i, line, sizeof(line));
    draw_btn(ui.pwr[i], line, ui.game.armed == pwr_kind[i],
             game_arm_ok(&ui.game, pwr_kind[i]));
  }
  draw_btn(ui.menu_btn, "Menu", 0, 1);
  text_left(ui.font_small, DGRAY, ui.menu_btn.x + ui.menu_btn.w + 16,
            ui.menu_btn.y + 12,
            ui.coach ? "Tap the card to dismiss" :
            ui.game.armed == PWR_SMASH ? "Tap a stone — or Smash again to cancel" :
            ui.game.armed == PWR_BOLT ? "Tap a row — or Bolt again to cancel" :
            ui.busy ? "Clinking..." : "Swipe a stone, or tap two");

  if (full) FullUpdate();
  else SoftUpdate();
}

static void refresh_board(void) {
  if (ui.screen != SCR_PLAY) {
    draw_screen(1);
    return;
  }
  draw_play(0);
  PartialUpdate(ui.board_x - 4, ui.board_y - 4,
                ui.tile * BOARD_N + 8, ui.tile * BOARD_N + 8);
  PartialUpdate(0, 0, ui.sw, ui.hud.h);
}

static void draw_title(void) {
  char cont[64], num[32];
  static const char *labels[7] = {
    "Classic", "Endless", "Timed", "Continue", "High Scores", "How to Play", "Quit"
  };
  int i;
  ClearScreen();
  FillArea(0, 0, ui.sw, ui.sh, COL_BG);
  text_center(ui.font_title, BLACK, ui.sw / 2, 70, APP_NAME);
  text_center(ui.font_ui, DGRAY, ui.sw / 2, 150, "PocketBook Verse Pro Color");
  text_center(ui.font_small, DGRAY, ui.sw / 2, 200, "Match 3  ·  Build bursts  ·  Chain cascades");
  {
    int gx = (ui.sw - 7 * 56) / 2;
    for (i = 1; i <= N_COLORS; i++) {
      int cx = gx + (i - 1) * 56 + 28;
      draw_stone_shape(cx, 270, 22, i, BLACK);
    }
  }
  refresh_save_peek();
  for (i = 0; i < 7; i++) {
    const char *lab = labels[i];
    if (i == 3 && ui.has_save) {
      format_num(num, sizeof(num), ui.save_peek.score);
      snprintf(cont, sizeof(cont), "Continue · %s %s",
               game_mode_name(ui.save_peek.mode), num);
      lab = cont;
    }
    draw_btn(ui.title_btn[i], lab, 0, i != 3 || ui.has_save);
  }
  if (ui.toast[0])
    text_center(ui.font_small, BLACK, ui.sw / 2, ui.sh - 40, ui.toast);
  FullUpdate();
}

static void draw_pause(void) {
  int i;
  const char *labs[4] = {"Resume", "New game", "How to Play", "Quit to title"};
  ClearScreen();
  FillArea(0, 0, ui.sw, ui.sh, COL_BG);
  text_center(ui.font_title, BLACK, ui.sw / 2, 80, "Paused");
  for (i = 0; i < 4; i++) {
    Rect r = ui.title_btn[i];
    r.y = 320 + i * 100;
    draw_btn(r, labs[i], 0, 1);
  }
  FullUpdate();
}

static void draw_over(void) {
  char line[96], num[32];
  Rect again, menu;
  ClearScreen();
  FillArea(0, 0, ui.sw, ui.sh, COL_BG);
  text_center(ui.font_title, BLACK, ui.sw / 2, 80, "Game Over");
  text_center(ui.font_hud, BLACK, ui.sw / 2, 170, game_over_text(&ui.game));
  format_num(num, sizeof(num), ui.game.score);
  snprintf(line, sizeof(line), "Score  %s", num);
  text_center(ui.font_hud, BLACK, ui.sw / 2, 250, line);
  snprintf(line, sizeof(line), "Level %d   ·   %s   ·   Best cascade x%d",
           ui.game.level, game_mode_name(ui.game.mode), ui.game.best_combo);
  text_center(ui.font_ui, DGRAY, ui.sw / 2, 320, line);
  snprintf(line, sizeof(line), "Moves %d   ·   Cleared %d", ui.game.moves, ui.game.stones_cleared);
  text_center(ui.font_small, DGRAY, ui.sw / 2, 380, line);
  again = ui.title_btn[0];
  again.y = 520;
  menu = ui.title_btn[1];
  menu.y = 620;
  draw_btn(again, "Play again", 0, 1);
  draw_btn(menu, "Title", 0, 1);
  FullUpdate();
}

static void draw_scores(void) {
  char line[96], num[32];
  int i, y;
  Rect back, tab;
  ClearScreen();
  FillArea(0, 0, ui.sw, ui.sh, COL_BG);
  text_center(ui.font_title, BLACK, ui.sw / 2, 40, "High Scores");
  tab.x = 40;
  tab.y = 160;
  tab.w = ui.sw - 80;
  tab.h = 64;
  draw_btn(tab, game_mode_name(ui.score_mode), 0, 1);
  text_center(ui.font_small, DGRAY, ui.sw / 2, 236, "Swipe or tap to switch mode");
  y = 290;
  if (ui.game.score_n[ui.score_mode] == 0)
    text_center(ui.font_ui, DGRAY, ui.sw / 2, 400, "No scores yet");
  for (i = 0; i < ui.game.score_n[ui.score_mode]; i++) {
    format_num(num, sizeof(num), ui.game.scores[ui.score_mode][i].score);
    snprintf(line, sizeof(line), "%2d.  %s    Lv %d", i + 1, num,
             ui.game.scores[ui.score_mode][i].level);
    text_left(ui.font_hud, BLACK, 60, y, line);
    y += 72;
  }
  back = ui.title_btn[5];
  back.y = ui.sh - 120;
  draw_btn(back, "Back", 0, 1);
  FullUpdate();
}

static void draw_help(void) {
  Rect back, next;
  const char *p0 =
    "Swipe a stone toward a neighbor, or tap two adjacent stones. "
    "Everything is on-screen — no keys required.\n\n"
    "Stones use color and shape so they stay readable on Kaleido 3:\n"
    "circle, diamond, triangle, square, hex, plus, ring.\n\n"
    "Cascades score more. The level meter fills as you clear stones. "
    "Each new level grants extra tools.";
  const char *p1 =
    "Specials\n"
    "4 in a row — Burst: explodes a 3x3.\n"
    "L or T — Star: clears its row and column.\n"
    "5 in a row — Prism: swap with any stone to wipe that color. "
    "Two prisms wipe the board.\n\n"
    "Tools\n"
    "Hint — show a legal swap (arrow).\n"
    "Smash — destroy one stone (specials detonate).\n"
    "Bolt — destroy a whole row.\n"
    "Mix — shuffle the board.\n\n"
    "Classic ends when no moves remain. Endless never ends. "
    "Timed: 75 seconds; a cascade of 2+ adds 2 seconds.\n"
    "Swipe How to Play / High Scores pages to change them.";
  ClearScreen();
  FillArea(0, 0, ui.sw, ui.sh, COL_BG);
  text_center(ui.font_title, BLACK, ui.sw / 2, 36, "How to Play");
  set_font(ui.font_ui, BLACK);
  DrawTextRect(40, 140, ui.sw - 80, ui.sh - 320,
               ui.help_page ? p1 : p0, ALIGN_LEFT);
  next.x = 40;
  next.y = ui.sh - 200;
  next.w = ui.sw - 80;
  next.h = 72;
  back = next;
  back.y = ui.sh - 110;
  draw_btn(next, ui.help_page ? "Previous page" : "Next page", 0, 1);
  draw_btn(back, "Back", 0, 1);
  FullUpdate();
}

static void draw_screen(int full) {
  layout();
  if (ui.screen == SCR_TITLE) draw_title();
  else if (ui.screen == SCR_PLAY) draw_play(full);
  else if (ui.screen == SCR_PAUSE) draw_pause();
  else if (ui.screen == SCR_OVER) draw_over();
  else if (ui.screen == SCR_SCORES) draw_scores();
  else draw_help();
}

static void dismiss_coach(void) {
  ui.coach = 0;
  ui.coach_done = 1;
  persist_prefs();
  draw_screen(1);
}

static void use_hint(void) {
  if (ui.busy) return;
  if (game_use_hint(&ui.game, &ui.hint_r0, &ui.hint_c0, &ui.hint_r1, &ui.hint_c1)) {
    ui.hint_on = 1;
    set_toast("Hint: swap along the arrow");
    persist_game();
  } else {
    set_toast(ui.game.hints <= 0 ? "No hints left" : "No move found");
  }
  refresh_board();
}

static void use_mix(void) {
  if (ui.busy) return;
  if (game_use_shuffle(&ui.game)) {
    set_toast("Board shuffled");
    persist_game();
  } else {
    set_toast("No shuffles left");
  }
  draw_screen(0);
}

static int try_play_swap(int r0, int c0, int r1, int c1) {
  int kind;
  if (!board_adjacent(r0, c0, r1, c1)) return 0;
  kind = game_try_swap(&ui.game, r0, c0, r1, c1);
  if (kind == SWAP_NONE) {
    start_bad_swap(r0, c0, r1, c1);
    return 0;
  }
  ui.pref_r = r1;
  ui.pref_c = c1;
  if (kind == SWAP_PRISM) {
    if (!board_plan_prism(&ui.game.board, r0, c0, r1, c1, &ui.plan))
      return 0;
  } else if (!board_plan_color(&ui.game.board, r1, c1, &ui.plan)) {
    return 0;
  }
  start_from_plan();
  return 1;
}

static void play_cell(int r, int c) {
  WavePlan p;
  if (ui.busy || ui.coach) return;
  if (ui.game.armed == PWR_SMASH) {
    if (!board_plan_smash(&ui.game.board, r, c, &p) || ui.game.smashes <= 0) {
      set_toast("Cannot smash that");
      refresh_board();
      return;
    }
    ui.game.smashes--;
    ui.game.moves++;
    ui.game.armed = PWR_NONE;
    ui.plan = p;
    start_from_plan();
    return;
  }
  if (ui.game.armed == PWR_BOLT) {
    if (!board_plan_bolt(&ui.game.board, r, c, &p) || ui.game.bolts <= 0) {
      set_toast("Cannot strike that");
      refresh_board();
      return;
    }
    ui.game.bolts--;
    ui.game.moves++;
    ui.game.armed = PWR_NONE;
    ui.plan = p;
    start_from_plan();
    return;
  }
  if (ui.sel_r < 0) {
    ui.sel_r = r;
    ui.sel_c = c;
    ui.cur_r = r;
    ui.cur_c = c;
    refresh_board();
    return;
  }
  if (ui.sel_r == r && ui.sel_c == c) {
    ui.sel_r = ui.sel_c = -1;
    refresh_board();
    return;
  }
  if (board_adjacent(ui.sel_r, ui.sel_c, r, c)) {
    int sr = ui.sel_r, sc = ui.sel_c;
    try_play_swap(sr, sc, r, c);
    return;
  }
  ui.sel_r = r;
  ui.sel_c = c;
  ui.cur_r = r;
  ui.cur_c = c;
  refresh_board();
}

static int swipe_axis(int x0, int y0, int x1, int y1, int *dr, int *dc) {
  int dx = x1 - x0, dy = y1 - y0;
  int adx = dx < 0 ? -dx : dx;
  int ady = dy < 0 ? -dy : dy;
  int need = ui.tile / 3;
  if (need < 36) need = 36;
  if (adx < need && ady < need) return 0;
  if (adx >= ady) {
    *dr = 0;
    *dc = dx > 0 ? 1 : -1;
  } else {
    *dr = dy > 0 ? 1 : -1;
    *dc = 0;
  }
  return 1;
}

static int try_board_swipe(int x, int y) {
  int r1, c1, dr, dc;
  if (ui.busy || ui.coach || ui.game.armed != PWR_NONE) return 0;
  if (ui.touch_r < 0 || ui.touch_c < 0) return 0;
  if (cell_at(x, y, &r1, &c1) &&
      board_adjacent(ui.touch_r, ui.touch_c, r1, c1)) {
    return try_play_swap(ui.touch_r, ui.touch_c, r1, c1);
  }
  if (!swipe_axis(ui.touch_x0, ui.touch_y0, x, y, &dr, &dc)) return 0;
  r1 = ui.touch_r + dr;
  c1 = ui.touch_c + dc;
  if (!board_in_bounds(r1, c1)) return 0;
  return try_play_swap(ui.touch_r, ui.touch_c, r1, c1);
}

static void play_tap(int x, int y) {
  int r, c, i;
  int kinds[N_PWR] = {PWR_HINT, PWR_SMASH, PWR_BOLT, PWR_SHUFFLE};
  if (ui.coach) {
    dismiss_coach();
    return;
  }
  if (hit_rect(ui.menu_btn, x, y)) {
    ui.screen = SCR_PAUSE;
    persist_game();
    draw_screen(1);
    return;
  }
  for (i = 0; i < N_PWR; i++) {
    if (!hit_rect(ui.pwr[i], x, y)) continue;
    if (kinds[i] == PWR_HINT) use_hint();
    else if (kinds[i] == PWR_SHUFFLE) use_mix();
    else {
      if (!game_arm_ok(&ui.game, kinds[i])) {
        set_toast("None left");
        refresh_board();
        return;
      }
      ui.game.armed = (ui.game.armed == kinds[i]) ? PWR_NONE : kinds[i];
      set_toast(ui.game.armed == PWR_NONE ? "Cancelled" :
                ui.game.armed == PWR_SMASH ? "Smash armed — tap again to cancel" :
                "Bolt armed — tap again to cancel");
      refresh_board();
    }
    return;
  }
  if (cell_at(x, y, &r, &c)) play_cell(r, c);
}

static void confirm_handler(int button) {
  if (button != 1) {
    ui.pending = ACT_NONE;
    return;
  }
  if (ui.pending == ACT_NEW) {
    start_mode(ui.pending_mode);
  } else if (ui.pending == ACT_QUIT) {
    persist_game();
    persist_scores();
    CloseApp();
  }
  ui.pending = ACT_NONE;
}

static void ask_new_game(int mode) {
  ui.pending = ACT_NEW;
  ui.pending_mode = mode;
  Dialog(ICON_QUESTION, APP_NAME, "Start a new game? The current run will be lost.",
         "Yes", "No", confirm_handler);
}

static void ask_quit(void) {
  ui.pending = ACT_QUIT;
  Dialog(ICON_QUESTION, APP_NAME, "Quit Clink?", "Yes", "No", confirm_handler);
}

static void title_tap(int x, int y) {
  int i;
  ui.toast[0] = '\0';
  for (i = 0; i < 7; i++) {
    if (!hit_rect(ui.title_btn[i], x, y)) continue;
    if (i == 0) start_mode(MODE_CLASSIC);
    else if (i == 1) start_mode(MODE_ENDLESS);
    else if (i == 2) start_mode(MODE_TIMED);
    else if (i == 3) try_continue();
    else if (i == 4) {
      ui.score_mode = MODE_CLASSIC;
      ui.screen = SCR_SCORES;
      draw_screen(1);
    } else if (i == 5) {
      ui.help_page = 0;
      ui.screen = SCR_HELP;
      draw_screen(1);
    } else {
      ask_quit();
    }
    return;
  }
}

static void pause_tap(int x, int y) {
  int i;
  for (i = 0; i < 4; i++) {
    Rect r = ui.title_btn[i];
    r.y = 320 + i * 100;
    if (!hit_rect(r, x, y)) continue;
    if (i == 0) {
      ui.screen = SCR_PLAY;
      draw_screen(1);
    } else if (i == 1) {
      ask_new_game(ui.game.mode);
    } else if (i == 2) {
      ui.help_page = 0;
      ui.screen = SCR_HELP;
      draw_screen(1);
    } else {
      persist_game();
      ui.screen = SCR_TITLE;
      ClearTimer(tick_timer);
      draw_screen(1);
    }
    return;
  }
}

static void over_tap(int x, int y) {
  Rect again = ui.title_btn[0], menu = ui.title_btn[1];
  again.y = 520;
  menu.y = 620;
  if (hit_rect(again, x, y)) start_mode(ui.game.mode);
  else if (hit_rect(menu, x, y)) {
    ui.screen = SCR_TITLE;
    draw_screen(1);
  }
}

static void scores_tap(int x, int y) {
  Rect tab, back;
  int dr, dc;
  tab.x = 40;
  tab.y = 160;
  tab.w = ui.sw - 80;
  tab.h = 64;
  back = ui.title_btn[5];
  back.y = ui.sh - 120;
  if (ui.touch_on && swipe_axis(ui.touch_x0, ui.touch_y0, x, y, &dr, &dc) && dc != 0) {
    ui.score_mode = (ui.score_mode + dc + N_MODES) % N_MODES;
    draw_screen(1);
    return;
  }
  if (hit_rect(tab, x, y)) {
    ui.score_mode = (ui.score_mode + 1) % N_MODES;
    draw_screen(1);
  } else if (hit_rect(back, x, y)) {
    ui.screen = SCR_TITLE;
    draw_screen(1);
  }
}

static void help_tap(int x, int y) {
  Rect next, back;
  int dr, dc;
  next.x = 40;
  next.y = ui.sh - 200;
  next.w = ui.sw - 80;
  next.h = 72;
  back = next;
  back.y = ui.sh - 110;
  if (ui.touch_on && swipe_axis(ui.touch_x0, ui.touch_y0, x, y, &dr, &dc) && dc != 0) {
    ui.help_page = dc > 0 ? 1 : 0;
    draw_screen(1);
    return;
  }
  if (hit_rect(next, x, y)) {
    ui.help_page = !ui.help_page;
    draw_screen(1);
  } else if (hit_rect(back, x, y)) {
    ui.screen = (ui.game.status == ST_PLAYING && ui.game.moves > 0) ? SCR_PAUSE : SCR_TITLE;
    draw_screen(1);
  }
}

static void move_cursor(int dir) {
  int n = ui.cur_r * BOARD_N + ui.cur_c + dir;
  if (n < 0) n = BOARD_N * BOARD_N - 1;
  if (n >= BOARD_N * BOARD_N) n = 0;
  ui.cur_r = n / BOARD_N;
  ui.cur_c = n % BOARD_N;
  ui.show_cur = 1;
  if (ui.screen == SCR_PLAY) refresh_board();
}

static int main_handler(int type, int par1, int par2) {
  if (type == EVT_INIT) {
    memset(&ui, 0, sizeof(ui));
    ui.sel_r = ui.sel_c = -1;
    ui.sw = ScreenWidth();
    ui.sh = ScreenHeight();
    ui.font_title = OpenFont(DEFAULTFONTB, 52, 1);
    if (!ui.font_title) ui.font_title = OpenFont(DEFAULTFONT, 52, 1);
    ui.font_hud = OpenFont(DEFAULTFONTB, 26, 1);
    if (!ui.font_hud) ui.font_hud = OpenFont(DEFAULTFONT, 26, 1);
    ui.font_ui = OpenFont(DEFAULTFONT, 22, 1);
    ui.font_small = OpenFont(DEFAULTFONT, 18, 1);
    SetPanelType(0);
    game_load_scores(&ui.game, SCORE_PATH);
    game_load_prefs(&ui.coach_done, PREF_PATH);
    ui.screen = SCR_TITLE;
    layout();
    return 0;
  }

  if (type == EVT_SHOW || type == EVT_REPAINT) {
    draw_screen(1);
    return 0;
  }

  if (type == EVT_EXIT) {
    persist_game();
    persist_scores();
    persist_prefs();
    ClearTimer(beat_timer);
    ClearTimer(tick_timer);
    if (ui.font_title) CloseFont(ui.font_title);
    if (ui.font_hud) CloseFont(ui.font_hud);
    if (ui.font_ui) CloseFont(ui.font_ui);
    if (ui.font_small) CloseFont(ui.font_small);
    return 0;
  }

  if (type == EVT_KEYPRESS) {
    if (par1 == KEY_HOME) {
      persist_game();
      CloseApp();
      return 0;
    }
    if (par1 == KEY_BACK || par1 == KEY_MENU) {
      if (ui.screen == SCR_PLAY) {
        ui.screen = SCR_PAUSE;
        persist_game();
        draw_screen(1);
      } else if (ui.screen != SCR_TITLE) {
        ui.screen = SCR_TITLE;
        draw_screen(1);
      } else {
        ask_quit();
      }
      return 0;
    }
    if (ui.screen == SCR_PLAY && !ui.busy && !ui.coach) {
      if (par1 == KEY_PREV || par1 == KEY_LEFT || par1 == KEY_UP)
        move_cursor(-1);
      else if (par1 == KEY_NEXT || par1 == KEY_RIGHT || par1 == KEY_DOWN)
        move_cursor(1);
      else if (par1 == KEY_OK)
        play_cell(ui.cur_r, ui.cur_c);
    }
    return 0;
  }

  if (type == EVT_POINTERDOWN || type == EVT_TOUCHDOWN) {
    ui.show_cur = 0;
    ui.touch_on = 1;
    ui.touch_x0 = par1;
    ui.touch_y0 = par2;
    ui.swipe_used = 0;
    ui.touch_r = ui.touch_c = -1;
    if (ui.screen == SCR_PLAY && !ui.busy && !ui.coach)
      cell_at(par1, par2, &ui.touch_r, &ui.touch_c);
    return 0;
  }

  if (type == EVT_POINTERMOVE || type == EVT_TOUCHMOVE) {
    if (ui.screen == SCR_PLAY && ui.touch_on && !ui.swipe_used) {
      if (try_board_swipe(par1, par2))
        ui.swipe_used = 1;
    }
    return 0;
  }

  if (type == EVT_POINTERUP || type == EVT_TOUCHUP) {
    int x = par1, y = par2;
    if (ui.screen == SCR_PLAY && ui.touch_on && !ui.swipe_used) {
      if (try_board_swipe(x, y))
        ui.swipe_used = 1;
    }
    if (!ui.swipe_used) {
      if (ui.screen == SCR_TITLE) title_tap(x, y);
      else if (ui.screen == SCR_PLAY) play_tap(x, y);
      else if (ui.screen == SCR_PAUSE) pause_tap(x, y);
      else if (ui.screen == SCR_OVER) over_tap(x, y);
      else if (ui.screen == SCR_SCORES) scores_tap(x, y);
      else if (ui.screen == SCR_HELP) help_tap(x, y);
    }
    ui.touch_on = 0;
    ui.swipe_used = 0;
    ui.touch_r = ui.touch_c = -1;
    return 0;
  }

  return 0;
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  InkViewMain(main_handler);
  return 0;
}
