#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

enum AppErr {
    AppErr_None = 0,
    AppErr_Unspecified,

    AppErr_InvalidInput,
    AppErr_NotUninit,
    AppErr_InitWindowErr,
    AppErr_InitVkInstanceErr,
    AppErr_InitVkSurfaceErr,
    AppErr_InitVkDeviceErr,
    AppErr_InitVkSwapchainErr,
    AppErr_InitVkImageViewErr,
    AppErr_InitVkRenderPassErr,
    AppErr_InitVkGraphicsPipelineErr,
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
    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    uint32_t swapchain_images_n;
    VkImage *swapchain_images; // array with size of swapchain_images_n
    VkImageView *swapchain_image_views; // array with size of swapchain_images_n
    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
};

enum AppErr app_init(struct App *app, const char * const path);
int app_is_init(struct App *app);
enum AppErr app_run(struct App *app);
enum AppErr app_free(struct App *app);
