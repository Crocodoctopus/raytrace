#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <assert.h>
#include <vulkan/vulkan_core.h>

#include "app.h"
#include "util.h"

int create_vk_instance(VkInstance *instance);
int create_vk_device(
        VkInstance instance, 
        VkSurfaceKHR surface, 
        VkPhysicalDevice *pdevice, 
        VkDevice *device, 
        uint32_t *gqf, uint32_t *pqf);
int query_device_swapchain_support(
        VkPhysicalDevice physical_device,
        VkSurfaceKHR surface,
        VkSurfaceCapabilitiesKHR *capabilities,
        uint32_t *formats_n,
        VkSurfaceFormatKHR **formats,
        uint32_t *present_modes_n,
        VkPresentModeKHR **present_modes); 
int create_vk_swapchain(
        VkDevice device, 
        VkPhysicalDevice pdevice, 
        VkSurfaceKHR surface, 
        const struct SwapchainSupportDetails *details,
        uint32_t gqf, uint32_t pqf, 
        VkSwapchainKHR *swapchain, 
        VkFormat *format, 
        VkExtent2D *extent);
int create_vk_image_views(
        VkDevice device, 
        int n, 
        VkImage *images, 
        VkFormat format, 
        VkImageView *image_views);
int create_vk_render_pass(VkDevice device, VkFormat format, VkRenderPass *render_pass);
int create_shader_modules(
        VkDevice device,
        const char *path,
        uint32_t shaders_n,
        const char **shader_names,
        VkShaderModule *shader_modules);
int create_graphics_pipeline(
        VkDevice device, 
        const char * const path, 
        VkFormat *swapchain_format,
        VkPipelineLayout *pipeline_layout, 
        VkPipeline *pipeline);
int create_vertex_buffer(VkDevice device, size_t size, VkBuffer *buffer);
int create_framebuffers(
        VkDevice device,
        VkRenderPass render_pass,
        VkExtent2D extent,
        uint32_t n,
        VkImageView *image_views,
        VkFramebuffer **framebuffers);
int create_command_pool(
        VkDevice device, 
        uint32_t graphics_queue_family, 
        VkCommandPool *command_pool, 
        VkCommandBuffer *command_buffer);
int draw(struct App *app); // TODO

enum AppErr app_init(struct App *app, const char *path) {
#if DEBUG_INPUT_VALIDATION
    // Check inputs.
    if (app == NULL || path == NULL)
        return AppErr_InvalidInput;

    // Check if app is zeroed.
    if (!IS_ZERO_PTR(app))
        return AppErr_NotUninit;
#endif

    int result = 0; // Generic int for returns.
   
    // Set up GLFW.
    int glfw_status = glfwInit();
    if (glfw_status == GLFW_FALSE)
        return AppErr_GlfwInitErr;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    app->window = glfwCreateWindow(800, 600, "Raytrace", NULL, NULL);
    if (app->window == NULL) return AppErr_InitWindowErr;

    // Create vulkan instance
    result = create_vk_instance(&app->instance);
    if (result > 0) return AppErr_InitVkInstanceErr;

    // Create vulkan surface though GLFW.
    result = glfwCreateWindowSurface(app->instance, app->window, NULL, &app->surface);
    if (result > 0) return AppErr_InitVkSurfaceErr;

    // Create vulkan device.
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    uint32_t graphics_queue_family = -1;
    uint32_t present_queue_family = -1;
    result = create_vk_device(
            app->instance, 
            app->surface, 
            &physical_device, 
            &app->device, 
            &graphics_queue_family, 
            &present_queue_family);
    if (result > 0) return AppErr_InitVkDeviceErr;
    
    // Extract vk queues.
    vkGetDeviceQueue(app->device, graphics_queue_family, 0, &app->graphics_queue);
    vkGetDeviceQueue(app->device, present_queue_family, 0, &app->present_queue);
  
    // Query physical device for swapchain support details.
    result = swapchain_support_details_init(&app->swapchain_support, physical_device, app->surface);
    assert(result == 0); // Only way this can fail is via an input error.

