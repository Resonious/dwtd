#include <emscripten/emscripten.h>

typedef const char SoundClip;
typedef const char MusicClip;

EM_JS(const char *, create_audio, (const char *pathCstr), {
    var path = UTF8ToString(pathCstr);

    var sound = document.createElement('audio');
    sound.src = path;
    sound.id = path;
    sound.setAttribute('preload', 'auto');
    sound.setAttribute('controls', 'none');
    sound.style.display = 'none';
    document.body.appendChild(sound);

    return pathCstr;
});

#define create_music create_audio
#define create_sound create_audio

EM_JS(void, play_sound, (const char *pathCstr), {
    var path = UTF8ToString(pathCstr);
    var sound = document.getElementById(path);

    sound.loop = false;
    sound.currentTime = 0;
    sound.play();
});

EM_JS(void, play_music, (const char *pathCstr, bool loop), {
    var path = UTF8ToString(pathCstr);
    var music = document.getElementById(path);

    var currentSong = window.currentSong;
    if (currentSong != null && currentSong != '')
        document.getElementById(currentSong).pause();

    music.loop = loop;
    music.volume = 1;
    music.currentTime = 0.0;
    music.play();
    window.currentSong = path;
});

EM_JS(void, fade_out_music, (int speed), {
    // Music has to be playing
    var currentSong = window.currentSong;
    if (currentSong == null || currentSong == '') return;
    var music = document.getElementById(currentSong);

    // Use setInterval to reduce volume to 0
    var volume = 1.0;
    var interval = setInterval(() => {
        if (volume > 0) {
            volume -= 0.1;
            music.volume = volume.toFixed(2);
        } else {
            clearInterval(interval);
            window.currentSong = null;
            music.pause();
        }
    }, 1000 / speed);
});

EM_JS(void, fade_in_music, (const char *pathCstr, int speed), {
    var path = UTF8ToString(pathCstr);
    var music = document.getElementById(path);
    var currentSong = window.currentSong;

    // Stop any currently-playing music
    if (currentSong != null && currentSong != '')
        document.getElementById(currentSong).pause();

    // Play new song starting at 0 volume
    music.loop = true;
    music.volume = 0.01;
    music.currentTime = 0.0;
    music.play();
    window.currentSong = path;

    // Gradually increase volume
    var volume = 0.0;
    var interval = setInterval(() => {
        if (volume > 1) {
            music.volume = 1;
            clearInterval(interval);
        } else {
            volume += 0.1;
            music.volume = volume.toFixed(2);
        }
    }, 1000 / speed);
});

EM_JS(bool, music_is_playing, (), {
    var currentSong = window.currentSong;

    if (currentSong == null || currentSong == '')
        return false;

    var music = document.getElementById(currentSong);

    return !music.paused && !music.ended;
});



void (*emscripten_main_loop)();
bool setup_timing = false;

void main_loop_wrapper() {
    if (!setup_timing) {
        emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, 17);
        setup_timing = true;
    }
    emscripten_main_loop();
}


void start_looping(void (*fun)()) {
    emscripten_main_loop = fun;
    emscripten_set_main_loop(main_loop_wrapper, 0, 1);
}
