set -e

sh build-mruby.sh

g++ duel-with-the-devil/main.cpp \
  -Iduel-with-the-devil/mruby/include \
  -lSDL2 \
  -lSDL2_mixer \
  -L./duel-with-the-devil/mruby/build/host/lib \
  -lmruby \
  -lmruby_core \
  -std=c++11 \
  -o dwtd