    // Create swapchain.
    result = create_vk_swapchain(
            app->device, 
            physical_device, 
            app->surface, 
            &app->swapchain_support,
            graphics_queue_family, 
            present_queue_family, 
            &app->swapchain, 
            &app->swapchain_format, 
            &app->swapchain_extent);
    if (result > 0) return AppErr_InitVkSwapchainErr;

    // Extract swapchain images.
    vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->swapchain_images_n, NULL);
    app->swapchain_images = calloc(app->swapchain_images_n, sizeof(VkImage));
    vkGetSwapchainImagesKHR(
            app->device, 
            app->swapchain, 
            &app->swapchain_images_n, 
            app->swapchain_images);
    app->swapchain_image_views = calloc(app->swapchain_images_n, sizeof(VkImageView));
    result = create_vk_image_views(
            app->device, 
            app->swapchain_images_n, 
            app->swapchain_images, 
            app->swapchain_format, 
            app->swapchain_image_views); 
    if (result > 0) return AppErr_InitVkImageViewErr; 

    // Create graphics pipeline. 
    result = create_graphics_pipeline(
            app->device, 
            path,
            &app->swapchain_format,
            &app->pipeline_layout, 
            &app->pipeline);
    if (result > 0) return AppErr_InitVkGraphicsPipelineErr;

    // Create command pool and alloc command buffer.
    result = create_command_pool(
            app->device,
            graphics_queue_family,
            &app->command_pool,
            &app->command_buffer);
    if (result > 0) return AppErr_InitCommandPoolErr;

    // Create sync objects. (TODO: error check this?)
    VkSemaphoreCreateInfo semaphore_cinfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    vkCreateSemaphore(app->device, &semaphore_cinfo, NULL, &app->image_available);
    vkCreateSemaphore(app->device, &semaphore_cinfo, NULL, &app->render_finished);
    VkFenceCreateInfo fence_cinfo = { 
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    vkCreateFence(app->device, &fence_cinfo, NULL, &app->in_flight);

    return AppErr_None;
}

enum AppErr app_run(struct App *app) {
    while(!glfwWindowShouldClose(app->window)) {
        glfwPollEvents();

        // Draw.
        int i = draw(app);
        if (i > 0) return AppErr_Unspecified;
    }
    
    // Wait for the device to finish.
    vkDeviceWaitIdle(app->device);

    return AppErr_None;
}

enum AppErr app_free(struct App *app) {
#if DEBUG_INPUT_VALIDATION
    if (app == NULL) return AppErr_InvalidInput;
#endif

    // Syncronization.
    vkDestroySemaphore(app->device, app->image_available, NULL);
    vkDestroySemaphore(app->device, app->render_finished, NULL);
    vkDestroyFence(app->device, app->in_flight, NULL);
    app->image_available = VK_NULL_HANDLE;
    app->render_finished = VK_NULL_HANDLE;
    app->in_flight = VK_NULL_HANDLE;

    // Command pool.
    vkDestroyCommandPool(app->device, app->command_pool, NULL);
    app->command_pool = VK_NULL_HANDLE;
    app->command_buffer = VK_NULL_HANDLE;

    // Buffers.
    vkDestroyBuffer(app->device, app->vert_pos, NULL);
    vkDestroyBuffer(app->device, app->vert_color, NULL);

    // Pipeline.
    vkDestroyPipeline(app->device, app->pipeline, NULL);
    vkDestroyPipelineLayout(app->device, app->pipeline_layout, NULL);
    app->pipeline_layout = VK_NULL_HANDLE;
    app->pipeline = VK_NULL_HANDLE; 

    // Image views.
    for (int i = 0; i < app->swapchain_images_n; i++)
        vkDestroyImageView(app->device, app->swapchain_image_views[i], NULL);
    free(app->swapchain_image_views);
    free(app->swapchain_images);
    app->swapchain_images_n = 0;
    app->swapchain_images = NULL;
    app->swapchain_image_views = NULL;

    // Swapchain.
    vkDestroySwapchainKHR(app->device, app->swapchain, NULL);
    app->swapchain = VK_NULL_HANDLE;
    ZERO(app->swapchain_format);
    ZERO(app->swapchain_extent);
    
    // Swapchain support details.
    swapchain_support_details_free(&app->swapchain_support); // Zeroes itself.

