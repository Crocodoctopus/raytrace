glslc src/tri.frag -o bin/tri.frag.spv
glslc src/tri.vert -o bin/tri.vert.spv
gcc -c src/swapchain_support_details.c -o build/scsd.o
gcc -c src/util.c -o build/util.o
gcc -c src/main.c -o build/main.o
gcc -c src/app.c -o build/app.o
gcc build/util.o build/main.o build/app.o build/scsd.o -o bin/main -lglfw -lvulkan
