/*
 * shell.h - 简单 Shell 头文件
 */
#ifndef _SHELL_H
#define _SHELL_H

/* 初始化并启动 Shell */
void shell_init(void);

/* Shell 主循环 (返回条件: desktop 命令或 reboot) */
void shell_run(void);

/* 请求 Shell 退出 */
void shell_request_exit(void);

#endif /* _SHELL_H */