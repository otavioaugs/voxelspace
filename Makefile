build:
	gcc ./src/*.c `sdl2-config --libs --cflags` -lm -Wall -Wextra -o voxelspace

run:
	./voxelspace

clean:
	rm voxelspace