    // Logical device
    vkDestroyDevice(app->device, NULL);
    app->device = VK_NULL_HANDLE;
    app->graphics_queue = VK_NULL_HANDLE;
    app->present_queue = VK_NULL_HANDLE;

    // Surface.
    vkDestroySurfaceKHR(app->instance, app->surface, NULL);
    app->surface = VK_NULL_HANDLE;

    // Instance.
    vkDestroyInstance(app->instance, NULL);
    app->instance = VK_NULL_HANDLE;

    // Window.
    glfwDestroyWindow(app->window);
    app->window = NULL;

    // GLFW.
    glfwTerminate();
    
    return AppErr_None;
}

// Returns the first extension that does not match, or -1 on success.
int verify_instance_extensions(uint32_t extensions_n, const char **extensions) {
#if DEBUG_INPUT_VALIDATION
    if (extensions_n == 0) return INT_MAX;
    if (extensions == NULL) return INT_MAX;
    for (int i = 0; i < extensions_n; i++)
        if (extensions[i] == NULL) return INT_MAX;
#endif

    // Get number of available extensions.
    uint32_t available_extensions_n;
    vkEnumerateInstanceExtensionProperties(NULL, &available_extensions_n, NULL);

    // Get names of said extensions.
    VkExtensionProperties *available_extensions = 
        malloc(available_extensions_n * sizeof(*available_extensions));
    vkEnumerateInstanceExtensionProperties(NULL, &available_extensions_n, available_extensions);
    
    int i = 0;
    for (; i < extensions_n; i += 1) {
        // Check if extensions[i] can be found in availabie_extensions.
        const char *i_extension = extensions[i];
        for (int j = 0; j < available_extensions_n; j += 1) {
            const char *j_extension = available_extensions[j].extensionName;
            int cmp = strcmp(i_extension, j_extension);
            if (cmp == 0) goto continue_outer;
        }

        // If we reach here, extensions[i] did not find a match.
        break;
continue_outer:;
    }

    //
    free(available_extensions);
    
    //
    return i < extensions_n ? i : -1;
}

// Returns the first layer that does not match, or -1 on success.
int verify_instance_validation_layers(uint32_t layers_n, const char **layers) {
#if DEBUG_INPUT_VALIDATION
    if (layers_n == 0) return INT_MAX;
    if (layers == NULL) return INT_MAX;
    for (int i = 0; i < layers_n; i++)
        if (layers[i] == NULL) return INT_MAX;
#endif

    // Get number of available validation layers.
    uint32_t available_layers_n;
    vkEnumerateInstanceLayerProperties(&available_layers_n, NULL);

    // Get names of said validation layers.
    VkLayerProperties *available_layers = 
        malloc(available_layers_n * sizeof(*available_layers));
    vkEnumerateInstanceLayerProperties(&available_layers_n, available_layers);
   
    // Check if each element of layers is contained in avalable_layers.
    int i = 0;
    for (; i < layers_n; i += 1) {
        // Check if layers[i] can be found in availabie_layers.
        const char *i_layer = layers[i];
        for (int j = 0; j < available_layers_n; j += 1) {
            const char *j_layer = available_layers[j].layerName;
            int cmp = strcmp(i_layer, j_layer);
            if (cmp == 0) goto continue_outer;
        }

        // If we reach here, layers[i] did not find a match.
        break;
continue_outer:;
    }

    //
    free(available_layers);
    
    // If the above loop didn't make it to completion, return error. 
    return i < layers_n ? i : -1;
}

int create_vk_instance(VkInstance *instance) {
#if DEBUG_INPUT_VALIDATION
    if (instance == NULL) return 1;
    if (*instance != VK_NULL_HANDLE) return 1;
#endif

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Vulkan Ray Trace",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "None",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };
   
    //
    uint32_t glfw_extensions_n = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extensions_n);
    int verify_ext_result = verify_instance_extensions(glfw_extensions_n, glfw_extensions);
    if (verify_ext_result != -1) return 2;

    //
    const char *validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
    int validation_layers_n = sizeof(validation_layers) / sizeof(*validation_layers);
    int verify_vl_result = verify_instance_validation_layers(validation_layers_n, validation_layers);
    if (verify_vl_result != -1) return 3;

    //
    VkInstanceCreateInfo instance_cinfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = glfw_extensions_n,
        .ppEnabledExtensionNames = glfw_extensions,
        .enabledLayerCount = validation_layers_n,
        .ppEnabledLayerNames = validation_layers,
    };

    //
    int make_instance_result = vkCreateInstance(&instance_cinfo, NULL, instance);
    if (make_instance_result != VK_SUCCESS) return 4; // Failed to create VK instance.

    //
    return 0;
}

