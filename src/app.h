#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

enum AppErr {
    AppErr_None,
};

// Reify application.
struct App {
    // GLFW
    GLFWwindow *window;

    // Vulkan stuff
};

enum AppErr app_init(struct App *app, const char * const path);
enum AppErr app_run(struct App *app);
enum AppErr app_free(struct App *app);
