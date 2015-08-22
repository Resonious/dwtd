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
    SDL_Texture* sword;
    SDL_Texture* swoosh;
    SDL_Texture* grid_cell;
    SDL_Texture* cloud;
};

struct Musics {
    Mix_Music* ominous;
};

enum Controls { UP, DOWN, LEFT, RIGHT };
enum Actions { UNSPECIFIED, GO_UP, GO_DOWN, GO_LEFT, GO_RIGHT, NONE };

struct Player {
    SDL_Texture* tex;
    SDL_Texture* sword_tex;
    SDL_Texture* swoosh_tex;
    int cell_x, cell_y, prev_cell_x, prev_cell_y;
    // This increments each frame, and is set to 0 when an action is used.
    unsigned int frame_counter;
    // The animation frame of the player.
    int frame;
    Actions current_action, previous_action, next_action;
    // Cooldown between actions.
    int action_timeout;
    bool flipped;

    // The cell_y value of the ground.
    const int ground_level = 2;

    Player(SDL_Renderer* rend, SDL_Surface* surface, Textures* textures, Uint32 color) {
        if (color != 0) ChangeColorTo(surface, color);
        tex        = SDL_CreateTextureFromSurface(rend, surface);
        sword_tex  = textures->sword;
        swoosh_tex = textures->swoosh;
        frame      = 0;
        current_action  = NONE;
        previous_action = NONE;
        next_action     = UNSPECIFIED;
        action_timeout = 0;
        cell_x = 1;
        cell_y = ground_level;
        prev_cell_x = 1;
        prev_cell_y = ground_level;
        flipped = false;
    }

    ~Player() {
        SDL_DestroyTexture(tex);
    }

    SDL_RendererFlip sdl_flip() {
        return flipped ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    }

    void animate() {
        frame_counter += 1;
        switch (current_action) {
        case NONE:
            if (frame_counter % 25 == 0) {
                frame += 1;
                if (frame > 2) frame = 0;
            }
            break;

        default: break;
        }
    }

    void update() {
        animate();

        if (action_timeout > 0) {
            action_timeout -= 1;
            if (action_timeout == 0) {
                // previous_action = current_action;
                if (next_action == UNSPECIFIED)
                    next_action = NONE;
            }
        }
        if (action_timeout <= 0) {
            if (next_action != UNSPECIFIED) {
                previous_action = current_action;
                current_action  = next_action;
                next_action     = UNSPECIFIED;
                if (current_action != NONE) action_timeout  = 18;
                frame_counter   = 0;

                prev_cell_x = cell_x;
                prev_cell_y = cell_y;

                switch (current_action) {
                case NONE:
                    if (cell_y != ground_level) {
                        cell_y = ground_level;
                        OutputDebugString("Fell to the ground!\n");
                    }
                    break;

                case GO_UP:
                    if (cell_y == ground_level)
                        cell_y -= 1;
                    else {
                        OutputDebugString("Flopped\n");
                        cell_y = ground_level;
                    }
                    break;

                case GO_DOWN:
                    if (cell_y == ground_level)
                        OutputDebugString("Crouch!!\n");
                    else {
                        cell_y = ground_level;
                        OutputDebugString("Slam!\n");
                    }
                    break;

                case GO_LEFT:
                    cell_x -= 1;
                    flipped = true;
                    if (cell_y < ground_level)
                        cell_y = ground_level;
                    if (cell_x < 1)
                        OutputDebugString("Falling!!!!\n");
                    break;

                case GO_RIGHT:
                    cell_x += 1;
                    flipped = false;
                    if (cell_y < ground_level)
                        cell_y = ground_level;
                    if (cell_x > 5)
                        OutputDebugString("Falling!!!!\n");
                    break;

                default:
                    break;
                }

                next_action = UNSPECIFIED;
            }
        }
    }

