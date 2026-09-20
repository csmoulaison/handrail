#ifndef handrail_VK_H_INCLUDED
#define handrail_VK_H_INCLUDED

#if PLATFORM == PLATFORM_WINDOWS
#define VK_USE_PLATFORM_WIN32_KHR 
#endif
#define VK_NO_PROTOTYPES
#define VOLK_IMPLEMENTATION
#include <vulkan/vulkan.h>
#include <volk/volk.h>

void vk_init(char* app_name, Stack* scratch);

#ifdef CSM_IMPLEMENTATION

void vk_init(char* app_name, Stack* scratch) {
    assert(volkInitialize() == VK_SUCCESS);

    // Instance
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = app_name;
    app_info.apiVersion = VK_API_VERSION_1_3;

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
    // TODO: Prioritize devices we want, just using 0 for now.
    vkGetPhysicalDeviceProperties2(devices[0], &device_properties);
    printf("Device selected: %s\n", device_properties.properties.deviceName);

    // Device queues
    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(devices[0], &queue_family_count, NULL);
    VkQueueFamilyProperties* queue_families = STACK_ARRAY(scratch, VkQueueFamilyProperties, queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(devices[0], &queue_family_count, queue_families);

    i32 graphics_queue = -1;
    for(i32 i = 0; i < queue_family_count; i++) {
        if(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT
        && vkGetPhysicalDeviceWin32PresentationSupportKHR(devices[0], i)) {
            graphics_queue = i;
            break;
        }
    }
    assert(graphics_queue != -1);
}

#endif
#endif