int find_present_queue_family(
        VkPhysicalDevice device, 
        VkSurfaceKHR surface, 
        uint32_t *present_queue_family) {
#if DEBUG_INPUT_VALIDATION
    if (device == VK_NULL_HANDLE) return 1;
    if (surface == VK_NULL_HANDLE) return 1;
    if (present_queue_family == NULL) return 1;
#endif

    //
    uint32_t queue_families_n;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_families_n, NULL);

    //
    for (int i = 0; i < queue_families_n; i += 1) {
        VkBool32 present_support = 0;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
        if (present_support) {
            *present_queue_family = i;
            return 0;
        }
    }

    return 2;
}

int find_graphics_queue_family(VkPhysicalDevice device, uint32_t *graphics_queue_family) {
    if (device == VK_NULL_HANDLE) return 1;
    if (graphics_queue_family == NULL) return 1;

    //
    uint32_t queue_family_n;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_n, NULL);

    //
    VkQueueFamilyProperties* queue_families =
        malloc(queue_family_n * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_n, queue_families);

    //
    int i = 0;
    for (; i < queue_family_n; i += 1) {
        VkQueueFamilyProperties* queue_family = &queue_families[i];
        if (queue_family->queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            *graphics_queue_family = i;
            break;
        }
    }

    //
    free(queue_families);
    return (i == queue_family_n) ? 2 : 0;
}

// Returns the first extension that does not match, or -1 on success.
int verify_device_extensions(VkPhysicalDevice physical_device, uint32_t extensions_n, const char **extensions) {
    // Get number of available extensions.
    uint32_t available_extensions_n;
    vkEnumerateDeviceExtensionProperties(physical_device, NULL, &available_extensions_n, NULL);

    // Get names of said extensions.
    VkExtensionProperties *available_extensions = 
        malloc(available_extensions_n * sizeof(*available_extensions));
    vkEnumerateDeviceExtensionProperties(physical_device, NULL, &available_extensions_n, available_extensions);
    
    int i = 0;
    for (; i < extensions_n; i += 1) {
        // Check if extensions[i] can be found in availabie_extensions.
        const char *i_extension = extensions[i];
        for (int j = 0; j < available_extensions_n; j += 1) {
            const char *j_extension = available_extensions[j].extensionName;
            int cmp = strcmp(i_extension, j_extension);
            if (cmp == 0) goto continue_outer;
        }

        // If we reach here, extensions[i] did not find a match.
        break;
continue_outer:;
    }

    //
    free(available_extensions);
    
    //
    return i < extensions_n ? i : -1;
}

