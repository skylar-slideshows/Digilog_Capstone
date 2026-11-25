/*
 * Levetop's LT768x library (LT768_Lib V1.3), ported to the Digilog master controller's
 * STM32G474 (FMC bus, HAL). Changes from Levetop's copy: comments converted from GBK to UTF-8,
 * the STM32F103 includes replaced, and LT768_HW_Reset() uses the board's DISP_RESET pin.
 * The bus itself is in if_port.c.
 */
/********************* COPYRIGHT  **********************
* File Name        : LT768_Lib.h
* Author           : Levetop Electronics
* Version          : V1.3
* Date             : 2019-5-14
* Description      : LT768x所有功能函数
********************************************************/

#ifndef _LT768_Lib_H
#define _LT768_Lib_H
#include "LT768.h"
#include "if_port.h"

//外部晶振
#define XI_4M            0
#define XI_8M            0
#define XI_10M    	     1
#define XI_12M           0

//分辨率
#define LCD_XSIZE_TFT	1024
#define LCD_YSIZE_TFT	600
//参数
#define LCD_VBPD		20
#define LCD_VFPD		12
#define LCD_VSPW		3
#define LCD_HBPD		140
#define LCD_HFPD		160
#define LCD_HSPW		20

#define color256_black   0x00
#define color256_white   0xff
#define color256_red     0xe0
#define color256_green   0x1c
#define color256_blue    0x03
#define color256_yellow  color256_red|color256_green
#define color256_cyan    color256_green|color256_blue
#define color256_purple  color256_red|color256_blue
 
#define color65k_black   0x0000
#define color65k_white   0xffff
#define color65k_red     0xf800
#define color65k_green   0x07e0
#define color65k_blue    0x001f
#define color65k_yellow  color65k_red|color65k_green
#define color65k_cyan    color65k_green|color65k_blue
#define color65k_purple  color65k_red|color65k_blue

#define color65k_grayscale1    2113
#define color65k_grayscale2    2113*2
#define color65k_grayscale3    2113*3
#define color65k_grayscale4    2113*4
#define color65k_grayscale5    2113*5
#define color65k_grayscale6    2113*6
#define color65k_grayscale7    2113*7
#define color65k_grayscale8    2113*8
#define color65k_grayscale9    2113*9
#define color65k_grayscale10   2113*10
#define color65k_grayscale11   2113*11
#define color65k_grayscale12   2113*12
#define color65k_grayscale13   2113*13
#define color65k_grayscale14   2113*14
#define color65k_grayscale15   2113*15
#define color65k_grayscale16   2113*16
#define color65k_grayscale17   2113*17
#define color65k_grayscale18   2113*18
#define color65k_grayscale19   2113*19
#define color65k_grayscale20   2113*20
#define color65k_grayscale21   2113*21
#define color65k_grayscale22   2113*22
#define color65k_grayscale23   2113*23
#define color65k_grayscale24   2113*24
#define color65k_grayscale25   2113*25
#define color65k_grayscale26   2113*26
#define color65k_grayscale27   2113*27
#define color65k_grayscale28   2113*28
#define color65k_grayscale29   2113*29
#define color65k_grayscale30   2113*30

#define color16M_black   0x00000000
#define color16M_white   0x00ffffff
#define color16M_red     0x00ff0000
#define color16M_green   0x0000ff00
#define color16M_blue    0x000000ff
#define color16M_yellow  color16M_red|color16M_green
#define color16M_cyan    color16M_green|color16M_blue
#define color16M_purple  color16M_red|color16M_blue

/* LCD color */
#define White          0xFFFF
#define Black          0x0000
#define Grey           0xF7DE
#define Blue           0x001F
#define Blue2          0x051F
#define Red            0xF800
#define Magenta        0xF81F
#define Green          0x07E0
#define Cyan           0x7FFF
#define Yellow         0xFFE0

