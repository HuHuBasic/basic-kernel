/*
 * version.h - 系统版本号模块
 * 定义 Basic OS 版本信息、构建号和内核版本
 */
#ifndef VERSION_H
#define VERSION_H

/* 系统版本号 */
#define BASIC_OS_VERSION_MAJOR  2
#define BASIC_OS_VERSION_MINOR  0
#define BASIC_OS_VERSION_PATCH  0
#define BASIC_OS_VERSION_STR    "2.0.0"

/* 构建信息 */
#define BASIC_OS_BUILD_NUMBER   20260726
#define BASIC_OS_BUILD_STR      "20260726.1"
#define BASIC_OS_CODENAME       "Desktop"

/* 内核版本 */
#define KERNEL_VERSION_MAJOR    2
#define KERNEL_VERSION_MINOR    0
#define KERNEL_VERSION_PATCH    0
#define KERNEL_VERSION_STR      "kernel-2.0.0-x86"

/* 桌面环境版本 */
#define DESKTOP_VERSION_STR     "Desktop v2.0"

/* 完整版本字符串 */
#define BASIC_OS_FULL_NAME      "HU basic OS v" BASIC_OS_VERSION_STR
#define BASIC_OS_FULL_STRING    "HU basic OS v" BASIC_OS_VERSION_STR " (" BASIC_OS_CODENAME ")"

/* 版权信息 */
#define BASIC_OS_COPYRIGHT      "Copyright (c) 2026 HuHuBasic. MIT License."

/* 获取版本信息的对外接口 */
static inline const char* version_get_string(void) {
    return BASIC_OS_FULL_STRING;
}

static inline const char* version_get_kernel(void) {
    return KERNEL_VERSION_STR;
}

static inline const char* version_get_build(void) {
    return BASIC_OS_BUILD_STR;
}

static inline const char* version_get_desktop(void) {
    return DESKTOP_VERSION_STR;
}

#endif /* VERSION_H */