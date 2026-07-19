/*
 * apps.h - 内核应用安装程序框架
 */
#ifndef _APPS_H
#define _APPS_H

#include <stdint.h>
#include <stddef.h>

/* 最大应用数 */
#define MAX_APPS 32
#define APP_NAME_LEN 32
#define APP_DESC_LEN 80

/* 应用状态 */
#define APP_STATE_NOT_INSTALLED 0
#define APP_STATE_INSTALLED     1
#define APP_STATE_RUNNING       2

/* 应用入口函数类型 */
typedef void (*app_entry_t)(void);

/* 应用描述符 */
typedef struct {
    char        name[APP_NAME_LEN];      /* 应用名称 */
    char        desc[APP_DESC_LEN];      /* 应用描述 */
    char        version[8];              /* 版本号 */
    uint32_t    size;                    /* 大小 (字节) */
    uint8_t     state;                   /* 安装状态 */
    app_entry_t entry;                   /* 入口函数 */
} app_t;

/* 初始化应用管理器 */
void apps_init(void);

/* 注册一个应用 */
int  apps_register(const char *name, const char *desc, const char *version,
                   uint32_t size, app_entry_t entry);

/* 安装应用 */
int  apps_install(const char *name);

/* 卸载应用 */
int  apps_uninstall(const char *name);

/* 运行应用 */
int  apps_run(const char *name);

/* 获取已安装应用列表 */
int  apps_list_installed(char *buffer, int max_len);

/* 获取所有可用应用列表 */
int  apps_list_all(char *buffer, int max_len);

/* 获取应用数量 */
int  apps_count(void);

/* 按索引获取应用信息 (供应用中心等使用) */
int  apps_get_by_index(int index, app_t *out);

/* 获取应用状态 */
int  apps_get_state(const char *name);

#endif /* _APPS_H */