CC			= g++
CFLAGS		= -std=c++11 -Wall -march=native -O3
LINKFLAGS	= -lX11 -lGL -lGLU -lglut -lm -lXmu
SRCS		= main.cpp common/matrix.cpp common/vector.cpp common/camera.cpp common/gauss.cpp common/utility.cpp common/util.cpp
OBJS		= $(SRCS:.cpp=.o)
PROG		= main

all: $(SRCS) $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(INCFLAGS) $(LINKFLAGS)

.cpp.o:
	$(CC) $(CFLAGS) $< -c -o $@ $(INCFLAGS)

clean:
	rm $(OBJS) $(PROG)