#define Line0          0
#define Line1          24
#define Line2          48
#define Line3          72
#define Line4          96
#define Line5          120
#define Line6          144
#define Line7          168
#define Line8          192
#define Line9          216
#define Line10         240
#define Line11         264
#define Line12         288
#define Line13         312
#define Line14         336
#define Line15         360
#define Line16         384
#define Line17         408
#define Line18         432
#define Line19         456
#define Line20         480
#define Line21         504
#define Line22         528
#define Line23         552
#define Line24         576
#define Line25         600

void LT768_Init(void);

/* 写数据到内存 */
void MPU8_8bpp_Memory_Write(unsigned short x,unsigned short y,unsigned short w,unsigned short h,const unsigned char *data);
void MPU8_16bpp_Memory_Write(unsigned short x,unsigned short y,unsigned short w,unsigned short h,const unsigned char *data);
void MPU8_24bpp_Memory_Write(unsigned short x,unsigned short y,unsigned short w,unsigned short h,const unsigned char *data); 
void MPU16_16bpp_Memory_Write(unsigned short x,unsigned short y,unsigned short w,unsigned short h,const unsigned short *data);
void MPU16_24bpp_Mode1_Memory_Write(unsigned short x,unsigned short y,unsigned short w,unsigned short h,const unsigned short *data); 
void MPU16_24bpp_Mode2_Memory_Write(unsigned short x,unsigned short y,unsigned short w,unsigned short h,const unsigned short *data);

/* 硬件画线段 */
void LT768_DrawLine(unsigned short X1,unsigned short Y1,unsigned short X2,unsigned short Y2,unsigned long LineColor);
void LT768_DrawLine_Width(unsigned short X1,unsigned short Y1,unsigned short X2,unsigned short Y2,unsigned long LineColor,unsigned short Width);

/* 硬件画圆 */
void LT768_DrawCircle(unsigned short XCenter,unsigned short YCenter,unsigned short R,unsigned long CircleColor);
void LT768_DrawCircle_Fill(unsigned short XCenter,unsigned short YCenter,unsigned short R,unsigned long ForegroundColor);
void LT768_DrawCircle_Width(unsigned short XCenter,unsigned short YCenter,unsigned short R,unsigned long CircleColor,unsigned long ForegroundColor,unsigned short Width);

/* 硬件画椭圆 */
void LT768_DrawEllipse(unsigned short XCenter,unsigned short YCenter,unsigned short X_R,unsigned short Y_R,unsigned long EllipseColor);
void LT768_DrawEllipse_Fill(unsigned short XCenter,unsigned short YCenter,unsigned short X_R,unsigned short Y_R,unsigned long ForegroundColor);
void LT768_DrawEllipse_Width(unsigned short XCenter,unsigned short YCenter,unsigned short X_R,unsigned short Y_R,unsigned long EllipseColor,unsigned long ForegroundColor,unsigned short Width);   

/* 硬件画矩形 */
void LT768_DrawSquare(unsigned short X1,unsigned short Y1,unsigned short X2,unsigned short Y2,unsigned long SquareColor);
void LT768_DrawSquare_Fill(unsigned short X1,unsigned short Y1,unsigned short X2,unsigned short Y2,unsigned long ForegroundColor);
void LT768_DrawSquare_Width(unsigned short X1,unsigned short Y1,unsigned short X2,unsigned short Y2,unsigned long SquareColor,unsigned long ForegroundColor,unsigned short Width);

/* 硬件画圆角矩形 */
void LT768_DrawCircleSquare(unsigned short X1,unsigned short Y1,unsigned short X2,unsigned short Y2,unsigned short X_R,unsigned short Y_R,unsigned long CircleSquareColor);
void LT768_DrawCircleSquare_Fill(unsigned short X1,unsigned short Y1,unsigned short X2,unsigned short Y2,unsigned short X_R,unsigned short Y_R,unsigned long ForegroundColor);
void LT768_DrawCircleSquare_Width(unsigned short X1,unsigned short Y1,unsigned short X2,unsigned short Y2,unsigned short X_R,unsigned short Y_R,unsigned long CircleSquareColor,unsigned long ForegroundColor,unsigned short Width);

