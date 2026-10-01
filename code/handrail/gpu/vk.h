#ifndef handrail_vk_h_INCLUDED
#define handrail_vk_h_INCLUDED

// Vulkan backend for render.h. Bindless throughout: asset buffer regions live in
// one device-local buffer addressed by device address, every texture sits in
// one descriptor array, and the game writes each frame's globals, instances,
// and indirect draw commands straight into a persistently mapped buffer.

#ifndef VK_FRAMES_IN_FLIGHT
#define VK_FRAMES_IN_FLIGHT 2
#endif

#ifndef VK_MAX_SWAPCHAIN_IMAGES
#define VK_MAX_SWAPCHAIN_IMAGES 8
#endif

#ifndef VK_VALIDATION
#define VK_VALIDATION DEBUG_ASSERTIONS
#endif

#if PLATFORM == PLATFORM_LINUX
#define VK_USE_PLATFORM_XLIB_KHR
#include <X11/Xlib.h>
#elif PLATFORM == PLATFORM_WINDOWS
#define VK_USE_PLATFORM_WIN32_KHR
#elif PLATFORM == PLATFORM_WEB
#endif
#define VK_NO_PROTOTYPES
#include <volk/volk.h>

#define VK_DEPTH_FORMAT VK_FORMAT_D32_SFLOAT

#define VK_VERIFY(vk_function) { \
    VkResult vk_result = (vk_function); \
    if(vk_result != VK_SUCCESS) { \
        log_exit("Vulkan error %d: %s", vk_result, #vk_function); \
    } \
}

typedef struct {
#if PLATFORM == PLATFORM_LINUX
    Display*  display;
    Window    window;
#elif PLATFORM == PLATFORM_WINDOWS
    HWND      hwnd;
    HINSTANCE hinstance;
#elif PLATFORM == PLATFORM_WEB
    i32       tmp;
#endif
} VkPlatformWindow;

// One large allocation of one memory type, handed out linearly. Buffer arenas
// wrap the whole allocation in a single VkBuffer; image arenas have no buffer.
typedef struct {
    VkDeviceMemory memory;
    VkBuffer       buffer;
    u64            address;
    u64            size;
    u64            head;
    u8*            mapped;
} VkArena;

// How frame begin paces presents. Version 2 of the extensions is preferred, since
// it reports support per surface where version 1 only reports it per device.
typedef enum {
    VK_PRESENT_WAIT_MODE_NONE, // FIFO alone: the present queue fills up to the image count
    VK_PRESENT_WAIT_MODE_1,    // VK_KHR_present_id + VK_KHR_present_wait
    VK_PRESENT_WAIT_MODE_2     // VK_KHR_present_id2 + VK_KHR_present_wait2
} VkPresentWaitMode;

typedef struct {
    VkCommandBuffer command_buffer;
    VkFence         fence;
    VkSemaphore     image_acquired;
    u64             memory_offset; // Into host_arena
    // Set once this frame's command buffer has written its GPU timestamps
    bool            timestamps_written;
} VkFrame;

typedef struct {
    VkInstance               instance;
    VkSurfaceKHR             surface;
    VkPhysicalDevice         physical_device;
    VkDevice                 device;
    VkQueue                  queue;
    VkCommandPool            command_pool;

    // Swapchain and depth buffer, recreated on resize
    VkSwapchainKHR           swapchain;
    VkFormat                 swapchain_format;
    VkExtent2D               swapchain_extent;
    VkImage                  swapchain_images[VK_MAX_SWAPCHAIN_IMAGES];
    VkImageView              swapchain_views[VK_MAX_SWAPCHAIN_IMAGES];
    VkSemaphore              render_finished[VK_MAX_SWAPCHAIN_IMAGES];
    u32                      swapchain_images_len;
    bool                     swapchain_stale;
    iv2                      swapchain_window_size; // The window size the swapchain was built for
    VkArena                  depth_arena;
    VkImage                  depth_image;
    VkImageView              depth_view;

    // Frames in flight
    VkFrame                  frames[VK_FRAMES_IN_FLIGHT];
    u32                      frame_index;
    u32                      image_index;

    // Present pacing: each present is tagged with an id, and frame begin waits for
    // the last one to reach the screen, so at most one finished frame is queued
    VkPresentWaitMode        present_wait_mode;
    u64                      present_id; // Of the last present on this swapchain, 0 before the first

    // GPU frame timing: two timestamps per frame in flight, reported as PROFILE_GPU.
    // timestamp_query_pool is VK_NULL_HANDLE if the queue can't write timestamps.
    VkQueryPool              timestamp_query_pool;
    f32                      timestamp_period;    // Nanoseconds per tick
    u64                      timestamp_mask;      // Valid bits of a timestamp

    // GPU memory
    VkArena                  host_arena;   // Per-frame memory, mapped
    VkArena                  buffer_arena; // Asset buffer regions
    VkArena                  image_arena;  // Textures
    u64                      buffer_region_addresses[RENDER_MAX_BUFFER_REGIONS];

    // Resources
    VkImage                  textures[RENDER_MAX_TEXTURES];
    VkImageView              texture_views[RENDER_MAX_TEXTURES];
    VkSampler                samplers[RENDER_MAX_SAMPLERS];
    VkDescriptorSetLayout    descriptor_set_layout;
    VkDescriptorPool         descriptor_pool;
    VkDescriptorSet          descriptor_set;
    VkPipelineLayout         pipeline_layout;
    VkPipeline               pipelines[RENDER_MAX_PASSES];
    u32                      pipelines_len;
} VkContext;

// Create the instance, device, surface, swapchain, and per-frame memory.
void vk_init(VkContext* vk, String app_name, VkPlatformWindow window, iv2 window_size, Stack* scratch);
// Upload the regions and textures the game asked for and build its pipelines.
void vk_load_assets(VkContext* vk, RenderSetup* setup);
// Wait for the last present to reach the screen (when present wait is supported) and
// for a free frame, acquire a swapchain image, and point frame at that frame's memory.
// Recreates the swapchain when window_size has changed since it was built.
void vk_frame_begin(VkContext* vk, iv2 window_size, RenderFrame* frame);
// Record, submit, and present everything the game wrote into frame.
void vk_frame_end(VkContext* vk, RenderFrame* frame);

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_vk_h_IMPLEMENTED)
#define handrail_vk_h_IMPLEMENTED

#include "volk/volk.c"

#define VK_FRAME_COMMANDS_SIZE (RENDER_MAX_PASSES * RENDER_FRAME_MAX_COMMANDS * sizeof(RenderDrawCommand))
#define VK_FRAME_MEMORY_SIZE   (RENDER_FRAME_GLOBALS_SIZE + RENDER_FRAME_INSTANCES_SIZE + VK_FRAME_COMMANDS_SIZE)

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* user_data)
{
    if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        log_print(LOG_ERROR, "Vulkan: %s", data->pMessage);
    } else {
        log_print(LOG_WARN, "Vulkan: %s", data->pMessage);
    }
    return VK_FALSE;
}

