typedef Mix_Chunk SoundClip;
typedef Mix_Music MusicClip;


bool audio_initialized = false;
int mix_channel_count = 15;
int mix_min_channel = 0;
int mix_max_channel = 6;
int mix_current_channel = mix_min_channel;


void initialize_audio() {
    if ((Mix_Init(MIX_INIT_OGG) & MIX_INIT_OGG) != MIX_INIT_OGG) { exit(2); }
        Mix_AllocateChannels(mix_channel_count);

    if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 2048) == -1)
        exit(3);
    audio_initialized = true;
}


Mix_Music *create_music(const char *path) {
    if (!audio_initialized) initialize_audio();
    char full_path[256];
    strcpy(full_path, "assets/");
    strcpy(full_path+7, path);
    return Mix_LoadMUS(full_path);
}

Mix_Chunk *create_sound(const char *path) {
    if (!audio_initialized) initialize_audio();
    char full_path[256];
    strcpy(full_path, "assets/");
    strcpy(full_path+7, path);
    return Mix_LoadWAV(full_path);
}

void play_sound(Mix_Chunk *sound) {
    Mix_PlayChannel(mix_current_channel, sound, 0);

    mix_current_channel += 1;
    if (mix_current_channel > mix_max_channel)
        mix_current_channel = mix_min_channel;
}

void play_music(Mix_Music *song, bool loop) {
    Mix_PlayMusic(song, loop ? -1 : 0);
}

void fade_out_music(int speed) {
    Mix_FadeOutMusic(speed);
}

void fade_in_music(Mix_Music *song, int speed) {
    Mix_FadeInMusic(song, -1, speed);
}

bool music_is_playing() {
    return Mix_PlayingMusic();
}


void start_looping(void (*fun)()) {
    Uint64 milliseconds_per_tick = 1000 / SDL_GetPerformanceFrequency();

    while (true) {
        Uint64 frame_start = SDL_GetPerformanceCounter();

        fun();

        Uint64 frame_end = SDL_GetPerformanceCounter();
        Uint64 frame_ms = (frame_end - frame_start) * milliseconds_per_tick;

        if (frame_ms < 17)
            SDL_Delay(17 - frame_ms);
    }
}
