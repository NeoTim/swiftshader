// Copyright 2019 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "Driver.hpp"

#if defined(_WIN64)
#    include "Windows.h"
#    define OS_WINDOWS 1
#elif defined(__APPLE__)
#    include "dlfcn.h"
#    define OS_MAC 1
#elif defined(__linux__)
#    include "dlfcn.h"
#    define OS_LINUX 1
#else
#    error Unimplemented platform
#endif

Driver::Driver() : vk_icdGetInstanceProcAddr(nullptr), dll(nullptr)
{
#define VK_GLOBAL(N, R, ...) N = nullptr;
#include "VkGlobalFuncs.hpp"
#undef VK_GLOBAL

#define VK_INSTANCE(N, R, ...) N = nullptr;
#include "VkInstanceFuncs.hpp"
#undef VK_INSTANCE
}

Driver::~Driver()
{
    unload();
}

bool Driver::loadSwiftShader()
{
#if OS_WINDOWS
#    if defined(NDEBUG)
    return load("../../out/Release_x64/vk_swiftshader.dll");
#    else
    return load("../../out/Debug_x64/vk_swiftshader.dll");
#    endif
#elif OS_MAC
    return load("./out/Darwin/libvk_swiftshader.dylib");
#elif OS_LINUX
    return load("./out/Linux/libvk_swiftshader.so");
#else
#    error Unimplemented platform
#endif
}

bool Driver::loadSystem()
{
#if OS_LINUX
    return load("libvulkan.so.1");
#else
    return false;
#endif
}

bool Driver::load(const char* path)
{
#if OS_WINDOWS
    dll = LoadLibraryA(path);
#elif(OS_MAC || OS_LINUX)
    dll = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
#else
    return false;
#endif
    if(dll == nullptr)
    {
        return false;
    }

    // Is the driver an ICD?
    if(!lookup(&vk_icdGetInstanceProcAddr, "vk_icdGetInstanceProcAddr"))
    {
        // Nope, attempt to use the loader version.
        if(!lookup(&vk_icdGetInstanceProcAddr, "vkGetInstanceProcAddr"))
        {
            return false;
        }
    }

#define VK_GLOBAL(N, R, ...)                                             \
    if(auto pfn = vk_icdGetInstanceProcAddr(nullptr, #N))                \
    {                                                                    \
        N = reinterpret_cast<decltype(N)>(pfn);                          \
    }
#include "VkGlobalFuncs.hpp"
#undef VK_GLOBAL

    return true;
}

void Driver::unload()
{
    if(!isLoaded())
    {
        return;
    }

#if OS_WINDOWS
    FreeLibrary((HMODULE)dll);
#elif OS_LINUX
    dlclose(dll);
#endif

#define VK_GLOBAL(N, R, ...) N = nullptr;
#include "VkGlobalFuncs.hpp"
#undef VK_GLOBAL

#define VK_INSTANCE(N, R, ...) N = nullptr;
#include "VkInstanceFuncs.hpp"
#undef VK_INSTANCE
}

bool Driver::isLoaded() const
{
    return dll != nullptr;
}

bool Driver::resolve(VkInstance instance)
{
    if(!isLoaded())
    {
        return false;
    }

#define VK_INSTANCE(N, R, ...)                             \
    if(auto pfn = vk_icdGetInstanceProcAddr(instance, #N)) \
    {                                                      \
        N = reinterpret_cast<decltype(N)>(pfn);            \
    }                                                      \
    else                                                   \
    {                                                      \
        return false;                                      \
    }
#include "VkInstanceFuncs.hpp"
#undef VK_INSTANCE

    return true;
}

void* Driver::lookup(const char* name)
{
#if OS_WINDOWS
    return GetProcAddress((HMODULE)dll, name);
#elif(OS_MAC || OS_LINUX)
    return dlsym(dll, name);
#else
    return nullptr;
#endif
}