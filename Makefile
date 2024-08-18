INCLUDE_DIRS = /usr/local/ffmpeg/include /usr/local/opencv/include/opencv4/ /usr/local/sqlite/include ./Arcsoft/inc/
LIB_DIRS = /usr/local/ffmpeg/lib/ /usr/local/opencv/lib/ /usr/local/sqlite/lib/ ./Arcsoft/lib/linux_x64/

INCLUDE_FLAGS = $(addprefix -I, $(INCLUDE_DIRS))
LDFLAGS = $(addprefix -L, $(LIB_DIRS))
# CFLAGS = -lavformat -lavdevice -lavfilter -lavcodec -lavutil -lswscale -lswresample -lasound -lSDL2 -lpthread -lz -lxcb -lm
CFLAGS = -lSDL2 -lSDL -lm -lasound  -lavformat -lavcodec -lavutil -lavfilter -lavdevice -lswresample -lswscale -lpthread `pkg-config --cflags --libs opencv` -larcsoft_face_engine -larcsoft_face

all:
	g++ ffmpeg_camera_asfort.cpp init_asfort.cpp -o ffmpeg_camera_asfort $(INCLUDE_FLAGS) $(LDFLAGS) $(CFLAGS)