// Returns -1 if no memory type matches.
static i32 vk_memory_type_index(VkContext* vk, u32 type_bits, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties properties;
    vkGetPhysicalDeviceMemoryProperties(vk->physical_device, &properties);
    for(i32 i = 0; i < properties.memoryTypeCount; i++) {
        if((type_bits & (1 << i)) && (properties.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    return -1;
}

// Allocate an arena. With buffer_usage set, it is wrapped in one buffer with a
// device address. Memory flags are tried in order until one is available.
static VkArena vk_arena_new(VkContext* vk, char* name, u64 size, u32 type_bits, VkBufferUsageFlags buffer_usage, VkMemoryPropertyFlags* flags, i32 flags_len) {
    VkArena arena = {};
    arena.size = size;

    VkMemoryRequirements requirements = {};
    requirements.size = size;
    requirements.memoryTypeBits = type_bits;
    if(buffer_usage != 0) {
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size        = size;
        buffer_info.usage       = buffer_usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_VERIFY(vkCreateBuffer(vk->device, &buffer_info, NULL, &arena.buffer));
        vkGetBufferMemoryRequirements(vk->device, arena.buffer, &requirements);
    }

    i32 memory_type = -1;
    i32 flags_index = 0;
    VkMemoryPropertyFlags memory_flags = 0;
    for(i32 i = 0; i < flags_len && memory_type == -1; i++) {
        memory_type = vk_memory_type_index(vk, requirements.memoryTypeBits, flags[i]);
        memory_flags = flags[i];
        flags_index = i;
    }
    if(memory_type == -1) {
        log_exit("Vulkan: no memory type for arena %s (%" PRIu64 " bytes)", name, (u64)requirements.size);
    }
    if(flags_index > 0) {
        log_print(LOG_RENDER, "Vulkan: arena %s fell back to memory flags 0x%x (preferred 0x%x)",
                  name, (u32)memory_flags, (u32)flags[0]);
    }
    log_print(LOG_RENDER, "Vulkan: arena %s, %" PRIu64 " bytes, memory type %i, flags 0x%x, buffer usage 0x%x",
              name, (u64)requirements.size, memory_type, (u32)memory_flags, (u32)buffer_usage);

    VkMemoryAllocateFlagsInfo allocate_flags = {};
    allocate_flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocate_flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo allocate_info = {};
    allocate_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.pNext           = buffer_usage != 0 ? &allocate_flags : NULL;
    allocate_info.allocationSize  = requirements.size;
    allocate_info.memoryTypeIndex = memory_type;
    VK_VERIFY(vkAllocateMemory(vk->device, &allocate_info, NULL, &arena.memory));

    if(buffer_usage != 0) {
        VK_VERIFY(vkBindBufferMemory(vk->device, arena.buffer, arena.memory, 0));
        VkBufferDeviceAddressInfo address_info = {};
        address_info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        address_info.buffer = arena.buffer;
        arena.address = vkGetBufferDeviceAddress(vk->device, &address_info);
    }
    if(memory_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        VK_VERIFY(vkMapMemory(vk->device, arena.memory, 0, VK_WHOLE_SIZE, 0, (void**)&arena.mapped));
    }
    return arena;
}

static void vk_arena_free(VkContext* vk, VkArena* arena) {
    if(arena->buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(vk->device, arena->buffer, NULL);
    }
    if(arena->memory != VK_NULL_HANDLE) {
        vkFreeMemory(vk->device, arena->memory, NULL);
    }
    *arena = (VkArena){};
}

// Returns the offset of the allocation within the arena.
static u64 vk_arena_alloc(VkArena* arena, u64 size, u64 alignment) {
    u64 offset = (arena->head + alignment - 1) / alignment * alignment;
    assert(offset + size <= arena->size);
    arena->head = offset + size;
    return offset;
}

static void vk_image_barrier(VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags aspect,
    VkImageLayout old_layout, VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access,
    VkImageLayout new_layout, VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access)
{
    VkImageMemoryBarrier2 barrier = {};
    barrier.sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask                = src_stage;
    barrier.srcAccessMask               = src_access;
    barrier.dstStageMask                = dst_stage;
    barrier.dstAccessMask               = dst_access;
    barrier.oldLayout                   = old_layout;
    barrier.newLayout                   = new_layout;
    barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                       = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo dependency = {};
    dependency.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers    = &barrier;
    vkCmdPipelineBarrier2(command_buffer, &dependency);
}

static VkImageView vk_image_view_new(VkContext* vk, VkImage image, VkFormat format, VkImageAspectFlags aspect) {
    VkImageViewCreateInfo view_info = {};
    view_info.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image                       = image;
    view_info.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format                      = format;
    view_info.subresourceRange.aspectMask = aspect;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = 1;
    VkImageView view;
    VK_VERIFY(vkCreateImageView(vk->device, &view_info, NULL, &view));
    return view;
}

// Create (or recreate) the swapchain, its views, and the depth buffer. reason
// is only logged.
static void vk_swapchain_create(VkContext* vk, iv2 window_size, char* reason) {
    vk->swapchain_window_size = window_size;
    VkSwapchainKHR old_swapchain = vk->swapchain;
    if(old_swapchain != VK_NULL_HANDLE) {
        VK_VERIFY(vkDeviceWaitIdle(vk->device));
        for(i32 i = 0; i < vk->swapchain_images_len; i++) {
            vkDestroyImageView(vk->device, vk->swapchain_views[i], NULL);
            vkDestroySemaphore(vk->device, vk->render_finished[i], NULL);
        }
        vkDestroyImageView(vk->device, vk->depth_view, NULL);
        vkDestroyImage(vk->device, vk->depth_image, NULL);
        vk_arena_free(vk, &vk->depth_arena);
    }

    // Extent
    VkSurfaceCapabilitiesKHR capabilities;
    VK_VERIFY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->physical_device, vk->surface, &capabilities));
    VkExtent2D extent = capabilities.currentExtent;
    if(extent.width == 0xFFFFFFFF) {
        extent.width  = (u32)window_size.x;
        extent.height = (u32)window_size.y;
    }
    if(extent.width  < capabilities.minImageExtent.width)  extent.width  = capabilities.minImageExtent.width;
    if(extent.height < capabilities.minImageExtent.height) extent.height = capabilities.minImageExtent.height;
    if(extent.width  > capabilities.maxImageExtent.width)  extent.width  = capabilities.maxImageExtent.width;
    if(extent.height > capabilities.maxImageExtent.height) extent.height = capabilities.maxImageExtent.height;
    if(extent.width == 0)  extent.width  = 1;
    if(extent.height == 0) extent.height = 1;
    vk->swapchain_extent = extent;

    // Under FIFO, finished frames wait in the present queue until their vblank, so
    // every image beyond the first is up to a frame of latency. Use the fewest allowed.
    u32 image_count = capabilities.minImageCount;

    // Swapchain
    VkSwapchainCreateInfoKHR swapchain_info = {};
    swapchain_info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
#ifdef VK_KHR_present_wait2
    if(vk->present_wait_mode == VK_PRESENT_WAIT_MODE_2) {
        swapchain_info.flags        = VK_SWAPCHAIN_CREATE_PRESENT_ID_2_BIT_KHR | VK_SWAPCHAIN_CREATE_PRESENT_WAIT_2_BIT_KHR;
    }
#endif
    swapchain_info.surface          = vk->surface;
    swapchain_info.minImageCount    = image_count;
    swapchain_info.imageFormat      = vk->swapchain_format;
    swapchain_info.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchain_info.imageExtent      = extent;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.preTransform     = capabilities.currentTransform;
    swapchain_info.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode      = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_info.clipped          = VK_TRUE;
    swapchain_info.oldSwapchain     = old_swapchain;
    VK_VERIFY(vkCreateSwapchainKHR(vk->device, &swapchain_info, NULL, &vk->swapchain));
    vk->present_id = 0;
    if(old_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(vk->device, old_swapchain, NULL);
    }

    // Images, views, and a present semaphore per image
    VK_VERIFY(vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &vk->swapchain_images_len, NULL));
    assert(vk->swapchain_images_len <= VK_MAX_SWAPCHAIN_IMAGES);
    VK_VERIFY(vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &vk->swapchain_images_len, vk->swapchain_images));
    for(i32 i = 0; i < vk->swapchain_images_len; i++) {
        vk->swapchain_views[i] = vk_image_view_new(vk, vk->swapchain_images[i], vk->swapchain_format, VK_IMAGE_ASPECT_COLOR_BIT);
        VkSemaphoreCreateInfo semaphore_info = {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_VERIFY(vkCreateSemaphore(vk->device, &semaphore_info, NULL, &vk->render_finished[i]));
    }

    // Depth buffer
    VkImageCreateInfo depth_info = {};
    depth_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depth_info.imageType     = VK_IMAGE_TYPE_2D;
    depth_info.format        = VK_DEPTH_FORMAT;
    depth_info.extent        = (VkExtent3D){ extent.width, extent.height, 1 };
    depth_info.mipLevels     = 1;
    depth_info.arrayLayers   = 1;
    depth_info.samples       = VK_SAMPLE_COUNT_1_BIT;
    depth_info.tiling        = VK_IMAGE_TILING_OPTIMAL;
    depth_info.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_VERIFY(vkCreateImage(vk->device, &depth_info, NULL, &vk->depth_image));
    VkMemoryRequirements depth_requirements;
    vkGetImageMemoryRequirements(vk->device, vk->depth_image, &depth_requirements);
    VkMemoryPropertyFlags depth_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    vk->depth_arena = vk_arena_new(vk, "depth", depth_requirements.size, depth_requirements.memoryTypeBits, 0, &depth_flags, 1);
    VK_VERIFY(vkBindImageMemory(vk->device, vk->depth_image, vk->depth_arena.memory, 0));
    vk->depth_view = vk_image_view_new(vk, vk->depth_image, VK_DEPTH_FORMAT, VK_IMAGE_ASPECT_DEPTH_BIT);

    log_print(LOG_RENDER, "Vulkan: swapchain created (%s). %ux%u, %u images, format %i, present mode FIFO",
              reason, extent.width, extent.height, vk->swapchain_images_len, (i32)vk->swapchain_format);
    vk->swapchain_stale = false;
}

