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
    SDL_Texture* platform;
    SDL_Texture* sword;
    SDL_Texture* horns;
    SDL_Texture* grid_cell;
    SDL_Texture* cloud;
};

struct Musics {
    Mix_Music* ominous;
    Mix_Music* death;
};

struct Sounds {
    Mix_Chunk* move;
    Mix_Chunk* jump;
    Mix_Chunk* swipe;
    Mix_Chunk* crouch;
    Mix_Chunk* clash1;
    Mix_Chunk* clash2;
};

enum Controls { UP, DOWN, LEFT, RIGHT };
enum Actions { UNSPECIFIED, GO_UP, GO_DOWN, GO_LEFT, GO_RIGHT, NONE };

struct Player {
    SDL_Texture* tex;
    SDL_Texture* swoosh_tex;
    SDL_Texture* sword_tex;
    SDL_Texture* horns_tex;
    Sounds* sfx;
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
    bool falling;
    bool dead;
    bool devil;

    // The cell_y value of the ground.
    const int ground_level = 2;

    Player(SDL_Renderer* rend, SDL_Surface* surface, SDL_Surface* swoosh_surf, Textures* textures, Sounds* sounds, Uint32 color) {
        if (color != 0) {
            ChangeColorTo(surface, color);
            ChangeColorTo(swoosh_surf, color);
        }
        tex        = SDL_CreateTextureFromSurface(rend, surface);
        swoosh_tex = SDL_CreateTextureFromSurface(rend, swoosh_surf);
        SDL_SetTextureAlphaMod(tex, 155);
        SDL_SetTextureAlphaMod(swoosh_tex, 155);
        sword_tex  = textures->sword;
        horns_tex  = textures->horns;
        sfx        = sounds;
        frame      = 2;
        current_action  = NONE;
        previous_action = NONE;
        next_action     = UNSPECIFIED;
        action_timeout  = 0;
        cell_x          = 1;
        cell_y          = ground_level;
        prev_cell_x     = 1;
        prev_cell_y     = ground_level;
        flipped         = false;
        moved           = false;
        falling         = false;
        dead  = false;
        devil = false;
    }

    ~Player() {
        SDL_DestroyTexture(tex);
    }

    void refresh_texture(SDL_Renderer* rend, SDL_Surface* surface) {
        SDL_DestroyTexture(tex);
        tex = SDL_CreateTextureFromSurface(rend, surface);
    }

    SDL_RendererFlip sdl_flip() {
        return flipped ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    }

    void assign_frame() {
        frame_counter += 1;
        switch (current_action) {
        case NONE:
            if (frame < 2 || frame > 4) frame = 1;
            if (frame_counter % 25 == 0) {
                frame += 1;
                if (frame > 4) frame = 2;
            }
            break;

        case GO_UP:
            if (cell_y == ground_level)
                frame = 1;
            else
                if (frame_counter > 3) frame = 6;
                else frame = 5;

            break;

        case GO_DOWN:
            if (cell_y == ground_level && prev_cell_y < ground_level)
                frame = 2;
            else
                frame = 1;
            break;

        case GO_LEFT: case GO_RIGHT:
            if (previous_action == GO_UP)
                frame = 0;
            else
                frame = 2;
            break;

        default:
            frame = 2;
            if (cell_y == ground_level && prev_cell_y < ground_level)
                frame = 1;
            break;
        }
    }

    void update() {
        if (falling || dead) { frame_counter += 1; return; }
        assign_frame();

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
                    if (cell_y == ground_level) {
                        cell_y -= 1;
                        Mix_PlayChannel(2, sfx->jump, 0);
                    }
                    else {
                        OutputDebugString("Flopped\n");
                        cell_y = ground_level;
                    }
                    break;

                case GO_DOWN:
                    Mix_PlayChannel(3, sfx->crouch, 0);
                    if (cell_y == ground_level)
                        OutputDebugString("Crouch!!\n");
                    else {
                        cell_y = ground_level;
                        OutputDebugString("Slam!\n");
                    }
                    break;

                case GO_LEFT:
                    cell_x -= 1;
                    if (previous_action != GO_DOWN)
                        flipped = true;
                    if (cell_y < ground_level) {
                        Mix_PlayChannel(4, sfx->swipe, 0);
                        cell_y = ground_level;
                    }
                    else
                        Mix_PlayChannel(1, sfx->move, 0);
                    if (cell_x < 1)
                        OutputDebugString("Falling!!!!\n");
                    break;

                case GO_RIGHT:
                    cell_x += 1;
                    if (previous_action != GO_DOWN)
                        flipped = false;
                    if (cell_y < ground_level) {
                        cell_y = ground_level;
                        Mix_PlayChannel(4, sfx->swipe, 0);
                    }
                    else
                        Mix_PlayChannel(1, sfx->move, 0);
                    if (cell_x > 5)
                        OutputDebugString("Falling!!!!\n");
                    break;

                default:
                    break;
                }

