#ifdef _WIN32
#include <Windows.h>
#endif

#include <stdio.h>
#include "SDL/SDL.h"
#include "SDL/SDL_image.h"
#include "SDL/SDL_mixer.h"

#undef main

struct Textures {
    SDL_Texture* player_tex;
    SDL_Texture* sword_tex;
};

struct Musics {
    Mix_Music* ominous;
};

void ChangeBlackTo(SDL_Surface* surface, Uint32 color) {
    if (surface->format->BytesPerPixel != 4) exit(5);

    for (int x = 0; x < surface->w; x++) for (int y = 0; y < surface->h; y++)
    {
        Uint8* pixel_addr = (Uint8*)surface->pixels + y * surface->pitch + x * 4;
        Uint32 pixel = *((Uint32*)pixel_addr);

        if (pixel == 0xFF000000)
            *(Uint32 *)pixel_addr = color;
    }
}

#ifdef _WIN32
int WinMain(HINSTANCE hinst, HINSTANCE prev, LPSTR cmdline, int cmdshow) {
#else
int main() {
#endif
    // === SDL and asset stuff ===
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Surface* loading_surface;
    Textures tex;
    Musics music;

    // === Initialize things ===
    if (!SDL_Init(SDL_INIT_AUDIO) == -1) { exit(1); }
    if (Mix_Init(MIX_INIT_OGG) & MIX_INIT_OGG != MIX_INIT_OGG) { exit(2); }

    window = SDL_CreateWindow(
        "A Duel With the Devil",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        640, 480, 0
    );
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawColor(renderer, 165, 226, 216, 255);

    // === Load images ===
    loading_surface = IMG_Load("assets/player.png");
    ChangeBlackTo(loading_surface, 0xE21B1BFF);
    tex.player_tex = SDL_CreateTextureFromSurface(renderer, loading_surface);
    SDL_FreeSurface(loading_surface);

    // === Load sounds ===
    if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 2048) == -1)
        exit(3);
    music.ominous = Mix_LoadMUS("assets/music/ominous.ogg");
    if (!music.ominous) {
        char shit[100];
        sprintf(shit, "Problem loading music: %s\n", Mix_GetError());
        OutputDebugString(shit);
        // exit(4);
    }
    else
        Mix_PlayMusic(music.ominous, -1);

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