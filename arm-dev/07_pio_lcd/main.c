/**
 * @file    main.c
 * @brief   07 — PIO+DMA 终端 (shell 库 + 帧缓冲渲染)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "lcd_spi.pio.h"
#include "BSP/LCD/lcd.h"
#include "BSP/LED/led.h"
#include "BSP/SPI/spi.h"
#include "BSP/SDIO/spi_sdcard.h"
#include "BSP/UART/uart.h"
#include "shell/shell.h"
#include "ff.h"

#define LCD_W 240
#define LCD_H 135
#define COLS   30
#define ROWS   8
#define FONT_W 8
#define FONT_H 16
#define STATUSBAR_Y (ROWS * FONT_H)
#define BUF_SIZE (LCD_W * LCD_H * 2)

#define MOSI_PIN 11
#define SCK_PIN  10

static uint8_t fb[BUF_SIZE];
static int pio_sm = -1, pio_ofs = -1, dma_ch = -1;
static bool pio_ok = false;
static bool sd_ok  = false;
static char screen[ROWS][COLS+1];
static int  cx = 0, cy = 0;

/* ---- 帧缓冲字符 ---- */
static const uint8_t font6x8[][6] = {
    [0]={0x7C,0x12,0x11,0x12,0x7C,0x00},[1]={0x7F,0x49,0x49,0x49,0x36,0x00},
    [2]={0x3E,0x41,0x41,0x41,0x22,0x00},[3]={0x7F,0x41,0x41,0x22,0x1C,0x00},
    [4]={0x7F,0x49,0x49,0x49,0x41,0x00},[5]={0x7F,0x09,0x09,0x09,0x01,0x00},
    [6]={0x3E,0x41,0x49,0x49,0x7A,0x00},[7]={0x7F,0x08,0x08,0x08,0x7F,0x00},
    [8]={0x41,0x7F,0x41,0x00,0x00,0x00},[9]={0x20,0x40,0x41,0x3F,0x01,0x00},
    [10]={0x7F,0x08,0x14,0x22,0x41,0x00},[11]={0x7F,0x40,0x40,0x40,0x40,0x00},
    [12]={0x7F,0x02,0x0C,0x02,0x7F,0x00},[13]={0x7F,0x04,0x08,0x10,0x7F,0x00},
    [14]={0x3E,0x41,0x41,0x41,0x3E,0x00},[15]={0x7F,0x09,0x09,0x09,0x06,0x00},
    [16]={0x3E,0x41,0x51,0x21,0x5E,0x00},[17]={0x7F,0x09,0x19,0x29,0x46,0x00},
    [18]={0x46,0x49,0x49,0x49,0x31,0x00},[19]={0x01,0x01,0x7F,0x01,0x01,0x00},
    [20]={0x3F,0x40,0x40,0x40,0x3F,0x00},[21]={0x1F,0x20,0x40,0x20,0x1F,0x00},
    [22]={0x3F,0x40,0x38,0x40,0x3F,0x00},[23]={0x63,0x14,0x08,0x14,0x63,0x00},
    [24]={0x07,0x08,0x70,0x08,0x07,0x00},[25]={0x61,0x51,0x49,0x45,0x43,0x00},
    [26]={0x3E,0x51,0x49,0x45,0x3E,0x00},[27]={0x42,0x7F,0x40,0x00,0x00,0x00},
    [28]={0x62,0x51,0x49,0x49,0x46,0x00},[29]={0x22,0x49,0x49,0x49,0x36,0x00},
    [30]={0x18,0x14,0x12,0x7F,0x10,0x00},[31]={0x27,0x45,0x45,0x45,0x39,0x00},
    [32]={0x3E,0x49,0x49,0x49,0x32,0x00},[33]={0x01,0x71,0x09,0x05,0x03,0x00},
    [34]={0x36,0x49,0x49,0x49,0x36,0x00},[35]={0x26,0x49,0x49,0x49,0x3E,0x00},
    [36]={0x00,0x00,0x00,0x00,0x00,0x00},[37]={0x00,0x00,0x5F,0x00,0x00,0x00},
    [38]={0x00,0x60,0x60,0x00,0x00,0x00},[39]={0x08,0x08,0x08,0x08,0x08,0x00},
    [40]={0x40,0x40,0x40,0x40,0x40,0x00},[41]={0x00,0x36,0x36,0x00,0x00,0x00},
    [42]={0x60,0x18,0x06,0x01,0x00,0x00},[43]={0x14,0x14,0x14,0x14,0x14,0x00},
    [44]={0x02,0x01,0x51,0x09,0x06,0x00},
};

