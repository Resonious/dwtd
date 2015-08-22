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
    bool moved;

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
        moved = false;
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
                moved = false;
                if (next_action == UNSPECIFIED)
                    next_action = NONE;
            }
        }
        if (action_timeout <= 0) {
            if (next_action != UNSPECIFIED) {
                previous_action = current_action;
                current_action  = next_action;
                next_action     = UNSPECIFIED;
                frame_counter   = 0;
                if (current_action != NONE) {
                    action_timeout = 18;
                    moved = true;
                }

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

            // Shift shadow image forwards a bit so that it doesn't looks like we're teleporting.
            SDL_Rect shadowdest = fdest;
            if (current_action == GO_RIGHT) shadowdest.x += (10 + frame_counter) * scale;
            if (current_action == GO_LEFT)  shadowdest.x -= (10 + frame_counter) * scale;

            SDL_SetTextureAlphaMod(tex, 155 - frame_counter * 9);
            SDL_RenderCopyEx(renderer, tex, &fsrc, &shadowdest, 0, 0, sdl_flip());
            SDL_SetTextureAlphaMod(tex, 255);

            // Swoosh frame:
            fsrc.x += (frame_counter / 2) * 45;
            // Shift further forwards for swoosh.
            if (current_action == GO_RIGHT) fdest.x += 20 * scale;
            if (current_action == GO_LEFT)  fdest.x -= 20 * scale;
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

#define keyreleased(ctl) (controls[ctl] && !last_controls[ctl])
#define keypressed(ctl) (!controls[ctl] && last_controls[ctl])
#define NUM_CLOUDS 15

bool any(bool controls[4]) {
    return controls[0] || controls[1] || controls[2] || controls[3];
}

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
    AnyPopup();

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

    // TODO this isn't used yet... Will it ever be?
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

    // ====================== Initialize Things That Do Stuff ======================
    Player player(renderer, player_surface, &tex, 0);
    for (int i = 0; i < NUM_CLOUDS; i++)
        clouds[i].initialize(window_width, window_height, tex.cloud);

    // ============ Stuff for game loop management ===============
    SDL_Event event;
    Uint64 milliseconds_per_tick = 1000 / SDL_GetPerformanceFrequency();
    int key_count;
    const Uint8* keys = SDL_GetKeyboardState(&key_count);
    // If we press and release a key while player is still cooling down from movement, we
    // want to use that key.
    Actions reserve_action = UNSPECIFIED;
    bool keys_pressed_since_move[4];
    memset(keys_pressed_since_move, 0, sizeof(keys));
    // int frames_before_reserve = 4;
    bool moved_from[4];
    memset(moved_from, 0, sizeof(moved_from));
    bool player_just_moved = false;

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

        if (moved_from[UP] && !controls[UP]) moved_from[UP] = false;
        if (moved_from[DOWN] && !controls[DOWN]) controls[DOWN] = false;
        if (moved_from[LEFT] && !controls[LEFT]) controls[LEFT] = false;
        if (moved_from[RIGHT] && !controls[RIGHT]) controls[RIGHT] = false;

        // ================= Game Logic ===================
        if (player.moved && player.frame_counter > 0) {
            if (keys_pressed_since_move[RIGHT]) {
                if (keyreleased(RIGHT)) reserve_action = GO_RIGHT;
            }
            else keys_pressed_since_move[RIGHT] = keypressed(RIGHT);

            if (keys_pressed_since_move[LEFT]) {
                if (keyreleased(LEFT)) reserve_action = GO_LEFT;
            }
            else keys_pressed_since_move[LEFT] = keypressed(LEFT);

            if (keys_pressed_since_move[UP]) {
                if (keyreleased(UP)) reserve_action = GO_UP;
            }
            else keys_pressed_since_move[UP] = keypressed(UP);

            if (keys_pressed_since_move[DOWN]) {
                if (keyreleased(DOWN)) reserve_action = GO_DOWN;
            }
            else keys_pressed_since_move[DOWN] = keypressed(DOWN);
        }

        if      (controls[RIGHT] && !moved_from[RIGHT]) player.next_action = GO_RIGHT;
        else if (controls[LEFT] && !moved_from[LEFT])  player.next_action = GO_LEFT;
        else if (controls[UP] && !moved_from[UP])    player.next_action = GO_UP;
        else if (controls[DOWN] && !moved_from[DOWN])  player.next_action = GO_DOWN;
        else if (reserve_action != UNSPECIFIED) player.next_action = reserve_action;

        player.update();

        if (player.moved) {
            if (player.frame_counter == 0) {
                player_just_moved = true;
                memset(moved_from, 0, sizeof(moved_from));
                int key_from_action = player.current_action - 1;
                if (key_from_action >= 0 && key_from_action < sizeof(keys))
                    moved_from[key_from_action] = true;

                reserve_action = UNSPECIFIED;
                memset(keys_pressed_since_move, 0, sizeof(keys_pressed_since_move));
            }
        }
        else if (player_just_moved) {
            memset(moved_from, 0, sizeof(moved_from));
        }

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