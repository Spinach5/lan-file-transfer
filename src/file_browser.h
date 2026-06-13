#ifndef FILE_BROWSER_H
#define FILE_BROWSER_H

#include <SDL2/SDL.h>
#include "ui.h"

void file_browser_init(struct app_state *state);
void file_browser_render(SDL_Renderer *r, struct app_state *state);
bool file_browser_handle_event(SDL_Event *e, struct app_state *state);

#endif /* FILE_BROWSER_H */
