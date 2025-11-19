/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_VIRTUALIZATION_ZLOG_H_
#define ZEPHYR_INCLUDE_VIRTUALIZATION_ZLOG_H_

#include <zephyr/kernel.h>

#define ANSI_COLOR_RED     "\033[0;31m"  /* ERROR: 红色 */
#define ANSI_COLOR_YELLOW  "\033[0;33m"  /* WARN: 黄色 */
#define ANSI_COLOR_WHITE   "\033[0;37m"  /* INFO: 白色 */
#define ANSI_COLOR_RESET   "\033[0m"
#define ZVM_LOG_LOCATION_FMT "[ZVM] (%s:%d in %s) "

#ifdef CONFIG_LOG

    #define ZVM_LOG_ERR(fmt, ...) \
        printk(ANSI_COLOR_RED    ZVM_LOG_LOCATION_FMT "ERROR: " fmt ANSI_COLOR_RESET, \
               __FILE__, __LINE__, __func__, ##__VA_ARGS__)

    #define ZVM_LOG_WARN(fmt, ...) \
        printk(ANSI_COLOR_YELLOW ZVM_LOG_LOCATION_FMT "WARN:  " fmt ANSI_COLOR_RESET, \
               __FILE__, __LINE__, __func__, ##__VA_ARGS__)

    #ifdef CONFIG_ZVM_DEBUG_LOG_INFO
        #define ZVM_LOG_INFO(fmt, ...) \
            printk(ANSI_COLOR_WHITE "[ZVM INFO]:  " fmt ANSI_COLOR_RESET, ##__VA_ARGS__)
    #else
        #define ZVM_LOG_INFO(...)
    #endif

    #ifdef CONFIG_ZVM_DEBUG
        #define ZVM_LOG_DEBUG(fmt, ...) \
            printk("\033[36m[ZVM DEBUG]: (%s) (line: %d)\t\033[31m" fmt "\033[0m", __func__,__LINE__, ##__VA_ARGS__)
    #else
        #define ZVM_LOG_DEBUG(...)
    #endif
    
#endif

#endif /* ZEPHYR_INCLUDE_VIRTUALIZATION_ZLOG_H_ */