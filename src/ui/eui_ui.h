// eui_ui.h — eui_neo.h 的精简 include 面，供 heibu.ui.* / heibu.app_actions 模块的
// global module fragment 使用。只含 widget / DslAppConfig 声明 + core::async + app::* 声明，
// 不含 app 入口机制（eui/detail/dsl_app_impl.h —— 那由 app_config.cpp 经 own-main 提供）。
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "eui/dsl_app.h"
#include "eui/dsl.h"
#include "eui/image.h"
#include "eui/json.h"
#include "eui/network.h"
#include "eui/platform.h"
#include "eui/signal.h"
#include "eui/types.h"
// Heibu 特有：app_actions 用 core::async（异步查询）与 app::requestUpdate()（触发重排）。
#include "eui/async.h"
#include "eui/app.h"
#include "components/components.h"
