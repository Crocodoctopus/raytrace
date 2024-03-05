#include "app.h"

enum AppErr app_init(struct App *app, const char *path) {
    app->window = NULL;
    return AppErr_None;
}

enum AppErr app_run(struct App *app) {
    return AppErr_None;
}

enum AppErr app_free(struct App *app) {
    return AppErr_None;
}
