# Shuffle assets around... We don't want the audio in the emscripten
# fake filesystem.
rm -rf web/pub/music
rm -rf web/pub/sfx
mv assets/music web/pub/music
mv assets/sfx web/pub/sfx
function copy_assets_back {
  cp -r web/pub/music assets/music
  cp -r web/pub/sfx assets/sfx
}
trap copy_assets_back EXIT

# Build mruby
if !(cd duel-with-the-devil/mruby && rake); then
  echo "mruby build failed"
  exit 123
fi

# Build the game
emcc duel-with-the-devil/main.cpp \
  -std=c++11 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s USE_SDL=2 \
  -s ASSERTIONS=2 \
  -I./duel-with-the-devil/mruby/include \
  -L./duel-with-the-devil/mruby/build/wasm/lib \
  -lmruby \
  -o web/pub/dwtd.html \
  --preload-file assets
# IF YOU DON'T WANT WASM FOR SOME REASON: -s WASM=0 
