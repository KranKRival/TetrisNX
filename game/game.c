#include <assert.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "xSDL.h"
#include "2D.h"
#include "text_image.h"
#include "screens.h"
#include "game.h"
#include "assets.h"

#include "scores.h"

#include <stdbool.h>

#define WHITE_INIT_CODE {255, 255, 255, 255}
#define BLACK_INIT_CODE {0,   0,   0,   255}

#define COLOR0_INIT_CODE {255, 0,   0,   255}
#define COLOR1_INIT_CODE {0,   255, 0,   255}
#define COLOR2_INIT_CODE {0,   0,   255, 255}
#define COLOR3_INIT_CODE {255, 255, 0,   255}
#define COLOR4_INIT_CODE {255, 0,   255, 255}
#define COLOR5_INIT_CODE {0,   255, 255, 255}
#define COLOR6_INIT_CODE {0,   128, 255, 255}

static const SDL_Color BLACK = BLACK_INIT_CODE;
static const SDL_Color WHITE = WHITE_INIT_CODE;
static const SDL_Color PANEL_BORDER_COLOR = WHITE_INIT_CODE;

typedef struct Point2D GridPoint2D;
typedef struct Dim2D GridDim2D;

static bool paused = false;
void toggle_pause() {
  paused = !paused;
}

static bool HeldKey_Left = false;
static bool HeldKey_Right = false;
static bool HeldKey_Up = false;
static bool HeldKey_Down = false;
static bool HeldKey_A = false;
static bool HeldKey_B = false;

enum {
  PANEL_ROWS = 20,
  PANEL_COLS = 10,
  PADDING_PX = 30,

  // In tetris, a piece always is made out of 4 blocks.
  NUM_PIECE_PARTS = 4,

  // Initial fall delay.
  FALL_DELAY_MS = 300,

  // At most 1 key press each KEY_PRESS_DELAY.
  KEY_PRESS_DELAY = 150,

  // There are 6 different kinds of tetris pieces.
  NUM_DIFFERENT_PIECES = 7
};

struct FallingPiece {
  GridPoint2D blocks[NUM_PIECE_PARTS];
  GridPoint2D relative; // 0,0 means bottom-left
  SDL_Color color;
};

struct Score {
  // label_text is supposed to hold the "Score" text.
  // points_text is supposed to hold a numeric string representing how many
  // points the player has.
  struct TextImage label_text, points_text;
  int points;
};

struct Pause {
  struct TextImage pause_text , dummy;
  int ppp;
};

struct Panel {
  PixelDim2D block_dim;
  SDL_Rect geom;

  /**
   * Empty blocks are black.
   *
   * Row 0 is the row on the bottom. Column 0 is the column on the left. Rows
   * grow from bottom->up and columns from left->right.
   */
  SDL_Color blocks[PANEL_ROWS][PANEL_COLS];

  struct FallingPiece falling_piece, next_piece;
};

static const SDL_Color colors[NUM_DIFFERENT_PIECES] = {
  COLOR0_INIT_CODE, COLOR1_INIT_CODE, COLOR2_INIT_CODE, COLOR3_INIT_CODE,
  COLOR4_INIT_CODE, COLOR5_INIT_CODE, COLOR6_INIT_CODE

};

struct PieceTemplate {
  GridPoint2D fills[NUM_PIECE_PARTS];
  GridDim2D size;
};

static const struct PieceTemplate
template[NUM_DIFFERENT_PIECES] = {
  { .fills = { {0, 0}, {1, 0}, {2, 0}, {3, 0} },
    .size = {4, 1} },

  { .fills = { {0, 0}, {0, 1}, {1, 0}, {1, 1} },
    .size = {2, 2} },

  { .fills = { {0, 0}, {1, 0}, {1, 1}, {2, 1} },
    .size = {2, 1} },

  { .fills = { {0, 1}, {1, 1}, {1, 0}, {2, 0} },
    .size = {3, 1} },

  { .fills = { {0, 0}, {1, 0}, {2, 0}, {2, 1} },
    .size = {3, 2} },

  { .fills = { {0, 1}, {1, 1}, {2, 1}, {2, 0} },
    .size = {3, 2} },

  { .fills = { {0, 0}, {1, 0}, {2, 0}, {1, 1} },
    .size = {3, 2} }
};