/* 硬件画三角形 */
void LT768_DrawTriangle(unsigned short X1,unsigned short Y1,unsigned short X2,unsigned short Y2,unsigned short X3,unsigned short Y3,unsigned long TriangleColor);
void LT768_DrawTriangle_Fill(unsigned short X1,unsigned short Y1,unsigned short X2,unsigned short Y2,unsigned short X3,unsigned short Y3,unsigned long ForegroundColor);
void LT768_DrawTriangle_Frame(unsigned short X1,unsigned short Y1,unsigned short X2,unsigned short Y2,unsigned short X3,unsigned short Y3,unsigned long TriangleColor,unsigned long ForegroundColor);

/* 硬件画四边形 */
void LT768_DrawQuadrilateral(unsigned short X1,unsigned short Y1,unsigned short X2,unsigned short Y2,unsigned short X3,unsigned short Y3,unsigned short X4,unsigned short Y4,unsigned long ForegroundColor);
void LT768_DrawQuadrilateral_Fill(unsigned short X1,unsigned short Y1,unsigned short X2,unsigned short Y2,unsigned short X3,unsigned short Y3,unsigned short X4,unsigned short Y4,unsigned long ForegroundColor);
void LT768_DrawTriangle_Frame(unsigned short X1,unsigned short Y1,unsigned short X2,unsigned short Y2,unsigned short X3,unsigned short Y3,unsigned long TriangleColor ,unsigned long ForegroundColor);

/* 硬件画五边形 */
void LT768_DrawPentagon(unsigned short X1,unsigned short Y1,unsigned short X2,unsigned short Y2,unsigned short X3,unsigned short Y3,unsigned short X4,unsigned short Y4,unsigned short X5,unsigned short Y5,unsigned long ForegroundColor);
void LT768_DrawPentagon_Fill(unsigned short X1,unsigned short Y1,unsigned short X2,unsigned short Y2,unsigned short X3,unsigned short Y3,unsigned short X4,unsigned short Y4,unsigned short X5,unsigned short Y5,unsigned long ForegroundColor);

/* 硬件画曲线 */
void LT768_DrawLeftUpCurve(unsigned short XCenter,unsigned short YCenter,unsigned short X_R,unsigned short Y_R,unsigned long CurveColor);
void LT768_DrawLeftDownCurve(unsigned short XCenter,unsigned short YCenter,unsigned short X_R,unsigned short Y_R,unsigned long CurveColor);
void LT768_DrawRightUpCurve(unsigned short XCenter,unsigned short YCenter,unsigned short X_R,unsigned short Y_R,unsigned long CurveColor);
void LT768_DrawRightDownCurve(unsigned short XCenter,unsigned short YCenter,unsigned short X_R,unsigned short Y_R,unsigned long CurveColor);
void LT768_SelectDrawCurve(unsigned short XCenter ,unsigned short YCenter,unsigned short X_R,unsigned short Y_R,unsigned long CurveColor,unsigned short  Dir);

/* 硬件画1/4椭圆 */
void LT768_DrawLeftUpCurve_Fill(unsigned short XCenter,unsigned short YCenter,unsigned short X_R,unsigned short Y_R,unsigned long ForegroundColor);
void LT768_DrawLeftDownCurve_Fill(unsigned short XCenter,unsigned short YCenter,unsigned short X_R,unsigned short Y_R,unsigned long ForegroundColor);
void LT768_DrawRightUpCurve_Fill(unsigned short XCenter,unsigned short YCenter,unsigned short X_R,unsigned short Y_R,unsigned long ForegroundColor);
void LT768_DrawRightDownCurve_Fill(unsigned short XCenter,unsigned short YCenter,unsigned short X_R,unsigned short Y_R,unsigned long ForegroundColor);
void LT768_SelectDrawCurve_Fill(unsigned short XCenter,unsigned short YCenter,unsigned short X_R,unsigned short Y_R,unsigned long CurveColor,unsigned short  Dir);

