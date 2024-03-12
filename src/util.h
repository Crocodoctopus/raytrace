#include <vulkan/vulkan.h>
#include <stdint.h>
#include <string.h>

#define DEBUG 1

#define IS_ZERO(t) memcheck(&t, 0 sizeof(t))
#define IS_ZERO_PTR(t) memcheck(t, 0, sizeof(*t))
#define ZERO(t) memset(&t, 0, sizeof(t))

int memcheck(void *ptr, uint8_t val, size_t size);
size_t strlcpy(char *dst, const char *src, size_t size);
size_t strlcat(char *dst, const char *src, size_t size);
const char *vk_result_to_string(VkResult result);