static int ch2idx(char ch) {
    if (ch>='A'&&ch<='Z') return ch-'A';
    if (ch>='a'&&ch<='z') return ch-'a';
    if (ch>='0'&&ch<='9') return 26+ch-'0';
    switch(ch){case' ':return 36;case'!':return 37;case'.':return 38;case'-':return 39;
        case'_':return 40;case':':return 41;case'/':return 42;case'=':return 43;case'?':return 44;}
    return -1;
}

static void fb_char(int x, int y, char ch, uint16_t fg, uint16_t bg) {
    int px=x*FONT_W, py=y*FONT_H;
    if (ch<' '||ch>'~') ch=' ';
    int fi=ch2idx(ch);
    uint8_t fhi=fg>>8, flo=fg&0xFF, bhi=bg>>8, blo=bg&0xFF;
    for (int fy=0;fy<FONT_H;fy++) {
        int idx=(py+fy)*LCD_W*2;
        for (int fx=0;fx<FONT_W;fx++) {
            bool on=(fi>=0&&fy<6&&fx<6)&&(font6x8[fi][fy]&(0x80>>fx));
            int i=idx+(px+fx)*2;
            if(on){fb[i]=fhi;fb[i+1]=flo;}else{fb[i]=bhi;fb[i+1]=blo;}
        }
    }
}

/* ---- PIO+DMA ---- */
static void pio_init(void) {
    if(pio_ok)return;
    if(!pio_can_add_program(pio0,&lcd_spi8_program))return;
    pio_ofs=pio_add_program(pio0,&lcd_spi8_program);
    pio_sm=pio_claim_unused_sm(pio0,false);
    if(pio_sm<0)return;
    lcd_spi8_program_init(pio0,pio_sm,pio_ofs,MOSI_PIN,SCK_PIN);
    pio_sm_set_clkdiv(pio0,pio_sm,6.0f);
    dma_ch=dma_claim_unused_channel(false);
    if(dma_ch<0)return;
    dma_channel_config cfg=dma_channel_get_default_config(dma_ch);
    channel_config_set_transfer_data_size(&cfg,DMA_SIZE_8);
    channel_config_set_read_increment(&cfg,true);
    channel_config_set_write_increment(&cfg,false);
    channel_config_set_dreq(&cfg,pio_get_dreq(pio0,pio_sm,true));
    dma_channel_configure(dma_ch,&cfg,&pio0->txf[pio_sm],fb,BUF_SIZE,false);
    pio_ok=true;
}

static void cpu_flush(int x,int y,int w,int h){
    lcd_set_window(x,y,x+w-1,y+h-1);lcd_write_cmd(0x2C);LCD_WR(1);LCD_CS(0);
    for(int r=0;r<h;r++) lcd_write_data(fb+((y+r)*LCD_W+x)*2,w*2);
    LCD_CS(1);
}

static void pio_flush(int x,int y,int w,int h){
    if(!pio_ok)pio_init();
    if(!pio_ok){cpu_flush(x,y,w,h);return;}
    lcd_set_window(x,y,x+w-1,y+h-1);lcd_write_cmd(0x2C);LCD_WR(1);LCD_CS(0);
    gpio_set_function(MOSI_PIN,GPIO_FUNC_PIO0);gpio_set_function(SCK_PIN,GPIO_FUNC_PIO0);
    int off=(y*LCD_W+x)*2;
    dma_channel_set_read_addr(dma_ch,fb+off,false);
    dma_channel_set_trans_count(dma_ch,w*h*2,true);
    dma_channel_wait_for_finish_blocking(dma_ch);
    LCD_CS(1);
    gpio_set_function(MOSI_PIN,GPIO_FUNC_SPI);gpio_set_function(SCK_PIN,GPIO_FUNC_SPI);
}