/* 硬件画圆柱 */
unsigned char LT768_DrawCylinder(unsigned short XCenter,unsigned short YCenter,unsigned short X_R,unsigned short Y_R,unsigned short H,unsigned long CylinderColor,unsigned long ForegroundColor);

/* 硬件画四棱柱 */
void LT768_DrawQuadrangular(unsigned short X1,unsigned short Y1,unsigned short X2,unsigned short Y2,unsigned short X3,unsigned short Y3,unsigned short X4,unsigned short Y4,unsigned short X5,unsigned short Y5,unsigned short X6,unsigned short Y6,unsigned long QuadrangularColor,unsigned long ForegroundColor);

/* 表格 */
void LT768_MakeTable(unsigned short X1,unsigned short Y1,unsigned short W,unsigned short H,unsigned short Line,unsigned short Row,unsigned long  TableColor,unsigned long  ItemColor,unsigned long  ForegroundColor,unsigned short width1,unsigned short width2,unsigned char  mode);

/* 线性模式下DMA传输数据到SDRAM中 */
void LT768_DMA_24bit_Linear(unsigned char SCS,unsigned char Clk,unsigned long flash_addr,unsigned long memory_ad,unsigned long data_num);
void LT768_DMA_32bit_Linear(unsigned char SCS,unsigned char Clk,unsigned long flash_addr,unsigned long memory_ad,unsigned long data_num);

/* 区块模式下DMA传输数据到SDRAM中 */
void LT768_DMA_24bit_Block(unsigned char SCS,unsigned char Clk,unsigned short X1,unsigned short Y1 ,unsigned short X_W,unsigned short Y_H,unsigned short P_W,unsigned long Addr);
void LT768_DMA_32bit_Block(unsigned char SCS,unsigned char Clk,unsigned short X1,unsigned short Y1 ,unsigned short X_W,unsigned short Y_H,unsigned short P_W,unsigned long Addr);

/* 使用内建字库 */
void LT768_Select_Internal_Font_Init(unsigned char Size,unsigned char XxN,unsigned char YxN,unsigned char ChromaKey,unsigned char Alignment);
void LT768_Print_Internal_Font_String(unsigned short x,unsigned short y,unsigned long FontColor,unsigned long BackGroundColor ,char *c);

/* nor flash使用外建字库 */
/* 16*16 24*24 32*32 */
void LT768_Select_Outside_Font_Init(unsigned char SCS,unsigned char Clk,unsigned long FlashAddr,unsigned long MemoryAddr,unsigned long Num,unsigned char Size,unsigned char XxN,unsigned char YxN,unsigned char ChromaKey,unsigned char Alignment);
void LT768_Print_Outside_Font_String(unsigned short x,unsigned short y,unsigned long FontColor,unsigned long BackGroundColor,unsigned char *c);
void LT768_Print_Outside_Font_GBK_String(unsigned short x,unsigned short y,unsigned long FontColor,unsigned long BackGroundColor,unsigned char *c);
/* 48*48 72*72全角 */
void LT768_BTE_Memory_Copy_ColorExpansion_8(unsigned long S0_Addr,unsigned short YS0,unsigned long Des_Addr,unsigned short Des_W,unsigned short XDes,unsigned short YDes,unsigned short X_W,unsigned short Y_H,unsigned long Foreground_color,unsigned long Background_color);
void LT768_BTE_Memory_Copy_ColorExpansion_Chroma_key_8(unsigned long S0_Addr,unsigned short YS0,unsigned long Des_Addr,unsigned short Des_W,unsigned short XDes,unsigned short YDes,unsigned short X_W,unsigned short Y_H,unsigned long Foreground_color);
void LT768_Print_Outside_Font_GB2312_48_72(unsigned char SCS,unsigned char Clk,unsigned long FlashAddr,unsigned long MemoryAddr,unsigned long ShowAddr,unsigned short width,unsigned char Size,unsigned char ChromaKey,unsigned short x,unsigned short y,unsigned long FontColor,unsigned long BackGroundColor,unsigned short w,unsigned short s,unsigned char *c);
void LT768_Print_Outside_Font_BIG5_48_72(unsigned char SCS,unsigned char Clk,unsigned long FlashAddr,unsigned long MemoryAddr,unsigned long ShowAddr,unsigned short width,unsigned char Size,unsigned char ChromaKey,unsigned short x,unsigned short y,unsigned long FontColor,unsigned long BackGroundColor,unsigned short w,unsigned short s,unsigned char *c);
void LT768_Print_Outside_Font_GBK_48_72(unsigned char SCS,unsigned char Clk,unsigned long FlashAddr,unsigned long MemoryAddr,unsigned long ShowAddr,unsigned short width,unsigned char Size,unsigned char ChromaKey,unsigned short x,unsigned short y,unsigned long FontColor,unsigned long BackGroundColor,unsigned short w,unsigned short s,unsigned char *c);		

