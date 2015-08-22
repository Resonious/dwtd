#ifdef _WIN32
#include <Windows.h>
#endif

#include <stdio.h>
#include "SDL/SDL.h"
#include "SDL/SDL_image.h"
#include "SDL/SDL_mixer.h"

// SDL_main doesn't seem to work with VS2015
#undef main

// Oh god a global. Sue me.
int scale = 2;

void ChangeColorTo(SDL_Surface* surface, Uint32 color) {
    if (surface->format->BytesPerPixel != 4) exit(5);

    for (int x = 0; x < surface->w; x++) for (int y = 0; y < surface->h; y++)
    {
        Uint8* pixel_addr = (Uint8*)surface->pixels + y * surface->pitch + x * 4;
        Uint32 pixel = *((Uint32*)pixel_addr);

        if (pixel != 0x00000000)
            *(Uint32 *)pixel_addr = color;
    }
}

struct Textures {
    SDL_Texture* background;
    SDL_Texture* sword_tex;
};

struct Musics {
    Mix_Music* ominous;
};

enum Controls { UP, DOWN, LEFT, RIGHT };

enum Actions { GO_UP, GO_DOWN, GO_LEFT, GO_RIGHT, NONE };

struct Player {
    SDL_Texture* tex;
    int cell_x, cell_y;
    int frame;

    const int ground_level = 2;

    Player(SDL_Renderer* rend, SDL_Surface* surface, Uint32 color) {
        if (color != 0) ChangeColorTo(surface, color);
        tex = SDL_CreateTextureFromSurface(rend, surface);
        frame = 0;
    }

    ~Player() {
        SDL_DestroyTexture(tex);
    }

    void render(SDL_Renderer* renderer) {
        SDL_Rect src;
        src.x = frame * 45;
        src.y = 0;
        src.w = 45;
        src.h = 45;

        SDL_Rect dest;
        dest.x = cell_x * 45 * scale;
        dest.y = (17 + cell_y * 45) * scale;
        dest.w = 45 * scale;
        dest.h = 45 * scale;

        SDL_RenderCopy(renderer, tex, &src, &dest);
    }
};

#define pressed(ctl) (controls[ctl] && !last_controls[ctl])

#ifdef _WIN32
int WinMain(HINSTANCE hinst, HINSTANCE prev, LPSTR cmdline, int cmdshow) {
#else
int main() {
#endif
    // =============== SDL and asset stuff ==================
    SDL_Window* window;
    int window_width = 720, window_height = 512;
    SDL_Renderer* renderer;
    SDL_Surface* player_surface;
    SDL_Surface* temp_surface;
    Textures tex;
    Musics music;
    bool last_controls[4];
    bool controls[4];

    // ================== Initialize things ====================
    if (!SDL_Init(SDL_INIT_AUDIO) == -1) { exit(1); }
    if (Mix_Init(MIX_INIT_OGG) & MIX_INIT_OGG != MIX_INIT_OGG) { exit(2); }

    window = SDL_CreateWindow(
        "A Duel With the Devil",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        window_width, window_height, 0
    );
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawColor(renderer, 165, 226, 216, 255);

    memset(&last_controls, 0, sizeof(last_controls));
    memset(&controls, 0, sizeof(controls));

    // =================== Load Some Assets ===================
    player_surface = IMG_Load("assets/player.png");

    temp_surface = IMG_Load("assets/mountaintop.png");
    tex.background = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_Rect bg_rect;
    bg_rect.x = 0; bg_rect.y = 0; bg_rect.w = window_width; bg_rect.h = window_height;
    SDL_FreeSurface(temp_surface);

    if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 2048) == -1)
        exit(3);
    music.ominous = Mix_LoadMUS("assets/music/ominous.ogg");
    if (!music.ominous) {
        char shit[100];
        sprintf(shit, "Problem loading music: %s\n", Mix_GetError());
        OutputDebugString(shit);
    }
    else
        Mix_PlayMusic(music.ominous, -1);

    // ====================== Initialize Player ======================
    Player player(renderer, player_surface, 0);
    player.cell_x = 1;
    player.cell_y = 1;

    // ============ Stuff for game loop management ===============
    SDL_Event event;
    Uint64 milliseconds_per_tick = 1000 / SDL_GetPerformanceFrequency();
    int key_count;
    const Uint8* keys = SDL_GetKeyboardState(&key_count);

    while (true) {
        // ============= Frame Setup =================
        Uint64 frame_start = SDL_GetPerformanceCounter();

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT: exit(0); break;
            }
        }

        controls[UP]    = keys[SDL_SCANCODE_UP]    || keys[SDL_SCANCODE_W];
        controls[DOWN]  = keys[SDL_SCANCODE_DOWN]  || keys[SDL_SCANCODE_S];
        controls[LEFT]  = keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A];
        controls[RIGHT] = keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D];

        // ================= Game Logic ===================
        if (pressed(RIGHT)) player.cell_x += 1;
        if (pressed(LEFT)) player.cell_x -= 1;
        if (pressed(UP)) player.cell_y -= 1;
        if (pressed(DOWN)) player.cell_y += 1;

        // ====================== Render ========================
        SDL_RenderClear(renderer);

        SDL_RenderCopy(renderer, tex.background, NULL, NULL);
        player.render(renderer);

        SDL_RenderPresent(renderer);

        memcpy(&last_controls, &controls, sizeof(controls));

        // ======================= Cap Framerate =====================
        Uint64 frame_end = SDL_GetPerformanceCounter();
        Uint64 frame_ms = (frame_end - frame_start) * milliseconds_per_tick;

        if (frame_ms < 17)
            SDL_Delay(17 - frame_ms);
    }

	return 0;
}