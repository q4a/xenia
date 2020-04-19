/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2019 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/ui/vk/vulkan_provider.h"

#include <cstring>
#include <vector>

#include "xenia/base/cvar.h"
#include "xenia/base/logging.h"
#include "xenia/base/math.h"
#include "xenia/base/platform.h"
#include "xenia/ui/vk/vulkan_context.h"
#include "xenia/ui/vk/vulkan_util.h"

DEFINE_bool(vk_validation, false, "Enable Vulkan validation layers.", "Vulkan");
DEFINE_int32(vk_device, -1,
             "Index of the Vulkan physical device to use. -1 to use any "
             "compatible.",
             "Vulkan");

namespace xe {
namespace ui {
namespace vk {

std::unique_ptr<VulkanProvider> VulkanProvider::Create(Window* main_window) {
  std::unique_ptr<VulkanProvider> provider(new VulkanProvider(main_window));
  if (!provider->Initialize()) {
    xe::FatalError(
        "Unable to initialize Vulkan 1.1 graphics subsystem.\n"
        "\n"
        "Ensure you have the latest drivers for your GPU and that it supports "
        "Vulkan, and install the latest Vulkan runtime from "
        "https://vulkan.lunarg.com/sdk/home.\n"
        "\n"
        "See https://xenia.jp/faq/ for more information and a list of "
        "supported GPUs.");
    return nullptr;
  }
  return provider;
}

VulkanProvider::VulkanProvider(Window* main_window)
    : GraphicsProvider(main_window) {}

VulkanProvider::~VulkanProvider() {
  if (device_ != VK_NULL_HANDLE) {
    vkDestroyDevice(device_, nullptr);
  }
  if (instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(instance_, nullptr);
  }
}

uint32_t VulkanProvider::FindMemoryType(
    uint32_t memory_type_bits_requirement,
    VkMemoryPropertyFlags required_properties) const {
  uint32_t memory_index;
  while (xe::bit_scan_forward(memory_type_bits_requirement, &memory_index)) {
    memory_type_bits_requirement &= ~(uint32_t(1) << memory_index);
    VkMemoryPropertyFlags properties =
        physical_device_memory_properties_.memoryTypes[memory_index]
            .propertyFlags;
    if ((properties & required_properties) == required_properties) {
      return memory_index;
    }
  }
  return UINT32_MAX;
}

bool VulkanProvider::Initialize() {
  if (volkInitialize() != VK_SUCCESS) {
    XELOGE("Failed to initialize the Vulkan loader volk");
    return false;
  }

  const uint32_t api_version = VK_MAKE_VERSION(1, 1, 0);

  // Create the instance.
  VkApplicationInfo application_info;
  application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application_info.pNext = nullptr;
  application_info.pApplicationName = "Xenia";
  application_info.applicationVersion = 1;
  application_info.pEngineName = "Xenia";
  application_info.engineVersion = 1;
  application_info.apiVersion = api_version;
  const char* const validation_layers[] = {
      "VK_LAYER_LUNARG_standard_validation",
  };
  const char* const instance_extensions[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
#if XE_PLATFORM_WIN32
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif XE_PLATFORM_LINUX
#ifdef GDK_WINDOWING_X11
    VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#else
#error No Vulkan surface extension for the GDK backend defined yet.
#endif
#else
#error No Vulkan surface extension for the platform defined yet.
#endif
  };
  VkInstanceCreateInfo instance_create_info;
  instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_create_info.pNext = nullptr;
  instance_create_info.flags = 0;
  instance_create_info.pApplicationInfo = &application_info;
  if (cvars::vk_validation) {
    instance_create_info.enabledLayerCount =
        uint32_t(xe::countof(validation_layers));
    instance_create_info.ppEnabledLayerNames = validation_layers;
  } else {
    instance_create_info.enabledLayerCount = 0;
    instance_create_info.ppEnabledLayerNames = nullptr;
  }
  instance_create_info.enabledExtensionCount =
      uint32_t(xe::countof(instance_extensions));
  instance_create_info.ppEnabledExtensionNames = instance_extensions;
  if (vkCreateInstance(&instance_create_info, nullptr, &instance_) !=
      VK_SUCCESS) {
    XELOGE("Failed to create a Vulkan instance");
    return false;
  }
  volkLoadInstance(instance_);

  // Get a supported physical device.
  physical_device_ = nullptr;
  uint32_t physical_device_count;
  if (vkEnumeratePhysicalDevices(instance_, &physical_device_count, nullptr) !=
      VK_SUCCESS) {
    XELOGE("Failed to get Vulkan physical device count");
    return false;
  }
  std::vector<VkPhysicalDevice> physical_devices;
  physical_devices.resize(physical_device_count);
  if (vkEnumeratePhysicalDevices(instance_, &physical_device_count,
                                 physical_devices.data()) != VK_SUCCESS) {
    XELOGE("Failed to get Vulkan physical devices");
    return false;
  }
  uint32_t physical_device_index, physical_device_index_end;
  if (cvars::vk_device >= 0) {
    physical_device_index = uint32_t(cvars::vk_device);
    physical_device_index_end =
        std::min(physical_device_index + 1, physical_device_count);
  } else {
    physical_device_index = 0;
    physical_device_index_end = physical_device_count;
  }
  std::vector<VkExtensionProperties> physical_device_extensions;
  std::vector<VkQueueFamilyProperties> queue_families;
  bool sparse_residency_buffer = false;
  for (; physical_device_index < physical_device_index_end;
       ++physical_device_index) {
    VkPhysicalDevice physical_device = physical_devices[physical_device_index];
    vkGetPhysicalDeviceProperties(physical_device,
                                  &physical_device_properties_);
    if (physical_device_properties_.apiVersion < api_version) {
      continue;
    }
    vkGetPhysicalDeviceFeatures(physical_device, &physical_device_features_);
    if (!physical_device_features_.geometryShader) {
      continue;
    }
    uint32_t physical_device_extension_count;
    if (vkEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                             &physical_device_extension_count,
                                             nullptr) != VK_SUCCESS) {
      continue;
    }
    physical_device_extensions.resize(physical_device_extension_count);
    if (vkEnumerateDeviceExtensionProperties(
            physical_device, nullptr, &physical_device_extension_count,
            physical_device_extensions.data()) != VK_SUCCESS) {
      continue;
    }
    bool supports_swapchain = false;
    for (uint32_t i = 0; i < physical_device_extension_count; ++i) {
      const char* extension_name = physical_device_extensions[i].extensionName;
      if (!std::strcmp(extension_name, "VK_KHR_swapchain")) {
        supports_swapchain = true;
        break;
      }
    }
    if (!supports_swapchain) {
      continue;
    }
    sparse_residency_buffer = physical_device_features_.sparseBinding &&
                              physical_device_features_.sparseResidencyBuffer;
    // Get a queue supporting graphics and compute, and if available, also
    // sparse memory management.
    graphics_queue_family_ = UINT32_MAX;
    uint32_t queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                             &queue_family_count, nullptr);
    queue_families.resize(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &queue_family_count, queue_families.data());
    const uint32_t queue_flags_required =
        VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    for (uint32_t i = 0; i < queue_family_count; ++i) {
      const VkQueueFamilyProperties& queue_family_properties =
          queue_families[i];
      // Arbitrary copying done when loading textures.
      if (queue_family_properties.minImageTransferGranularity.width > 1 ||
          queue_family_properties.minImageTransferGranularity.height > 1 ||
          queue_family_properties.minImageTransferGranularity.depth > 1) {
        continue;
      }
      if ((queue_family_properties.queueFlags & queue_flags_required) !=
          queue_flags_required) {
        continue;
      }
      graphics_queue_family_ = i;
      if (!sparse_residency_buffer ||
          (queue_family_properties.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)) {
        // Found a fully compatible queue family, stop searching for a family
        // that support both graphics/compute/transfer and sparse binding.
        break;
      }
    }
    if (graphics_queue_family_ == UINT32_MAX) {
      continue;
    }
    if (!(queue_families[graphics_queue_family_].queueFlags &
          VK_QUEUE_SPARSE_BINDING_BIT)) {
      sparse_residency_buffer = false;
    }
    physical_device_ = physical_device;
    break;
  }
  if (physical_device_ == VK_NULL_HANDLE) {
    XELOGE("Failed to get a supported Vulkan physical device");
    return false;
  }
  // TODO(Triang3l): Check if VK_EXT_fragment_shader_interlock and
  // fragmentShaderSampleInterlock are supported.

