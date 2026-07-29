Maze3D - split lightweight build

This project uses a Unity Build:
- main.cpp calls runGame().
- src/Game.cpp includes the smaller implementation modules in dependency order.
- Compile main.cpp and src/Game.cpp only. Do not compile the other src/*.cpp files separately.

Direct compile from the Maze3D folder:
  g++ main.cpp src/Game.cpp -o maze3d -std=c++17 $(sdl2-config --cflags --libs) -lSDL2_ttf -lSDL2_image

Run:
  ./maze3d

CMake:
  cmake -S . -B build
  cmake --build build
  ./build/maze3d

Assets:
The game currently loads wall.jpg and enemy.png from the program's working folder.
For direct compilation, either run from the project folder after copying assets there,
or use:
  cp assets/wall.jpg .
  cp assets/enemy.png .