void vk_init(VkContext* vk, String app_name, VkPlatformWindow window, iv2 window_size, Stack* scratch) {
    VK_VERIFY(volkInitialize());

    // Instance layers and extensions
    char* layers[1];
    u32 layers_len = 0;
    bool validation = false;
#if VK_VALIDATION
    u32 available_layers_len = 0;
    VK_VERIFY(vkEnumerateInstanceLayerProperties(&available_layers_len, NULL));
    VkLayerProperties* available_layers = (VkLayerProperties*)stack_alloc(scratch, available_layers_len * sizeof(VkLayerProperties));
    VK_VERIFY(vkEnumerateInstanceLayerProperties(&available_layers_len, available_layers));
    for(i32 i = 0; i < available_layers_len; i++) {
        if(strcmp(available_layers[i].layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            layers[layers_len++] = "VK_LAYER_KHRONOS_validation";
            validation = true;
        }
    }
    if(validation) {
        log_print(LOG_RENDER, "Vulkan validation layer enabled");
    } else {
        log_print(LOG_WARN, "Vulkan validation layer requested but not installed");
    }
#endif

    char* extensions[4];
    u32 extensions_len = 0;
    extensions[extensions_len++] = VK_KHR_SURFACE_EXTENSION_NAME;
#if PLATFORM == PLATFORM_LINUX
    extensions[extensions_len++] = VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
#elif PLATFORM == PLATFORM_WINDOWS
    extensions[extensions_len++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#elif PLATFORM == PLATFORM_WEB
#endif
    if(validation) {
        extensions[extensions_len++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }
    // Needed to ask whether the surface supports present wait 2
    bool surface_capabilities2 = false;
    u32 available_extensions_len = 0;
    VK_VERIFY(vkEnumerateInstanceExtensionProperties(NULL, &available_extensions_len, NULL));
    VkExtensionProperties* available_extensions = (VkExtensionProperties*)stack_alloc(scratch, available_extensions_len * sizeof(VkExtensionProperties));
    VK_VERIFY(vkEnumerateInstanceExtensionProperties(NULL, &available_extensions_len, available_extensions));
    for(i32 i = 0; i < available_extensions_len; i++) {
        if(strcmp(available_extensions[i].extensionName, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME) == 0) {
            extensions[extensions_len++] = VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME;
            surface_capabilities2 = true;
        }
    }
    for(i32 i = 0; i < extensions_len; i++) {
        log_print(LOG_RENDER, "Vulkan instance extension: %s", extensions[i]);
    }

    // Instance
    char* app_name_cstr = (char*)stack_alloc(scratch, app_name.len + 1);
    memcpy(app_name_cstr, app_name.text, app_name.len);
    app_name_cstr[app_name.len] = '\0';

    VkApplicationInfo app_info = {};
    app_info.sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = app_name_cstr;
    app_info.pEngineName      = "handrail";
    app_info.apiVersion       = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo        = &app_info;
    instance_info.enabledLayerCount       = layers_len;
    instance_info.ppEnabledLayerNames     = (const char* const*)layers;
    instance_info.enabledExtensionCount   = extensions_len;
    instance_info.ppEnabledExtensionNames = (const char* const*)extensions;
    VK_VERIFY(vkCreateInstance(&instance_info, NULL, &vk->instance));
    volkLoadInstance(vk->instance);

    if(validation) {
        VkDebugUtilsMessengerCreateInfoEXT messenger_info = {};
        messenger_info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        messenger_info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        messenger_info.pfnUserCallback = vk_debug_callback;
        // Never destroyed: it lives as long as the process
        VkDebugUtilsMessengerEXT debug_messenger;
        VK_VERIFY(vkCreateDebugUtilsMessengerEXT(vk->instance, &messenger_info, NULL, &debug_messenger));
    }

    // Surface
#if PLATFORM == PLATFORM_LINUX
    VkXlibSurfaceCreateInfoKHR surface_info = {};
    surface_info.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surface_info.dpy    = window.display;
    surface_info.window = window.window;
    VK_VERIFY(vkCreateXlibSurfaceKHR(vk->instance, &surface_info, NULL, &vk->surface));
#elif PLATFORM == PLATFORM_WINDOWS
    VkWin32SurfaceCreateInfoKHR surface_info = {};
    surface_info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_info.hwnd      = window.hwnd;
    surface_info.hinstance = window.hinstance;
    VK_VERIFY(vkCreateWin32SurfaceKHR(vk->instance, &surface_info, NULL, &vk->surface));
#elif PLATFORM == PLATFORM_WEB
    panic();
#endif

    // Physical device: prefer a discrete GPU with a queue family that can
    // both draw and present to our surface.
    u32 queue_family = 0;
    u32 devices_len = 0;
    VK_VERIFY(vkEnumeratePhysicalDevices(vk->instance, &devices_len, NULL));
    VkPhysicalDevice* devices = (VkPhysicalDevice*)stack_alloc(scratch, devices_len * sizeof(VkPhysicalDevice));
    VK_VERIFY(vkEnumeratePhysicalDevices(vk->instance, &devices_len, devices));
    i32 best_score = -1;
    for(i32 i = 0; i < devices_len; i++) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(devices[i], &properties);
        if(properties.apiVersion < VK_API_VERSION_1_3) {
            log_print(LOG_RENDER, "Vulkan: skipping %s, API version %u.%u is below 1.3", properties.deviceName,
                      VK_API_VERSION_MAJOR(properties.apiVersion), VK_API_VERSION_MINOR(properties.apiVersion));
            continue;
        }

        u32 families_len = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &families_len, NULL);
        VkQueueFamilyProperties* families = (VkQueueFamilyProperties*)stack_alloc(scratch, families_len * sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &families_len, families);
        i32 family = -1;
        for(i32 j = 0; j < families_len && family == -1; j++) {
            VkBool32 present = VK_FALSE;
            VK_VERIFY(vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], j, vk->surface, &present));
            if((families[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
                family = j;
            }
        }
        if(family == -1) {
            log_print(LOG_RENDER, "Vulkan: skipping %s, no queue family can both draw and present", properties.deviceName);
            continue;
        }

        i32 score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 2 : 1;
        if(score > best_score) {
            best_score = score;
            vk->physical_device = devices[i];
            queue_family = family;
        }
    }
    if(best_score == -1) {
        log_exit("No Vulkan 1.3 device can present to this window");
    }
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(vk->physical_device, &device_properties);
    log_print(LOG_RENDER, "Vulkan device: %s", device_properties.deviceName);
    log_print(LOG_RENDER, "Vulkan device: type %i, API %u.%u.%u, driver 0x%x, queue family %u",
              (i32)device_properties.deviceType,
              VK_API_VERSION_MAJOR(device_properties.apiVersion), VK_API_VERSION_MINOR(device_properties.apiVersion),
              VK_API_VERSION_PATCH(device_properties.apiVersion), device_properties.driverVersion, queue_family);

    // Present wait, preferring version 2. Each version needs its id and wait
    // extensions, both features, and for version 2, the surface's support too.
    bool has_present_id = false;
    bool has_present_wait = false;
#ifdef VK_KHR_present_wait2
    bool has_present_id2 = false;
    bool has_present_wait2 = false;
#endif
    u32 device_extensions_available_len = 0;
    VK_VERIFY(vkEnumerateDeviceExtensionProperties(vk->physical_device, NULL, &device_extensions_available_len, NULL));
    VkExtensionProperties* device_extensions_available = (VkExtensionProperties*)stack_alloc(scratch, device_extensions_available_len * sizeof(VkExtensionProperties));
    VK_VERIFY(vkEnumerateDeviceExtensionProperties(vk->physical_device, NULL, &device_extensions_available_len, device_extensions_available));
    for(i32 i = 0; i < device_extensions_available_len; i++) {
        char* name = device_extensions_available[i].extensionName;
        if(strcmp(name, VK_KHR_PRESENT_ID_EXTENSION_NAME) == 0)   has_present_id = true;
        if(strcmp(name, VK_KHR_PRESENT_WAIT_EXTENSION_NAME) == 0) has_present_wait = true;
#ifdef VK_KHR_present_wait2
        if(strcmp(name, VK_KHR_PRESENT_ID_2_EXTENSION_NAME) == 0)   has_present_id2 = true;
        if(strcmp(name, VK_KHR_PRESENT_WAIT_2_EXTENSION_NAME) == 0) has_present_wait2 = true;
#endif
    }

    char* device_extensions[3];
    u32 device_extensions_len = 0;
    device_extensions[device_extensions_len++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    void* present_wait_features = NULL; // Chained onto the device's features
    vk->present_wait_mode = VK_PRESENT_WAIT_MODE_NONE;
#ifdef VK_KHR_present_wait2
    VkPhysicalDevicePresentId2FeaturesKHR present_id2_features = {};
    present_id2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_2_FEATURES_KHR;
    VkPhysicalDevicePresentWait2FeaturesKHR present_wait2_features = {};
    present_wait2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_2_FEATURES_KHR;
    present_wait2_features.pNext = &present_id2_features;
    if(surface_capabilities2 && has_present_id2 && has_present_wait2) {
        VkPhysicalDeviceFeatures2 features = {};
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features.pNext = &present_wait2_features;
        vkGetPhysicalDeviceFeatures2(vk->physical_device, &features);

        VkSurfaceCapabilitiesPresentId2KHR surface_present_id2 = {};
        surface_present_id2.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_PRESENT_ID_2_KHR;
        VkSurfaceCapabilitiesPresentWait2KHR surface_present_wait2 = {};
        surface_present_wait2.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_PRESENT_WAIT_2_KHR;
        surface_present_wait2.pNext = &surface_present_id2;
        VkSurfaceCapabilities2KHR surface_capabilities = {};
        surface_capabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
        surface_capabilities.pNext = &surface_present_wait2;
        VkPhysicalDeviceSurfaceInfo2KHR surface_info2 = {};
        surface_info2.sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
        surface_info2.surface = vk->surface;
        VK_VERIFY(vkGetPhysicalDeviceSurfaceCapabilities2KHR(vk->physical_device, &surface_info2, &surface_capabilities));

        if(present_id2_features.presentId2 && present_wait2_features.presentWait2
        && surface_present_id2.presentId2Supported && surface_present_wait2.presentWait2Supported) {
            vk->present_wait_mode = VK_PRESENT_WAIT_MODE_2;
            device_extensions[device_extensions_len++] = VK_KHR_PRESENT_ID_2_EXTENSION_NAME;
            device_extensions[device_extensions_len++] = VK_KHR_PRESENT_WAIT_2_EXTENSION_NAME;
            present_wait_features = &present_wait2_features;
        }
    }
#else
    (void)surface_capabilities2;
#endif
    VkPhysicalDevicePresentIdFeaturesKHR present_id_features = {};
    present_id_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR;
    VkPhysicalDevicePresentWaitFeaturesKHR present_wait1_features = {};
    present_wait1_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR;
    present_wait1_features.pNext = &present_id_features;
    if(vk->present_wait_mode == VK_PRESENT_WAIT_MODE_NONE && has_present_id && has_present_wait) {
        VkPhysicalDeviceFeatures2 features = {};
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features.pNext = &present_wait1_features;
        vkGetPhysicalDeviceFeatures2(vk->physical_device, &features);
        if(present_id_features.presentId && present_wait1_features.presentWait) {
            vk->present_wait_mode = VK_PRESENT_WAIT_MODE_1;
            device_extensions[device_extensions_len++] = VK_KHR_PRESENT_ID_EXTENSION_NAME;
            device_extensions[device_extensions_len++] = VK_KHR_PRESENT_WAIT_EXTENSION_NAME;
            present_wait_features = &present_wait1_features;
        }
    }
    log_print(LOG_RENDER, "Vulkan: present wait %s",
              vk->present_wait_mode == VK_PRESENT_WAIT_MODE_2 ? "2"
            : vk->present_wait_mode == VK_PRESENT_WAIT_MODE_1 ? "1"
            : "unsupported, presents can queue up to the swapchain image count");

    // Device
    f32 queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {};
    queue_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = queue_family;
    queue_info.queueCount       = 1;
    queue_info.pQueuePriorities = &queue_priority;

    VkPhysicalDeviceVulkan12Features vk12_features = {};
    vk12_features.sType                                     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12_features.descriptorIndexing                        = VK_TRUE;
    vk12_features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    vk12_features.descriptorBindingPartiallyBound           = VK_TRUE;
    vk12_features.runtimeDescriptorArray                    = VK_TRUE;
    vk12_features.bufferDeviceAddress                       = VK_TRUE;
    vk12_features.scalarBlockLayout                         = VK_TRUE;
    vk12_features.pNext                                     = present_wait_features;

    VkPhysicalDeviceVulkan13Features vk13_features = {};
    vk13_features.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vk13_features.pNext            = &vk12_features;
    vk13_features.synchronization2 = VK_TRUE;
    vk13_features.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceFeatures vk10_features = {};
    vk10_features.multiDrawIndirect         = VK_TRUE;
    vk10_features.drawIndirectFirstInstance = VK_TRUE;

    VkDeviceCreateInfo device_info = {};
    device_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext                   = &vk13_features;
    device_info.queueCreateInfoCount    = 1;
    device_info.pQueueCreateInfos       = &queue_info;
    device_info.enabledExtensionCount   = device_extensions_len;
    device_info.ppEnabledExtensionNames = (const char* const*)device_extensions;
    device_info.pEnabledFeatures        = &vk10_features;
    VK_VERIFY(vkCreateDevice(vk->physical_device, &device_info, NULL, &vk->device));
    volkLoadDevice(vk->device);
    vkGetDeviceQueue(vk->device, queue_family, 0, &vk->queue);

    // Swapchain format: prefer 8-bit BGRA/RGBA UNORM, otherwise whatever comes first
    u32 formats_len = 0;
    VK_VERIFY(vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physical_device, vk->surface, &formats_len, NULL));
    VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*)stack_alloc(scratch, formats_len * sizeof(VkSurfaceFormatKHR));
    VK_VERIFY(vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physical_device, vk->surface, &formats_len, formats));
    vk->swapchain_format = formats[0].format;
    for(i32 i = 0; i < formats_len; i++) {
        if((formats[i].format == VK_FORMAT_B8G8R8A8_UNORM || formats[i].format == VK_FORMAT_R8G8B8A8_UNORM)
        && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            vk->swapchain_format = formats[i].format;
            break;
        }
    }
    vk_swapchain_create(vk, window_size, "initial");

    // Command pool and frames in flight
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = queue_family;
    VK_VERIFY(vkCreateCommandPool(vk->device, &pool_info, NULL, &vk->command_pool));

    // GPU timestamps, if the queue family supports them
    u32 families_len = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vk->physical_device, &families_len, NULL);
    VkQueueFamilyProperties* families = (VkQueueFamilyProperties*)stack_alloc(scratch, families_len * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(vk->physical_device, &families_len, families);
    u32 timestamp_bits = families[queue_family].timestampValidBits;
    if(timestamp_bits == 0 || device_properties.limits.timestampPeriod == 0.0f) {
        log_print(LOG_RENDER, "Vulkan: queue family %u can't write timestamps, GPU frame time is off", queue_family);
    } else {
        vk->timestamp_period = device_properties.limits.timestampPeriod;
        vk->timestamp_mask = timestamp_bits >= 64 ? ~0ull : (1ull << timestamp_bits) - 1;
        VkQueryPoolCreateInfo query_pool_info = {};
        query_pool_info.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        query_pool_info.queryType  = VK_QUERY_TYPE_TIMESTAMP;
        query_pool_info.queryCount = 2 * VK_FRAMES_IN_FLIGHT;
        VK_VERIFY(vkCreateQueryPool(vk->device, &query_pool_info, NULL, &vk->timestamp_query_pool));
        log_print(LOG_RENDER, "Vulkan: GPU timestamps, %u valid bits, %.3f ns per tick", timestamp_bits, vk->timestamp_period);
    }

    // Per-frame memory: prefer device-local host-visible memory (resizable BAR),
    // so the GPU reads the game's writes without going over the bus each draw.
    VkMemoryPropertyFlags host_flags[2] = {
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };
    vk->host_arena = vk_arena_new(vk, "host frames", VK_FRAMES_IN_FLIGHT * VK_FRAME_MEMORY_SIZE, 0xFFFFFFFF,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, host_flags, 2);

    for(i32 i = 0; i < VK_FRAMES_IN_FLIGHT; i++) {
        VkFrame* frame = &vk->frames[i];
        frame->memory_offset = vk_arena_alloc(&vk->host_arena, VK_FRAME_MEMORY_SIZE, 256);

        VkCommandBufferAllocateInfo command_buffer_info = {};
        command_buffer_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_buffer_info.commandPool        = vk->command_pool;
        command_buffer_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_buffer_info.commandBufferCount = 1;
        VK_VERIFY(vkAllocateCommandBuffers(vk->device, &command_buffer_info, &frame->command_buffer));

        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_VERIFY(vkCreateFence(vk->device, &fence_info, NULL, &frame->fence));

        VkSemaphoreCreateInfo semaphore_info = {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_VERIFY(vkCreateSemaphore(vk->device, &semaphore_info, NULL, &frame->image_acquired));
    }
}

void vk_load_assets(VkContext* vk, RenderSetup* setup) {
    assert(setup->buffer_regions_len <= RENDER_MAX_BUFFER_REGIONS);
    assert(setup->textures_len <= RENDER_MAX_TEXTURES);
    assert(setup->samplers_len <= RENDER_MAX_SAMPLERS);
    assert(setup->passes_len <= RENDER_MAX_PASSES);
    VkMemoryPropertyFlags device_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    // Buffer regions, packed into one device-local buffer
    u64 regions_size = 0;
    for(i32 i = 0; i < setup->buffer_regions_len; i++) {
        regions_size += (setup->buffer_regions[i].size + 255) / 256 * 256;
    }
    vk->buffer_arena = vk_arena_new(vk, "buffer regions", regions_size > 0 ? regions_size : 256, 0xFFFFFFFF,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, &device_flags, 1);

    // Texture images, packed into one device-local allocation
    VkFormat texture_formats[RENDER_MAX_TEXTURES];
    u64 texture_memory_offsets[RENDER_MAX_TEXTURES];
    u64 textures_memory_size = 0;
    u64 textures_pixels_size = 0;
    u32 textures_type_bits = 0xFFFFFFFF;
    for(i32 i = 0; i < setup->textures_len; i++) {
        TextureAsset* texture = setup->textures[i];
        texture_formats[i] = texture->format == TEXTURE_FORMAT_RGBA ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8_UNORM;
        textures_pixels_size += texture->width * texture->height * (texture->format == TEXTURE_FORMAT_RGBA ? 4 : 1);

        VkImageCreateInfo image_info = {};
        image_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType     = VK_IMAGE_TYPE_2D;
        image_info.format        = texture_formats[i];
        image_info.extent        = (VkExtent3D){ texture->width, texture->height, 1 };
        image_info.mipLevels     = 1;
        image_info.arrayLayers   = 1;
        image_info.samples       = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling        = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VK_VERIFY(vkCreateImage(vk->device, &image_info, NULL, &vk->textures[i]));

        VkMemoryRequirements requirements;
        vkGetImageMemoryRequirements(vk->device, vk->textures[i], &requirements);
        texture_memory_offsets[i] = (textures_memory_size + requirements.alignment - 1) / requirements.alignment * requirements.alignment;
        textures_memory_size = texture_memory_offsets[i] + requirements.size;
        textures_type_bits &= requirements.memoryTypeBits;
    }
    if(setup->textures_len > 0) {
        vk->image_arena = vk_arena_new(vk, "textures", textures_memory_size, textures_type_bits, 0, &device_flags, 1);
        for(i32 i = 0; i < setup->textures_len; i++) {
            VK_VERIFY(vkBindImageMemory(vk->device, vk->textures[i], vk->image_arena.memory, texture_memory_offsets[i]));
            vk->texture_views[i] = vk_image_view_new(vk, vk->textures[i], texture_formats[i], VK_IMAGE_ASPECT_COLOR_BIT);
        }
    }

    // Stage everything in one host-visible buffer
    VkMemoryPropertyFlags staging_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    log_print(LOG_RENDER, "Vulkan: loading %u buffer regions (%" PRIu64 " bytes), %u textures (%" PRIu64 " pixel bytes), %u samplers, %u passes",
              setup->buffer_regions_len, regions_size, setup->textures_len, textures_pixels_size, setup->samplers_len, setup->passes_len);
    VkArena staging = vk_arena_new(vk, "staging", regions_size + textures_pixels_size + 16, 0xFFFFFFFF,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &staging_flags, 1);

    VkCommandBufferAllocateInfo command_buffer_info = {};
    command_buffer_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_info.commandPool        = vk->command_pool;
    command_buffer_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_info.commandBufferCount = 1;
    VkCommandBuffer command_buffer;
    VK_VERIFY(vkAllocateCommandBuffers(vk->device, &command_buffer_info, &command_buffer));
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_VERIFY(vkBeginCommandBuffer(command_buffer, &begin_info));

    // Copy buffer regions
    for(i32 i = 0; i < setup->buffer_regions_len; i++) {
        RenderBufferRegion* region = &setup->buffer_regions[i];
        u64 dst_offset = vk_arena_alloc(&vk->buffer_arena, region->size, 256);
        u64 src_offset = vk_arena_alloc(&staging, region->size, 16);
        memcpy(&staging.mapped[src_offset], region->data, region->size);
        vk->buffer_region_addresses[i] = vk->buffer_arena.address + dst_offset;
        if(region->size == 0) continue;

        VkBufferCopy copy = {};
        copy.srcOffset = src_offset;
        copy.dstOffset = dst_offset;
        copy.size      = region->size;
        vkCmdCopyBuffer(command_buffer, staging.buffer, vk->buffer_arena.buffer, 1, &copy);
    }

    // Copy texture pixels
    for(i32 i = 0; i < setup->textures_len; i++) {
        TextureAsset* texture = setup->textures[i];
        u64 pixels_size = texture->width * texture->height * (texture->format == TEXTURE_FORMAT_RGBA ? 4 : 1);
        u64 src_offset = vk_arena_alloc(&staging, pixels_size, 16);
        memcpy(&staging.mapped[src_offset], &setup->texture_pixels[texture->pixels_offset], pixels_size);

        vk_image_barrier(command_buffer, vk->textures[i], VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

        VkBufferImageCopy copy = {};
        copy.bufferOffset                = src_offset;
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent                 = (VkExtent3D){ texture->width, texture->height, 1 };
        vkCmdCopyBufferToImage(command_buffer, staging.buffer, vk->textures[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        vk_image_barrier(command_buffer, vk->textures[i], VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    }

    // Submit and wait, then release the staging memory
    VK_VERIFY(vkEndCommandBuffer(command_buffer));
    VkSubmitInfo submit_info = {};
    submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers    = &command_buffer;
    VK_VERIFY(vkQueueSubmit(vk->queue, 1, &submit_info, VK_NULL_HANDLE));
    VK_VERIFY(vkQueueWaitIdle(vk->queue));
    vkFreeCommandBuffers(vk->device, vk->command_pool, 1, &command_buffer);
    vk_arena_free(vk, &staging);

    // Samplers
    for(i32 i = 0; i < setup->samplers_len; i++) {
        RenderSamplerDesc* desc = &setup->samplers[i];
        VkFilter filter = desc->filter == RENDER_FILTER_LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        VkSamplerAddressMode address = desc->address == RENDER_ADDRESS_REPEAT ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VkSamplerCreateInfo sampler_info = {};
        sampler_info.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter    = filter;
        sampler_info.minFilter    = filter;
        sampler_info.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_info.addressModeU = address;
        sampler_info.addressModeV = address;
        sampler_info.addressModeW = address;
        sampler_info.maxLod       = VK_LOD_CLAMP_NONE;
        VK_VERIFY(vkCreateSampler(vk->device, &sampler_info, NULL, &vk->samplers[i]));
    }

    // Descriptor set: every texture and sampler, bound once for the whole frame
    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[0].descriptorCount = RENDER_MAX_TEXTURES;
    bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding         = 1;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindings[1].descriptorCount = RENDER_MAX_SAMPLERS;
    bindings[1].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorBindingFlags binding_flags[2] = { VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT };

    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info = {};
    binding_flags_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    binding_flags_info.bindingCount  = 2;
    binding_flags_info.pBindingFlags = binding_flags;

    VkDescriptorSetLayoutCreateInfo set_layout_info = {};
    set_layout_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_info.pNext        = &binding_flags_info;
    set_layout_info.bindingCount = 2;
    set_layout_info.pBindings    = bindings;
    VK_VERIFY(vkCreateDescriptorSetLayout(vk->device, &set_layout_info, NULL, &vk->descriptor_set_layout));

    VkDescriptorPoolSize pool_sizes[2] = {
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, RENDER_MAX_TEXTURES },
        { VK_DESCRIPTOR_TYPE_SAMPLER,       RENDER_MAX_SAMPLERS },
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets       = 1;
    pool_info.poolSizeCount = 2;
    pool_info.pPoolSizes    = pool_sizes;
    VK_VERIFY(vkCreateDescriptorPool(vk->device, &pool_info, NULL, &vk->descriptor_pool));

    VkDescriptorSetAllocateInfo set_info = {};
    set_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_info.descriptorPool     = vk->descriptor_pool;
    set_info.descriptorSetCount = 1;
    set_info.pSetLayouts        = &vk->descriptor_set_layout;
    VK_VERIFY(vkAllocateDescriptorSets(vk->device, &set_info, &vk->descriptor_set));

    VkDescriptorImageInfo image_infos[RENDER_MAX_TEXTURES] = {};
    for(i32 i = 0; i < setup->textures_len; i++) {
        image_infos[i].imageView   = vk->texture_views[i];
        image_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    VkDescriptorImageInfo sampler_infos[RENDER_MAX_SAMPLERS] = {};
    for(i32 i = 0; i < setup->samplers_len; i++) {
        sampler_infos[i].sampler = vk->samplers[i];
    }
    VkWriteDescriptorSet writes[2] = {};
    u32 writes_len = 0;
    if(setup->textures_len > 0) {
        writes[writes_len].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writes_len].dstSet          = vk->descriptor_set;
        writes[writes_len].dstBinding      = 0;
        writes[writes_len].descriptorCount = setup->textures_len;
        writes[writes_len].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[writes_len].pImageInfo      = image_infos;
        writes_len++;
    }
    if(setup->samplers_len > 0) {
        writes[writes_len].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writes_len].dstSet          = vk->descriptor_set;
        writes[writes_len].dstBinding      = 1;
        writes[writes_len].descriptorCount = setup->samplers_len;
        writes[writes_len].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
        writes[writes_len].pImageInfo      = sampler_infos;
        writes_len++;
    }
    vkUpdateDescriptorSets(vk->device, writes_len, writes, 0, NULL);

    // Pipeline layout shared by every pass
    VkPushConstantRange push_range = {};
    push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push_range.size       = sizeof(RenderPushConstants);

    VkPipelineLayoutCreateInfo layout_info = {};
    layout_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount         = 1;
    layout_info.pSetLayouts            = &vk->descriptor_set_layout;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges    = &push_range;
    VK_VERIFY(vkCreatePipelineLayout(vk->device, &layout_info, NULL, &vk->pipeline_layout));

    // One pipeline per pass. There is no vertex input: shaders pull vertices
    // from buffer regions themselves.
    for(i32 i = 0; i < setup->passes_len; i++) {
        RenderPassDesc* pass = &setup->passes[i];

        VkShaderModule modules[2];
        ShaderData* shaders[2] = { pass->vertex_shader, pass->fragment_shader };
        VkPipelineShaderStageCreateInfo stages[2] = {};
        for(i32 j = 0; j < 2; j++) {
            VkShaderModuleCreateInfo module_info = {};
            module_info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_info.codeSize = shaders[j]->code_size;
            module_info.pCode    = shaders[j]->code;
            VK_VERIFY(vkCreateShaderModule(vk->device, &module_info, NULL, &modules[j]));
            stages[j].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[j].stage  = shaders[j]->stage == SHADER_STAGE_VERTEX ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[j].module = modules[j];
            stages[j].pName  = "main";
        }

        VkPipelineVertexInputStateCreateInfo vertex_input = {};
        vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
        input_assembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewport = {};
        viewport.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport.viewportCount = 1;
        viewport.scissorCount  = 1;

        // The viewport is flipped (see vk_frame_end) so clip space is y-up as
        // in OpenGL, which keeps counter-clockwise triangles front facing.
        VkPipelineRasterizationStateCreateInfo rasterization = {};
        rasterization.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode    = pass->cull == RENDER_CULL_BACK ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
        rasterization.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample = {};
        multisample.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depth = {};
        depth.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth.depthTestEnable  = pass->depth_test;
        depth.depthWriteEnable = pass->depth_write;
        depth.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;

        VkPipelineColorBlendAttachmentState blend_attachment = {};
        blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        if(pass->blend == RENDER_BLEND_ALPHA) {
            blend_attachment.blendEnable         = VK_TRUE;
            blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
            blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;
        }
        VkPipelineColorBlendStateCreateInfo blend = {};
        blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments    = &blend_attachment;

        VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic = {};
        dynamic.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates    = dynamic_states;

        VkPipelineRenderingCreateInfo rendering = {};
        rendering.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering.colorAttachmentCount    = 1;
        rendering.pColorAttachmentFormats = &vk->swapchain_format;
        rendering.depthAttachmentFormat   = VK_DEPTH_FORMAT;

        VkGraphicsPipelineCreateInfo pipeline_info = {};
        pipeline_info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.pNext               = &rendering;
        pipeline_info.stageCount          = 2;
        pipeline_info.pStages             = stages;
        pipeline_info.pVertexInputState   = &vertex_input;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState      = &viewport;
        pipeline_info.pRasterizationState = &rasterization;
        pipeline_info.pMultisampleState   = &multisample;
        pipeline_info.pDepthStencilState  = &depth;
        pipeline_info.pColorBlendState    = &blend;
        pipeline_info.pDynamicState       = &dynamic;
        pipeline_info.layout              = vk->pipeline_layout;
        VK_VERIFY(vkCreateGraphicsPipelines(vk->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &vk->pipelines[i]));
        log_print(LOG_RENDER, "Vulkan: pipeline %i created (blend %i, cull %i, depth test %i, depth write %i)",
                  i, (i32)pass->blend, (i32)pass->cull, (i32)pass->depth_test, (i32)pass->depth_write);

        vkDestroyShaderModule(vk->device, modules[0], NULL);
        vkDestroyShaderModule(vk->device, modules[1], NULL);
    }
    vk->pipelines_len = setup->passes_len;
}

void vk_frame_begin(VkContext* vk, iv2 window_size, RenderFrame* out_frame) {
    VkFrame* frame = &vk->frames[vk->frame_index];

    // Wait for the last present to reach the screen, so this frame's present is the
    // only one queued and the input read after this is as fresh as it can be. The
    // timeout keeps a hidden window, which may never present, from stalling the loop.
    if(vk->present_wait_mode != VK_PRESENT_WAIT_MODE_NONE && vk->present_id > 0 && !vk->swapchain_stale) {
        u64 timeout = 100ull * 1000 * 1000;
        VkResult result = VK_SUCCESS;
        if(vk->present_wait_mode == VK_PRESENT_WAIT_MODE_1) {
            result = vkWaitForPresentKHR(vk->device, vk->swapchain, vk->present_id, timeout);
        } else {
#ifdef VK_KHR_present_wait2
            VkPresentWait2InfoKHR wait_info = {};
            wait_info.sType     = VK_STRUCTURE_TYPE_PRESENT_WAIT_2_INFO_KHR;
            wait_info.presentId = vk->present_id;
            wait_info.timeout   = timeout;
            result = vkWaitForPresent2KHR(vk->device, vk->swapchain, &wait_info);
#endif
        }
        if(result == VK_TIMEOUT) {
            log_print(LOG_RENDER_VERBOSE, "Vulkan: present %" PRIu64 " not on screen after 100 ms", vk->present_id);
        } else if(result == VK_ERROR_OUT_OF_DATE_KHR) {
            log_print(LOG_RENDER, "Vulkan: present wait out of date, recreating swapchain");
            vk->swapchain_stale = true;
        } else if(result != VK_SUBOPTIMAL_KHR) {
            VK_VERIFY(result);
        }
    }

    VK_VERIFY(vkWaitForFences(vk->device, 1, &frame->fence, VK_TRUE, UINT64_MAX));

    // The fence has signaled, so this frame's timestamps from its last use are ready
    if(frame->timestamps_written) {
        u64 timestamps[2];
        VK_VERIFY(vkGetQueryPoolResults(vk->device, vk->timestamp_query_pool, 2 * vk->frame_index, 2,
                                        sizeof(timestamps), timestamps, sizeof(u64), VK_QUERY_RESULT_64_BIT));
        u64 ticks = ((timestamps[1] & vk->timestamp_mask) - (timestamps[0] & vk->timestamp_mask)) & vk->timestamp_mask;
        profile_add(PROFILE_GPU, (u64)((f64)ticks * (f64)vk->timestamp_period));
        frame->timestamps_written = false;
    }

    // Acquire a swapchain image, recreating the swapchain when it no longer fits the window
    bool window_resized = !iv2_eq(window_size, vk->swapchain_window_size);
    if(window_resized || vk->swapchain_stale) {
        vk_swapchain_create(vk, window_size, window_resized ? "window resized" : "stale");
    }
    VkResult result = vkAcquireNextImageKHR(vk->device, vk->swapchain, UINT64_MAX, frame->image_acquired, VK_NULL_HANDLE, &vk->image_index);
    if(result == VK_ERROR_OUT_OF_DATE_KHR) {
        vk_swapchain_create(vk, window_size, "acquire out of date");
        result = vkAcquireNextImageKHR(vk->device, vk->swapchain, UINT64_MAX, frame->image_acquired, VK_NULL_HANDLE, &vk->image_index);
    }
    if(result == VK_SUBOPTIMAL_KHR) {
        log_print(LOG_RENDER, "Vulkan: acquire suboptimal, recreating swapchain next frame");
        vk->swapchain_stale = true;
    } else {
        VK_VERIFY(result);
    }
    log_print(LOG_RENDER_VERBOSE, "Vulkan: frame %u acquired image %u", vk->frame_index, vk->image_index);
    VK_VERIFY(vkResetFences(vk->device, 1, &frame->fence));

    // Hand the game this frame's slice of mapped memory
    u8* memory = &vk->host_arena.mapped[frame->memory_offset];
    *out_frame = (RenderFrame){};
    out_frame->globals   = memory;
    out_frame->instances = memory + RENDER_FRAME_GLOBALS_SIZE;
    for(i32 i = 0; i < RENDER_MAX_PASSES; i++) {
        out_frame->commands[i] = (RenderDrawCommand*)(memory + RENDER_FRAME_GLOBALS_SIZE + RENDER_FRAME_INSTANCES_SIZE) + i * RENDER_FRAME_MAX_COMMANDS;
    }
}

void vk_frame_end(VkContext* vk, RenderFrame* render_frame) {
    VkFrame* frame = &vk->frames[vk->frame_index];
    VkCommandBuffer command_buffer = frame->command_buffer;
    VkImage image = vk->swapchain_images[vk->image_index];

    VK_VERIFY(vkResetCommandBuffer(command_buffer, 0));
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_VERIFY(vkBeginCommandBuffer(command_buffer, &begin_info));
    if(vk->timestamp_query_pool != VK_NULL_HANDLE) {
        vkCmdResetQueryPool(command_buffer, vk->timestamp_query_pool, 2 * vk->frame_index, 2);
    }

    // Begin rendering
    vk_image_barrier(command_buffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    vk_image_barrier(command_buffer, vk->depth_image, VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    // Start the GPU timer after the barriers, which wait for the acquired image,
    // so the time measures rendering rather than waiting on the swapchain
    if(vk->timestamp_query_pool != VK_NULL_HANDLE) {
        vkCmdWriteTimestamp2(command_buffer, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, vk->timestamp_query_pool, 2 * vk->frame_index);
    }

    v4 clear = render_frame->clear_color;
    VkRenderingAttachmentInfo color_attachment = {};
    color_attachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attachment.imageView   = vk->swapchain_views[vk->image_index];
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.clearValue.color = (VkClearColorValue){ .float32 = { clear.x, clear.y, clear.z, clear.w } };

    VkRenderingAttachmentInfo depth_attachment = {};
    depth_attachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth_attachment.imageView   = vk->depth_view;
    depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depth_attachment.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp     = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.clearValue.depthStencil.depth = 1.0f;

    VkRenderingInfo rendering_info = {};
    rendering_info.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea.extent    = vk->swapchain_extent;
    rendering_info.layerCount           = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments    = &color_attachment;
    rendering_info.pDepthAttachment     = &depth_attachment;
    vkCmdBeginRendering(command_buffer, &rendering_info);

    // Negative height flips y so clip space is y-up, matching handrail's math.h
    VkViewport viewport = {};
    viewport.y        = (f32)vk->swapchain_extent.height;
    viewport.width    = (f32)vk->swapchain_extent.width;
    viewport.height   = -(f32)vk->swapchain_extent.height;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor = {};
    scissor.extent = vk->swapchain_extent;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    // One indirect draw per pass
    RenderPushConstants push = {};
    push.globals   = vk->host_arena.address + frame->memory_offset;
    push.instances = push.globals + RENDER_FRAME_GLOBALS_SIZE;
    memcpy(push.buffer_regions, vk->buffer_region_addresses, sizeof(push.buffer_regions));
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->pipeline_layout, 0, 1, &vk->descriptor_set, 0, NULL);
    vkCmdPushConstants(command_buffer, vk->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RenderPushConstants), &push);
    for(i32 i = 0; i < vk->pipelines_len; i++) {
        u32 commands_len = render_frame->commands_len[i];
        log_print(LOG_RENDER_VERBOSE, "Vulkan: pass %i, %u draw commands", i, commands_len);
        if(commands_len == 0) continue;
        assert(commands_len <= RENDER_FRAME_MAX_COMMANDS);
        u64 commands_offset = frame->memory_offset + RENDER_FRAME_GLOBALS_SIZE + RENDER_FRAME_INSTANCES_SIZE
                            + i * RENDER_FRAME_MAX_COMMANDS * sizeof(RenderDrawCommand);
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->pipelines[i]);
        vkCmdDrawIndirect(command_buffer, vk->host_arena.buffer, commands_offset, commands_len, sizeof(RenderDrawCommand));
    }

    vkCmdEndRendering(command_buffer);
    vk_image_barrier(command_buffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
    if(vk->timestamp_query_pool != VK_NULL_HANDLE) {
        vkCmdWriteTimestamp2(command_buffer, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, vk->timestamp_query_pool, 2 * vk->frame_index + 1);
        frame->timestamps_written = true;
    }
    VK_VERIFY(vkEndCommandBuffer(command_buffer));

    // Submit
    VkSemaphoreSubmitInfo wait_info = {};
    wait_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    wait_info.semaphore = frame->image_acquired;
    wait_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSemaphoreSubmitInfo signal_info = {};
    signal_info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signal_info.semaphore = vk->render_finished[vk->image_index];
    signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    VkCommandBufferSubmitInfo command_buffer_info = {};
    command_buffer_info.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_buffer_info.commandBuffer = command_buffer;
    VkSubmitInfo2 submit_info = {};
    submit_info.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.waitSemaphoreInfoCount   = 1;
    submit_info.pWaitSemaphoreInfos      = &wait_info;
    submit_info.commandBufferInfoCount   = 1;
    submit_info.pCommandBufferInfos      = &command_buffer_info;
    submit_info.signalSemaphoreInfoCount = 1;
    submit_info.pSignalSemaphoreInfos    = &signal_info;
    VK_VERIFY(vkQueueSubmit2(vk->queue, 1, &submit_info, frame->fence));

    // Present
    VkPresentInfoKHR present_info = {};
    present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = &vk->render_finished[vk->image_index];
    present_info.swapchainCount     = 1;
    present_info.pSwapchains        = &vk->swapchain;
    present_info.pImageIndices      = &vk->image_index;

    // Tag the present so the next frame begin can wait for it
    VkPresentIdKHR present_id_info = {};
    present_id_info.sType          = VK_STRUCTURE_TYPE_PRESENT_ID_KHR;
    present_id_info.swapchainCount = 1;
    present_id_info.pPresentIds    = &vk->present_id;
#ifdef VK_KHR_present_wait2
    VkPresentId2KHR present_id2_info = {};
    present_id2_info.sType          = VK_STRUCTURE_TYPE_PRESENT_ID_2_KHR;
    present_id2_info.swapchainCount = 1;
    present_id2_info.pPresentIds    = &vk->present_id;
    if(vk->present_wait_mode == VK_PRESENT_WAIT_MODE_2) {
        present_info.pNext = &present_id2_info;
    }
#endif
    if(vk->present_wait_mode == VK_PRESENT_WAIT_MODE_1) {
        present_info.pNext = &present_id_info;
    }
    if(vk->present_wait_mode != VK_PRESENT_WAIT_MODE_NONE) {
        vk->present_id++;
    }

    VkResult result = vkQueuePresentKHR(vk->queue, &present_info);
    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        log_print(LOG_RENDER, "Vulkan: present %s, recreating swapchain next frame",
                  result == VK_SUBOPTIMAL_KHR ? "suboptimal" : "out of date");
        vk->swapchain_stale = true;
    } else {
        VK_VERIFY(result);
    }

    vk->frame_index = (vk->frame_index + 1) % VK_FRAMES_IN_FLIGHT;
}

#endif