/* nor flash使用自定义字库 */
/* 16*16 24*24 32*32 48*48 72*72全角 */
int Get_User_Font_P(char chH,char chL);
void LT768_Print_Outside_UserDefineFont_GB2312(unsigned char SCS,unsigned char Clk,unsigned long FlashAddr,unsigned long MemoryAddr,unsigned long ShowAddr,unsigned short width,unsigned char Size,unsigned char ChromaKey,unsigned short x,unsigned short y,unsigned long FontColor,unsigned long BackGroundColor,unsigned short w,unsigned short s,unsigned char *c);				

/* 文字光标 */
void LT768_Text_cursor_Init(unsigned char On_Off_Blinking,unsigned short Blinking_Time,unsigned short X_W,unsigned short Y_W);
void LT768_Enable_Text_Cursor(void);
void LT768_Disable_Text_Cursor(void);

/*  图像光标 */
void LT768_Graphic_cursor_Init(unsigned char Cursor_N,unsigned char Color1,unsigned char Color2,unsigned short X_Pos,unsigned short Y_Pos,unsigned char *Cursor_Buf);
void LT768_Set_Graphic_cursor_Pos(unsigned char Cursor_N,unsigned short X_Pos,unsigned short Y_Pos);
void LT768_Enable_Graphic_Cursor(void);
void LT768_Disable_Graphic_Cursor(void);

