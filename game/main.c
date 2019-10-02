#include <stdio.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>

#include "2D.h"
#include "assets.h"

#include "text_image.h"
#include "screens.h"
#include "menu.h"
#include "game.h"
#include "scores.h"
#include "music.h"

#include "xSDL.h"

enum {
  WIN_WIDTH = 260,
  WIN_HEIGHT = 1000
};

static const char *WIN_TITLE = "Tetris";

static SDL_Window *window;
static SDL_Renderer *rend;
static PixelDim2D screen_size;
static struct ScreenObject all_screens[NUM_SCREENS];
static struct ScreenObject *current;

static int
init_video(void) {
  window = SDL_CreateWindow(WIN_TITLE, SDL_WINDOWPOS_UNDEFINED,
    SDL_WINDOWPOS_UNDEFINED, WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_SHOWN);
  //COND_ERET_IF0(window, -1, SDL_GetError());

  rend = SDL_CreateRenderer(window, -1,
    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  //COND_ERET_IF0(rend, -1, SDL_GetError());

  return 0;
}

static int
init_screens(void) {
  screen_size.w = WIN_WIDTH;
  screen_size.h = WIN_HEIGHT;

  init_menu(rend, &screen_size);
  init_game(rend, &screen_size);
  init_scores(rend, &screen_size);
  current = all_screens + MENU_SCREEN;
  return 0;
}

static int
init(void) {
  SDL_Init(SDL_INIT_EVERYTHING);
  init_video();
  TTF_Init();
  IMG_Init(IMG_INIT_PNG);
  init_assets(rend);
  init_music();
  init_screens();

  return 0;
}

static int
game_loop(void) {
  SDL_assert(current);
  play_new();
  current->focus();
  SDL_Texture *bg = get_bg_img();

  while (1) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        return 0;
      }
      current->handle_event(&e);
    }
    current->update();
    SDL_RenderCopy(rend, bg, 0, 0), SDL_GetError();
    current->render();
    SDL_RenderPresent(rend);
  }

  return 0;
}

static void
cleanup(void) {
  for (int i = 0; i < NUM_SCREENS; i++) {
    if (all_screens[i].destroy) {
      all_screens[i].destroy();
    }
  }
  xSDL_DestroyRenderer(&rend);
  xSDL_DestroyWindow(&window);
  TTF_Quit();
  IMG_Quit();
  SDL_Quit();
}

static void
error_quit() {
  //report_error();
  cleanup();
  //free_error(0);
  exit(EXIT_FAILURE);
}

void
register_screen(const enum ScreenId which,
                const struct ScreenObject *screen) {
  const int screen_i = (int) which;
  SDL_assert(screen_i >= 0);
  SDL_assert(screen_i < NUM_SCREENS);
  all_screens[screen_i] = *screen;
}

void
change_screen(const enum ScreenId which) {
  const int screen_i = (int) which;
  SDL_assert(screen_i >= 0);
  SDL_assert(screen_i < NUM_SCREENS);
  current = all_screens + screen_i;
  if (which == GAME_SCREEN) {
    play_new();
  }
  current->focus();
  return;


  error_quit();
}

int
main(int argc, char *argv[]) 
{
   for (int i = 0; i < 2; i++) {
        if (SDL_JoystickOpen(i) == NULL) 
        {
            //WriteLogFile("sdmc://WriteLogFile.log", LogType_Info,"SDL_JoystickOpen: %s\n", SDL_GetError());
        }
        else
        {
            //WriteLogFile("sdmc://WriteLogFile.log", LogType_Info,"SDL_JoystickOpen: OK\n");
        }
    } 

  init();
  game_loop();
  cleanup();
  return 0;


  error_quit();
}
