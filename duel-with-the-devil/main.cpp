#include "RakPeerInterface.h"
#include "MessageIdentifiers.h"
#include "BitStream.h"
#include "RakNetTypes.h"  // MessageID
#include "packetids.h"

#ifdef _WIN32
#include <Windows.h>
#else
#define OutputDebugString(x)
#endif

#include <stdio.h>
#ifdef _WIN32
#include "SDL/SDL.h"
#include "SDL/SDL_image.h"
#include "SDL/SDL_mixer.h"
#else
#include "SDL2/SDL.h"
#include "SDL2/SDL_image.h"
#include "SDL2/SDL_mixer.h"
#endif

// SDL_main doesn't seem to work with VS2015
#undef main

// Oh god a global. Sue me.
int scale = 2;

void ChangeColorTo(SDL_Surface* surface, Uint32 color, SDL_Rect* area) {
    if (surface->format->BytesPerPixel != 4) exit(5);

    int x_start, y_start, width, height;

    if (area) {
        x_start = area->x;
        y_start = area->y;
        width   = area->w;
        height  = area->h;

        if (x_start + width > surface->w)
            width -= x_start + width - surface->w;
        if (y_start + height > surface->w)
            height -= y_start + height - surface->w;
        if (x_start < 0)
            x_start = 0;
        if (y_start < 0)
            y_start = 0;
    }
    else {
        x_start = 0;
        y_start = 0;
        width = surface->w;
        height = surface->h;
    }

    for (int x = x_start; x < x_start + width; x++) for (int y = y_start; y < y_start + height; y++)
    {
        Uint8* pixel_addr = (Uint8*)surface->pixels + y * surface->pitch + x * 4;
        Uint32 pixel = *((Uint32*)pixel_addr);

        if ((pixel >> (8*3)) == 0x000000FF && pixel != 0xFF808080)
            *(Uint32 *)pixel_addr = color;
    }
}
void ChangeColorTo(SDL_Surface* surface, Uint32 color) {
    ChangeColorTo(surface, color, NULL);
}

struct Textures {
    SDL_Texture* background;
    SDL_Texture* platform;
    SDL_Texture* blank;
    SDL_Texture* sword;
    SDL_Texture* horns;
    SDL_Texture* grid_cell;
    SDL_Texture* cloud;
    SDL_Texture* boss_horns;
    SDL_Texture* thanks_for_playing;

    SDL_Texture* connecting_to_server;
    SDL_Texture* press_h_to_host;
    SDL_Texture* awaiting_client;
};

struct Musics {
    Mix_Music* ominous;
    Mix_Music* ominous2;
    Mix_Music* dark;
    Mix_Music* death;
    Mix_Music* boss;
    Mix_Music* gameover;
};

struct Sounds {
    Mix_Chunk* move;
    Mix_Chunk* jump;
    Mix_Chunk* swipe;
    Mix_Chunk* crouch;
    Mix_Chunk* clash1;
    Mix_Chunk* clash2;
    Mix_Chunk* kill;
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
    int idle_speed;
    bool flipped;
    bool moved;
    bool falling;
    bool dead;
    bool devil;
    bool winner;
    bool killer;
    bool action_done;
    bool boss;
    bool remote;
    // Used to log behavior for final battle
    int*** training;

    // The cell_y value of the ground.
    const int ground_level = 2;

    void initialize() {
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
        frame = 2;
        dead  = false;
        winner = false;
        killer = false;
        action_done = false;
        idle_speed = 25;
    }

    // NOTE not even going to try and clean this up in the destructor.
    void initialize_training_data() {
        // Here's the plan:
        // action = training[X_DIST][Y_DIST][ENEMY_ACTION]
        training = (int***)malloc(5 * sizeof(void*)) + 2;
        for (int i = -2; i < 3; i++) {
            training[i] = (int**)malloc(5 * sizeof(void*)) + 2;
            for (int j = -2; j < 3; j++) {
                training[i][j] = (int*)malloc(NONE * sizeof(Actions));
                for (int k = 0; k < NONE; k++) {
                    training[i][j][k] = UNSPECIFIED;
                }
            }
        }
    }

    Actions predict_action_from(Player* other) {
        if (current_action < 0) return UNSPECIFIED;

        int x_dist = other->cell_x - cell_x;
        if (abs(x_dist) > 2) return UNSPECIFIED;

        int y_dist = other->cell_y - cell_y;
        if (abs(y_dist) > 2) return UNSPECIFIED;

        return (Actions)training[x_dist][y_dist][current_action];
    }

    void train_against(Player* other) {
        int x_dist = other->cell_x - cell_x;
        if (abs(x_dist) > 2) return;

        if (x_dist > 0 && next_action == GO_LEFT) return;
        if (x_dist < 0 && next_action == GO_RIGHT) return;

        int y_dist = other->cell_y - cell_y;
        if (abs(y_dist) > 2) return;

        // Just overwrite the previous... Maybe do weights if there's time/incentive.
        training[x_dist][y_dist][other->current_action] = next_action;
    }

    Player(SDL_Renderer* rend, SDL_Surface* surface, SDL_Surface* swoosh_surf, Textures* textures, Sounds* sounds, Uint32 color) {
        if (color != 0) {
            ChangeColorTo(surface, color);
            ChangeColorTo(swoosh_surf, color);
        }
        tex        = SDL_CreateTextureFromSurface(rend, surface);
        swoosh_tex = SDL_CreateTextureFromSurface(rend, swoosh_surf);
        SDL_SetTextureAlphaMod(swoosh_tex, 155);
        sword_tex  = textures->sword;
        horns_tex  = textures->horns;
        sfx        = sounds;
        initialize();
        devil = false;
        boss = false;
        remote = false;
    }

    ~Player() {
        SDL_DestroyTexture(tex);
        SDL_DestroyTexture(swoosh_tex);
    }

    void refresh_texture(SDL_Renderer* rend, SDL_Surface* surface, SDL_Surface* swoosh_surf, Uint32 color) {
        if (color != 0) {
            ChangeColorTo(surface, color);
            ChangeColorTo(swoosh_surf, color);
        }
        SDL_DestroyTexture(tex);
        SDL_DestroyTexture(swoosh_tex);
        tex = SDL_CreateTextureFromSurface(rend, surface);
        swoosh_tex = SDL_CreateTextureFromSurface(rend, swoosh_surf);
    }

    void win() {
        winner = true;
        frame_counter = 0;
        if (killer) frame = 8;
    }

