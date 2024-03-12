#include <vulkan/vulkan.h>

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    uint32_t formats_n;
    VkSurfaceFormatKHR* formats;
    uint32_t present_modes_n;
    VkPresentModeKHR* present_modes;
};

int swapchain_support_details_init(
        struct SwapchainSupportDetails *details, 
        VkPhysicalDevice pdevice, 
        VkSurfaceKHR surface);
void swapchain_support_details_free(struct SwapchainSupportDetails *details);
