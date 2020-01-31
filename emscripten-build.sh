emcc duel-with-the-devil/main.cpp -std=c++11 -s ALLOW_MEMORY_GROWTH=1 -s USE_SDL=2 -s ASSERTIONS=2 -o web/pub/dwtd.html --preload-file assets
# BUGGERS: -s USE_SDL_IMAGE=2 -s USE_SDL_MIXER=2 
# IF YOU DON'T WANT WASM FOR SOME REASON: -s WASM=0 
