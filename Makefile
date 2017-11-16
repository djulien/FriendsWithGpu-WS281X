#INCLUDES = /usr/local/include/SDL2
#set -x
# https://www.gnu.org/software/make/manual/html_node/Text-Functions.html#Text-Functions

#OBJS specifies which files to compile as part of the project
#OBJS = sdl4test.cpp
#OBJS = sdl-mp3.cpp
OBJS = src/DataCanvas.cpp
##OBJS=member-test.cpp
#OBJS = 42_texture_streaming-trim.cpp

#CC specifies which compiler we're using
CC = g++ -std=c++0x
#CC = g++ -std=c++11
#CC = g++-4.9 -std=c++14

#COMPILER_FLAGS specifies the additional compilation options we're using
# -w suppresses all warnings
#COMPILER_FLAGS = -w -I/usr/local/include/SDL2
COMPILER_FLAGS = -Wall -pedantic -Wvariadic-macros `sdl2-config --cflags --libs`
#DEBUG_FLAGS = -E -g
#DEBUG_FLAGS = -S -g
DEBUG_FLAGS = -g

#LINKER_FLAGS specifies the libraries we're linking against
#LINKER_FLAGS = -lSDL2 -lSDL2main
#LINKER_FLAGS = -lSDL2_image -lSDL2_mixer -lGL -lGLU -lglut  # -lSDL2_ttf -lSDL2_mixer # -lncurses
LINKER_FLAGS = `sdl2-config --libs`

#OBJ_NAME specifies the name of our exectuable
#OBJ_NAME = sdl2test

#This is the target that compiles our executable
all : $(OBJS)
	$(CC) $(OBJS) $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(LINKER_FLAGS) -o $(OBJS:.cpp=)

#EOF#
