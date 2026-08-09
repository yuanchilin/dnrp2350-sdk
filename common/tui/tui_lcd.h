/**
 * @file    tui_lcd.h
 * @brief   TUI → LCD 同步镜像 — 解析 TUI 的 ANSI 输出, 在屏幕上复现
 *
 * TUI 已按 LCD 尺寸定制 (30 列 x 8 行, 8x16 字体), 虚拟屏即 LCD 全屏, 无切省。
 */
#ifndef __TUI_LCD_H
#define __TUI_LCD_H

/* 启用镜像: 接管 tui 输出流结尾, 开始维护屏幕缓冲 */
void tui_lcd_attach(void);
/* 解除镜像, 恢复 LCD 归 console 管 */
void tui_lcd_detach(void);

#endif /* __TUI_LCD_H */