/* Automatically generated from Vulkan vk.xml and video.xml; DO NOT EDIT!
 *
 * This file is generated from Vulkan vk.xml file covered
 * by the following copyright and permission notice:
 *
 * Copyright 2015-2024 The Khronos Group Inc.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 * and from Vulkan video.xml file covered
 * by the following copyright and permission notice:
 *
 * Copyright 2021-2024 The Khronos Group Inc.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#ifndef __WINE_VULKAN_THUNKS_H
#define __WINE_VULKAN_THUNKS_H

#define WINE_VK_VERSION VK_API_VERSION_1_4

/* Functions for which we have custom implementations outside of the thunks. */
VkResult wine_vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo *pAllocateInfo, VkCommandBuffer *pCommandBuffers);
VkResult wine_vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo *pAllocateInfo, const VkAllocationCallbacks *pAllocator, VkDeviceMemory *pMemory);
VkResult wine_vkCreateBuffer(VkDevice device, const VkBufferCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer);
VkResult wine_vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkCommandPool *pCommandPool, void *client_ptr);
VkResult wine_vkCreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugReportCallbackEXT *pCallback);
VkResult wine_vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pMessenger);
VkResult wine_vkCreateDeferredOperationKHR(VkDevice device, const VkAllocationCallbacks *pAllocator, VkDeferredOperationKHR *pDeferredOperation);
VkResult wine_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDevice *pDevice, void *client_ptr);
VkResult wine_vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkImage *pImage);
VkResult wine_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkInstance *pInstance, void *client_ptr);
VkResult wine_vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSemaphore *pSemaphore);
void wine_vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks *pAllocator);
void wine_vkDestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks *pAllocator);
void wine_vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks *pAllocator);
void wine_vkDestroyDeferredOperationKHR(VkDevice device, VkDeferredOperationKHR operation, const VkAllocationCallbacks *pAllocator);
void wine_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator);
void wine_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *pAllocator);
void wine_vkDestroySemaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks *pAllocator);
VkResult wine_vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char *pLayerName, uint32_t *pPropertyCount, VkExtensionProperties *pProperties);
VkResult wine_vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t *pPropertyCount, VkLayerProperties *pProperties);
VkResult wine_vkEnumerateInstanceExtensionProperties(const char *pLayerName, uint32_t *pPropertyCount, VkExtensionProperties *pProperties);
VkResult wine_vkEnumerateInstanceVersion(uint32_t *pApiVersion);
VkResult wine_vkEnumeratePhysicalDeviceGroups(VkInstance instance, uint32_t *pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties);
VkResult wine_vkEnumeratePhysicalDeviceGroupsKHR(VkInstance instance, uint32_t *pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties);
VkResult wine_vkEnumeratePhysicalDevices(VkInstance instance, uint32_t *pPhysicalDeviceCount, VkPhysicalDevice *pPhysicalDevices);
void wine_vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer *pCommandBuffers);
void wine_vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *pAllocator);
VkResult wine_vkGetCalibratedTimestampsEXT(VkDevice device, uint32_t timestampCount, const VkCalibratedTimestampInfoKHR *pTimestampInfos, uint64_t *pTimestamps, uint64_t *pMaxDeviation);
VkResult wine_vkGetCalibratedTimestampsKHR(VkDevice device, uint32_t timestampCount, const VkCalibratedTimestampInfoKHR *pTimestampInfos, uint64_t *pTimestamps, uint64_t *pMaxDeviation);
void wine_vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue *pQueue);
void wine_vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2 *pQueueInfo, VkQueue *pQueue);
VkResult wine_vkGetMemoryWin32HandleKHR(VkDevice device, const VkMemoryGetWin32HandleInfoKHR *pGetWin32HandleInfo, HANDLE *pHandle);
VkResult wine_vkGetMemoryWin32HandlePropertiesKHR(VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, HANDLE handle, VkMemoryWin32HandlePropertiesKHR *pMemoryWin32HandleProperties);
VkResult wine_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(VkPhysicalDevice physicalDevice, uint32_t *pTimeDomainCount, VkTimeDomainKHR *pTimeDomains);
VkResult wine_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR(VkPhysicalDevice physicalDevice, uint32_t *pTimeDomainCount, VkTimeDomainKHR *pTimeDomains);
void wine_vkGetPhysicalDeviceExternalBufferProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo *pExternalBufferInfo, VkExternalBufferProperties *pExternalBufferProperties);
void wine_vkGetPhysicalDeviceExternalBufferPropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo *pExternalBufferInfo, VkExternalBufferProperties *pExternalBufferProperties);
void wine_vkGetPhysicalDeviceExternalFenceProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo *pExternalFenceInfo, VkExternalFenceProperties *pExternalFenceProperties);
void wine_vkGetPhysicalDeviceExternalFencePropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo *pExternalFenceInfo, VkExternalFenceProperties *pExternalFenceProperties);
void wine_vkGetPhysicalDeviceExternalSemaphoreProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo *pExternalSemaphoreInfo, VkExternalSemaphoreProperties *pExternalSemaphoreProperties);
void wine_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo *pExternalSemaphoreInfo, VkExternalSemaphoreProperties *pExternalSemaphoreProperties);
VkResult wine_vkGetPhysicalDeviceImageFormatProperties2(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2 *pImageFormatInfo, VkImageFormatProperties2 *pImageFormatProperties);
VkResult wine_vkGetPhysicalDeviceImageFormatProperties2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2 *pImageFormatInfo, VkImageFormatProperties2 *pImageFormatProperties);
VkResult wine_vkGetSemaphoreCounterValue(VkDevice device, VkSemaphore semaphore, uint64_t *pValue);
VkResult wine_vkGetSemaphoreCounterValueKHR(VkDevice device, VkSemaphore semaphore, uint64_t *pValue);
VkResult wine_vkGetSemaphoreWin32HandleKHR(VkDevice device, const VkSemaphoreGetWin32HandleInfoKHR *pGetWin32HandleInfo, HANDLE *pHandle);
VkResult wine_vkImportSemaphoreWin32HandleKHR(VkDevice device, const VkImportSemaphoreWin32HandleInfoKHR *pImportSemaphoreWin32HandleInfo);
VkResult wine_vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void **ppData);
VkResult wine_vkMapMemory2KHR(VkDevice device, const VkMemoryMapInfo *pMemoryMapInfo, void **ppData);
VkResult wine_vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo *pBindInfo, VkFence fence);
VkResult wine_vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits, VkFence fence);
VkResult wine_vkQueueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2 *pSubmits, VkFence fence);
VkResult wine_vkQueueSubmit2KHR(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2 *pSubmits, VkFence fence);
VkResult wine_vkSignalSemaphore(VkDevice device, const VkSemaphoreSignalInfo *pSignalInfo);
VkResult wine_vkSignalSemaphoreKHR(VkDevice device, const VkSemaphoreSignalInfo *pSignalInfo);
void wine_vkUnmapMemory(VkDevice device, VkDeviceMemory memory);
VkResult wine_vkUnmapMemory2KHR(VkDevice device, const VkMemoryUnmapInfo *pMemoryUnmapInfo);
VkResult wine_vkWaitSemaphores(VkDevice device, const VkSemaphoreWaitInfo *pWaitInfo, uint64_t timeout);
VkResult wine_vkWaitSemaphoresKHR(VkDevice device, const VkSemaphoreWaitInfo *pWaitInfo, uint64_t timeout);
VkResult wine_wine_vkAcquireKeyedMutex(VkDevice device, VkDeviceMemory memory, uint64_t key, uint32_t timeout_ms);
VkResult wine_wine_vkReleaseKeyedMutex(VkDevice device, VkDeviceMemory memory, uint64_t key);

#endif /* __WINE_VULKAN_THUNKS_H */