    void render(SDL_Renderer* renderer) {
        SDL_Rect src = { frame * 45, 0,   45, 45 };

        SDL_Rect dest = {
            (cell_x * 45 * scale), ((17 + cell_y * 45) * scale),
            (45 * scale), (45 * scale)
        };

        if (current_action == GO_RIGHT || current_action == GO_LEFT) {
            SDL_Rect fsrc = { 0, 0, 45, 45 };
            SDL_Rect fdest = {
                (prev_cell_x * 45 * scale), ((17 + prev_cell_y * 45) * scale),
                (45 * scale), (45 * scale)
            };

            if (current_action == GO_RIGHT) fdest.x += 10;
            if (current_action == GO_LEFT)  fdest.x -= 10;

            SDL_SetTextureAlphaMod(tex, 155 - frame_counter * 2);
            SDL_RenderCopyEx(renderer, tex, &fsrc, &fdest, 0, 0, sdl_flip());
            SDL_SetTextureAlphaMod(tex, 255);

            fsrc.x += (frame_counter / 7 - 1) * 45;
            if (current_action == GO_RIGHT) fdest.x += 20;
            if (current_action == GO_LEFT)  fdest.x -= 20;
            SDL_RenderCopyEx(renderer, swoosh_tex, &fsrc, &fdest, 0, 0, sdl_flip());
        }

        SDL_RenderCopyEx(renderer, tex, &src, &dest, 0, 0, sdl_flip());
        SDL_RenderCopyEx(renderer, sword_tex, &src, &dest, 0, 0, sdl_flip());
    }
};

struct Cloud {
    SDL_Texture* tex;
    SDL_Rect rect;
    int speed;
    int move_timeout;
    int frame;
    int screen_width;

    void initialize(int screen_width, int screen_height, SDL_Texture* tex) {
        this->tex = tex;
        this->screen_width = screen_width;
        rect = {
            (rand() % screen_width) - 128*scale, screen_height - rand() % (72*scale),
            128 * scale, 64 * scale
        };
        if (rand() % 10 > 7) rect.y -= 80;
        frame = rand() % 4;
        speed = rand() % 5;
        move_timeout = 0;
    }

    void render(SDL_Renderer* renderer) {
        SDL_Rect src = { 128 * frame, 0, 128, 64 };
        SDL_RenderCopy(renderer, tex, &src, &rect);

        if (move_timeout > 0)
            move_timeout -= 1;
        else {
            rect.x += 1;
            move_timeout = 10 - speed;

            if (rect.x > screen_width)
                rect.x = -128 * scale;
        }
    }
};

#define keypressed(ctl) (controls[ctl] && !last_controls[ctl])
const int NUM_CLOUDS = 15;

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
    Cloud clouds[NUM_CLOUDS];

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
    player_surface = IMG_Load("assets/player/player.png");

    temp_surface = IMG_Load("assets/mountaintop.png");
    tex.background = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);

    temp_surface = IMG_Load("assets/player/sword.png");
    tex.sword = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);

    temp_surface = IMG_Load("assets/player/swoosh.png");
    tex.swoosh = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_SetTextureAlphaMod(tex.swoosh, 155);
    SDL_FreeSurface(temp_surface);

    temp_surface = IMG_Load("assets/ui/gridcell.png");
    tex.grid_cell = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);

    temp_surface = IMG_Load("assets/clouds.png");
    tex.cloud = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_SetTextureAlphaMod(tex.cloud, 155);
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

    // ====================== Initialize Player And Scenery ======================
    Player player(renderer, player_surface, &tex, 0);
    for (int i = 0; i < NUM_CLOUDS; i++)
        clouds[i].initialize(window_width, window_height, tex.cloud);

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
        if      (controls[RIGHT]) player.next_action = GO_RIGHT;
        else if (controls[LEFT])  player.next_action = GO_LEFT;
        else if (controls[UP])    player.next_action = GO_UP;
        else if (controls[DOWN])  player.next_action = GO_DOWN;
        else player.next_action = UNSPECIFIED;

        player.update();

        // ====================== Render ========================
        SDL_RenderClear(renderer);

        SDL_RenderCopy(renderer, tex.background, NULL, NULL);
        player.render(renderer);
        for (int i = 0; i < NUM_CLOUDS; i++)
            clouds[i].render(renderer);

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