/* 区块传输引擎（BitBLT） */
void BTE_Solid_Fill(unsigned long Des_Addr,unsigned short Des_W,unsigned short XDes,unsigned short YDes,unsigned short color,unsigned short X_W,unsigned short Y_H);
void LT768_BTE_Memory_Copy(unsigned long S0_Addr,unsigned short S0_W,unsigned short XS0,unsigned short YS0,unsigned long S1_Addr,unsigned short S1_W,unsigned short XS1,unsigned short YS1,unsigned long Des_Addr,unsigned short Des_W,unsigned short XDes,unsigned short YDes,unsigned int ROP_Code,unsigned short X_W,unsigned short Y_H);
void LT768_BTE_Memory_Copy_Chroma_key(unsigned long S0_Addr,unsigned short S0_W,unsigned short XS0,unsigned short YS0,unsigned long Des_Addr,unsigned short Des_W,unsigned short XDes,unsigned short YDes,unsigned long Background_color,unsigned short X_W,unsigned short Y_H);
void LT768_BTE_Pattern_Fill(unsigned char P_8x8_or_16x16,unsigned long S0_Addr,unsigned short S0_W,unsigned short XS0,unsigned short YS0,unsigned long Des_Addr,unsigned short Des_W, unsigned short XDes,unsigned short YDes,unsigned int ROP_Code ,unsigned short X_W,unsigned short Y_H);
void LT768_BTE_Pattern_Fill_With_Chroma_key(unsigned char P_8x8_or_16x16,unsigned long S0_Addr,unsigned short S0_W,unsigned short XS0,unsigned short YS0,unsigned long Des_Addr,unsigned short Des_W,unsigned short XDes,unsigned short YDes,unsigned int ROP_Code,unsigned long Background_color,unsigned short X_W,unsigned short Y_H);
void LT768_BTE_MCU_Write_MCU_16bit(unsigned long S1_Addr,unsigned short S1_W,unsigned short XS,unsigned short YS1,unsigned long Des_Addr,unsigned short Des_W,unsigned short XDes,unsigned short YDes,unsigned int ROP_Code,unsigned short X_W,unsigned short Y_H ,const unsigned short *data);
void LT768_BTE_MCU_Write_Chroma_key_MCU_16bit(unsigned long Des_Addr,unsigned short Des_W,unsigned short XDes,unsigned short YDes,unsigned long Background_color,unsigned short X_W,unsigned short Y_H,const unsigned short *data);
void LT768_BTE_MCU_Write_ColorExpansion_MCU_16bit(unsigned long Des_Addr,unsigned short Des_W,unsigned short XDes,unsigned short YDes,unsigned short X_W,unsigned short Y_H,unsigned long Foreground_color ,unsigned long Background_color ,const unsigned short *data);
void LT768_BTE_MCU_Write_ColorExpansion_Chroma_key_MCU_16bit(unsigned long Des_Addr,unsigned short Des_W,unsigned short XDes,unsigned short YDes,unsigned short X_W,unsigned short Y_H,unsigned long Foreground_color,const unsigned short *data);
void BTE_Alpha_Blending(unsigned long S0_Addr,unsigned short S0_W,unsigned short XS0,unsigned short YS0,unsigned long S1_Addr,unsigned short S1_W,unsigned short XS1,unsigned short YS1,unsigned long Des_Addr,unsigned short Des_W,unsigned short XDes,unsigned short YDes,unsigned short X_W,unsigned short Y_H,unsigned char alpha);
void BTE_Pixel_8bpp_Alpha_Blending(unsigned long S0_Addr,unsigned short S0_W,unsigned short XS0,unsigned short YS0,unsigned long S1_Addr,unsigned short S1_W,unsigned short XS1,unsigned short YS1,unsigned long Des_Addr,unsigned short Des_W,unsigned short XDes,unsigned short YDes,unsigned short X_W,unsigned short Y_H);
void BTE_Pixel_16bpp_Alpha_Blending(unsigned long S0_Addr,unsigned short S0_W,unsigned short XS0,unsigned short YS0,unsigned long S1_Addr,unsigned short S1_W,unsigned short XS1,unsigned short YS1,unsigned long Des_Addr,unsigned short Des_W,unsigned short XDes,unsigned short YDes,unsigned short X_W,unsigned short Y_H);

/*  PIP */
void LT768_PIP_Init(unsigned char On_Off,unsigned char Select_PIP,unsigned long PAddr,unsigned short XP,unsigned short YP,unsigned long ImageWidth,unsigned short X_Dis,unsigned short Y_Dis,unsigned short X_W,unsigned short Y_H);
void LT768_Set_DisWindowPos(unsigned char On_Off,unsigned char Select_PIP,unsigned short X_Dis,unsigned short Y_Dis);

/*  PWM */
void LT768_PWM0_Init(unsigned char on_off,unsigned char Clock_Divided,unsigned char Prescalar,unsigned short Count_Buffer,unsigned short Compare_Buffer);
void LT768_PWM1_Init(unsigned char on_off,unsigned char Clock_Divided,unsigned char Prescalar,unsigned short Count_Buffer,unsigned short Compare_Buffer);
void LT768_PWM0_Duty(unsigned short Compare_Buffer);
void LT768_PWM1_Duty(unsigned short Compare_Buffer);

/*  Standby Mode */
void LT768_Standby(void);
void LT768_Wkup_Standby(void);