/* ---- 终端绘制 ---- */
static void term_draw_line(int y){
    for(int x=0;x<COLS;x++) fb_char(x,y,screen[y][x]?screen[y][x]:' ',GREEN,BLACK);
    pio_flush(0,y*FONT_H,LCD_W,FONT_H);
}
static void lcd_draw_all(void){
    for(int y=0;y<ROWS;y++)for(int x=0;x<COLS;x++)fb_char(x,y,screen[y][x]?screen[y][x]:' ',GREEN,BLACK);
    pio_flush(0,0,LCD_W,ROWS*FONT_H);
    /* 状态栏 */
    for(int x=0;x<COLS;x++) fb_char(x,0,' ',WHITE,0x1082);
    pio_flush(0,STATUSBAR_Y,LCD_W,FONT_H);
}
static void lcd_putc(char c){
    if(c=='\n'){cx=0;cy++;if(cy>=ROWS){for(int i=0;i<ROWS-1;i++)memcpy(screen[i],screen[i+1],COLS);memset(screen[ROWS-1],' ',COLS);lcd_draw_all();cy=ROWS-1;}}
    else if(c=='\r') cx=0;
    else if(c=='\b'){if(cx>0)cx--;}
    else if(c>=' '){if(cx>=COLS){cx=0;cy++;if(cy>=ROWS){for(int i=0;i<ROWS-1;i++)memcpy(screen[i],screen[i+1],COLS);memset(screen[ROWS-1],' ',COLS);lcd_draw_all();cy=ROWS-1;}} screen[cy][cx++]=c; term_draw_line(cy);}
}
static void lcd_print(const char*s){while(*s)lcd_putc(*s++);}
static void lcd_println(const char*s){lcd_print(s);lcd_putc('\n');}