static struct Panel panel;
static struct Score score;
static struct Pause pause;
static Uint32 last_update_ms;
static SDL_Texture *block;
static SDL_Renderer *g_rend;
static PixelDim2D screen_dim;

static void
destroy(void) {
  destroy_text_image(&score.label_text);
  destroy_text_image(&score.points_text);
  destroy_text_image(&pause.pause_text);
}

static int
panel_has_block(int x, int y) {
  return !xSDL_ColorEq(panel.blocks[y] + x, &BLACK);
}

static int
is_colliding(void) {
  const GridPoint2D *rel = &panel.falling_piece.relative;
  const GridPoint2D *blocks = panel.falling_piece.blocks;

  for (int i = 0; i < NUM_PIECE_PARTS; i++) {
    int x = rel->x + blocks[i].x;
    int y = rel->y + blocks[i].y;
    if (x < 0 || x >= PANEL_COLS || y < 0 ||
        (y < PANEL_ROWS && panel_has_block(x, y)))
    {
      return 1;
    }
  }
  return 0;
}

static void
flip(struct FallingPiece *piece) {
  GridPoint2D *blocks = piece->blocks;
  int adj_x = 0;
  int adj_y = 0;

  // Perform a pi/2 rad rotation.
  for (int i = 0; i < NUM_PIECE_PARTS; i++) {
    int x = blocks[i].x;
    int y = blocks[i].y;
    int xp = -y;
    int yp = x;
    blocks[i].x = xp;
    blocks[i].y = yp;

    // Gather negatives.
    if (yp < 0 && -yp > adj_y) {
      adj_y = -yp;
    }
    if (xp < 0 && -xp > adj_x) {
      adj_x = -xp;
    }
  }

  // Normalize from the negatives gathered so that nothing is negative in the
  // end result.
  for (int i = 0; i < NUM_PIECE_PARTS; i++) {
    blocks[i].x += adj_x;
    blocks[i].y += adj_y;
  }
}

static void
flip_back(struct FallingPiece *piece) {
  flip(piece);
  flip(piece);
  flip(piece);
}

static int handle_event(const SDL_Event *e) 
{
  if (e->type == SDL_JOYBUTTONDOWN) 
  {
    if (e->jbutton.which == 0) 
    {
      switch (e->jbutton.button) {
      case 15: // Key Down
      if (!paused)
      {
        HeldKey_Down = true;
      }
      break;
      case 12: // Key Left
      if (!paused)
      {
        HeldKey_Left = true;
      }
      break;
      case 14: // Key Right
      if (!paused)
      {
        HeldKey_Right = true;
      }
      break;
      case 13: // Key Up
      if (!paused)
      {
        HeldKey_Up = true;
        
      }
      break;
      case 0: // Key A
      if (!paused)
      {
         HeldKey_A = true;
        
      }
      break;
      case 1: // Key B
      if (!paused)
      {
         HeldKey_B = true;
        
      }
      break;
      case 11:
      HeldKey_Left = false;
      HeldKey_Right = false;
      HeldKey_Up = false;
      HeldKey_Down = false;
      HeldKey_A = false;
      HeldKey_B = false;
      paused = false;
      Mix_ResumeMusic();
      change_screen(MENU_SCREEN);
      break;
      case 10:
      toggle_pause();
      break;
    }
    
    }
  }
  if (e->type == SDL_JOYBUTTONUP) 
  {
    if (e->jbutton.which == 0) 
    {
      switch (e->jbutton.button) {
      case 15: // Key Down
      if (!paused)
      {
        HeldKey_Down = false;
      }
      break;
      case 12: // Key Left
      if (!paused)
      {
        HeldKey_Left = false;
      }
      break;
      case 14: // Key Right
      if (!paused)
      {
        HeldKey_Right = false;
      }
      break;
      case 13: // Key Up
      if (!paused)
      {
        HeldKey_Up = false;
      }
      break;
      case 0: // Key A
      if (!paused)
      {
        HeldKey_A = false;
      }
      break;
      case 1: // Key B
      if (!paused)
      {
        HeldKey_B = false;
      }
      break;
    }
    
    }
  }
  
  return 0;
}

