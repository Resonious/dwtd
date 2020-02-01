typedef int SoundClip;
typedef int MusicClip;

bool dummy_music;

void initialize_audio() {
    dummy_music = false;
}

int *create_music(const char *path) {
    return NULL;
}

int *create_sound(const char *path) {
    return NULL;
}

void play_sound(int *sound) {
}

void play_music(int *song, bool loop) {
    dummy_music = true;
}

void fade_out_music(int speed) {
    dummy_music = false;
}

void fade_in_music(int *song, int speed) {
    dummy_music = true;
}

bool music_is_playing() {
    return music;
}


void start_looping(void (*fun)()) {
    while (true) fun();
}


#define load_player_scripts() \
    if (argc > 1) { \
        enemy.load_script_file(argv[1]); \
    } \
    if (argc > 2) { \
        player.load_script_file(argv[2]); \
    }
