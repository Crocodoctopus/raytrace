#include <vulkan/vulkan_core.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "swapchain_support_details.h"

int swapchain_support_details_init(
        struct SwapchainSupportDetails *details, 
        VkPhysicalDevice physical_device, 
        VkSurfaceKHR surface) {
    if (details == NULL) return 1;
    if (!IS_ZERO_PTR(details)) return 1;
    if (physical_device == VK_NULL_HANDLE) return 1;
    if (surface == VK_NULL_HANDLE) return 1;

    //
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &details->capabilities);

    //
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &details->formats_n, NULL);
    details->formats = calloc(details->formats_n, sizeof(VkSurfaceFormatKHR));
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &details->formats_n, details->formats);

    //
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &details->present_modes_n, NULL);
    details->present_modes = calloc(details->present_modes_n, sizeof(VkPresentModeKHR));
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &details->present_modes_n, details->present_modes);

    return 0;
}

void swapchain_support_details_free(struct SwapchainSupportDetails *details) {
    free(details->formats);
    free(details->present_modes);

    ZERO(details->capabilities);
    details->formats_n = 0;
    details->formats = NULL;
    details->present_modes_n = 0;
    details->present_modes = NULL;
}
