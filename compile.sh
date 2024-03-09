glslangValidator -V -s src/tri.frag -o bin/tri.frag.spv
glslangValidator -V -s src/tri.vert -o bin/tri.vert.spv
gcc -c src/main.c -o build/main.o
gcc -c src/app.c -o build/app.o
gcc build/main.o build/app.o -o bin/main -lglfw -lvulkan
