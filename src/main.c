#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "app.h"

char *next_dir(char *str) {
    char *last = str + 1;
    while (1) {
        if (*last == 0) return str;
        if (*last == '/') return last;
        last += 1;
    }
}

int main(int argc, char *argv[]) {
    // Get working directory.
    char path[256];
    char *ptr = getcwd(path, 256);
    assert(ptr == path);

    // Concat local executable path.
    int l = strnlen(path, 256);
    path[l] = '/';
    strncpy(path + l + 1, argv[0], 256 - l - 1);

    // Strip program name from path.
    char *last = path + l;
    while (last != (last = next_dir(last)));
    *(last + 1) = 0;

    // Start app.
    struct App app;
    enum AppErr err;

    err = app_init(&app, path);
    assert(err == AppErr_None);

    err = app_run(&app);
    assert(err == AppErr_None);

    err = app_free(&app);
    assert(err == AppErr_None);
}