  // Get the needed info about the physical device.
  vkGetPhysicalDeviceMemoryProperties(physical_device_,
                                      &physical_device_memory_properties_);

  // Log physical device properties.
  XELOGVK("Vulkan physical device: {} (vendor {:04X}, device {:04X})",
          physical_device_properties_.deviceName,
          physical_device_properties_.vendorID,
          physical_device_properties_.deviceID);

  // Create a logical device and a queue.
  float queue_priority = 1.0f;
  VkDeviceQueueCreateInfo queue_create_info;
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.pNext = nullptr;
  queue_create_info.flags = 0;
  queue_create_info.queueFamilyIndex = graphics_queue_family_;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &queue_priority;
  const char* const device_extensions[] = {
      "VK_KHR_swapchain",
  };
  // TODO(Triang3l): Add VK_EXT_fragment_shader_interlock if supported.
  VkDeviceCreateInfo device_create_info;
  device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.pNext = nullptr;
  device_create_info.flags = 0;
  device_create_info.queueCreateInfoCount = 1;
  device_create_info.pQueueCreateInfos = &queue_create_info;
  device_create_info.enabledLayerCount = 0;
  device_create_info.ppEnabledLayerNames = nullptr;
  device_create_info.enabledExtensionCount =
      uint32_t(xe::countof(device_extensions));
  device_create_info.ppEnabledExtensionNames = device_extensions;
  device_create_info.pEnabledFeatures = nullptr;
  if (vkCreateDevice(physical_device_, &device_create_info, nullptr,
                     &device_) != VK_SUCCESS) {
    XELOGE("Failed to create a Vulkan device");
    return false;
  }
  volkLoadDevice(device_);
  vkGetDeviceQueue(device_, graphics_queue_family_, 0, &graphics_queue_);

  return true;
}

std::unique_ptr<GraphicsContext> VulkanProvider::CreateContext(
    Window* target_window) {
  auto new_context =
      std::unique_ptr<VulkanContext>(new VulkanContext(this, target_window));
  if (!new_context->Initialize()) {
    return nullptr;
  }
  return std::unique_ptr<GraphicsContext>(new_context.release());
}

std::unique_ptr<GraphicsContext> VulkanProvider::CreateOffscreenContext() {
  auto new_context =
      std::unique_ptr<VulkanContext>(new VulkanContext(this, nullptr));
  if (!new_context->Initialize()) {
    return nullptr;
  }
  return std::unique_ptr<GraphicsContext>(new_context.release());
}

}  // namespace vk
}  // namespace ui
}  // namespace xe