void HandleHeldKeys()
{
  GridPoint2D *rel = &panel.falling_piece.relative;
  if (HeldKey_Down)
  {
    SDL_Delay(50);
    rel->y--;
        if (is_colliding()) {
          rel->y++;
        }
        else {
          last_update_ms = SDL_GetTicks();
        }
  }
  if (HeldKey_Left)
  {
    SDL_Delay(90);
    rel->x--;
        if (is_colliding()) {
          rel->x++;
        }
  }
  if (HeldKey_Right)
  {
    SDL_Delay(90);
    rel->x++;
        if (is_colliding()) {
          rel->x--;
        }
  }
  if (HeldKey_Up)
  {
    SDL_Delay(200);
    flip(&panel.falling_piece);
        if (is_colliding()) {
          flip_back(&panel.falling_piece);
        }
  }
  if (HeldKey_A)
  {
    SDL_Delay(200);
    flip(&panel.falling_piece);
        if (is_colliding()) {
          flip_back(&panel.falling_piece);
        }
  }
  if (HeldKey_B)
  {
    SDL_Delay(50);
    rel->y--;
        if (is_colliding()) {
          rel->y++;
        }
        else {
          last_update_ms = SDL_GetTicks();
        }
  }
}

/**
 * Black colors indicate nothing is falling.
 */
static int
is_falling(void) {
  return !xSDL_ColorEq(&panel.falling_piece.color, &BLACK);
}

static void
update_next_piece(void) 
{
  int piece_num = rand()%NUM_DIFFERENT_PIECES;
  memcpy(panel.next_piece.blocks,
         template[piece_num].fills,
         NUM_PIECE_PARTS * sizeof (GridPoint2D));
  panel.next_piece.color = colors[piece_num];
  int num_flips = rand()%4;
  for (int i = 0; i < num_flips; i++) {
    flip(&panel.next_piece);
  }
  // Depending on how many flips took place, height becomes width.
  if (num_flips % 2 == 1) {
    panel.next_piece.relative = (GridPoint2D) {
      .x = PANEL_COLS/2 - template[piece_num].size.h/2,
      .y = PANEL_ROWS - template[piece_num].size.w/2 - 1
    };
  }
  else {
    panel.next_piece.relative = (GridPoint2D) {
      .x = PANEL_COLS/2 - template[piece_num].size.w/2,
      .y = PANEL_ROWS - template[piece_num].size.h/2 - 1
    };
  }
}

static void spawn_piece(void) 
{
  SDL_assert(!is_falling());
  memcpy(&panel.falling_piece,
         &panel.next_piece,
         sizeof (struct FallingPiece));
  SDL_assert(is_falling());
  update_next_piece();
}

static void reset_piece(void) 
{
  panel.falling_piece.color = BLACK;
  SDL_assert(!is_falling());
}

static void
empty_line(int line) {
  /* memset(panel.blocks[line], 0, PANEL_COLS * sizeof (SDL_Color)); */
  for (int j = 0; j < PANEL_COLS; j++) {
    panel.blocks[line][j] = BLACK;
  }
}

static void eliminate_line(int line) 
{
  for (int i = line+1; i < PANEL_ROWS; i++) 
  {
    memcpy(panel.blocks+i-1,
           panel.blocks+i,
           PANEL_COLS * sizeof (SDL_Color));
  }
  empty_line(PANEL_ROWS-1);
}

static int try_score_line(int line) 
{
  for (int i = 0; i < PANEL_COLS; i++) 
  {
    if (xSDL_ColorEq(panel.blocks[line]+i, &BLACK)) 
    {
      return 0;
    }
  }
  eliminate_line(line);
  // 1, 2 or 3 points depending on how high the player is (=D).
  return line < 5 ? 1 : (line < 13 ? 2 : 3);
}

static int refresh_points_text(void) 
{
  char text[30];
  snprintf(text, sizeof text, "       %d       ", score.points);
  destroy_text_image(&score.points_text);
  init_text_image(&score.points_text, get_medium_font(), text, g_rend, &RED);
  score.points_text.pos = (Point2D) 
  {
    .x = PADDING_PX + panel.geom.w - score.points_text.dim.w + 44,
    .y = PADDING_PX
  };
  return 0;
}

