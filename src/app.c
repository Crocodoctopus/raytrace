#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "app.h"

int create_vk_instance(VkInstance *instance);

enum AppErr app_init(struct App *app, const char *path) {
    int result = 0; // Generic int for returns.

    // Check inputs.
    if (app == NULL || path == NULL)
        return AppErr_Unspecified;

    // Check if app is zeroed.
    for (size_t i = 0; i < sizeof(*app); i++) {
        if (((char*)app)[i] != 0) 
            return AppErr_Unspecified;
    }
   
    // Set up GLFW.
    int glfw_status = glfwInit();
    if (glfw_status == GLFW_FALSE)
        return AppErr_Unspecified;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    app->window = glfwCreateWindow(800, 600, "Raytrace", NULL, NULL);
    if (app->window == NULL)
        return AppErr_Unspecified;

    // Create vulkan instance
    result = create_vk_instance(&app->instance);
    if (result > 0)
        return AppErr_Unspecified;

    // Create vulkan surface though GLFW.
    result = glfwCreateWindowSurface(app->instance, app->window, NULL, &app->surface);
    if (result != 0)
        return AppErr_Unspecified; // Error: could not create vulkan surface.

    return AppErr_None;
}

enum AppErr app_run(struct App *app) {
    return AppErr_None;
}

enum AppErr app_free(struct App *app) {
    glfwTerminate();
    return AppErr_None;
}

// Returns the first extension that does not match, or -1 on success.
int verify_extensions(uint32_t extensions_n, const char **extensions) {
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
int verify_validation_layers(uint32_t layers_n, const char **layers) {
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
    if (*instance != VK_NULL_HANDLE) return 1;

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Vulkan Ray Trace",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "None",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };
   
    //
    uint32_t glfw_extensions_n = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extensions_n);
    int verify_ext_result = verify_extensions(glfw_extensions_n, glfw_extensions);
    if (verify_ext_result != -1) return 2;

    //
    const char *validation_layers[] = { };
    int validation_layers_n = sizeof(validation_layers) / sizeof(*validation_layers);
    int verify_vl_result = verify_validation_layers(validation_layers_n, validation_layers);
    if (verify_vl_result != -1) return 3;

    //
    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = glfw_extensions_n,
        .ppEnabledExtensionNames = glfw_extensions,
        .enabledLayerCount = validation_layers_n,
        .ppEnabledLayerNames = validation_layers,
    };

    //
    int make_instance_result = vkCreateInstance(&create_info, NULL, instance);
    if (make_instance_result != VK_SUCCESS) return 4; // Failed to create VK instance.

    //
    return 0;
}