                if ((cell_x <= 0 || cell_x >= 7) && cell_y == ground_level && previous_action != GO_DOWN) {
                    frame = 7;
                    frame_counter = 0;
                    falling = true;
                    if (cell_x <= 0) flipped = false;
                    else flipped = true;
                    dead = true;
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
        SDL_Point center {21, 29};
        double angle = 0;

        if (falling) {
            int direction = cell_x <= 0 ? 1 : -1;
            dest.x += (frame_counter / 10) * direction;
            dest.y += 10 + frame_counter * 2;
            angle = double(frame_counter) * 1.15 * -double(direction);
            if (frame_counter > 60) {
                dest.w -= (frame_counter - 60) / 2;
                dest.h -= (frame_counter - 60) / 2;
            }
        }
        else if (dead) {
            // TODO death?
        }
        else {
            // =========================== Special Effects For Moving Left/Right ======================
            if (current_action == GO_RIGHT || current_action == GO_LEFT) {
                // Shadow frame 2 -----v
                SDL_Rect fsrc = { 45 * 2, 0, 45, 45 };
                SDL_Rect fdest = {
                    (prev_cell_x * 45 * scale), ((17 + prev_cell_y * 45) * scale),
                    (45 * scale), (45 * scale)
                };

                // Shift shadow image forwards a bit so that it doesn't looks like we're teleporting.
                SDL_Rect shadowdest = fdest;
                if (current_action == GO_RIGHT) shadowdest.x += (10 + frame_counter) * scale;
                if (current_action == GO_LEFT)  shadowdest.x -= (10 + frame_counter) * scale;
                if (previous_action == GO_UP) shadowdest.y += frame_counter * scale;

                SDL_SetTextureAlphaMod(tex, 155 - frame_counter * 9);
                SDL_RenderCopyEx(renderer, tex, &fsrc, &shadowdest, 0, 0, sdl_flip());
                SDL_SetTextureAlphaMod(tex, 255);

                // Sword swoosh!
                if (previous_action == GO_UP) {
                    int swoosh_frame = 0 + (frame_counter / 2);
                    if (swoosh_frame < 4) {
                        fsrc.x = swoosh_frame * 45;
                        if (current_action == GO_RIGHT) fdest.x += 50 * scale;
                        if (current_action == GO_LEFT)  fdest.x -= 50 * scale;
                        fdest.y += 40 * scale;
                        SDL_RenderCopyEx(renderer, swoosh_tex, &fsrc, &fdest, 0, 0, sdl_flip());
                    }
                }
                else {
                    // Swoosh frame:
                    int swoosh_frame = 7 + (frame_counter / 2);
                    if (swoosh_frame < 10) {
                        fsrc.x = swoosh_frame * 45;
                        // Shift further forwards for swoosh.
                        if (current_action == GO_RIGHT) fdest.x += 20 * scale;
                        if (current_action == GO_LEFT)  fdest.x -= 20 * scale;
                        SDL_RendererFlip flip = (SDL_RendererFlip)(previous_action == GO_DOWN ? sdl_flip() - 1 : sdl_flip());
                        SDL_RenderCopyEx(renderer, swoosh_tex, &fsrc, &fdest, 0, 0, flip);
                    }
                }
            }
        // =========================== Special Effects For Jumping ============================
            else if (current_action == GO_UP && previous_action != GO_UP) {
                // Shadow frame 6 -----v
                SDL_Rect fsrc = { 45 * 6, 0, 45, 45 };
                SDL_Rect fdest = {
                    (prev_cell_x * 45 * scale), ((17 + prev_cell_y * 45) * scale),
                    (45 * scale), (45 * scale)
                };

                SDL_Rect shadowdest = fdest;
                shadowdest.y -= (10 + frame_counter) * scale;

                SDL_SetTextureAlphaMod(tex, 155 - frame_counter * 9);
                SDL_RenderCopyEx(renderer, tex, &fsrc, &shadowdest, 0, 0, sdl_flip());
                SDL_SetTextureAlphaMod(tex, 255);

                // Swoosh frame:
                int swoosh_frame = 4 + (frame_counter / 2);
                if (swoosh_frame < 6) {
                    fsrc.x = swoosh_frame * 45;
                    // Shift further upwards for swoosh.
                    fdest.y -= 10 * scale;
                    SDL_RenderCopyEx(renderer, swoosh_tex, &fsrc, &fdest, 0, 0, sdl_flip());
                }
            }
        }

        SDL_RenderCopyEx(renderer, tex, &src, &dest, angle, &center, sdl_flip());
        SDL_RenderCopyEx(renderer, sword_tex, &src, &dest, angle, &center, sdl_flip());
        if (devil) SDL_RenderCopyEx(renderer, horns_tex, &src, &dest, angle, &center, sdl_flip());
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

// ======================================= Player Collision ======================================
enum CollisionResult { NO_COLLISION = 0, CLASH, KILL };

// Here we cover all possibilities where p1 and p2 BOTH move into the same spot.
CollisionResult walk_into_eachother(Player* p1, Player* p2, Actions _right, Actions _left, int mod, Player** loser) {
    if (!(p1->cell_x == p2->cell_x && p1->cell_y == p2->cell_y)) return NO_COLLISION;

    if (p1->current_action == _right && p2->current_action == _left) {
        if (p2->previous_action == GO_UP && !(p1->previous_action == GO_UP || p1->previous_action == GO_DOWN)) {
            *loser = p1;
            return KILL;
        }
        else if (p1->previous_action == GO_UP && !(p2->previous_action == GO_UP || p2->previous_action == GO_DOWN)) {
            *loser = p2;
            return KILL;
        }
        else if (p1->previous_action == GO_UP && p2->previous_action == GO_UP) {
            p1->cell_x -= 1 * mod;
            p2->cell_x += 1 * mod;
            return CLASH;
        }
        else if (p1->previous_action == GO_DOWN && !p2->previous_action == GO_DOWN) {
            p2->cell_x += 1 * mod;
            return CLASH;
        }
        else if (p2->previous_action == GO_DOWN && !p1->previous_action == GO_DOWN) {
            p1->cell_x -= 1 * mod;
            return CLASH;
        }
        else if (p2->previous_action == GO_DOWN && p1->previous_action == GO_DOWN) {
            p1->cell_x -= 1 * mod;
            p2->cell_x += 1 * mod;
            return CLASH;
        }
        else if (p2->previous_action == _left && p1->previous_action != _right) {
            p1->cell_x -= 1 * mod;
            return CLASH;
        }
        else if (p1->previous_action == _right && p2->previous_action != _left) {
            p2->cell_x += 1 * mod;
            return CLASH;
        }
        else {
            p1->cell_x -= 1 * mod;
            p2->cell_x += 1 * mod;
            return CLASH;
        }
    }

    return NO_COLLISION;
}

void collide_players(Player* p1, Player* p2, Sounds* sfx) {
    Player* loser = NULL;

    CollisionResult walked = walk_into_eachother(p1, p2, GO_RIGHT, GO_LEFT, 1, &loser);
    if (!walked) walked =  walk_into_eachother(p1, p2, GO_LEFT, GO_RIGHT, -1, &loser);

    if (walked == CLASH) {
        Mix_Chunk* clash;
        if (rand() % 10 > 5) clash = sfx->clash1;
        else clash = sfx->clash2;
        Mix_PlayChannel(5, clash, 0);
    }
    else if (walked == KILL) {
        loser->dead = true;
    }
}

void dummy_ai(Player* player, Player* other_player, void* rawdata) {
    struct DummyAi {
        Uint8 initialized;
        bool go_right;
        int timer;
    };
    DummyAi* data = (DummyAi*) rawdata;
    if (!player->moved) {
        if (data->timer > 0) {
            data->timer -= 1;
        }
        else {
            if (data->go_right) {
                if (player->cell_x >= 6) {
                    data->go_right = false;
                    player->next_action = GO_LEFT;
                }
                else player->next_action = GO_RIGHT;
            }
            else {
                if (player->cell_x <= 1) {
                    data->go_right = true;
                    player->next_action = GO_RIGHT;
                }
                else player->next_action = GO_LEFT;
            }
            data->timer = 20;
        }
    }
}

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
    SDL_Surface* player_swoosh;
    SDL_Surface* dummy_surface;
    SDL_Surface* dummy_swoosh;
    SDL_Surface* temp_surface;
    Textures tex;
    Musics music;
    Sounds sfx;
    bool last_controls[4];
    bool controls[4];
    Cloud clouds[NUM_CLOUDS];
    void(*ai_function)(Player*, Player*, void*) = dummy_ai;
    void* ai_data = calloc(1024, 1);

    // ================== Initialize things ====================
    if (!SDL_Init(SDL_INIT_AUDIO) == -1) { exit(1); }
    if (Mix_Init(MIX_INIT_OGG) & MIX_INIT_OGG != MIX_INIT_OGG) { exit(2); }
    Mix_AllocateChannels(15);

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
    dummy_surface  = IMG_Load("assets/player/player.png");

    temp_surface = IMG_Load("assets/sky.png");
    tex.background = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);