/* ---- 命令 ---- */
static void cmd_ls(const char*arg){(void)arg;
    if(!sd_ok){lcd_println("no SD");return;}
    DIR dir;FILINFO fno;if(f_opendir(&dir,"/")!=FR_OK){lcd_println("fail");return;}
    char l[COLS+1];while(f_readdir(&dir,&fno)==FR_OK&&fno.fname[0]){snprintf(l,COLS+1,"%-20s%6lu",fno.fname,(unsigned long)fno.fsize);lcd_println(l);}
    f_closedir(&dir);
}
static void cmd_cat(const char*p){if(!sd_ok||!p){lcd_println("usage:cat<f>");return;}
    FIL fil;if(f_open(&fil,p,FA_READ)!=FR_OK){lcd_println("open fail");return;}
    char l[COLS+1];int li=0;uint8_t b[64];UINT br;
    while(f_read(&fil,b,64,&br)==FR_OK&&br>0){for(UINT i=0;i<br;i++){char c=(char)b[i];if(c=='\n'||li>=COLS){l[li]=0;lcd_println(l);li=0;}else if(c>=' '&&c<='~')l[li++]=c;}}if(li>0){l[li]=0;lcd_println(l);}
    f_close(&fil);
}
typedef struct __attribute__((packed)){uint16_t bfType;uint32_t bfSize;uint16_t bfReserved1,bfReserved2;uint32_t bfOffBits;}bmp_hdr_t;
typedef struct __attribute__((packed)){uint32_t biSize;int32_t biWidth,biHeight;uint16_t biPlanes,biBitCount;uint32_t biCompression,biSizeImage;}bmp_info_t;
static void cmd_view(const char*p){if(!sd_ok||!p||!*p){lcd_println("usage:view<bmp>");return;}
    FIL fil;UINT br;if(f_open(&fil,p,FA_READ)!=FR_OK){lcd_println("open fail");return;}
    bmp_hdr_t fh;bmp_info_t ih;f_read(&fil,&fh,sizeof(fh),&br);f_read(&fil,&ih,sizeof(ih),&br);
    if(fh.bfType!=0x4D42||ih.biBitCount!=24){f_close(&fil);lcd_println("bad BMP");return;}
    int iw=ih.biWidth,ihgt=ih.biHeight>0?ih.biHeight:-ih.biHeight,rb=(iw*3+3)&~3;bool td=ih.biHeight<0;
    int sx=iw>LCD_W?iw*10/LCD_W:10,sy=ihgt>LCD_H?ihgt*10/LCD_H:10,dw=iw>LCD_W?LCD_W:iw,dh=ihgt>LCD_H?LCD_H:ihgt;
    int ox=(LCD_W-dw)/2,oy=(LCD_H-dh)/2;uint8_t line[240*3+4];f_lseek(&fil,fh.bfOffBits);
    for(int ly=0;ly<dh;ly++){int by=td?ly*sy/10:ihgt-1-ly*sy/10;f_lseek(&fil,fh.bfOffBits+(FSIZE_t)by*rb);f_read(&fil,line,rb,&br);
        for(int lx=0;lx<dw;lx++){uint8_t*px=&line[lx*sx/10*3];uint16_t c=((px[2]>>3)<<11)|((px[1]>>2)<<5)|(px[0]>>3);int i=((oy+ly)*LCD_W+ox+lx)*2;fb[i]=c>>8;fb[i+1]=c&0xFF;}}
    f_close(&fil);pio_flush(0,0,LCD_W,LCD_H);
    while(uart_read_byte()>=0);while(uart_read_byte()<0)sleep_ms(50);while(uart_read_byte()>=0);
    lcd_draw_all();
}
static void cmd_free(const char*arg){(void)arg;if(!sd_ok){lcd_println("no SD");return;}uint32_t fk,tk;sd_init(&fk,&tk);char s[COLS+1];snprintf(s,COLS+1,"Free:%luMB Total:%luMB",fk>>10,tk>>10);lcd_println(s);}
static void cmd_sysinfo(const char*arg){(void)arg;lcd_println("RP2350A 150MHz");lcd_println("LCD PIO+DMA 25MHz");lcd_println(sd_ok?"SD OK":"SD N/A");}
static void cmd_clear(const char*arg){(void)arg;for(int i=0;i<ROWS;i++)memset(screen[i],' ',COLS);cx=cy=0;lcd_draw_all();}
static void cmd_snake(const char*arg){(void)arg;
    lcd_println("Snake! WASD Q=quit");int sx[256],sy[256],len=3,dx=1,dy=0,fx=10,fy=3;
    sx[0]=5;sy[0]=3;sx[1]=4;sy[1]=3;sx[2]=3;sy[2]=3;memset(fb,0,BUF_SIZE);pio_flush(0,0,LCD_W,LCD_H);
    while(1){int ch=uart_read_byte();if(ch>=0){switch(toupper(ch)){case'W':if(dy!=1){dx=0;dy=-1;}break;case'S':if(dy!=-1){dx=0;dy=1;}break;case'A':if(dx!=1){dx=-1;dy=0;}break;case'D':if(dx!=-1){dx=1;dy=0;}break;case'Q':lcd_println("QUIT");return;}while(uart_read_byte()>=0);}
    int nx=sx[0]+dx,ny=sy[0]+dy;if(nx<0||nx>=COLS||ny<0||ny>=ROWS){lcd_println("GAME OVER");return;}
    for(int i=0;i<len;i++)if(sx[i]==nx&&sy[i]==ny){char s[COLS+1];snprintf(s,COLS+1,"Score:%d",len-3);lcd_println(s);return;}
    for(int i=len;i>0;i--){sx[i]=sx[i-1];sy[i]=sy[i-1];}sx[0]=nx;sy[0]=ny;if(nx==fx&&ny==fy){len++;fx=rand()%COLS;fy=rand()%ROWS;}
    memset(fb,0,BUF_SIZE);for(int i=0;i<len;i++)for(int fy=2;fy<14;fy++)for(int fx=1;fx<7;fx++){int idx=((sy[i]*FONT_H+fy)*LCD_W+sx[i]*FONT_W+fx)*2;fb[idx]=0x07;fb[idx+1]=0xE0;}
    for(int fy=2;fy<14;fy++)for(int fx=1;fx<7;fx++){int idx=((fy*FONT_H+fy)*LCD_W+fx*FONT_W+fx)*2;fb[idx]=0xF8;fb[idx+1]=0x00;}
    pio_flush(0,0,LCD_W,LCD_H);sleep_ms(150);}
}

int main(void){
    led_init();LED(0);
    spi1_init();lcd_init();
    for(int i=0;i<ROWS;i++)memset(screen[i],' ',COLS);
    memset(fb,0,BUF_SIZE);lcd_draw_all();

    FATFS fs;sd_ok=(f_mount(&fs,"0:",1)==FR_OK);
    if(sd_ok){FIL t;if(f_open(&t,"/photo.bmp",FA_READ)==FR_OK){f_close(&t);cmd_view("/photo.bmp");}}

    shell_init("$ ");
    shell_register("ls","list files",cmd_ls);
    shell_register("cat","show file",cmd_cat);
    shell_register("view","show BMP",cmd_view);
    shell_register("free","SD space",cmd_free);
    shell_register("sysinfo","system info",cmd_sysinfo);
    shell_register("clear","clear screen",cmd_clear);
    shell_register("snake","play snake",cmd_snake);

    lcd_println("07 PIO Terminal");
    lcd_println("Type help");

    while(1){shell_poll();sleep_ms(1);}
    return 0;
}
