#include <stdio.h>
#include "SDL/SDL.h"
#include "SDL/SDL_image.h"

#undef main

struct Textures {
    SDL_Texture* player_tex;
    SDL_Texture* sword_tex;
};

int WinMain(int argc, char *argv[]) {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Surface* loading_surface;
    Textures tex;

    window = SDL_CreateWindow(
        "A Duel With the Devil",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        640, 480, 0
    );
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawColor(renderer, 165, 226, 216, 255);

    loading_surface = IMG_Load("assets/player.png");
    tex.player_tex = SDL_CreateTextureFromSurface(renderer, loading_surface);
    SDL_FreeSurface(loading_surface);

    SDL_Event event;

    while (true) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT: exit(0); break;
            }
        }

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, tex.player_tex, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

	return 0;
}