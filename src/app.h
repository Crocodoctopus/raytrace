#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

enum AppErr {
    AppErr_None,
    AppErr_Unspecified,
};

// Reify application.
struct App {
    // GLFW
    GLFWwindow *window;

    // Vulkan stuff
    VkInstance instance;
    VkSurfaceKHR surface;
    VkDevice device;
    VkQueue graphics_queue, present_queue;
};

enum AppErr app_init(struct App *app, const char * const path);
int app_is_init(struct App *app);
enum AppErr app_run(struct App *app);
enum AppErr app_free(struct App *app);
