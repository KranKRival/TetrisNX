#include <assert.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "2D.h"

#include "text_image.h"
#include "screens.h"
#include "assets.h"

enum {
  TOP_TITLE_OFFSET = 50,
  TOP_MENU_OFFSET = 250
};

static struct Dim2D screen_dim;
static struct TextImage title, new_game, scores;
static SDL_Renderer *g_rend;

static void
destroy(void) {
  destroy_text_image(&title);
  destroy_text_image(&new_game);
  destroy_text_image(&scores);
}

static int
handle_event(const SDL_Event *e) {
  //if (e->type == SDL_MOUSEBUTTONDOWN) {
    //int x, y;
    //SDL_GetMouseState(&x, &y);
    //if (in_bounds(&new_game, x, y)) {
      //change_screen(GAME_SCREEN);
    //}
    //else if (in_bounds(&scores, x, y)) {
      //change_screen(SCORES_SCREEN);
    //}
  //}

  if (e->type == SDL_JOYBUTTONDOWN && e->jbutton.button == 11) 
  {
    destroy();
    SDL_Quit();
    exit(EXIT_FAILURE);
    return 0;
    
  }
  else if (e->type == SDL_JOYBUTTONDOWN && e->jbutton.button == 10) 
  {
    change_screen(GAME_SCREEN);
  }
  return 0;
}

static int
update(void) {
  return 0;
}

static int
focus(void) {
  return 0;
}

static int
render(void) {
  render_text_image(&title);
  render_text_image(&new_game);
  render_text_image(&scores);
  return 0;
}

int
init_menu(SDL_Renderer *g_rend_, const PixelDim2D *screen_dim_) {
  g_rend = g_rend_;
  screen_dim = *screen_dim_;

  TTF_Font *title_font = get_large_font();
  TTF_Font *tetrisfont = get_tetris_font();
  TTF_Font *button_font = get_medium_font();
  TTF_Font *Smallfont = get_small_font();
  

  
    init_text_image(&title, tetrisfont, "   TetrisNX   ", g_rend, &GREEN);
    init_text_image(&new_game, Smallfont, "Press ( Plus ) for New Game", g_rend, &RED);
    init_text_image(&scores, Smallfont, "  Press ( Minus ) to Exit  ", g_rend, &RED);
  
  title.dim.w += 10;
  title.dim.h += 20;
  title.pos.x = hor_center_within(&title.dim, &screen_dim);
  title.pos.y = TOP_TITLE_OFFSET;
  new_game.dim.h += 20;
  new_game.pos.x = hor_center_within(&new_game.dim, &screen_dim);
  new_game.pos.y = TOP_MENU_OFFSET + 300;
  scores.dim.h += 20;
  scores.pos.x = hor_center_within(&scores.dim, &screen_dim);
  scores.pos.y = TOP_MENU_OFFSET + new_game.dim.h + 340;

  const struct ScreenObject self = {
    .destroy = destroy,
    .handle_event = handle_event,
    .update = update,
    .render = render,
    .focus = focus
  };
  register_screen(MENU_SCREEN, &self);

  return 0;

  destroy();
  return -1;
}