    temp_surface = IMG_Load("assets/mountain.png");
    tex.platform = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);

    temp_surface = IMG_Load("assets/player/sword.png");
    tex.sword = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);

    temp_surface = IMG_Load("assets/player/horns.png");
    tex.horns = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);

    player_swoosh = IMG_Load("assets/player/swoosh.png");
    dummy_swoosh  = IMG_Load("assets/player/swoosh.png");

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
    music.death = Mix_LoadMUS("assets/music/death.ogg");
    Mix_PlayMusic(music.ominous, -1);

    sfx.move   = Mix_LoadWAV("assets/sfx/move.wav");
    sfx.jump   = Mix_LoadWAV("assets/sfx/jump.wav");
    sfx.swipe  = Mix_LoadWAV("assets/sfx/swipe.wav");
    sfx.crouch = Mix_LoadWAV("assets/sfx/crouch.wav");
    sfx.clash1 = Mix_LoadWAV("assets/sfx/clash1.wav");
    sfx.clash2 = Mix_LoadWAV("assets/sfx/clash2.wav");

    // ====================== Initialize Things That Do Stuff ======================
    Player player(renderer, player_surface, player_swoosh, &tex, &sfx, 0);
    Player enemy(renderer, dummy_surface, dummy_swoosh, &tex, &sfx, 0xFF6984AD);
    enemy.cell_x = 6;
    enemy.flipped = true;

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
    // Unfortunately, this control idea is tricky if we don't want the player to move twice
    // every time you press a button once...
    bool moved_from[4];
    memset(moved_from, 0, sizeof(moved_from));
    bool player_just_moved = false;

    bool player_was_dead = false;
    bool enemy_was_dead = false;

    while (true) {
        // ============= Frame Setup =================
        Uint64 frame_start = SDL_GetPerformanceCounter();

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT: exit(0); break;
            }
        }

        // ====================== Input Processing / Control Logic ========================
        controls[UP]    = keys[SDL_SCANCODE_UP]    || keys[SDL_SCANCODE_W];
        controls[DOWN]  = keys[SDL_SCANCODE_DOWN]  || keys[SDL_SCANCODE_S];
        controls[LEFT]  = keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A];
        controls[RIGHT] = keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D];

        if (moved_from[UP] && !controls[UP]) moved_from[UP] = false;
        if (moved_from[DOWN] && !controls[DOWN]) controls[DOWN] = false;
        if (moved_from[LEFT] && !controls[LEFT]) controls[LEFT] = false;
        if (moved_from[RIGHT] && !controls[RIGHT]) controls[RIGHT] = false;

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
        else if (controls[LEFT]  && !moved_from[LEFT])  player.next_action = GO_LEFT;
        else if (controls[UP]    && !moved_from[UP])    player.next_action = GO_UP;
        else if (controls[DOWN]  && !moved_from[DOWN])  player.next_action = GO_DOWN;
        else if (reserve_action != UNSPECIFIED) player.next_action = reserve_action;

        // ========================== Game Logic =====================
        ai_function(&enemy, &player, ai_data);
        player.update();
        enemy.update();
        collide_players(&player, &enemy, &sfx);
        if (player.dead && !player_was_dead) {
            Mix_PlayMusic(music.death, 0);
            player_was_dead = true;
        }
        if (enemy.dead && !enemy_was_dead) {
            OutputDebugString("OMG U KILL THEM\n");
        }

        // ====================== Postmortem Control Logic ==============
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
        for (int i = 0; i < NUM_CLOUDS / 2; i++)
            clouds[i].render(renderer);

        SDL_RenderCopy(renderer, tex.platform, NULL, NULL);

        player.render(renderer);
        enemy.render(renderer);

        for (int i = NUM_CLOUDS / 2; i < NUM_CLOUDS; i++)
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