int create_vk_device(
        VkInstance instance, 
        VkSurfaceKHR surface, 
        VkPhysicalDevice *physical_device, 
        VkDevice *device, 
        uint32_t *graphics_queue_family, 
        uint32_t *present_queue_family) {
#if DEBUG_INPUT_VALIDATION
    if (instance == VK_NULL_HANDLE) return 1;
    if (surface == VK_NULL_HANDLE) return 1;
    if (physical_device == NULL) return 1;
    if (*physical_device != VK_NULL_HANDLE) return 1;
    if (device == NULL) return 1;
    if (*device != VK_NULL_HANDLE) return 1;
#endif

    // Take first physical device.
    uint32_t physical_devices_n;
    vkEnumeratePhysicalDevices(instance, &physical_devices_n, NULL);
    if (physical_devices_n == 0) return 2; // No physical devices.
    physical_devices_n = 1;
    vkEnumeratePhysicalDevices(instance, &physical_devices_n, physical_device);

    // TODO: Check physical device for compatibility.

    //
    int find_gqf_result = find_graphics_queue_family(*physical_device, graphics_queue_family);
    if (find_gqf_result > 0) return 3; // No graphics queue family.

    //
    int find_pqf_result = find_present_queue_family(*physical_device, surface, present_queue_family);
    if (find_pqf_result > 0) return 4; // No present queue family.

    //
    float queue_priority = 1.0;
    VkDeviceQueueCreateInfo queue_cinfos[2] = { 
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = *graphics_queue_family,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = *present_queue_family,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,

        },
    };


    // TODO
    VkPhysicalDeviceFeatures device_features = { 0 };

    //
    const char *device_extensions[] = {
        "VK_KHR_swapchain",
        "VK_KHR_dynamic_rendering",
    };
    const int device_extensions_n = sizeof(device_extensions) / sizeof(*device_extensions);

    //
    int vde = verify_device_extensions(*physical_device, device_extensions_n, device_extensions);
    if (vde > 0) return 5;

    const VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .dynamicRendering = VK_TRUE,
    };

    VkDeviceCreateInfo device_cinfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &dynamic_rendering_features,
        .pQueueCreateInfos = queue_cinfos,
        .queueCreateInfoCount = *present_queue_family == *graphics_queue_family ? 1 : 2,
        .pEnabledFeatures = &device_features,
        .enabledExtensionCount = device_extensions_n,
        .ppEnabledExtensionNames = device_extensions,
    };

    //
    int make_device_result = vkCreateDevice(*physical_device, &device_cinfo, NULL, device);
    if (make_device_result != VK_SUCCESS) return 6; // Could not create logical device.
    
    return 0;
}

int create_vk_swapchain(
        VkDevice device, 
        VkPhysicalDevice physical_device, 
        VkSurfaceKHR surface,
        const struct SwapchainSupportDetails *details,
        uint32_t graphics_queue_family, 
        uint32_t present_queue_family, 
        VkSwapchainKHR *swapchain, 
        VkFormat *format, 
        VkExtent2D *extent) {
#if DEBUG_INPUT_VALIDATION
    if (device == VK_NULL_HANDLE) return 1;
    if (physical_device == VK_NULL_HANDLE) return 1;
    if (surface == VK_NULL_HANDLE) return 1;
    if (swapchain == NULL) return 1;
    if (*swapchain != VK_NULL_HANDLE) return 1;
    if (format == NULL) return 1;
    if (extent == NULL) return 1;
#endif

    // Assume this format is supported.
    VkSurfaceFormatKHR surface_format = {
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    };
    /*for (int i = 0; i < details->formats_n; i++) {
        VkSurfaceFormatKHR* format = details->formats + i;
        int t_format = format->format == VK_FORMAT_B8G8R8A8_SRGB;
        int t_color_space = format->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        if (t_format && t_color_space) {
            surface_format = *format;
            break;
        }
    }*/

    // Assume this present mode is supported by the physical device.
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

    // Assume the swapchain extent is the size of the window.
    extent->width = 800;
    extent->height = 600;

    //
    VkSharingMode sharing_mode = (graphics_queue_family == present_queue_family)
        ? VK_SHARING_MODE_EXCLUSIVE 
        : VK_SHARING_MODE_CONCURRENT;
    uint32_t queue_family_indices[] = {
        [0] = graphics_queue_family,
        [1] = present_queue_family,
    };    

    // TODO: temp
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities);

    //
    VkSwapchainCreateInfoKHR swapchain_cinfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = capabilities.minImageCount,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = *extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = sharing_mode,
        .queueFamilyIndexCount = 2,
        .pQueueFamilyIndices = queue_family_indices,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    //
    uint32_t make_sc_result = vkCreateSwapchainKHR(
            device, 
            &swapchain_cinfo, 
            NULL, 
            swapchain);
    if (make_sc_result != VK_SUCCESS) return 2;

    //
    *format = surface_format.format;

    return 0;
}