/* Suspend Mode */
void LT768_Suspend(void);
void LT768_Wkup_Suspend(void);

/* Sleep Mode */
void LT768_SleepMode(void);
void LT768_Wkup_Sleep(void);

/* W25QXX */
void LT768_SPI_Init(u8 cs,u8 div);
void W25QXX_Enter_4Byte_AddressMode(void);
void LT_W25QXX_Read(u8* pBuffer,u32 ReadAddr,u16 NumByteToRead);

/* nand flash W25N01GV */
u8 W25N01GV_ReadSR(u8 reg);
void W25N01GV_Write_SR(u8 reg,u8 val);
void W25N01GV_Wait_Busy(void);
void W25N01GV_ContinuousRead_Mode(void);
void W25N01GV_Write_Page(u16 page);
void W25N01GV_ReadPageAddr_Data(u8* pBuffer,u32 PageNum,u32 PageAddr,u16 NumByteToRead);
void LT_W25N01GV_Read(u8* pBuffer,u32 ReadAddr,u16 NumByteToRead);

/* nand flash显示图片 */
void LT768_Nand_Pic(unsigned char SCS,unsigned short X1,unsigned short Y1,unsigned short X_W,unsigned short Y_H,unsigned long Addr,unsigned long lay0,unsigned long lay1);

/* nand flash使用外建字库初始化 */
/* 16*16 24*24 32*32 */
void NandFlash_Select_Outside_Font_Init(unsigned char SCS,unsigned char Clk,unsigned long FlashAddr,unsigned long MemoryAddr,unsigned long Num,unsigned char Size,unsigned char XxN,unsigned char YxN,unsigned char ChromaKey,unsigned char Alignment);				

/* nand flash使用外建字库 */
/* 48*48 72*72全角 */
void LT768_BTE_Memory_Copy_8(unsigned long S0_Addr,unsigned short S0_W,unsigned short XS0,unsigned short YS0,unsigned long S1_Addr,unsigned short S1_W,unsigned short XS1,unsigned short YS1,unsigned long Des_Addr,unsigned short Des_W,unsigned short XDes,unsigned short YDes,unsigned int ROP_Code,unsigned short X_W,unsigned short Y_H);		
void LT768_Nand_8bpp_font(unsigned char SCS,unsigned char Clk,unsigned short X1,unsigned short Y1,unsigned short X_W,unsigned short Y_H,unsigned long Addr,unsigned long lay1,unsigned long lay2);
void LT768_Print_Outside_Font_GB2312_48_72_Nand(unsigned char SCS,unsigned char Clk,unsigned long FlashAddr,unsigned long MemoryAddr1,unsigned long MemoryAddr2,unsigned long ShowAddr,unsigned short width,unsigned char Size,unsigned char ChromaKey,unsigned short x,unsigned short y,unsigned long FontColor,unsigned long BackGroundColor,unsigned short w,unsigned short s,unsigned char *c);			
void LT768_Print_Outside_Font_GBK_48_72_Nand(unsigned char SCS,unsigned char Clk,unsigned long FlashAddr,unsigned long MemoryAddr1,unsigned long MemoryAddr2,unsigned long ShowAddr,unsigned short width,unsigned char Size,unsigned char ChromaKey,unsigned short x,unsigned short y,unsigned long FontColor,unsigned long BackGroundColor,unsigned short w,unsigned short s,unsigned char *c);

/* nand flash使用自定义字库 */
/* 16*16 24*24 32*32 48*48 72*72全角 */
void LT768_Print_Outside_UserDefineFont_GB2312_Nand(unsigned char SCS,unsigned char Clk,unsigned long FlashAddr,unsigned long MemoryAddr1,unsigned long MemoryAddr2,unsigned long ShowAddr,unsigned short width,unsigned char Size,unsigned char ChromaKey,unsigned short x,unsigned short y,unsigned long FontColor,unsigned long BackGroundColor,unsigned short w,unsigned short s,unsigned char *c);
#endif

























