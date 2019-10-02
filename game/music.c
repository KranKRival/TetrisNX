#include <SDL2/SDL_mixer.h>

#include "xSDL.h"

#include "music.h"

static Mix_Music *music = NULL;

void init_music(void) {
 
}

int play_new(void) 
{
   
  
	if (SDL_Init(SDL_INIT_AUDIO) < 0)
		return -1;
			
	//Initialize SDL_mixer 
	if( Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 4096 ) == -1 ) 
		return -1; 
	
	// Load our sound effect
	
	
	// Load our music
	music = Mix_LoadMUS("assets/music.mp3");
	if (music == NULL)
		return -1;
	
	if ( Mix_PlayMusic( music, -1) == -1 )
		return -1;
		
	//while ( Mix_PlayingMusic() ) ;
  return 0;
}

void destroy_music(void) 
{
  // clean up our resources
	
	Mix_FreeMusic(music);
	
	// quit SDL_mixer
	Mix_CloseAudio();
}