static int try_score(void) 
{
  int pts = 0;
  int upper_bound = PANEL_ROWS;
  int i = 0;
  int lines = 0;
  while (i < upper_bound) 
  {
    int line_pts = try_score_line(i);
    if (line_pts != 0) {
      pts += line_pts;
      upper_bound--;
      lines++;
    }
    else 
    {
      i++;
    }
  }
  if (lines > 0) {
    // Each extra line you remove, you should double your points. If a line
    // gives you P points, removing 2 lines will give you 2P points, but
    // removing 3 lines (at once) will give you 4P; 4 lines 8P.
    pts <<= lines - 1;
    score.points += pts;
    refresh_points_text();
  }
  return 0;
}

static int fixate(void) {

  const GridPoint2D *rel = &panel.falling_piece.relative;
  const GridPoint2D *blocks = panel.falling_piece.blocks;

  for (int i = 0; i < NUM_PIECE_PARTS; i++) {
    int x = rel->x + blocks[i].x;
    int y = rel->y + blocks[i].y;
    if (y >= PANEL_ROWS) {
      continue;
    }
    panel.blocks[y][x] = panel.falling_piece.color;
  }
  reset_piece();
  try_score();
  return 0;
}

static int update(void) 
{
  HandleHeldKeys();
  if (paused)
  {
    Mix_PauseMusic();
  }
  else
  {
    Uint32 now_ms = SDL_GetTicks();
    
    if (is_falling()) 
    {
    Uint32 delta_ms = now_ms - last_update_ms;

    if (delta_ms > FALL_DELAY_MS) 
    {
      GridPoint2D *rel = &panel.falling_piece.relative;

      rel->y--;
      if (is_colliding()) {
        rel->y++;
        fixate();
      }
      last_update_ms = now_ms;
    }
  }
  else 
  {
    spawn_piece();
    if (is_colliding()) 
    {
      // If right after creation of new piece, it's already colliding, then
      // this game ended: add score and leave.
      HeldKey_Left = false;
      HeldKey_Right = false;
      HeldKey_Up = false;
      HeldKey_Down = false;
      HeldKey_A = false;
      HeldKey_B = false;
      add_score(score.points);
      change_screen(MENU_SCREEN);
    }
  }
    Mix_ResumeMusic();
  }
  
  return 0;
}

static void
empty_blocks(void) {
  for (int i = 0; i < PANEL_ROWS; i++) {
    empty_line(i);
  }
}

static int
focus(void) {
  last_update_ms = SDL_GetTicks();

  // On focus, a new game should be started.
  empty_blocks();
  reset_piece();
  score.points = 0;
  refresh_points_text();
  update_next_piece();
  return 0;
}

static int
render_panel_border(void) {
  const SDL_Rect border = {
    .x = 0, .y = 0,
    .w = panel.geom.w, .h = panel.geom.h
  };
  xSDL_SetRenderDrawColor(g_rend, &PANEL_BORDER_COLOR);
  SDL_RenderDrawRect(g_rend, &border);
  return 0;
}

static int
render_block(const SDL_Rect *rect, const SDL_Color *color) {
  xSDL_SetTextureColorMod(block, color);
  SDL_RenderCopy(g_rend, block, 0, rect);
  return 0;
}

static int
render_panel_blocks(void) {
  SDL_Rect block_rect = (SDL_Rect) {
    .w = panel.block_dim.w,
    .h = panel.block_dim.h
  };

  for (int i = 0; i < PANEL_ROWS; i++) {
    block_rect.y = (PANEL_ROWS - i - 1)*block_rect.h;
    for (int j = 0; j < PANEL_COLS; j++) {
      block_rect.x = block_rect.w*j;
      render_block(&block_rect, panel.blocks[i] + j);
    }
  }
  xSDL_SetTextureColorMod(block, &WHITE);
  return 0;
}