int create_vk_image_views(
        VkDevice device, 
        int n, 
        VkImage *images, 
        VkFormat format, 
        VkImageView *image_views) {
#if DEBUG_INPUT_VALIDATION
    if (device == VK_NULL_HANDLE) return 1;
    if (images == NULL) return 1;
    if (n == 0) return 1;
    if (image_views == NULL) return 1;
    for (int i = 0; i < n; i++)
        if (images[i] == VK_NULL_HANDLE || image_views[i] != VK_NULL_HANDLE)
            return 1;
#endif

    for (int i = 0; i < n; i++) {
        VkImageViewCreateInfo cinfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };

        uint32_t make_iv_result = vkCreateImageView(device, &cinfo, NULL, image_views + i);
        if (make_iv_result != VK_SUCCESS) return 1;
    }

    return 0;
}

int create_vk_render_pass(VkDevice device, VkFormat format, VkRenderPass *render_pass) {
#if DEBUG_INPUT_VALIDATION
    if (device == VK_NULL_HANDLE) return 1;
    if (render_pass == NULL) return 1;
    if (*render_pass != VK_NULL_HANDLE) return 1;
#endif

    //
    VkAttachmentDescription color_attachment = {
        .format = format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    //
    VkAttachmentReference attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    //
    VkSubpassDescription subpass_desc = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachment_ref,
    };

    //
    VkRenderPassCreateInfo render_pass_cinfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass_desc,
    };

    //
    int make_rp_result = vkCreateRenderPass(device, &render_pass_cinfo, NULL, render_pass);
    if (make_rp_result != VK_SUCCESS) return 2;

    return 0;
}

int create_shader_module(VkDevice device, const char *path, const char *name, VkShaderModule *shader_module) {
#if DEBUG_INPUT_VALIDATION
    if (device == VK_NULL_HANDLE) return 1;
    if (path == NULL) return 1;
    if (shader_module == NULL) return 1;
    if (*shader_module != VK_NULL_HANDLE) return 1;
#endif

    // Create shader path.
    char shader_path[256];
    size_t l = strlen(path);
    strncpy(shader_path, path, 256);
    strncat(shader_path, name, 256 - l);

    // Extract shader bytecode from file.
    FILE *file = fopen(shader_path, "rb+");
    if (file == NULL) return 2;
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    rewind(file);
    char *shader = malloc(size);
    fread(shader, 1, size, file);
    fclose(file);

    //
    VkShaderModuleCreateInfo shader_cinfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = (const uint32_t*)shader,
    };

    //
    uint32_t make_shader_result = vkCreateShaderModule(
            device, 
            &shader_cinfo, 
            NULL, 
            shader_module);
    free(shader);
    if (make_shader_result != VK_SUCCESS) return 3;

    return 0;
}

