#ifndef handrail_VK_H_INCLUDED
#define handrail_VK_H_INCLUDED

#if PLATFORM == PLATFORM_WINDOWS
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#define VK_NO_PROTOTYPES
#define VOLK_IMPLEMENTATION
#include <vulkan/vulkan.h>
#include <volk/volk.h>

typedef struct {
    HWND      hwnd;
    HINSTANCE hinstance;
} VkWin32Platform;

void vk_init(char* app_name, void* platform_context, Stack* scratch);

#ifdef CSM_IMPLEMENTATION

void vk_init(char* app_name, void* platform_context, Stack* scratch) {
    // TODO: replace all instances of this with VK_VERIFY macro
    assert(volkInitialize() == VK_SUCCESS);

    // Instance
    VkApplicationInfo app_info = {};
    app_info.sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = app_name;
    app_info.apiVersion       = VK_API_VERSION_1_3;

    u32 instance_extensions_count = 1;
    char* instance_extensions[1] = { VK_KHR_WIN32_SURFACE_EXTENSION_NAME };

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.enabledExtensionCount = instance_extensions_count;
    instance_info.ppEnabledExtensionNames = instance_extensions;

    VkInstance instance = {};
    assert(vkCreateInstance(&instance_info, NULL, &instance) == VK_SUCCESS);
    volkLoadInstance(instance);

    // Device selection
    u32 device_count = 0;
    assert(vkEnumeratePhysicalDevices(instance, &device_count, NULL) == VK_SUCCESS);
    VkPhysicalDevice* devices = STACK_ARRAY(scratch, VkPhysicalDevice, device_count);
    assert(vkEnumeratePhysicalDevices(instance, &device_count, devices) == VK_SUCCESS);
    printf("Device count %u\n", device_count);

    VkPhysicalDeviceProperties2 device_properties = {};
    device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    // TODO: Prioritize devices we want, just using 0 for now. This will
    // probably get entangled with the device queue querying below.
    VkPhysicalDevice physical_device = devices[0];
    vkGetPhysicalDeviceProperties2(physical_device, &device_properties);
    printf("Device selected: %s\n", device_properties.properties.deviceName);

    // Device queues
    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);
    VkQueueFamilyProperties* queue_families = STACK_ARRAY(scratch, VkQueueFamilyProperties, queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);

    i32 graphics_queue_family = -1;
    for(i32 i = 0; i < queue_family_count; i++) {
        if(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT
        && vkGetPhysicalDeviceWin32PresentationSupportKHR(physical_device, i)) {
            graphics_queue_family = i;
            break;
        }
    }
    assert(graphics_queue_family != -1);

    f32 graphics_queue_priority = 1.0;
    VkDeviceQueueCreateInfo graphics_queue_info = {};
    graphics_queue_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphics_queue_info.queueFamilyIndex = graphics_queue_family;
    graphics_queue_info.queueCount       = 1;
    graphics_queue_info.pQueuePriorities = &graphics_queue_priority;

    // Device extensions
    // NOTE: If we add more extensions, device_info.enabledExtensionCount needs
    // to change.
    char* swapchain_extension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkPhysicalDeviceVulkan12Features vk12_features = {};
    vk12_features.sType                                     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12_features.descriptorIndexing                        = true;
    vk12_features.shaderSampledImageArrayNonUniformIndexing = true;
    vk12_features.descriptorBindingVariableDescriptorCount  = true;
    vk12_features.runtimeDescriptorArray                    = true;
    vk12_features.bufferDeviceAddress                       = true;

    VkPhysicalDeviceVulkan13Features vk13_features = {};
    vk13_features.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vk13_features.pNext            = &vk12_features;
    vk13_features.synchronization2 = true;
    vk13_features.dynamicRendering = true;

    VkPhysicalDeviceFeatures vk10_features = {};
    vk10_features.samplerAnisotropy = true;

    // DEVIIICE... ASSEMBLE!
    VkDeviceCreateInfo device_info = {};
    device_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext                   = &vk13_features;
    device_info.queueCreateInfoCount    = 1;
    device_info.pQueueCreateInfos       = &graphics_queue_info;
    device_info.enabledExtensionCount   = 1;
    device_info.ppEnabledExtensionNames = &swapchain_extension;
    device_info.pEnabledFeatures        = &vk10_features;
    VkDevice device = {};
    assert(vkCreateDevice(physical_device, &device_info, NULL, &device) == VK_SUCCESS);

    VkQueue graphics_queue = {};
    vkGetDeviceQueue(device, graphics_queue_family, 0, &graphics_queue);

    // Surface
    VkWin32Platform* win32 = (VkWin32Platform*)platform_context;
    VkWin32SurfaceCreateInfoKHR surface_info = {};
    surface_info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_info.hwnd      = win32->hwnd;
    surface_info.hinstance = win32->hinstance;
    VkSurfaceKHR surface = {};
    assert(vkCreateWin32SurfaceKHR(instance, &surface_info, NULL, &surface) == VK_SUCCESS);

    VkSurfaceCapabilitiesKHR surface_capabilities = {};
    assert(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities) == VK_SUCCESS);

    VkExtent2D swapchain_extent = surface_capabilities.current_extent;
    if(surface_capabilities.current_extent.width == 0xffffffff) {
        swapchain_extent.width  = window_size.x;
        swapchain_extent.height = window_size.y;
    }

    // NOW: Create swapchain.
}

#endif
#endif
