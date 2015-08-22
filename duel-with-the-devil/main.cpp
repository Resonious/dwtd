#include <stdio.h>
#include "SDL/SDL.h"
#include "SDL/SDL_image.h"

#undef main

struct Textures {
    SDL_Texture* player_tex;
    SDL_Texture* sword_tex;
};

int WinMain() {
    // === SDL and asset stuff ===
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Surface* loading_surface;
    Textures tex;

    // === Initialize things ===
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

    // === Stuff for game loop management ===
    SDL_Event event;
    Uint64 milliseconds_per_tick = 1000 / SDL_GetPerformanceFrequency();

    while (true) {
        Uint64 frame_start = SDL_GetPerformanceCounter();

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT: exit(0); break;
            }
        }

        SDL_RenderClear(renderer);

        SDL_RenderCopy(renderer, tex.player_tex, NULL, NULL);

        SDL_RenderPresent(renderer);

        // Cap framerate at 60 frames/second
        Uint64 frame_end = SDL_GetPerformanceCounter();
        Uint64 frame_ms = (frame_end - frame_start) * milliseconds_per_tick;

        if (frame_ms < 17)
            SDL_Delay(17 - frame_ms);
    }

	return 0;
}