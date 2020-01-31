rm -rf web/pub/music
rm -rf web/pub/sfx
mv assets/music web/pub/music
mv assets/sfx web/pub/sfx

emcc duel-with-the-devil/main.cpp \
  -std=c++11 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s USE_SDL=2 -s \
  ASSERTIONS=2 \
  -o web/pub/dwtd.html \
  --preload-file assets
# IF YOU DON'T WANT WASM FOR SOME REASON: -s WASM=0 

cp -r web/pub/music assets/music
cp -r web/pub/sfx assets/sfx