    void die() {
        dead = true;
        frame_counter = 0;
        frame = 7;
    }

    void push_back() {
        cell_x += (flipped ? 1 : -1);
        if (action_timeout <= 15)
            action_timeout = 15;
    }

    void guard() { if(current_action == GO_DOWN) frame = 9; }

    SDL_RendererFlip sdl_flip() {
        return flipped ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    }

    void assign_frame() {
        frame_counter += 1;
        switch (current_action) {
        case NONE:
            if (frame < 2 || frame > 4) frame = 1;
            if (frame_counter % idle_speed == 0) {
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
            // Frame 9 (blocking) gets assigned by collision
            if (frame == 9) break;
            if (cell_y == ground_level && prev_cell_y < ground_level)
                frame = 2;
            else
                frame = 1;
            break;

        case GO_LEFT: case GO_RIGHT:
            // Slash
            if (previous_action == GO_UP) frame = 0;
            // Normal stance
            else frame = 2;
            break;

        default:
            frame = 2;
            if (cell_y == ground_level && prev_cell_y < ground_level)
                frame = 1;
            break;
        }
    }

    void update() {
        if (winner && !killer) { frame_counter += 1; assign_frame(); return; }
        else if (falling || dead || winner) { frame_counter += 1; return; }
        assign_frame();

        action_done = false;
        if (action_timeout > 0) {
            action_timeout -= 1;
            if (action_timeout == 0) {
                moved = false;
                action_done = true;
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
                    }
                    break;

                case GO_UP:
                    if (cell_y == ground_level) {
                        cell_y -= 1;
                        Mix_PlayChannel(2, sfx->jump, 0);
                    }
                    else {
                        cell_y = ground_level;
                    }
                    break;

                case GO_DOWN:
                    if (previous_action == GO_DOWN)
                        frame = 1;
                    else
                        Mix_PlayChannel(3, sfx->crouch, 0);
                    if (cell_y == ground_level)
                    { }
                    else {
                        cell_y = ground_level;
                    }
                    break;

                case GO_LEFT:
                    if (!remote) cell_x -= 1;
                    if (previous_action != GO_DOWN)
                        flipped = true;
                    if (cell_y < ground_level) {
                        Mix_PlayChannel(4, sfx->swipe, 0);
                        cell_y = ground_level;
                    }
                    else
                        Mix_PlayChannel(1, sfx->move, 0);
                    if (cell_x < 1) { }
                    break;

                case GO_RIGHT:
                    if (!remote) cell_x += 1;
                    if (previous_action != GO_DOWN)
                        flipped = false;
                    if (cell_y < ground_level) {
                        cell_y = ground_level;
                        Mix_PlayChannel(4, sfx->swipe, 0);
                    }
                    else
                        Mix_PlayChannel(1, sfx->move, 0);
                    if (cell_x > 5) {}
                    break;

                default:
                    break;
                }

                if ((cell_x <= 0 || cell_x >= 7) && cell_y == ground_level && previous_action != GO_DOWN) {
                    if (!remote) {
                        frame = 7;
                        frame_counter = 0;
                        falling = true;
                        if (cell_x <= 0) flipped = false;
                        else flipped = true;
                        dead = true;
                    }
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

        if (winner) {
            if (killer) {
                // Sword swoosh for killing strike
                SDL_Rect fsrc = { 0, 0, 45, 45 };
                SDL_Rect fdest = {
                    (cell_x * 45 * scale), ((17 + cell_y * 45) * scale),
                    (45 * scale), (45 * scale)
                };

                int swoosh_frame = 7 + (frame_counter / 5);
                if (swoosh_frame < 11) {
                    fsrc.x = swoosh_frame * 45;
                    if (current_action == GO_RIGHT) fdest.x += 50 * scale;
                    if (current_action == GO_LEFT)  fdest.x -= 50 * scale;
                    SDL_RenderCopyEx(renderer, swoosh_tex, &fsrc, &fdest, 0, 0, sdl_flip());
                }
            }
        }
        else if (falling) {
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
                if (devil || boss) {
                    SDL_SetTextureAlphaMod(horns_tex, 155 - frame_counter * 9);
                    SDL_RenderCopyEx(renderer, horns_tex, &fsrc, &shadowdest, angle, &center, sdl_flip());
                    SDL_SetTextureAlphaMod(horns_tex, 255);
                }
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
                    int swoosh_frame = 11 + (frame_counter / 2);
                    if (swoosh_frame < 14) {
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
                if (devil || boss) {
                    SDL_SetTextureAlphaMod(horns_tex, 155 - frame_counter * 9);
                    SDL_RenderCopyEx(renderer, horns_tex, &fsrc, &shadowdest, angle, &center, sdl_flip());
                    SDL_SetTextureAlphaMod(horns_tex, 255);
                }
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
        if (!boss) SDL_RenderCopyEx(renderer, sword_tex, &src, &dest, angle, &center, sdl_flip());
        if (devil || boss) SDL_RenderCopyEx(renderer, horns_tex, &src, &dest, angle, &center, sdl_flip());
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
// This is kinda sloppy. I'll do a better job next time. :[
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
        else if (p1->previous_action == GO_DOWN && p2->previous_action != GO_DOWN) {
            p2->cell_x += 1 * mod;
            return CLASH;
        }
        else if (p2->previous_action == GO_DOWN && p1->previous_action != GO_DOWN) {
            p1->cell_x -= 1 * mod;
            return CLASH;
        }
        else if (p2->previous_action == GO_DOWN && p1->previous_action == GO_DOWN) {
            p1->cell_x -= 1 * mod;
            p2->cell_x += 1 * mod;
            return CLASH;
        }
        else if (p2->previous_action == _left && p1->previous_action == GO_DOWN) {
            p2->cell_x += 1 * mod;
            return CLASH;
        }
        else if (p1->previous_action == _right && p2->previous_action == GO_DOWN) {
            p1->cell_x -= 1 * mod;
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

// Catches all cases where p1 walks into (or lands on) p2 while p2 is stationary
CollisionResult walk_into_another(Player* p1, Player* p2, Player** loser) {
    if (!(p1->cell_x == p2->cell_x && p1->cell_y == p2->cell_y)) return NO_COLLISION;
    if (p2->current_action == GO_LEFT || p2->current_action == GO_RIGHT) return NO_COLLISION;

    if (p2->current_action == GO_DOWN) {
        if ((p2->flipped && p1->current_action == GO_LEFT) || (!p2->flipped && p1->current_action == GO_RIGHT)) {
            *loser = p2;
            return KILL;
        }
        else {
            p2->guard();
            if (p1->previous_action == GO_UP)
                p2->push_back();
            p1->push_back();
            return CLASH;
        }
    }
    else if (p1->current_action == NONE) {
        // I think this means p1 fell onto p2
        // No clash - this does weird things in multiplayer sometimes
        // p1->push_back();
        return CLASH;
    }
    else if (p1->current_action == GO_DOWN) {
        // So this means p1 crushed p2 I guess?
        *loser = p2;
        return KILL;
    }
    else {
        *loser = p2;
        return KILL;
    }
}

int collide_players(Player* p1, Player* p2, Sounds* sfx) {
    Player* loser = NULL;

    CollisionResult collision = walk_into_eachother(p1, p2, GO_RIGHT, GO_LEFT, 1, &loser);
    if (!collision) collision = walk_into_eachother(p1, p2, GO_LEFT, GO_RIGHT, -1, &loser);
    if (!collision) collision = walk_into_another(p1, p2, &loser);
    if (!collision) collision = walk_into_another(p2, p1, &loser);

    if (collision == CLASH) {
        Mix_Chunk* clash;
        if (rand() % 10 > 5) clash = sfx->clash1;
        else clash = sfx->clash2;
        Mix_PlayChannel(5, clash, 0);
    }
    else if (collision == KILL) {
        // makes more sense to do this elsewhere for online play
        /*
        loser->die();
        loser->push_back();
        if (loser == p1) p2->killer = true;
        else             p1->killer = true;
        */
        loser->push_back();
        return loser == p1 ? 1 : 2;
    }
    return 0;
}

// ============================ Enemy AI Functions ==========================

void dummy_ai(Player* player, Player* other_player, void* rawdata) {
}

// ========= Patrol: Just walk back and forth across the stage ==============
void patrol_ai(Player* player, Player* other_player, void* rawdata) {
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
            data->timer = 29;
        }
    }
}

// ======================= Berserk: Jump slash left and right repeatedly. ===============
void berserk_ai(Player* player, Player* other_player, void* rawdata) {
    struct ConfusedAi {
        Uint8 initialized;
        int step;
        int timer;
    };
    ConfusedAi* data = (ConfusedAi*)rawdata;

    if (!data->initialized) {
        data->initialized = true;
        player->next_action = GO_LEFT;
        data->timer = 20;
    }

    if (data->timer > 0) {
        data->timer -= 1;
    }
    else if (!player->moved && abs(player->cell_x - other_player->cell_x) == 1) {
        player->next_action = GO_DOWN;
    }
    else {
        switch (data->step) {
        case 0:
            player->next_action = GO_LEFT;
            data->step = 1;
            data->timer = 35;
            break;

        case 1:
            player->next_action = GO_UP;
            data->step = 2;
            data->timer = 2;
            break;

        case 2:
            player->next_action = GO_RIGHT;
            data->step = 3;
            data->timer = 50;
            break;

        case 3:
            player->next_action = GO_UP;
            data->step = 4;
            data->timer = 2;
            break;

        case 4:
            player->next_action = GO_LEFT;
            data->step = 1;
            data->timer += 50;
            break;
        }
    }
}

// ===================== Sentinel: Hold down and move forwards when close to edge =======================
void sentinel_ai(Player* player, Player* other_player, void* rawdata) {
    struct SentinelAi {
        Uint8 initialized;
        int timer;
        bool slam_dunk;
    };
    SentinelAi* data = (SentinelAi*)rawdata;

    if (data->timer > 0)
        data->timer -= 1;
    else {
        if (player->cell_x >= 6) {
            if (data->slam_dunk) {
                player->next_action = GO_LEFT;
                data->timer = 30;
                data->slam_dunk = false;
            }
            else {
                player->next_action = GO_UP;
                data->timer = 14;
                data->slam_dunk = true;
            }
        }
        else if (player->cell_x <= 1) {
            if (data->slam_dunk) {
                player->next_action = GO_RIGHT;
                data->timer = 30;
                data->slam_dunk = false;
            }
            else {
                player->next_action = GO_UP;
                data->timer = 14;
                data->slam_dunk = true;
            }
        }
        else {
            if (abs(other_player->cell_x - player->cell_x) <= 1) {
                if (rand() % 100 > 95) {
                    if (player->cell_x < other_player->cell_x)
                        player->next_action = GO_RIGHT;
                    else
                        player->next_action = GO_LEFT;
                    data->timer = 20;
                }
                else
                    player->next_action = GO_DOWN;
            }
            else
                player->next_action = UNSPECIFIED;
        }
    }
}

// ============================ Slickster: Abuses sliding off the ledge and jumping back ==============
void slickster_ai(Player* player, Player* other_player, void* rawdata) {
    struct SlicksterAi {
        Uint8 initialized;
        int timer;
        int slick_step;
        int move_step;
    };
    SlicksterAi* data = (SlicksterAi*)rawdata;

    if (data->timer > 0) {
        data->timer -= 1;
        return;
    }

    if (abs(other_player->cell_x - player->cell_x) >= 2) {
        switch (data->slick_step) {
        case 0:
            player->next_action = GO_DOWN;
            data->timer = 14;
            data->slick_step += 1;
            break;

        case 1:
            player->next_action = GO_RIGHT;
            data->timer = 14;
            data->slick_step += 1;
            break;

        case 2:
            player->next_action = GO_UP;
            data->timer = 14;
            data->slick_step += 1;
            break;

        case 3:
            player->next_action = GO_LEFT;
            data->timer = 14;
            data->slick_step = 0;
            break;

        default:
            data->slick_step = 0;
            break;
        }
    }
    else {
        switch (data->move_step) {
        case 0:
            if (rand() % 10 >= 5)
                player->next_action = GO_UP;
            else
                player->next_action = GO_DOWN;
            data->timer = 14;
            data->move_step += 1;
            break;

        case 1:
            if (player->cell_x > other_player->cell_x)
                player->next_action = GO_LEFT;
            else
                player->next_action = GO_RIGHT;
            data->timer = 50;
            data->move_step += 1;

        case 2:
            if (player->cell_x > other_player->cell_x)
                player->next_action = GO_LEFT;
            else
                player->next_action = GO_RIGHT;
            data->timer = 35;
            data->move_step = 0;

        default:
            data->move_step = 0;
            break;
        }
    }
}

// ========================= Overvant: Does opposite of player character =============
void observant_ai(Player* player, Player* other_player, void* rawdata) {
    struct ObservantAi {
        Uint8 initialized;
        int timer;
        Actions action;
        bool realign;
    };
    ObservantAi* data = (ObservantAi*)rawdata;

    if (data->timer > 0) {
        data->timer -= 1;
        if (data->timer == 0 && !data->realign)
            player->next_action = data->action;
        return;
    }

    if ((other_player->cell_x - player->cell_x) % 2 == 0) {
        player->next_action = GO_DOWN;
        data->realign = true;
        data->timer = 14;
    }
    else if (data->realign) {
        if (player->cell_x > other_player->cell_x) player->next_action = GO_LEFT;
        else player->next_action = GO_RIGHT;
        data->realign = false;
        data->timer = 14;
    }
    else {
        if (other_player->next_action == GO_LEFT)
            data->action = GO_RIGHT;
        else if (other_player->next_action == GO_RIGHT)
            data->action = GO_LEFT;
        else
            data->action = other_player->next_action;

        if (player->cell_x <= 1 && player->next_action == GO_LEFT)
            data->action = UNSPECIFIED;
        else if (player->cell_x >= 6 && player->next_action == GO_RIGHT)
            data->action = UNSPECIFIED;

        if (!(data->action == UNSPECIFIED || data->action == NONE))
            data->timer = 15;
    }
}

// ========================= Sneaky: Tries to jump past the player =================
void sneaky_ai(Player* player, Player* other_player, void* rawdata) {
    struct SneakyAi {
        Uint8 initialized;
        int timer;
        bool get_at_em;
        bool finish_them;
    };
    SneakyAi* data = (SneakyAi*)rawdata;

    if (data->timer > 0) {
        data->timer -= 1;
        return;
    }

    if (data->get_at_em) {
        if (player->cell_x > other_player->cell_x) player->next_action = GO_LEFT;
        else player->next_action = GO_RIGHT;
        data->timer = 25;
        data->get_at_em = false;
        data->finish_them = true;
    }
    else if (data->finish_them) {
        if (player->cell_x > other_player->cell_x) player->next_action = GO_LEFT;
        else player->next_action = GO_RIGHT;
        data->timer = 48;
        data->finish_them = false;
    }
    else if (abs(player->cell_x - other_player->cell_x) % 2 == 0) {
        if (player->cell_x > other_player->cell_x) player->next_action = GO_LEFT;
        else player->next_action = GO_RIGHT;
    }
    else if (abs(player->cell_x - other_player->cell_x) == 1) {
        player->next_action = GO_UP;
        data->timer = 13;
        data->get_at_em = true;
    }
    else {
        if (rand() % 10 > 5)
            player->next_action = GO_UP;
        else if (rand() % 10 > 5)
            player->next_action = GO_DOWN;
        else {
            if (player->cell_x > other_player->cell_x) player->next_action = GO_LEFT;
            else player->next_action = GO_RIGHT;
        }
        data->timer = 60;
    }
}

// ========================= Neural: uses strategies influenced by the player =================
void neural_ai(Player* player, Player* other_player, void* rawdata) {
    struct NNAi {
        Uint8 initialized;
        int timer;
    };
    NNAi* data = (NNAi*)rawdata;

    if (data->timer % 15 == 0) {
        if (abs(other_player->cell_x - player->cell_x) <= 1) {
            data->timer = 0;
        }
    }

    if (data->timer > 0) {
        data->timer -= 1;
        return;
    }

    if (abs(other_player->cell_x - player->cell_x) <= 2) {
        player->next_action = other_player->predict_action_from(player);
        if (player->next_action == UNSPECIFIED) {
            if (player->current_action != GO_DOWN && rand() % 10 > 5)
                player->next_action = GO_DOWN;
            else {
                if (player->cell_x < other_player->cell_x)
                    player->next_action = GO_RIGHT;
                else
                    player->next_action = GO_LEFT;
            }
        }
        else {
            if (player->cell_x <= 1 && player->next_action == GO_LEFT)
                player->next_action = GO_RIGHT;
            else if (player->cell_x >= 6 && player->next_action == GO_RIGHT)
                player->next_action = GO_LEFT;
        }
        data->timer = 14;
    }
    else {
        if (player->current_action != GO_UP && rand() % 10 > 6) {
            player->next_action = GO_UP;
            data->timer = 14;
        }
        else {
            if (player->cell_x < other_player->cell_x)
                player->next_action = GO_RIGHT;
            else
                player->next_action = GO_LEFT;
            data->timer = 60;
        }
    }
}

// ======================= Boss: This is it!!! =========================
void boss_ai(Player* player, Player* other_player, void* rawdata) {
    struct BossAi {
        Uint8 initialized;
        int timer;
    };
    BossAi* data = (BossAi*)rawdata;

    if (data->timer % 3 == 0) {
        if (abs(other_player->cell_x - player->cell_x) <= 1) {
            data->timer = 0;
        }
    }

    if (data->timer > 0) {
        data->timer -= 1;
        return;
    }

    int dist = abs(other_player->cell_x - player->cell_x);
    if (dist <= 2) {
        if (dist == 1 && (other_player->next_action == GO_LEFT || other_player->next_action == GO_RIGHT)) {
            if (other_player->previous_action == GO_UP)
                    if (player->cell_x < other_player->cell_x)
                        player->next_action = GO_RIGHT;
                    else
                        player->next_action = GO_LEFT;
            else
                player->next_action = GO_DOWN;
            data->timer = 6;
        }
        else if (other_player->next_action == GO_UP) {
            player->next_action = GO_UP;
        }
        else {
            player->next_action = other_player->predict_action_from(player);
            if (player->next_action == UNSPECIFIED) {
                if (player->current_action != GO_DOWN && rand() % 10 > 5)
                    player->next_action = GO_DOWN;
                else {
                    if (player->cell_x < other_player->cell_x)
                        player->next_action = GO_RIGHT;
                    else
                        player->next_action = GO_LEFT;
                }
            }
            else {
                if (player->cell_x <= 1 && player->next_action == GO_LEFT)
                    player->next_action = GO_RIGHT;
                else if (player->cell_x >= 6 && player->next_action == GO_RIGHT)
                    player->next_action = GO_LEFT;
            }
            data->timer = 6;
        }
    }
    else {
        if (player->current_action != GO_UP && rand() % 10 > 3) {
            player->next_action = GO_UP;
            data->timer = 14;
        }
        else {
            if (player->cell_x < other_player->cell_x)
                player->next_action = GO_RIGHT;
            else
                player->next_action = GO_LEFT;
            data->timer = rand() % 10 > 7 ? 14 : 60;
        }
    }
}

#define keyreleased(ctl) (controls[ctl] && !last_controls[ctl])
#define keypressed(ctl) (!controls[ctl] && last_controls[ctl])
#define NUM_CLOUDS 15

#define HANDLE_PACKET for (packet = peer->Receive(); packet; peer->DeallocatePacket(packet), packet = peer->Receive()) switch (packet->data[0])
#define IS_HOSTING (mp_color != 0)

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
    SDL_Surface* enemy_surface;
    SDL_Surface* enemy_swoosh;
    SDL_Surface* temp_surface;
    Textures tex;
    Musics music;
    Sounds sfx;
    bool last_controls[4];
    bool controls[4];
    Cloud clouds[NUM_CLOUDS];
    void(*stages[])(Player*, Player*, void*) = {
        dummy_ai, patrol_ai, berserk_ai, slickster_ai, sneaky_ai,
        observant_ai, sentinel_ai, neural_ai
    };
    int stage = 0;
    void(*ai_function)(Player*, Player*, void*) = stages[stage];
    void* ai_data = calloc(128, 1);

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
    enemy_surface  = IMG_Load("assets/player/player.png");

    temp_surface = IMG_Load("assets/sky.png");
    tex.background = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);

    temp_surface = IMG_Load("assets/mountain.png");
    tex.platform = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);

    temp_surface = IMG_Load("assets/blank.png");
    tex.blank = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);

    temp_surface = IMG_Load("assets/player/sword.png");
    tex.sword = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);