int create_graphics_pipeline(
        VkDevice device, 
        const char * const path,
        VkFormat *swapchain_format,
        VkPipelineLayout *pipeline_layout, 
        VkPipeline *pipeline) {
#if DEBUG_INPUT_VALIDATION
    if (device == VK_NULL_HANDLE) return 1;
    if (path == NULL) return 1;
    if (pipeline_layout == NULL) return 1;
    if (*pipeline_layout != VK_NULL_HANDLE) return 1;
    if (pipeline == NULL) return 1;
    if (*pipeline != VK_NULL_HANDLE) return 1;
#endif

    int res = 0;

    // Create two shader modules.
    VkShaderModule shader_modules[2] = { VK_NULL_HANDLE };
    int r1 = create_shader_module(device, path, "tri.vert.spv", shader_modules + 0);
    int r2 = create_shader_module(device, path, "tri.frag.spv", shader_modules + 1);
    if (r1 > 0 || r2 > 0) {
        res = 2;
        goto fail;
    }

    // Create both shader stages.
    VkPipelineShaderStageCreateInfo shader_stage_cinfos[2] = {
        [0] = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = shader_modules[0],
            .pName = "main",
            .pSpecializationInfo = NULL,
        },
        [1] = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = shader_modules[1],
            .pName = "main",
            .pSpecializationInfo = NULL,
        },
    };

    //
    const VkVertexInputBindingDescription vertex_input_binding_descs[] = {
        {
            .binding = 0,
            .stride = 2 * sizeof(float),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
        {
            .binding = 1,
            .stride = 3 * sizeof(float),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        }
    };

    // 
    VkVertexInputAttributeDescription vertex_input_attribute_descs[] = {
        {
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
        },
        {
            .binding = 1,
            .location = 1,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
        }
    };
    
    // Pipeline input create info. 
    VkPipelineVertexInputStateCreateInfo vertex_input_cinfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 2,
        .pVertexBindingDescriptions = vertex_input_binding_descs,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = vertex_input_attribute_descs,
    };

    //
    VkPipelineInputAssemblyStateCreateInfo input_assembly_cinfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    //
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = 800.0f,
        .height = 600.0f,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    //
    VkRect2D scissor = {
        .offset = { .x = 0, .y = 0 },
        .extent = { .width = 800, .height = 600 },
    };

    //
    VkPipelineViewportStateCreateInfo viewport_state_cinfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    //
    VkPipelineRasterizationStateCreateInfo rasterization_cinfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
    };

    //
    VkPipelineMultisampleStateCreateInfo multisample_cinfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    //
    VkPipelineColorBlendAttachmentState color_blend_state = {
        .colorWriteMask = 
            VK_COLOR_COMPONENT_R_BIT 
            | VK_COLOR_COMPONENT_G_BIT 
            | VK_COLOR_COMPONENT_B_BIT 
            | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
    };
    VkPipelineColorBlendStateCreateInfo color_blend_state_cinfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_state,
    };

    //
    VkPipelineLayoutCreateInfo pipeline_layout_cinfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pushConstantRangeCount = 0,
    };
    int make_pl_result = vkCreatePipelineLayout(device, &pipeline_layout_cinfo, NULL, pipeline_layout);
    if (make_pl_result != VK_SUCCESS) {
        res = 3;
        goto fail;
    }

    VkPipelineRenderingCreateInfo pipeline_rendering_cinfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = swapchain_format,
    };

    //
    VkGraphicsPipelineCreateInfo pipeline_cinfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &pipeline_rendering_cinfo,
        .stageCount = 2,
        .pStages = shader_stage_cinfos,
        .pVertexInputState = &vertex_input_cinfo,
        .pInputAssemblyState = &input_assembly_cinfo,
        .pViewportState = &viewport_state_cinfo,
        .pRasterizationState = &rasterization_cinfo,
        .pMultisampleState = &multisample_cinfo,
        .pDepthStencilState = NULL,
        .pColorBlendState = &color_blend_state_cinfo, 
        .pDynamicState = NULL,
        .layout = *pipeline_layout,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    //
    int make_pipeline_result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_cinfo, NULL, pipeline);
    if (make_pipeline_result != VK_SUCCESS) {
        const char *out = vk_result_to_string(make_pipeline_result);
        printf("%s\n", out);
        res = 4;
        goto fail;
    }

fail:
    vkDestroyShaderModule(device, shader_modules[0], NULL);
    vkDestroyShaderModule(device, shader_modules[1], NULL);
    return res;
}

int create_vertex_buffer(VkDevice device, size_t size, VkBuffer *buffer) {
#if DEBUG_INPUT_VALIDATION
    if (device == VK_NULL_HANDLE) return 1;
    if (size == 0) return 1;
    if (buffer == NULL) return 1;
    if (*buffer != VK_NULL_HANDLE) return 1;
#endif
    VkBufferCreateInfo buffer_cinfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkResult result = vkCreateBuffer(device, &buffer_cinfo, NULL, buffer);
    if (result != VK_SUCCESS) return 2;

    return 0;
}

int create_framebuffers(
        VkDevice device,
        VkRenderPass render_pass,
        VkExtent2D extent,
        uint32_t n,
        VkImageView *image_views,
        VkFramebuffer **framebuffers) {
#if DEBUG_INPUT_VALIDATION
    if (device == VK_NULL_HANDLE) return 1;
    if (render_pass == VK_NULL_HANDLE) return 1;
    if (n == 0) return 1;
    if (image_views == NULL) return 1;
    for (int i = 0; i < n; i++)
        if (image_views[i] == VK_NULL_HANDLE) return 1;
    if (framebuffers == NULL) return 1;
    if (*framebuffers != NULL) return 1;
#endif

    //
    *framebuffers = calloc(n, sizeof(VkFramebuffer));

    //
    for (int i = 0; i < n; i++) {
        VkFramebufferCreateInfo framebuffer_cinfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = 1,
            .pAttachments = image_views + i,
            .width = extent.width,
            .height = extent.height,
            .layers = 1,
        };

        int create_fb_result = vkCreateFramebuffer(device, &framebuffer_cinfo, NULL, (*framebuffers) + i);
        if (create_fb_result != VK_SUCCESS) return 2;
    }

    return 0;
}

