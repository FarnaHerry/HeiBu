// 黑簿 own-main 入口 — 手写 GLFW 窗口循环（参照 eui-neo 0.5.6 的 glfw_app_main.cpp，去托盘/子窗口）。
// include 纪律：GLFW 与 eui 头在前，import std 在后。
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <eui_neo.h>
// 注意：不要在这里 include "eui/detail/dsl_app_impl.h"——其中 app:: 函数非 inline，
// 只能由 app_config.cpp 这一个 TU 提供定义，否则重复符号。

#include "core/platform/platform.h"
#include "core/render/render_backend.h"
#include "core/window/window_backend.h"
#include "core/input/input_state.h"   // core::releaseInputQueue

import std;

namespace {

float dpiScaleOf(GLFWwindow* window) {
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    glfwGetWindowContentScale(window, &scaleX, &scaleY);
    return (scaleX + scaleY) * 0.5f;
}

float pointerScaleOf(GLFWwindow* window) {
    int windowWidth = 0;
    int windowHeight = 0;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    if (windowWidth <= 0 || windowHeight <= 0) {
        return 1.0f;
    }
    return 0.5f * (static_cast<float>(framebufferWidth) / static_cast<float>(windowWidth) +
                   static_cast<float>(framebufferHeight) / static_cast<float>(windowHeight));
}

} // namespace

int main() {
    core::platform::repairCurrentWorkingDirectory();
    core::render::initializeRenderBackendLoader();
    if (!glfwInit()) {
        return -1;
    }

    core::window::WindowCreateRequest request;
    request.width = app::initialWindowWidth();
    request.height = app::initialWindowHeight();
    request.title = app::windowTitle();
    request.renderApi = core::render::windowRenderApi();

    auto* window = static_cast<GLFWwindow*>(core::window::createWindow(request));
    if (window == nullptr) {
        glfwTerminate();
        return -1;
    }

    auto backend = core::render::createRenderBackend(window);
    if (!backend || !backend->initialize()) {
        core::window::destroyWindow(window);
        glfwTerminate();
        return -1;
    }

    if (!app::initialize(window)) {
        app::shutdown();
        backend.reset();
        core::window::destroyWindow(window);
        glfwTerminate();
        return -1;
    }

    double lastFrameTime = core::window::timeSeconds();
    bool paintRequested = true;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        const double now = core::window::timeSeconds();
        const float deltaSeconds = static_cast<float>(now - lastFrameTime);
        lastFrameTime = now;

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        if (framebufferWidth <= 0 || framebufferHeight <= 0) {
            paintRequested = true;
            glfwWaitEvents();
            lastFrameTime = core::window::timeSeconds();
            continue;
        }

        const float dpiScale = dpiScaleOf(window);
        const float pointerScale = pointerScaleOf(window);

        backend->makeCurrent();

        // 6 参 app::update 内部已 dispatch async 完成回调 + consume update request，并触发重排。
        if (app::update(window, deltaSeconds, framebufferWidth, framebufferHeight, dpiScale, pointerScale)) {
            paintRequested = true;
        }

        if (paintRequested) {
            backend->beginFrame({
                window,
                core::window::nativeWindowInfo(window),
                framebufferWidth,
                framebufferHeight,
                dpiScale
            });
            {
                core::render::ScopedRenderBackend scoped(*backend);
                app::render(framebufferWidth, framebufferHeight, dpiScale);
            }
            backend->present();
            core::render::publishRenderFrameStats();
            paintRequested = false;
        } else if (!app::isAnimating()) {
            glfwWaitEvents();   // 空闲：等待下一个事件，避免忙循环
        }
    }

    core::releaseInputQueue(window);
    backend->makeCurrent();
    backend->releaseRenderCache();
    {
        core::render::ScopedRenderBackend scoped(*backend);
        app::shutdown();
    }
    backend.reset();
    core::window::destroyWindow(window);
    glfwTerminate();
    return 0;
}
