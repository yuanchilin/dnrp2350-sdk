/**
 * @file    main.c
 * @brief   07 — PIO+DMA 终端 (生成 PIO header, 已验证可用)
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/sync.h"
#include "lcd_spi.pio.h"
#include "BSP/LCD/lcd.h"
#include "BSP/LED/led.h"
#include "BSP/SPI/spi.h"
#include "BSP/UART/uart.h"
#include "console/console.h"
#include "shell/shell.h"
#include "commands/commands.h"
#include "ff.h"

#define LCD_W 240
#define LCD_H 135
#define MOSI   11
#define SCK    10
#define FB_SIZE (LCD_W * LCD_H * 2)

static uint8_t fb[FB_SIZE];
static int     pio_sm=-1, dma_ch=-1;
static bool    pio_ok=false;
/* PIO 32 位输出: 每字 = b0<<24|b1<<16|b2<<8|b3, 左移输出字节序 b0,b1,b2,b3 (MSB first)
 * 与 ST7789 的 RGB565 字节流一致(每像素 hi,lo) */
static uint32_t packed[(FB_SIZE + 3) / 4];

/* 字体改用 BSP 标准 asc2_1608 (定义于 lcdfont.h, 由 lcd.c 提供), 与 06 项目一致 */
extern const unsigned char asc2_1608[95][16];

static void fb_char(int col,int row,char ch,uint16_t fg,uint16_t bg){
    int px=col*8,py=row*16;
    int fi=ch-' '; if(fi<0||fi>=95)fi=0;   /* asc2_1608 自空格取模, 共 95 字符 */
    uint8_t fhi=fg>>8, flo=fg&0xFF, bhi=bg>>8, blo=bg&0xFF;
    /* asc2_1608 为行主序: 每字节一行(8bit=8列), 16 行 */
    for(int fy=0;fy<16;fy++){int idx=(py+fy)*LCD_W*2;
        uint8_t r=asc2_1608[fi][fy];
        for(int fx=0;fx<8;fx++){bool on=(r&(0x80>>fx))!=0;
            int i=idx+(px+fx)*2;
            if(on){fb[i]=fhi;fb[i+1]=flo;}else{fb[i]=bhi;fb[i+1]=blo;}
        }}
}

/* 每次刷屏前重启状态机, 并重排字节序 */
static void pio_flush(int x,int y,int w,int h){
    if(!pio_ok)return;
    int nbytes=w*h*2, nwords=(nbytes+3)/4;
    /* 区域可能跨多行 (如 8x16 字符), fb 每行 LCD_W 像素 → 行间跳 2*LCD_W 字节,
       不能按连续内存打包, 必须逐行按步长取字节 */
    for(int i=0;i<nwords;i++){
        uint32_t wd=0;
        for(int k=0;k<4;k++){
            int bi=i*4+k;
            if(bi>=nbytes)break;
            int ry=bi/(w*2), rem=bi%(w*2), rx=rem/2, bo=rem&1;
            uint8_t b=fb[((y+ry)*LCD_W+x+rx)*2+bo];
            wd|=((uint32_t)b)<<(24-k*8);
        }
        packed[i]=wd;
    }
    pio_sm_set_enabled(pio0,pio_sm,false);
    pio_sm_restart(pio0,pio_sm);
    pio_sm_clear_fifos(pio0,pio_sm);
    lcd_set_window(x,y,x+w-1,y+h-1);lcd_write_cmd(0x2C);LCD_WR(1);LCD_CS(0);
    gpio_set_function(MOSI,GPIO_FUNC_PIO0);gpio_set_function(SCK,GPIO_FUNC_PIO0);
    pio_sm_set_enabled(pio0,pio_sm,true);
    __dsb();
    dma_channel_set_read_addr(dma_ch,packed,false);
    dma_channel_set_trans_count(dma_ch,nwords,true);
    dma_channel_wait_for_finish_blocking(dma_ch);
    LCD_CS(1);gpio_set_function(MOSI,GPIO_FUNC_SPI);gpio_set_function(SCK,GPIO_FUNC_SPI);
}

static void pio_flush_cb(int x,int y,int w,int h){
    if(!pio_ok)return;
    pio_flush(x,y,w,h);
}

/* 命令回显: 固定暗绿 (与 help 命令名的亮绿区分); 命令输出: 默认灰 */
static void echo_colored(char c){ console_set_color(0x03E0, BLACK); console_putc(c); }
static void out_colored(const char *s){ console_set_color(GRAY, BLACK); console_write_ansi(s); console_putc('\n'); }

int main(void){
    led_init();LED(0);
    spi1_init();lcd_init();
    uart_init_dev();sleep_ms(50);while(uart_read_byte()>=0);

    /* PIO+DMA (已验证: lcd_spi8_program) */
    if(pio_can_add_program(pio0,&lcd_spi8_program)){
        int off=pio_add_program(pio0,&lcd_spi8_program);
        pio_sm=pio_claim_unused_sm(pio0,false);
        if(pio_sm>=0){
            lcd_spi8_program_init(pio0,pio_sm,off,MOSI,SCK);
            pio_sm_set_clkdiv(pio0,pio_sm,6.0f);
            pio_sm_set_enabled(pio0,pio_sm,true);
            dma_ch=dma_claim_unused_channel(false);
            if(dma_ch>=0){
                dma_channel_config dcfg=dma_channel_get_default_config(dma_ch);
                channel_config_set_transfer_data_size(&dcfg,DMA_SIZE_32);
                channel_config_set_read_increment(&dcfg,true);channel_config_set_write_increment(&dcfg,false);
                channel_config_set_dreq(&dcfg,pio_get_dreq(pio0,pio_sm,true));
                dma_channel_configure(dma_ch,&dcfg,&pio0->txf[pio_sm],packed,(FB_SIZE+3)/4,false);
                pio_ok=true;
            }
        }
    }

    console_init(30,8,8,16,16);
    if(pio_ok){
        /* 启用 PIO+DMA 刷屏渲染路径 (每次刷屏前重启状态机) */
        console_set_pio_mode(fb_char, pio_flush_cb);
    }

    FATFS fs;bool sd_ok=(f_mount(&fs,"0:",1)==FR_OK);
    shell_init("$ ");shell_set_ansi(true);
    shell_set_echo_cb(echo_colored);
    shell_set_output_cb((void (*)(const char*))out_colored);
    /* 启动初始化信息: 先于照片等待打印, 复位后串口即可见 */
    shell_print("\r\n");
    shell_print("== DNRP2350A PIO LCD Terminal ==\r\n");
    shell_print("SD: "); shell_print(sd_ok ? "OK" : "ERR"); shell_print("\r\n");
    shell_print("Type 'help' for commands\r\n");
    commands_init(sd_ok);commands_register_all();
    if(sd_ok)commands_view_file("/photo.bmp");

    console_println("07 PIO Terminal");
    console_println("Type help");

    while(1){shell_poll();sleep_ms(1);}
    return 0;
}