static int
render_falling_piece(void) {
  const int block_w = panel.block_dim.w;
  const int block_h = panel.block_dim.h;

  const GridPoint2D *rel = &panel.falling_piece.relative;

  // Remembering that vertical indices grow from bottom -> up.
  const int base_x_px = rel->x*block_w;
  const int base_y_px = (PANEL_ROWS - rel->y - 1)*block_h;

  SDL_Rect block_rect;
  block_rect.w = panel.block_dim.w;
  block_rect.h = panel.block_dim.h;

  for (int i = 0; i < NUM_PIECE_PARTS; i++) {
    const GridPoint2D *block = panel.falling_piece.blocks + i;

    // Remembering, again, that vertical indices grow from bottom -> up.
    block_rect.x = block->x*block_w + base_x_px;
    block_rect.y = -block->y*block_h + base_y_px;

    render_block(&block_rect, &panel.falling_piece.color);
  }
  xSDL_SetTextureColorMod(block, &WHITE);
  return 0;
}

static int
render_score(void) {
  render_text_image(&score.label_text);
  render_text_image(&score.points_text);
  if (paused)
  {
    render_text_image(&pause.pause_text);
  }
  
  ;
  return 0;
}

static int
render_next_piece(void) {
  SDL_Rect block_rect;
  block_rect.w = panel.block_dim.w;
  block_rect.h = panel.block_dim.h;

  const int base_x = PADDING_PX*2 + panel.geom.w;
  const int base_y = PADDING_PX*2 + MEDIUM_FONT_SIZE +
    block_rect.h*NUM_PIECE_PARTS;

  for (int i = 0; i < NUM_PIECE_PARTS; i++) {
    block_rect.x = base_x + panel.next_piece.blocks[i].x*block_rect.w;
    block_rect.y = base_y - panel.next_piece.blocks[i].y*block_rect.h;
    render_block(&block_rect, &panel.next_piece.color);
  }

  return 0;
}

static int
render(void) {
  SDL_Rect original_viewport;
  SDL_RenderGetViewport(g_rend, &original_viewport);

  SDL_RenderSetViewport(g_rend, &panel.geom);
  render_panel_blocks();
  render_falling_piece();
 render_panel_border();

  SDL_RenderSetViewport(g_rend, &original_viewport);
  render_score();
  render_next_piece();

  return 0;
}

int
init_game(SDL_Renderer *g_rend_, const PixelDim2D *screen_dim_) {
  g_rend = g_rend_;
  screen_dim = *screen_dim_;

  /*
   * horizontal arrangement:
   *  - (from left) PADDING . PANEL . PADDING . PANEL FOR NEXT PIECE . PADDING
   *
   * vertical arrangement:
   *  - (from top) PADDING . SCORE . PADDING . PANEL . PADDING
   *
   * block width:
   *  - Panel width / PANEL_COLS
   *
   * block height:
   *  - Panel height / PANEL_ROWS
   *
   * panel for next piece width:
   *  - 4 * block size
   */

  int panel_top_margin = PADDING_PX*2 + MEDIUM_FONT_SIZE;
  panel.block_dim = (PixelDim2D) {
    .w = (screen_dim.w - 3*PADDING_PX)/(PANEL_COLS+4),
    .h = (screen_dim.h - panel_top_margin - PADDING_PX)/PANEL_ROWS
  };
  panel.geom = (SDL_Rect) {
    .h = panel.block_dim.h*PANEL_ROWS,
    .w = panel.block_dim.w*PANEL_COLS,
    .x = PADDING_PX,
    .y = panel_top_margin
  };

  TTF_Font *font = get_medium_font();
  TTF_Font *font2 = get_large_font();
  init_text_image(&score.label_text, font, "Score:         ", g_rend, &GREEN);
  init_text_image(&score.points_text, font, "000000000000000", g_rend, &RED);
  init_text_image(&pause.pause_text, font2, "Paused         ", g_rend, &BLUE);

  score.label_text.pos = (Point2D) {.x = PADDING_PX - 1, .y = PADDING_PX};
  score.points_text.pos = (Point2D) { .x = PADDING_PX + panel.geom.w - score.points_text.dim.w + 44, .y = PADDING_PX };
  pause.pause_text.pos = (Point2D) { .x = PADDING_PX + panel.geom.w - pause.pause_text.dim.w + 200, .y = PADDING_PX + 500};
  block = get_tetris_block_img();

  const struct ScreenObject self = {
    .focus = focus,
    .render = render,
    .update = update,
    .handle_event = handle_event,
    .destroy = destroy
  };
  register_screen(GAME_SCREEN, &self);

  return 0;


  destroy();
  return -1;
}
