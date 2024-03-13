#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>
#include "swapchain_support_details.h"

enum AppErr {
    AppErr_None = 0,
    AppErr_Unspecified,
    AppErr_InvalidInput,
    AppErr_NotUninit,
    AppErr_GlfwInitErr,
    AppErr_InitWindowErr,
    AppErr_InitVkInstanceErr,
    AppErr_InitVkSurfaceErr,
    AppErr_InitVkDeviceErr,
    AppErr_InitVkSwapchainErr,
    AppErr_InitVkImageViewErr,
    AppErr_InitVkRenderPassErr,
    AppErr_InitVkGraphicsPipelineErr,
    AppErr_InitFramebuffersErr,
    AppErr_InitCommandPoolErr,
};

// Reify application.
struct App {
    // GLFW
    GLFWwindow *window;
    // Instance.
    VkInstance instance;
    VkSurfaceKHR surface;
    VkDevice device;
    VkQueue graphics_queue, present_queue;
    // Device swapchain support.
    struct SwapchainSupportDetails swapchain_support;
    // Swapchain.
    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    uint32_t swapchain_images_n;
    VkImage *swapchain_images; // array with size of swapchain_images_n
    VkImageView *swapchain_image_views; // array with size of swapchain_images_n
    // Pipeline.
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    // Command pool.
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    // Syncronization.
    VkSemaphore image_available;
    VkSemaphore render_finished;
    VkFence in_flight;
};

enum AppErr app_init(struct App *app, const char * const path);
enum AppErr app_free(struct App *app);

enum AppErr app_run(struct App *app);