    temp_surface = IMG_Load("assets/player/horns.png");
    tex.horns = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);

    temp_surface = IMG_Load("assets/player/bosshorns.png");
    tex.boss_horns = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);

    temp_surface = IMG_Load("assets/thanksforplaying.png");
    tex.thanks_for_playing = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);

    temp_surface = IMG_Load("assets/msg/connecting-to-server.png");
    tex.connecting_to_server = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);

    temp_surface = IMG_Load("assets/msg/h-to-host.png");
    tex.press_h_to_host = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);

    temp_surface = IMG_Load("assets/msg/awaiting-client.png");
    tex.awaiting_client = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);

    player_swoosh = IMG_Load("assets/player/swoosh.png");
    enemy_swoosh  = IMG_Load("assets/player/swoosh.png");

    temp_surface = IMG_Load("assets/clouds.png");
    tex.cloud = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_SetTextureAlphaMod(tex.cloud, 155);
    SDL_FreeSurface(temp_surface);

    if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 2048) == -1)
        exit(3);
    music.ominous  = Mix_LoadMUS("assets/music/ominous.ogg");
    music.ominous2 = Mix_LoadMUS("assets/music/ominous2.ogg");
    music.dark     = Mix_LoadMUS("assets/music/dark.ogg");
    music.death    = Mix_LoadMUS("assets/music/death.ogg");
    music.boss     = Mix_LoadMUS("assets/music/finalboss.ogg");
    music.gameover = Mix_LoadMUS("assets/music/gameover.ogg");
    Mix_PlayMusic(music.ominous, -1);

    sfx.move   = Mix_LoadWAV("assets/sfx/move.wav");
    sfx.jump   = Mix_LoadWAV("assets/sfx/jump.wav");
    sfx.swipe  = Mix_LoadWAV("assets/sfx/swipe.wav");
    sfx.crouch = Mix_LoadWAV("assets/sfx/crouch.wav");
    sfx.clash1 = Mix_LoadWAV("assets/sfx/clash1.wav");
    sfx.clash2 = Mix_LoadWAV("assets/sfx/clash2.wav");
    sfx.kill   = Mix_LoadWAV("assets/sfx/kill.wav");

    // ====================== Initialize Things That Do Stuff ======================
    Player player(renderer, player_surface, player_swoosh, &tex, &sfx, 0);
    // player.initialize_training_data();
    Player enemy(renderer, enemy_surface, enemy_swoosh, &tex, &sfx, 0xFF6984AD);
    enemy.remote = true;
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

    // Stuff for managing scene transitions
    bool player_was_dead = false;
    bool enemy_was_dead = false;
    int fade_timeout = 0;
    bool fading_in_blank = false;
    bool fading_out_blank = false;
    int fade_alpha = 0;

    Mix_Music* fade_music_to = NULL;
    Mix_Music* current_music = music.ominous;

    // Man this is getting insane
    bool played_death_music = false;
    bool boss_initialized = false;
    bool player_wins = false;

    Uint32 mp_color = 0;
    RakNet::SystemIndex mp_index = -1;
    struct { RakNet::SystemIndex index; Uint32 color; } mp_hosts[10];
    SDL_Surface* host_list_surf = NULL;
    SDL_Texture* host_list_tex = NULL;
    int host_list_timeout = 0;
    Uint32 num_hosts;

    bool started_multiplayer = false;
    bool make_player_boss = false;
    bool make_enemy_boss = false;

    // Hell yeah let's add more flags
    enum {
        NOT_CONNECTED, CONNECTING, CONNECTED, AWAITING_CLIENT, JOINING_GAME, PLAYING
    } multiplayer_status = NOT_CONNECTED;

    RakNet::RakPeerInterface* peer = RakNet::RakPeerInterface::GetInstance();
    RakNet::Packet* packet;
    RakNet::SystemAddress server_addr("45.63.16.189", 8081);

    while (true) {
        // ============= Frame Setup =================
        Uint64 frame_start = SDL_GetPerformanceCounter();

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                RakNet::RakPeerInterface::DestroyInstance(peer);
                exit(0);
                break;
            }
        }

        // ====================== Input Processing / Control Logic ========================
        controls[UP]    = keys[SDL_SCANCODE_UP]    || keys[SDL_SCANCODE_W];
        controls[DOWN]  = keys[SDL_SCANCODE_DOWN]  || keys[SDL_SCANCODE_S];
        controls[LEFT]  = keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A];
        controls[RIGHT] = keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D];

        if (multiplayer_status != PLAYING) {
            // ====================== Multiplayer setup ========================
            SDL_RenderClear(renderer);

            SDL_RenderCopy(renderer, tex.background, NULL, NULL);
            for (int i = 0; i < NUM_CLOUDS / 2; i++)
                clouds[i].render(renderer);

            SDL_RenderCopy(renderer, tex.platform, NULL, NULL);

            for (int i = NUM_CLOUDS / 2; i < NUM_CLOUDS; i++)
                clouds[i].render(renderer);

            switch (multiplayer_status) {
            case NOT_CONNECTED:
                // Draw "connecting to server" message
                SDL_RenderCopy(renderer, tex.connecting_to_server, NULL, NULL);
                // Start connecting or some shit.
                {
                    RakNet::SocketDescriptor sd;
                    peer->Startup(1, &sd, 1);
                }
                printf("Starting the client.\n");
                peer->Connect("45.63.16.189", 8081, 0,0);

                multiplayer_status = CONNECTING;
                break;

            case CONNECTING:
                // Draw "connecting to server" message
                SDL_RenderCopy(renderer, tex.connecting_to_server, NULL, NULL);

                HANDLE_PACKET {
                case ID_CONNECTION_REQUEST_ACCEPTED:
                    OutputDebugString("Our connection request has been accepted.\n");
                    multiplayer_status = CONNECTED;
                    server_addr = packet->systemAddress;
                    break;                    

                default:
                    OutputDebugString("UNABLE TO CONNECT\n");
                    peer->Connect("45.63.16.189", 8081, 0,0);
                    break;
                }

                break;

            case CONNECTED: {
                // Draw "press h to host" message
                SDL_RenderCopy(renderer, tex.press_h_to_host, NULL, NULL);

                // Ask for client list while we're at it.
                if (host_list_timeout <= 0) {
                    RakNet::BitStream color_req;
                    color_req.Write((RakNet::MessageID)ID_QUERY_HOSTS);
                    peer->Send(&color_req, HIGH_PRIORITY, RELIABLE_ORDERED, 0, server_addr, false);

                    host_list_timeout = 60 * 5;
                }
                else
                    host_list_timeout -= 1;

                // Check if user presses keys 1~0 and if there is a corrosponding player hosting
                for (int k = 0; k < 10; k++) {
                    int scancode = k + SDL_SCANCODE_1;
                    if (keys[scancode] && k < num_hosts) {

                        RakNet::BitStream join_req;
                        join_req.Write((RakNet::MessageID)ID_JOIN_GAME);
                        join_req.Write(mp_hosts[k].index);
                        peer->Send(&join_req, HIGH_PRIORITY, RELIABLE_ORDERED, 0, server_addr, false);

                        multiplayer_status = JOINING_GAME;
                        enemy.refresh_texture(renderer, enemy_surface, enemy_swoosh, mp_hosts[k].color);
                        player.cell_x = 1;
                        player.flipped = false;
                        player.idle_speed = 50;

                        goto joined_game;
                    }
                }

                // Host on 'h' press lol
                if (keys[SDL_SCANCODE_H]) {
                    RakNet::BitStream color_req;
                    color_req.Write((RakNet::MessageID)ID_NEW_HOST);
                    peer->Send(&color_req, HIGH_PRIORITY, RELIABLE_ORDERED, 0, server_addr, false);

                    player.cell_x = 6;
                    player.flipped = true;
                    enemy.cell_x = 1;
                    enemy.flipped = false;
                    enemy.refresh_texture(renderer, enemy_surface, enemy_swoosh, 0xFF000000);

                    multiplayer_status = AWAITING_CLIENT;
                }
                else {
                    // Grab any host list updates
                    HANDLE_PACKET { case ID_HOST_LIST: {
                        RakNet::BitStream resp(packet->data, packet->length, false);
                        resp.IgnoreBytes(sizeof(RakNet::MessageID));

                        if (!resp.Read((char*)&num_hosts, sizeof(Uint32))) {
                            OutputDebugString("FUCKED UP!!!"); break;
                        }
                        num_hosts = htonl(num_hosts);

                        if (num_hosts == 0) {
                            OutputDebugString("No hosts");
                            break;
                        }

                        SDL_Rect host_frame = {45 * 3, 0, 45, 45};
                        SDL_Rect host_spot = {5, 15, 45, 45};
                        if (host_list_surf != NULL) SDL_FreeSurface(host_list_surf);
                        host_list_surf = IMG_Load("assets/empty.png");

                        for (int i = 0; i < min(num_hosts, 10); i++) {
                            if (
                                !resp.Read((char*)&mp_hosts[i].index, sizeof(RakNet::SystemIndex))
                                ||
                                !resp.Read((char*)&mp_hosts[i].color, sizeof(Uint32))
                            ) {
                                OutputDebugString("FUCKED UP!!!");
                                SDL_FreeSurface(host_list_surf);
                                host_list_surf = NULL;
                                break;
                            }
                            mp_hosts[i].index = htons(mp_hosts[i].index);
                            mp_hosts[i].color = htonl(mp_hosts[i].color);

                            SDL_BlitSurface(player_surface, &host_frame, host_list_surf, &host_spot);
                            ChangeColorTo(host_list_surf, mp_hosts[i].color, &host_spot);

                            if (i == 4) {
                                host_spot.y = 15;
                                host_spot.x += 45;
                            }
                            else
                                host_spot.y += 45;
                        }

                        // Initialize or update host list texture
                        if (host_list_surf != NULL) {
                            if (host_list_tex == NULL)
                                host_list_tex = SDL_CreateTextureFromSurface(renderer, host_list_surf);
                            else
                                SDL_UpdateTexture(host_list_tex, NULL, host_list_surf->pixels, host_list_surf->pitch);
                        }
                    }}

                    // Draw hosts
                    if (host_list_tex != NULL)
                        SDL_RenderCopy(renderer, host_list_tex, NULL, NULL);
                }
            } break;

            case AWAITING_CLIENT: case JOINING_GAME:
            joined_game:
                // "awaiting client" message (if necessary)
                if (multiplayer_status == AWAITING_CLIENT)
                    SDL_RenderCopy(renderer, tex.awaiting_client, NULL, NULL);

                player.current_action = NONE;
                player.assign_frame();
                player.render(renderer);

                HANDLE_PACKET {
                case ID_REMOTE_DISCONNECTION_NOTIFICATION:
                    OutputDebugString("Another client has disconnected.\n");
                    break;
                case ID_REMOTE_CONNECTION_LOST:
                    OutputDebugString("Another client has lost the connection.\n");
                    break;

                case ID_HOST_STILL_GOOD: {
                    // Confirm we're still alive and interested in playing.
                    RakNet::BitStream ready;
                    ready.Write((RakNet::MessageID)ID_READY_TO_ROLL);
                    peer->Send(&ready, HIGH_PRIORITY, RELIABLE_ORDERED, 0, server_addr, false);
                } break;

                case ID_READY_TO_ROLL:
                    multiplayer_status = PLAYING;
                    fading_in_blank = true;
                    break;

                case ID_ASSIGNED_COLOR: {
                    RakNet::BitStream in(packet->data, packet->length, false);
                    in.IgnoreBytes(sizeof(RakNet::MessageID));

                    char msg[50];
                    if (
                        in.Read((char*)&mp_color, sizeof(Uint32))
                        &&
                        in.Read((char*)&mp_index, sizeof(mp_index))
                    ) {
                        mp_color = ntohl(mp_color);
                        sprintf(msg, "GOT COLOR %x AND INDEX %i", mp_color, mp_index);
                        OutputDebugString(msg);

                        player.refresh_texture(renderer, player_surface, player_swoosh, mp_color);
                        player.idle_speed = 50;
                    }
                    else {
                        sprintf(msg, "WTF HAPPEN ? Unread bits: %i\n", in.GetNumberOfUnreadBits());
                        OutputDebugString(msg);
                    }
                } break;

                case ID_DISCONNECTION_NOTIFICATION:
                    OutputDebugString("We have been disconnected.\n");
                    break;
                case ID_CONNECTION_LOST:
                    OutputDebugString("Connection lost.\n");
                    break;
                default:
                    OutputDebugString("Message with unknown identifier has arrived.\n");
                    break;
                }

                break;

            default: break;
            }

            SDL_RenderPresent(renderer);
        }
        else {
            // =========================================== Actual In-game logic ====================================

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

            if (controls[RIGHT] && !moved_from[RIGHT]) player.next_action = GO_RIGHT;
            else if (controls[LEFT] && !moved_from[LEFT])  player.next_action = GO_LEFT;
            else if (controls[UP] && !moved_from[UP])    player.next_action = GO_UP;
            else if (controls[DOWN] && !moved_from[DOWN])  player.next_action = GO_DOWN;
            else if (reserve_action != UNSPECIFIED) player.next_action = reserve_action;

            // if (!enemy.devil && player.action_timeout == 1) player.train_against(&enemy);

            // ========================== Game Logic =====================
            Actions enemy_next_action = UNSPECIFIED;
            int enemy_cell_x = -1;
            int enemy_cell_y = -1;
            bool player_actually_died = false;
            bool enemy_actually_died = false;

            HANDLE_PACKET {
            case ID_RECEIVE_INPUT: {
                RakNet::BitStream data(packet->data, packet->length, false);
                data.IgnoreBytes(sizeof(RakNet::MessageID));

                RakNet::uint24_t action, cell_x, cell_y;
                data.Read(action);
                data.Read(cell_x);
                data.Read(cell_y);

                enemy_next_action = (Actions)(uint32_t)action;
                enemy_cell_x = cell_x;
                enemy_cell_y = cell_y;
            } break;

            case ID_KILL_HAPPENED: {
                OutputDebugString("kill happen----------\n");

                RakNet::BitStream data(packet->data, packet->length, false);
                data.IgnoreBytes(sizeof(RakNet::MessageID));

                bool player_died = data.ReadBit();
                bool boss_mode = data.ReadBit();

                if (player_died) {
                    if (!player.dead) {
                        Mix_PlayChannel(7, sfx.kill, 0);
                        if (!enemy.boss) {
                            Mix_PlayMusic(music.death, 0);
                            played_death_music = true;
                        }

                        player.die();
                        player_was_dead = true;
                        enemy.killer = true;
                        enemy.win();
                    }
                    if (boss_mode && !(player.boss || enemy.boss)) make_enemy_boss = true;
                }
                else {
                    if (!enemy.dead) {
                        Mix_PlayChannel(7, sfx.kill, 0);

                        enemy.die();
                        enemy_was_dead = true;
                        player.killer = true;
                        player.win();
                    }
                    if (boss_mode && !(player.boss || enemy.boss)) make_player_boss = true;
                }

                fade_timeout = 60;
            } break;
            }

            if (!fading_out_blank) {
                if (enemy_next_action != UNSPECIFIED)
                    enemy.next_action = enemy_next_action;

                player.update();
                enemy.update();

                if (enemy_cell_x >= 0) enemy.cell_x = enemy_cell_x;

                int kill = collide_players(&player, &enemy, &sfx);

                bool player_died_this_frame = false;
                if (player.dead && !player_was_dead) {
                    Mix_PlayChannel(7, sfx.kill, 0);
                    /*
                    if (!enemy.boss) {
                        Mix_PlayMusic(music.death, 0);
                        played_death_music = true;
                    }
                    */
                    enemy.win();
                    player_was_dead = true;
                    player_died_this_frame = true;

                    // fade_timeout = 60;
                }
                bool enemy_died_this_frame = false;
                if (enemy.dead && !enemy_was_dead) {
                    Mix_PlayChannel(7, sfx.kill, 0);
                    player.win();
                    enemy_was_dead = true;
                    enemy_died_this_frame = true;

                    // fade_timeout = 60;
                }

                if (fade_timeout <= 0 && (kill != 0 || (player_died_this_frame || enemy_died_this_frame) || player.moved && player.frame_counter == 0)) {
                    // SEND PACKET
                    RakNet::BitStream req;
                    req.Write((RakNet::MessageID)ID_SEND_INPUT);
                    // Killed by another player flags
                    kill == 1 ? req.Write1() : req.Write0();
                    kill == 2 ? req.Write1() : req.Write0();

                    // Suicide flags
                    player_died_this_frame ? req.Write1() : req.Write0();
                    enemy_died_this_frame  ? req.Write1() : req.Write0();

                    if (player.moved && player.frame_counter == 0) {
                        req.Write1();
                        req.Write((RakNet::uint24_t)player.current_action);
                        req.Write((RakNet::uint24_t)player.cell_x);
                        req.Write((RakNet::uint24_t)player.cell_y);
                    }
                    else
                        req.Write0();

                    peer->Send(&req, HIGH_PRIORITY, RELIABLE_ORDERED, 0, server_addr, false);
                }
            }
            if (fade_timeout > 0) {
                fade_timeout -= 1;
                if (fade_timeout == 0) fading_in_blank = true;
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
                // TODO uh shit this might've been a bug. uncomment to fix bug???
                // player_just_moved = false;
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

            if (fading_in_blank) {
                fade_alpha += 2;
                if (fade_alpha > 255) {
                    SDL_RenderCopy(renderer, tex.blank, NULL, NULL);

                    // Keep incrementing alpha just so we don't have to hack in another timer lol...
                    if (fade_alpha >= 260) {
                        // Re-initialize shit
                        reserve_action = UNSPECIFIED;
                        bool plr_was_boss = player.boss;
                        bool enm_was_boss = enemy.boss;

                        player.initialize();
                        player.boss = plr_was_boss;
                        enemy.initialize();
                        enemy.boss = enm_was_boss;

                        if (IS_HOSTING) {
                            player.cell_x = 6;
                            player.flipped = true;
                            enemy.cell_x = 1;
                            enemy.flipped = false;
                        }
                        else {
                            enemy.cell_x = 6;
                            enemy.flipped = true;
                            player.cell_x = 1;
                            player.flipped = false;
                        }

                        if (!started_multiplayer) {
                            // Intense music
                            fade_music_to = music.ominous2;

                            // Red sky
                            SDL_DestroyTexture(tex.background);
                            temp_surface = IMG_Load("assets/redsky.png");
                            tex.background = SDL_CreateTextureFromSurface(renderer, temp_surface);
                            SDL_FreeSurface(temp_surface);

                            started_multiplayer = true;
                        }

                        enemy.devil = player_was_dead || plr_was_boss;
                        player.devil = enemy_was_dead || enm_was_boss;
                        player_was_dead = false;
                        enemy_was_dead = false;

                        fade_alpha = 255;
                        fading_in_blank = false;
                        fading_out_blank = true;

                        if (played_death_music) {
                            Mix_PlayMusic(current_music, -1);
                            played_death_music = false;
                        }
                        // TODO this shouldn't happen but keep it here for reference
                        else if (stage == 4 && !player.devil) {
                            // Intense music
                            fade_music_to = music.ominous2;

                            // Red sky
                            SDL_DestroyTexture(tex.background);
                            temp_surface = IMG_Load("assets/redsky.png");
                            tex.background = SDL_CreateTextureFromSurface(renderer, temp_surface);
                            SDL_FreeSurface(temp_surface);

                            // Faster clouds
                            for (int i = 0; i < NUM_CLOUDS; i++)
                                clouds[i].speed += 7;

                            player.devil = true;

                            enemy.refresh_texture(renderer, enemy_surface, enemy_swoosh, 0xFFFFE4BE);
                        }
                        // Again, shouldn't happen
                        else if (stage == 7 && !enemy.devil) {
                            fade_music_to = music.dark;
                            enemy.refresh_texture(renderer, enemy_surface, enemy_swoosh, 0xFF1D00FF);
                            enemy.devil = true;
                        }
                        // This has been repurposed for multiplayer
                        if (make_player_boss || make_enemy_boss) {
                            fade_music_to = music.boss;

                            // Load up the big man skin.
                            SDL_FreeSurface(enemy_surface);
                            SDL_FreeSurface(enemy_swoosh);
                            if (make_enemy_boss) {
                                enemy.boss = true;
                                enemy.devil = true;
                                enemy_surface = IMG_Load("assets/player/boss.png");
                                enemy_swoosh = IMG_Load("assets/player/boss_swoosh.png");
                                enemy.refresh_texture(renderer, enemy_surface, enemy_swoosh, 0);
                                enemy.horns_tex = tex.boss_horns;
                            }
                            else if (make_player_boss) {
                                player.boss = true;
                                player.devil = true;
                                player_surface = IMG_Load("assets/player/boss.png");
                                player_swoosh = IMG_Load("assets/player/boss_swoosh.png");
                                player.refresh_texture(renderer, player_surface, player_swoosh, 0);
                                player.horns_tex = tex.boss_horns;
                            }

                            // Final sky.
                            SDL_DestroyTexture(tex.background);
                            temp_surface = IMG_Load("assets/finalsky.png");
                            tex.background = SDL_CreateTextureFromSurface(renderer, temp_surface);
                            SDL_FreeSurface(temp_surface);

                            // Final mountain.
                            SDL_DestroyTexture(tex.platform);
                            temp_surface = IMG_Load("assets/finalmountain.png");
                            tex.platform = SDL_CreateTextureFromSurface(renderer, temp_surface);
                            SDL_FreeSurface(temp_surface);

                            // FASTER CLOUDS LETS GO.
                            for (int i = 0; i < NUM_CLOUDS; i++)
                                clouds[i].speed = 10;

                            // ai_function = boss_ai;

                            make_player_boss = false;
                            make_enemy_boss = false;
                            boss_initialized = true;
                        }
                        else if (player_wins) {
                            fade_alpha = 256;
                        }
                    }
                }
                else {
                    SDL_SetTextureAlphaMod(tex.blank, fade_alpha);
                    SDL_RenderCopy(renderer, tex.blank, NULL, NULL);
                }
            }
            else if (fading_out_blank) {
                // This should not happen in multiplayer!!
                if (player_wins) {
                    if (fade_alpha > 255) {
                        fade_alpha = 0;
                    }
                    if (fade_alpha != 255 && rand() % 10 > 6)
                        fade_alpha += 1;

                    SDL_RenderCopy(renderer, tex.blank, NULL, NULL);
                    SDL_SetTextureAlphaMod(tex.thanks_for_playing, fade_alpha);
                    SDL_RenderCopy(renderer, tex.thanks_for_playing, NULL, NULL);
                }
                else {
                    fade_alpha -= 2;
                    if (fade_alpha <= 0) {
                        fade_alpha = 0;
                        fading_out_blank = false;
                    }
                    SDL_SetTextureAlphaMod(tex.blank, fade_alpha);
                    SDL_RenderCopy(renderer, tex.blank, NULL, NULL);
                }
            }

            SDL_RenderPresent(renderer);

            memcpy(&last_controls, &controls, sizeof(controls));
        }
        // ===================== Handle Music Transition =================
        if (fade_music_to) {
            if (current_music) {
                Mix_FadeOutMusic(boss_initialized ? 100 : 500);
                current_music = NULL;
            }
            else if (!Mix_PlayingMusic()) {
                current_music = fade_music_to;
                Mix_FadeInMusic(current_music, -1, boss_initialized ? 100 : 500);
                fade_music_to = NULL;
            }
        }

        // ======================= Cap Framerate =====================
        Uint64 frame_end = SDL_GetPerformanceCounter();
        Uint64 frame_ms = (frame_end - frame_start) * milliseconds_per_tick;

        if (frame_ms < 17)
            SDL_Delay(17 - frame_ms);
    }

    return 0;
}