int create_command_pool(
        VkDevice device, 
        uint32_t graphics_queue_family, 
        VkCommandPool *command_pool, 
        VkCommandBuffer *command_buffer) {
#if DEBUG_INPUT_VALIDATION
    if (device == VK_NULL_HANDLE) return 1;
    if (graphics_queue_family == UINT32_MAX) return 1;
    if (command_pool == NULL) return 1;
    if (*command_pool != VK_NULL_HANDLE) return 1;
    if (command_buffer == NULL) return 1;
    if (*command_buffer != VK_NULL_HANDLE) return 1;
#endif

    int result = 0;

    VkCommandPoolCreateInfo pool_cinfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphics_queue_family,
    };

    result = vkCreateCommandPool(device, &pool_cinfo, NULL, command_pool);
    if (result != VK_SUCCESS) return 2;

    VkCommandBufferAllocateInfo buffer_cinfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = *command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    result = vkAllocateCommandBuffers(device, &buffer_cinfo, command_buffer);
    if (result != VK_SUCCESS) return 3;

    return 0;
}

// Lazy copout
int draw(struct App *app) {
#if DEBUG_INPUT_VALIDATION
    if (app == NULL) return 1;
#endif

    VkResult result = VK_RESULT_MAX_ENUM;

    // Wait for signal that rendering is finished.
    vkWaitForFences(app->device, 1, &app->in_flight, VK_TRUE, UINT64_MAX);
    vkResetFences(app->device, 1, &app->in_flight);

    // Aquire next swapchain image.
    uint32_t img_index = 0;
    vkAcquireNextImageKHR(app->device, app->swapchain, UINT64_MAX, app->image_available, VK_NULL_HANDLE, &img_index);
    
    // Reset command buffer for pushing.
    vkResetCommandBuffer(app->command_buffer, 0);

    // The next lines just submit commands to the command buffer.
    VkCommandBufferBeginInfo command_buffer_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0,
        .pInheritanceInfo = NULL,
    };

    result = vkBeginCommandBuffer(app->command_buffer, &command_buffer_begin_info);
    if (result != VK_SUCCESS) return 2;

    const VkImageMemoryBarrier imb = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = app->swapchain_images[img_index],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    vkCmdPipelineBarrier(
            app->command_buffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            0,
            NULL,
            0,
            NULL,
            1,
            &imb);

    const VkRenderingAttachmentInfo attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = app->swapchain_image_views[img_index],
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {{{ 0.0f, 0.0f, 0.0f, 1.0f }}},
    };
    const VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = { 0, 0 },
            .extent = app->swapchain_extent,
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachment_info,
    }; 
    vkCmdBeginRendering(app->command_buffer, &rendering_info);
    vkCmdBindPipeline(app->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->pipeline);
    vkCmdDraw(app->command_buffer, 3, 1, 0, 0);
    vkCmdEndRendering(app->command_buffer);

    const VkImageMemoryBarrier imb2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = app->swapchain_images[img_index],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    vkCmdPipelineBarrier(
            app->command_buffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0,
            NULL,
            0,
            NULL,
            1,
            &imb2);

    // vkCmdSetViewport
    // vkCmdSetScissor

    result = vkEndCommandBuffer(app->command_buffer);
    if (result != VK_SUCCESS) return 3;

    // Submit command buffer.
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &app->image_available,
        .pWaitDstStageMask = wait_stages, 
        .commandBufferCount = 1,
        .pCommandBuffers = &app->command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &app->render_finished,
    };

    result = vkQueueSubmit(app->graphics_queue, 1, &submit_info, app->in_flight);
    if (result != VK_SUCCESS) return 4;

    // Present?
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &app->render_finished,
        .swapchainCount = 1,
        .pSwapchains = &app->swapchain,
        .pImageIndices = &img_index,
        .pResults = NULL,
    };

    vkQueuePresentKHR(app->present_queue, &present_info);

    return 0;
}
