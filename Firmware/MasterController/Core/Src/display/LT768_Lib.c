/*
 * Levetop's LT768x library (LT768_Lib V1.3), ported to the Digilog master controller's
 * STM32G474 (FMC bus, HAL). Changes from Levetop's copy: comments converted from GBK to UTF-8,
 * the STM32F103 includes replaced, and LT768_HW_Reset() uses the board's DISP_RESET pin.
 * The bus itself is in if_port.c.
 */
/********************* COPYRIGHT  **********************
* File Name        : LT768_Lib.c
* Author           : Levetop Electronics
* Version          : V1.3
* Date             : 2019-5-14
* Description      : LT768x所有功能函数
********************************************************/

#include "LT768_Lib.h"
#include "main.h" // DISP_RESET_Pin, HAL

unsigned char CCLK;    // LT768的内核时钟频率    
unsigned char MCLK;    // SDRAM的时钟频率
unsigned char SCLK;    // LCD的扫描时钟频率

//---------------------------------------------------------------------------------------------------------------------------------

//复位LT768
void LT768_HW_Reset(void)
{
	// DISP_RESET (PD3) is open drain in CubeMX: low holds the LT7683 in reset, released it
	// goes high through the module's pull-up. The LT7683 only takes a reset held for 256 of
	// its clocks, so 100 ms is plenty either way.
	HAL_GPIO_WritePin(DISP_RESET_GPIO_Port, DISP_RESET_Pin, GPIO_PIN_RESET);
	Delay_ms(100);
	HAL_GPIO_WritePin(DISP_RESET_GPIO_Port, DISP_RESET_Pin, GPIO_PIN_SET);
	Delay_ms(100);
}

//检查LT768系统
void System_Check_Temp(void)
{
	unsigned char i=0;
	unsigned char temp=0;
	unsigned char system_ok=0;
	do
	{
		if((LCD_StatusRead()&0x02)==0x00)    
		{
			Delay_ms(1);                  //若MCU 速度太快，必要時使用
			LCD_CmdWrite(0x01);
			Delay_ms(1);                  //若MCU 速度太快，必要時使用
			temp =LCD_DataRead();
			if((temp & 0x80)==0x80)       //检测CCR寄存器PLL是否准备好
			{
				system_ok=1;
				i=0;
			}
			else
			{
				Delay_ms(1); //若MCU 速度太快，必要時使用
				LCD_CmdWrite(0x01);
				Delay_ms(1); //若MCU 速度太快，必要時使用
				LCD_DataWrite(0x80);
			}
		}
		else
		{
			system_ok=0;
			i++;
		}
		if(system_ok==0 && i==5)
		{
			LT768_HW_Reset(); //note1
			i=0;
		}
	}while(system_ok==0);
}

void LT768_PLL_Initial(void) 
{    
	unsigned int temp = 0;
	unsigned int temp1;
	
	unsigned short lpllOD_sclk, lpllOD_cclk, lpllOD_mclk;
	unsigned short lpllR_sclk, lpllR_cclk, lpllR_mclk;
	unsigned short lpllN_sclk, lpllN_cclk, lpllN_mclk;
	
	temp = (LCD_HBPD + LCD_HFPD + LCD_HSPW + LCD_XSIZE_TFT) * (LCD_VBPD + LCD_VFPD + LCD_VSPW+LCD_YSIZE_TFT) * 60;   
	
	temp1 = (temp%1000000)/100000;
	if(temp1>5)
		 temp = temp / 1000000 + 1;
	else temp = temp / 1000000;
	
	SCLK = temp;
	temp = temp * 3;
	MCLK = temp;
	CCLK = temp;
	
	if(CCLK > 100)	CCLK = 100;
	if(MCLK > 100)	MCLK = 100;
	if(SCLK > 65)		SCLK = 65;

#if XI_4M 	
	
	lpllOD_sclk = 3;
	lpllOD_cclk = 2;
	lpllOD_mclk = 2;
	lpllR_sclk  = 2;
	lpllR_cclk  = 2;
	lpllR_mclk  = 2;
	lpllN_mclk  = MCLK;      
	lpllN_cclk  = CCLK;    
	lpllN_sclk  = 2*SCLK; 
	
#endif

#if XI_8M 	
	
	lpllOD_sclk = 3;
	lpllOD_cclk = 2;
	lpllOD_mclk = 2;
	lpllR_sclk  = 2;
	lpllR_cclk  = 4;
	lpllR_mclk  = 4;
	lpllN_mclk  = MCLK;      
	lpllN_cclk  = CCLK;    
	lpllN_sclk  = SCLK; 
	
#endif

#if XI_10M 	
	
	lpllOD_sclk = 3;
	lpllOD_cclk = 2;
	lpllOD_mclk = 2;
	lpllR_sclk  = 5;
	lpllR_cclk  = 5;
	lpllR_mclk  = 5;
	lpllN_mclk  = MCLK;      
	lpllN_cclk  = CCLK;    
	lpllN_sclk  = 2*SCLK; 
	
#endif

#if XI_12M 	
	
	lpllOD_sclk = 3;
	lpllOD_cclk = 2;
	lpllOD_mclk = 2;
	lpllR_sclk  = 3;
	lpllR_cclk  = 6;
	lpllR_mclk  = 6;
	lpllN_mclk  = MCLK;      
	lpllN_cclk  = CCLK;    
	lpllN_sclk  = SCLK; 
	
#endif   
      
	LCD_CmdWrite(0x05);
	LCD_DataWrite((lpllOD_sclk<<6) | (lpllR_sclk<<1) | ((lpllN_sclk>>8)&0x1));
	LCD_CmdWrite(0x07);
	LCD_DataWrite((lpllOD_mclk<<6) | (lpllR_mclk<<1) | ((lpllN_mclk>>8)&0x1));
	LCD_CmdWrite(0x09);
	LCD_DataWrite((lpllOD_cclk<<6) | (lpllR_cclk<<1) | ((lpllN_cclk>>8)&0x1));

	LCD_CmdWrite(0x06);
	LCD_DataWrite(lpllN_sclk);
	LCD_CmdWrite(0x08);
	LCD_DataWrite(lpllN_mclk);
	LCD_CmdWrite(0x0a);
	LCD_DataWrite(lpllN_cclk);
      
	LCD_CmdWrite(0x00);
	Delay_us(1);
	LCD_DataWrite(0x80);

	Delay_ms(1);	//单PLL铆
}


void LT768_SDRAM_initail(unsigned char mclk)
{
	unsigned short sdram_itv;

	LCD_RegisterWrite(0xe0,0x29);      
	LCD_RegisterWrite(0xe1,0x03);	//CAS:2=0x02�ACAS:3=0x03
	sdram_itv = (64000000 / 8192) / (1000/mclk) ;
	sdram_itv-=2;

	LCD_RegisterWrite(0xe2,sdram_itv);
	LCD_RegisterWrite(0xe3,sdram_itv >>8);
	LCD_RegisterWrite(0xe4,0x01);
	Check_SDRAM_Ready();
	Delay_ms(1);
}


void Set_LCD_Panel(void)
{
	//**[01h]**//   
	TFT_16bit();	
	//TFT_18bit();
	//TFT_24bit(); 
	
	#if STM32_FSMC_8
	Host_Bus_8bit();    //主机总线8bit
	#else
	Host_Bus_16bit();	//主机总线16bit
	#endif
      
	//**[02h]**//
	RGB_16b_16bpp();
	//RGB_16b_24bpp_mode1();
	MemWrite_Left_Right_Top_Down();	
	//MemWrite_Down_Top_Left_Right();
      
	//**[03h]**//
	Graphic_Mode();
	Memory_Select_SDRAM();
     
	PCLK_Falling();	       	//REG[12h]:下降沿 
	//PCLK_Rising();
	
	VSCAN_T_to_B();	        //REG[12h]:从上到下
	//VSCAN_B_to_T();				//从下到上
	
	PDATA_Set_RGB();        //REG[12h]:Select RGB output
	//PDATA_Set_RBG();
	//PDATA_Set_GRB();
	//PDATA_Set_GBR();
	//PDATA_Set_BRG();
	//PDATA_Set_BGR();

	HSYNC_Low_Active();     //REG[13h]:		  
	//HSYNC_High_Active();
	
	VSYNC_Low_Active();     //REG[13h]:			
	//VSYNC_High_Active();
	
	DE_High_Active();       //REG[13h]:	
	//DE_Low_Active();
 
	LCD_HorizontalWidth_VerticalHeight(LCD_XSIZE_TFT ,LCD_YSIZE_TFT);	
	LCD_Horizontal_Non_Display(LCD_HBPD);	                            
	LCD_HSYNC_Start_Position(LCD_HFPD);	                              
	LCD_HSYNC_Pulse_Width(LCD_HSPW);		                            	
	LCD_Vertical_Non_Display(LCD_VBPD);	                                
	LCD_VSYNC_Start_Position(LCD_VFPD);	                              
	LCD_VSYNC_Pulse_Width(LCD_VSPW);		                            	

	Memory_XY_Mode();	//Block mode (X-Y coordination addressing);块模式
	Memory_16bpp_Mode();	
}

void LT768_initial(void)
{
	LT768_PLL_Initial();
	LT768_SDRAM_initail(MCLK);
	Set_LCD_Panel();
}

void LT768_Init(void)
{
	Delay_ms(100);                    //delay for LT768 power on
	LT768_HW_Reset();                 //LT768复位
	System_Check_Temp();	            //检测复位是否成功
	Delay_ms(100);
	while(LCD_StatusRead()&0x02);	    //Initial_Display_test	and  set SW2 pin2 = 1
	LT768_initial();
}

//--------------------------------------------------------------------------------------------------------------------------------------------

void MPU8_8bpp_Memory_Write
(
 unsigned short x           // x坐标
,unsigned short y           // y坐标
,unsigned short w           // 宽度
,unsigned short h           // 高度
,const unsigned char *data  // 数据首地址
)
{														  
	unsigned short i,j;
	Graphic_Mode();
	Active_Window_XY(x,y);
	Active_Window_WH(w,h); 					
	Goto_Pixel_XY(x,y);
	LCD_CmdWrite(0x04);
	for(i=0;i< h;i++)
	{
		for(j=0;j< w;j++)
		{
		 Check_Mem_WR_FIFO_not_Full();
		 LCD_DataWrite(*data);
		 data++;
		}
	}
	Check_Mem_WR_FIFO_Empty();
}	 

			
void MPU8_16bpp_Memory_Write
(
 unsigned short x           // x坐标
,unsigned short y           // y坐标
,unsigned short w           // 宽度
,unsigned short h           // 高度
,const unsigned char *data  // 数据首地址
)
{
	unsigned short i,j;
	Graphic_Mode();
    Active_Window_XY(x,y);
	Active_Window_WH(w,h); 					
	Goto_Pixel_XY(x,y);
	LCD_CmdWrite(0x04);
	for(i=0;i< h;i++)
	{	
		for(j=0;j< w;j++)
		{
		 Check_Mem_WR_FIFO_not_Full();
		 LCD_DataWrite(*data);
		 data++;
		 Check_Mem_WR_FIFO_not_Full();
		 LCD_DataWrite(*data);
		 data++;
		}
	}
	Check_Mem_WR_FIFO_Empty();
}		 


void MPU8_24bpp_Memory_Write 
(
 unsigned short x           // x坐标
,unsigned short y           // y坐标
,unsigned short w           // 宽度
,unsigned short h           // 高度
,const unsigned char *data  // 数据首地址
)

{
	unsigned short i,j;
	Graphic_Mode();
    Active_Window_XY(x,y);
	Active_Window_WH(w,h); 					
	Goto_Pixel_XY(x,y);
	LCD_CmdWrite(0x04);
	for(i=0;i< h;i++)
	{	
		for(j=0;j< w;j++)
		{
		 Check_Mem_WR_FIFO_not_Full();
		 LCD_DataWrite(*data);
		 data++;
		 Check_Mem_WR_FIFO_not_Full();
		 LCD_DataWrite(*data);
		 data++;
		 Check_Mem_WR_FIFO_not_Full();
		 LCD_DataWrite(*data);
		 data++;
		}
	}
	Check_Mem_WR_FIFO_Empty();
}



void MPU16_16bpp_Memory_Write 
(
 unsigned short x            // x坐标
,unsigned short y            // y坐标
,unsigned short w            // 宽度
,unsigned short h            // 高度
,const unsigned short *data  // 数据首地址
)			
{
	unsigned short i,j;
	Graphic_Mode();
    Active_Window_XY(x,y);
	Active_Window_WH(w,h); 					
	Goto_Pixel_XY(x,y);
	LCD_CmdWrite(0x04);
	for(i=0;i< h;i++)
	{	
		for(j=0;j< w;j++)
		{
		 Check_Mem_WR_FIFO_not_Full();
		 LCD_DataWrite_Pixel(*data);
		 data++;
		}
	}
	Check_Mem_WR_FIFO_Empty();
}




void MPU16_24bpp_Mode1_Memory_Write 
(
 unsigned short x            // x坐标
,unsigned short y            // y坐标
,unsigned short w            // 宽度
,unsigned short h            // 高度
,const unsigned short *data  // 数据首地址
)	
{
	unsigned short i,j;
	Graphic_Mode();
    Active_Window_XY(x,y);
	Active_Window_WH(w,h); 					
	Goto_Pixel_XY(x,y);
	LCD_CmdWrite(0x04);
	for(i=0;i< h;i++)
	{	
		for(j=0;j< w/2;j++)
		{
		 LCD_DataWrite_Pixel(*data);
		 Check_Mem_WR_FIFO_not_Full();
		 data++;
		 LCD_DataWrite_Pixel(*data);
		 Check_Mem_WR_FIFO_not_Full();
		 data++;
		 LCD_DataWrite_Pixel(*data);
		 Check_Mem_WR_FIFO_not_Full();
		 data++;
		}
	}
	Check_Mem_WR_FIFO_Empty();
}


void MPU16_24bpp_Mode2_Memory_Write
(
 unsigned short x            // x坐标
,unsigned short y            // y坐标
,unsigned short w            // 宽度
,unsigned short h            // 高度
,const unsigned short *data  // 数据首地址
)	
{
	unsigned short i,j;
	Graphic_Mode();
    Active_Window_XY(x,y);
	Active_Window_WH(w,h); 					
	Goto_Pixel_XY(x,y);
	LCD_CmdWrite(0x04);
	for(i=0;i< h;i++)
	{	
		for(j=0;j< w;j++)
		{
		 Check_Mem_WR_FIFO_not_Full();
		 LCD_DataWrite_Pixel(*data);
		 data++;
		 Check_Mem_WR_FIFO_not_Full();
		 LCD_DataWrite_Pixel(*data);
		 data++;
		}
	}
	Check_Mem_WR_FIFO_Empty();
}
//--------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------- 线段 -----------------------------------------
void LT768_DrawLine
(
 unsigned short X1        // X1坐标
,unsigned short Y1        // Y1坐标
,unsigned short X2        // X2坐标
,unsigned short Y2        // Y2坐标
,unsigned long  LineColor // 线段颜色
)
{
	Foreground_color_65k(LineColor);
	Line_Start_XY(X1,Y1);
	Line_End_XY(X2,Y2);
	Start_Line();
	Check_2D_Busy();
}

void LT768_DrawLine_Width
(
 unsigned short X1        // X1坐标
,unsigned short Y1        // Y1坐标
,unsigned short X2        // X2坐标
,unsigned short Y2        // Y2坐标
,unsigned long  LineColor // 线段颜色
,unsigned short Width     // 线段宽度
)
{
	unsigned short  i = 0;
	signed  short x = 0, y = 0;
	double temp = 0;
	x = X2 - X1;
	y = Y2 - Y1;
	if(x == 0) temp = 2;
	else temp = -((double)y/(double)x);
	if(temp>=-1&&temp<=1)
	{
			while(Width--)
		{
			LT768_DrawLine(X1,Y1+i,X2,Y2+i,LineColor);
			i++;
		}	
	}
		
	else 
	{
			while(Width--)
		{
			LT768_DrawLine(X1+i,Y1,X2+i,Y2,LineColor);
			i++;
		}	
	}
}

//------------------------------------- 圆 -----------------------------------------
void LT768_DrawCircle
(
 unsigned short XCenter           // 圆心X位置
,unsigned short YCenter           // 圆心Y位置
,unsigned short R                 // 半径
,unsigned long CircleColor        // 画线颜色
)
{
	Foreground_color_65k(CircleColor);
	Circle_Center_XY(XCenter,YCenter);
	Circle_Radius_R(R);
	Start_Circle_or_Ellipse();
	Check_2D_Busy();
}

void LT768_DrawCircle_Fill
(
 unsigned short XCenter           // 圆心X位置
,unsigned short YCenter           // 圆心Y位置
,unsigned short R                 // 半径
,unsigned long ForegroundColor    // 背景颜色
)
{
	Foreground_color_65k(ForegroundColor);
	Circle_Center_XY(XCenter,YCenter);
	Circle_Radius_R(R);
	Start_Circle_or_Ellipse_Fill();
	Check_2D_Busy(); 
}

void LT768_DrawCircle_Width
(
 unsigned short XCenter          // 圆心X位置
,unsigned short YCenter          // 圆心Y位置
,unsigned short R                // 半径
,unsigned long CircleColor       // 画线颜色
,unsigned long ForegroundColor   // 背景颜色
,unsigned short Width            // 线宽
)
{
	LT768_DrawCircle_Fill(XCenter,YCenter,R+Width,CircleColor);
	LT768_DrawCircle_Fill(XCenter,YCenter,R,ForegroundColor);
}

//------------------------------------- 椭圆 -----------------------------------------
void LT768_DrawEllipse
(
 unsigned short XCenter          // 椭圆心X位置
,unsigned short YCenter          // 椭圆心Y位置
,unsigned short X_R              // 宽半径
,unsigned short Y_R              // 长半径
,unsigned long EllipseColor      // 画线颜色
)
{
	Foreground_color_65k(EllipseColor);
	Ellipse_Center_XY(XCenter,YCenter);
	Ellipse_Radius_RxRy(X_R,Y_R);
	Start_Circle_or_Ellipse();
	Check_2D_Busy(); 
}

void LT768_DrawEllipse_Fill
(
 unsigned short XCenter           // 椭圆心X位置
,unsigned short YCenter           // 椭圆心Y位置
,unsigned short X_R               // 宽半径
,unsigned short Y_R               // 长半径
,unsigned long ForegroundColor    // 背景颜色
)
{
	Foreground_color_65k(ForegroundColor);
	Ellipse_Center_XY(XCenter,YCenter);
	Ellipse_Radius_RxRy(X_R,Y_R);
	Start_Circle_or_Ellipse_Fill();
	Check_2D_Busy(); 
}

void LT768_DrawEllipse_Width
(
 unsigned short XCenter           // 椭圆心X位置
,unsigned short YCenter           // 椭圆心Y位置
,unsigned short X_R               // 宽半径
,unsigned short Y_R               // 长半径
,unsigned long EllipseColor       // 画线颜色
,unsigned long ForegroundColor    // 背景颜色
,unsigned short Width             // 线宽
)
{
	LT768_DrawEllipse_Fill(XCenter,YCenter,X_R+Width,Y_R+Width,EllipseColor);
	LT768_DrawEllipse_Fill(XCenter,YCenter,X_R,Y_R,ForegroundColor);
}

//------------------------------------- 矩形 -----------------------------------------
void LT768_DrawSquare
(
 unsigned short X1                // X1位置
,unsigned short Y1                // Y1位置
,unsigned short X2                // X2位置
,unsigned short Y2                // Y2位置
,unsigned long SquareColor        // 画线颜色
)
{
	Foreground_color_65k(SquareColor);
	Square_Start_XY(X1,Y1);
	Square_End_XY(X2,Y2);
	Start_Square();
	Check_2D_Busy(); 
}

void LT768_DrawSquare_Fill
(
 unsigned short X1                // X1位置
,unsigned short Y1                // Y1位置
,unsigned short X2                // X2位置
,unsigned short Y2                // Y2位置
,unsigned long ForegroundColor    // 背景颜色
)
{
	Foreground_color_65k(ForegroundColor);
	Square_Start_XY(X1,Y1);
	Square_End_XY(X2,Y2);
	Start_Square_Fill();
	Check_2D_Busy();
}

void LT768_DrawSquare_Width
(
 unsigned short X1                // X1位置
,unsigned short Y1                // Y1位置
,unsigned short X2                // X2位置
,unsigned short Y2                // Y2位置
,unsigned long SquareColor        // 画线颜色
,unsigned long ForegroundColor    // 背景颜色
,unsigned short Width             // 线宽
)
{
	LT768_DrawSquare_Fill(X1-Width,Y1-Width,X2+Width,Y2+Width,SquareColor);
	LT768_DrawSquare_Fill(X1,Y1,X2,Y2,ForegroundColor);
}

//------------------------------------- 圆角矩形 -----------------------------------------
void LT768_DrawCircleSquare
(
 unsigned short X1                // X1位置
,unsigned short Y1                // Y1位置
,unsigned short X2                // X2位置
,unsigned short Y2                // Y2位置
,unsigned short X_R               // 宽半径
,unsigned short Y_R               // 长半径
,unsigned long CircleSquareColor  // 画线颜色
)
{
	Foreground_color_65k(CircleSquareColor);
	Square_Start_XY(X1,Y1);
	Square_End_XY(X2,Y2); 
	Circle_Square_Radius_RxRy(X_R,Y_R);
	Start_Circle_Square();
	Check_2D_Busy();
}

void LT768_DrawCircleSquare_Fill
(
 unsigned short X1                // X1位置
,unsigned short Y1                // Y1位置
,unsigned short X2                // X2位置
,unsigned short Y2                // Y2位置
,unsigned short X_R               // 宽半径
,unsigned short Y_R               // 长半径
,unsigned long ForegroundColor  // 画线颜色
)
{
	Foreground_color_65k(ForegroundColor);
	Square_Start_XY(X1,Y1);
	Square_End_XY(X2,Y2); 
	Circle_Square_Radius_RxRy(X_R,Y_R);
	Start_Circle_Square_Fill();
	Check_2D_Busy(); 
}

void LT768_DrawCircleSquare_Width
(
 unsigned short X1                // X1位置
,unsigned short Y1                // Y1位置
,unsigned short X2                // X2位置
,unsigned short Y2                // Y2位置
,unsigned short X_R               // 宽半径
,unsigned short Y_R               // 长半径
,unsigned long CircleSquareColor  // 画线颜色
,unsigned long ForegroundColor    // 画线颜色
,unsigned short Width             // 宽度
)
{
	LT768_DrawCircleSquare_Fill(X1-Width,Y1-Width,X2+Width,Y2+Width,X_R,Y_R,CircleSquareColor);
	LT768_DrawCircleSquare_Fill(X1,Y1,X2,Y2,X_R,Y_R,ForegroundColor);
}

//------------------------------------- 三角形 -----------------------------------------
void LT768_DrawTriangle
(
 unsigned short X1              // X1位置
,unsigned short Y1              // Y1位置
,unsigned short X2              // X2位置
,unsigned short Y2              // Y2位置
,unsigned short X3              // X3位置
,unsigned short Y3              // Y3位置
,unsigned long TriangleColor    // 画线颜色
)
{
	Foreground_color_65k(TriangleColor);
	Triangle_Point1_XY(X1,Y1);
	Triangle_Point2_XY(X2,Y2);
	Triangle_Point3_XY(X3,Y3);
	Start_Triangle();
	Check_2D_Busy(); 
}



void LT768_DrawTriangle_Fill
(
 unsigned short X1              // X1位置
,unsigned short Y1              // Y1位置
,unsigned short X2              // X2位置
,unsigned short Y2              // Y2位置
,unsigned short X3              // X3位置
,unsigned short Y3              // Y3位置
,unsigned long ForegroundColor  // 画线颜色
)
{
	Foreground_color_65k(ForegroundColor);
	Triangle_Point1_XY(X1,Y1);
	Triangle_Point2_XY(X2,Y2);
	Triangle_Point3_XY(X3,Y3);
	Start_Triangle_Fill();
	Check_2D_Busy();
}

void LT768_DrawTriangle_Frame
(
 unsigned short X1              // X1位置
,unsigned short Y1              // Y1位置
,unsigned short X2              // X2位置
,unsigned short Y2              // Y2位置
,unsigned short X3              // X3位置
,unsigned short Y3              // Y3位置
,unsigned long TriangleColor    // 画线颜色
,unsigned long ForegroundColor  // 背景颜色
)
{
	LT768_DrawTriangle_Fill(X1,Y1,X2,Y2,X3,Y3,ForegroundColor);
	LT768_DrawTriangle(X1,Y1,X2,Y2,X3,Y3,TriangleColor);
}



//------------------------------------- 曲线 -----------------------------------------
void LT768_DrawLeftUpCurve
( 
 unsigned short XCenter           // 曲心X位置
,unsigned short YCenter           // 曲心Y位置
,unsigned short X_R               // 宽半径
,unsigned short Y_R               // 长半径
,unsigned long CurveColor         // 画线颜色
)
{
	Foreground_color_65k(CurveColor);
	Ellipse_Center_XY(XCenter,YCenter);
	Ellipse_Radius_RxRy(X_R,Y_R);
	Start_Left_Up_Curve();
	Check_2D_Busy(); 
}


void LT768_DrawLeftDownCurve
(
 unsigned short XCenter           // 曲心X位置
,unsigned short YCenter           // 曲心Y位置
,unsigned short X_R               // 宽半径
,unsigned short Y_R               // 长半径
,unsigned long CurveColor         // 画线颜色
)
{
	Foreground_color_65k(CurveColor);
	Ellipse_Center_XY(XCenter,YCenter);
	Ellipse_Radius_RxRy(X_R,Y_R);
	Start_Left_Down_Curve();
	Check_2D_Busy(); 
}


void LT768_DrawRightUpCurve
(
 unsigned short XCenter           // 曲心X位置
,unsigned short YCenter           // 曲心Y位置
,unsigned short X_R               // 宽半径
,unsigned short Y_R               // 长半径
,unsigned long CurveColor         // 画线颜色
)
{
	Foreground_color_65k(CurveColor);
	Ellipse_Center_XY(XCenter,YCenter);
	Ellipse_Radius_RxRy(X_R,Y_R);
	Start_Right_Up_Curve();
	Check_2D_Busy(); 
}


void LT768_DrawRightDownCurve
(
 unsigned short XCenter           // 曲心X位置
,unsigned short YCenter           // 曲心Y位置
,unsigned short X_R               // 宽半径
,unsigned short Y_R               // 长半径
,unsigned long CurveColor         // 画线颜色
)
{
	Foreground_color_65k(CurveColor);
	Ellipse_Center_XY(XCenter,YCenter);
	Ellipse_Radius_RxRy(X_R,Y_R);
	Start_Right_Down_Curve();
	Check_2D_Busy(); 
}


void LT768_SelectDrawCurve
(
 unsigned short XCenter           // 曲心X位置
,unsigned short YCenter           // 曲心Y位置
,unsigned short X_R               // 宽半径
,unsigned short Y_R               // 长半径
,unsigned long CurveColor         // 画线颜色
,unsigned short  Dir              // 方向
)
{
	switch(Dir)
	{
		case 0:LT768_DrawLeftDownCurve(XCenter,YCenter,X_R,Y_R,CurveColor);		break;
		case 1:LT768_DrawLeftUpCurve(XCenter,YCenter,X_R,Y_R,CurveColor);			break;
		case 2:LT768_DrawRightUpCurve(XCenter,YCenter,X_R,Y_R,CurveColor);		break;
		case 3:LT768_DrawRightDownCurve(XCenter,YCenter,X_R,Y_R,CurveColor);	break;
		default:																															break;
	}
}


//------------------------------------- 1/4实心椭圆 -----------------------------------------
void LT768_DrawLeftUpCurve_Fill
(
 unsigned short XCenter           // 曲心X位置
,unsigned short YCenter           // 曲心Y位置
,unsigned short X_R               // 宽半径
,unsigned short Y_R               // 长半径
,unsigned long ForegroundColor    // 背景颜色
)
{
	Foreground_color_65k(ForegroundColor);
	Ellipse_Center_XY(XCenter,YCenter);
	Ellipse_Radius_RxRy(X_R,Y_R);
	Start_Left_Up_Curve_Fill();
	Check_2D_Busy(); 
}


void LT768_DrawLeftDownCurve_Fill
(
 unsigned short XCenter           // 曲心X位置
,unsigned short YCenter           // 曲心Y位置
,unsigned short X_R               // 宽半径
,unsigned short Y_R               // 长半径
,unsigned long ForegroundColor    // 背景颜色
)
{
	Foreground_color_65k(ForegroundColor);
	Ellipse_Center_XY(XCenter,YCenter);
	Ellipse_Radius_RxRy(X_R,Y_R);
	Start_Left_Down_Curve_Fill();
	Check_2D_Busy(); 
}


void LT768_DrawRightUpCurve_Fill
(
 unsigned short XCenter           // 曲心X位置
,unsigned short YCenter           // 曲心Y位置
,unsigned short X_R               // 宽半径
,unsigned short Y_R               // 长半径
,unsigned long ForegroundColor    // 背景颜色
)
{
	Foreground_color_65k(ForegroundColor);
	Ellipse_Center_XY(XCenter,YCenter);
	Ellipse_Radius_RxRy(X_R,Y_R);
	Start_Right_Up_Curve_Fill();
	Check_2D_Busy(); 
}


void LT768_DrawRightDownCurve_Fill
(
 unsigned short XCenter           // 曲心X位置
,unsigned short YCenter           // 曲心Y位置
,unsigned short X_R               // 宽半径
,unsigned short Y_R               // 长半径
,unsigned long ForegroundColor    // 背景颜色
)
{
	Foreground_color_65k(ForegroundColor);
	Ellipse_Center_XY(XCenter,YCenter);
	Ellipse_Radius_RxRy(X_R,Y_R);
	Start_Right_Down_Curve_Fill();
	Check_2D_Busy(); 
}


void LT768_SelectDrawCurve_Fill
(
 unsigned short XCenter           // 曲心X位置
,unsigned short YCenter           // 曲心Y位置
,unsigned short X_R               // 宽半径
,unsigned short Y_R               // 长半径
,unsigned long CurveColor         // 画线颜色
,unsigned short  Dir              // 方向
)
{
	switch(Dir)
	{
		case 0:LT768_DrawLeftDownCurve_Fill(XCenter,YCenter,X_R,Y_R,CurveColor);		break;
		case 1:LT768_DrawLeftUpCurve_Fill(XCenter,YCenter,X_R,Y_R,CurveColor);			break;
		case 2:LT768_DrawRightUpCurve_Fill(XCenter,YCenter,X_R,Y_R,CurveColor);			break;
		case 3:LT768_DrawRightDownCurve_Fill(XCenter,YCenter,X_R,Y_R,CurveColor);		break;
		default:																																		break;
	}
}



//------------------------------------- 四边形 -----------------------------------------

void LT768_DrawQuadrilateral
(
 unsigned short X1              // X1位置
,unsigned short Y1              // Y1位置
,unsigned short X2              // X2位置
,unsigned short Y2              // Y2位置
,unsigned short X3              // X3位置
,unsigned short Y3              // Y3位置
,unsigned short X4              // X4位置
,unsigned short Y4              // Y4位置
,unsigned long ForegroundColor  // 画线颜色
)
{
	Foreground_color_65k(ForegroundColor);
	Triangle_Point1_XY(X1,Y1);
	Triangle_Point2_XY(X2,Y2);
	Triangle_Point3_XY(X3,Y3);
	Ellipse_Radius_RxRy(X4,Y4);

	LCD_CmdWrite(0x67);
	LCD_DataWrite(0x8d);
	Check_Busy_Draw();

	Check_2D_Busy(); 
}


void LT768_DrawQuadrilateral_Fill
(
 unsigned short X1              // X1位置
,unsigned short Y1              // Y1位置
,unsigned short X2              // X2位置
,unsigned short Y2              // Y2位置
,unsigned short X3              // X3位置
,unsigned short Y3              // Y3位置
,unsigned short X4              // X4位置
,unsigned short Y4              // Y4位置
,unsigned long ForegroundColor  // 画线颜色
)
{
	Foreground_color_65k(ForegroundColor);
	Triangle_Point1_XY(X1,Y1);
	Triangle_Point2_XY(X2,Y2);
	Triangle_Point3_XY(X3,Y3);
	Ellipse_Radius_RxRy(X4,Y4);

	LCD_CmdWrite(0x67);
	LCD_DataWrite(0xa7);
	Check_Busy_Draw();

	Check_2D_Busy(); 
}



//------------------------------------- 五边形 -----------------------------------------

void LT768_DrawPentagon
(
 unsigned short X1              // X1位置
,unsigned short Y1              // Y1位置
,unsigned short X2              // X2位置
,unsigned short Y2              // Y2位置
,unsigned short X3              // X3位置
,unsigned short Y3              // Y3位置
,unsigned short X4              // X4位置
,unsigned short Y4              // Y4位置
,unsigned short X5              // X5位置
,unsigned short Y5              // Y5位置
,unsigned long ForegroundColor  // 画线颜色
)
{
	Foreground_color_65k(ForegroundColor);
	Triangle_Point1_XY(X1,Y1);
	Triangle_Point2_XY(X2,Y2);
	Triangle_Point3_XY(X3,Y3);
	Ellipse_Radius_RxRy(X4,Y4);
	Ellipse_Center_XY(X5,Y5);

	LCD_CmdWrite(0x67);
	LCD_DataWrite(0x8F);
	Check_Busy_Draw();

	Check_2D_Busy(); 
}


void LT768_DrawPentagon_Fill
(
 unsigned short X1              // X1位置
,unsigned short Y1              // Y1位置
,unsigned short X2              // X2位置
,unsigned short Y2              // Y2位置
,unsigned short X3              // X3位置
,unsigned short Y3              // Y3位置
,unsigned short X4              // X4位置
,unsigned short Y4              // Y4位置
,unsigned short X5              // X5位置
,unsigned short Y5              // Y5位置
,unsigned long ForegroundColor  // 画线颜色
)
{
	Foreground_color_65k(ForegroundColor);
	Triangle_Point1_XY(X1,Y1);
	Triangle_Point2_XY(X2,Y2);
	Triangle_Point3_XY(X3,Y3);
	Ellipse_Radius_RxRy(X4,Y4);
	Ellipse_Center_XY(X5,Y5);

	LCD_CmdWrite(0x67);
	LCD_DataWrite(0xa9);
	Check_Busy_Draw();

	Check_2D_Busy(); 
}


//------------------------------------- 圆柱 -----------------------------------------
unsigned char LT768_DrawCylinder
(
 unsigned short XCenter           // 椭圆心X位置
,unsigned short YCenter           // 椭圆心Y位置
,unsigned short X_R               // 宽半径
,unsigned short Y_R               // 长半径
,unsigned short H                 // 高度
,unsigned long CylinderColor      // 画线颜色
,unsigned long ForegroundColor    // 背景颜色
)
{
	if(YCenter < H)	return 1;
	
	//底面椭圆
	LT768_DrawEllipse_Fill(XCenter,YCenter,X_R,Y_R,ForegroundColor);
	LT768_DrawEllipse(XCenter,YCenter,X_R,Y_R,CylinderColor);
	
	//中间矩形
	LT768_DrawSquare_Fill(XCenter-X_R,YCenter-H,XCenter+X_R,YCenter,ForegroundColor);
	
	//顶面椭圆
	LT768_DrawEllipse_Fill(XCenter,YCenter-H,X_R,Y_R,ForegroundColor);
	LT768_DrawEllipse(XCenter,YCenter-H,X_R,Y_R,CylinderColor);
	
	LT768_DrawLine(XCenter-X_R,YCenter,XCenter-X_R,YCenter-H,CylinderColor);
	LT768_DrawLine(XCenter+X_R,YCenter,XCenter+X_R,YCenter-H,CylinderColor);
	
	return 0;
}


//------------------------------------- 四棱柱 -----------------------------------------
void LT768_DrawQuadrangular
(
 unsigned short X1
,unsigned short Y1
,unsigned short X2
,unsigned short Y2
,unsigned short X3
,unsigned short Y3
,unsigned short X4
,unsigned short Y4
,unsigned short X5
,unsigned short Y5
,unsigned short X6
,unsigned short Y6
,unsigned long QuadrangularColor   // 画线颜色
,unsigned long ForegroundColor     // 背景颜色
)
{
	LT768_DrawSquare_Fill(X1,Y1,X5,Y5,ForegroundColor);
	LT768_DrawSquare(X1,Y1,X5,Y5,QuadrangularColor);
	
	LT768_DrawQuadrilateral_Fill(X1,Y1,X2,Y2,X3,Y3,X4,Y4,ForegroundColor);
	LT768_DrawQuadrilateral(X1,Y1,X2,Y2,X3,Y3,X4,Y4,QuadrangularColor);
	
	LT768_DrawQuadrilateral_Fill(X3,Y3,X4,Y4,X5,Y5,X6,Y6,ForegroundColor);
	LT768_DrawQuadrilateral(X3,Y3,X4,Y4,X5,Y5,X6,Y6,QuadrangularColor);
}


//----------------------------------------------------------------------表格-------------------------------------------------------------------
void LT768_MakeTable
(
	unsigned short X1,                  // 起始位置X1
	unsigned short Y1,                  // 起始位置X2
	unsigned short W,                   // 宽度
	unsigned short H,                   // 高度
	unsigned short Line,                // 行数
	unsigned short Row,                 // 列数
	unsigned long  TableColor,          // 线框颜色C1
	unsigned long  ItemColor,  					// 项目拦背景色C2
	unsigned long  ForegroundColor,     // 内部窗口背景色C3
	unsigned short width1,              // 内框宽度
	unsigned short width2,              // 外框宽度
	unsigned char  mode                 // 0：项目栏纵向   1：项目栏横向 
)
{
	unsigned short i = 0;
	unsigned short x2,y2;
	x2 = X1 + W * Row;
	y2 = Y1 + H * Line;
	
	LT768_DrawSquare_Width(X1,Y1,x2,y2,TableColor,ForegroundColor,width2);  
	
	if(mode == 0)		LT768_DrawSquare_Fill(X1,Y1,X1+W,y2,ItemColor);  
	else if(mode == 1)	LT768_DrawSquare_Fill(X1,Y1,x2,Y1+H,ItemColor); 
	
	for(i = 0 ; i < Line ; i++)
	{
		LT768_DrawLine_Width(X1,Y1+i*H,x2,Y1+i*H,TableColor,width1);
	}
	
	for(i = 0 ; i < Row ; i++)
	{
		LT768_DrawLine_Width(X1+i*W,Y1,X1+i*W,y2,TableColor,width1);
	}
}




//--------------------------------------------------------------------------------------------------------------------------------------------


void LT768_Color_Bar_ON(void)
{
	Color_Bar_ON();
}

void LT768_Color_Bar_OFF(void)
{
	Color_Bar_OFF();
}


//--------------------------------------------------------------------------------------------------------------------------------------------
/*--Flash-->DMA-->SDRAM--*/
/*address：24bit or 32bit*/

void LT768_DMA_24bit_Linear
(
 unsigned char SCS              // 选择外挂的SPI   : SCS：0       SCS：1
,unsigned char Clk              // SPI时钟分频参数 : SPI Clock = System Clock /{(Clk+1)*2}
,unsigned long flash_addr       // 要从flash读取数据的起始地址   
,unsigned long memory_addr      // 数据要传输到SDRAM的起始地址
,unsigned long data_num         // 传输的数据量
)
{
	
	Enable_SFlash_SPI();									             // 使能SPI功能
	if(SCS == 0)		Select_SFI_0();										 // 选择外挂的SPI0
	if(SCS == 1)		Select_SFI_1();										 // 选择外挂的SPI1
	
	Memory_Linear_Mode();
	Select_SFI_DMA_Mode();								             // 设置SPI的DMA模式
	
	SPI_Clock_Period(Clk);                             // SPI速率 
	SFI_DMA_Destination_Start_Address(memory_addr);  	 // 指定的内存的开始地址
	SFI_DMA_Transfer_Number(data_num);                 // DMA传输的数量
	SFI_DMA_Source_Start_Address(flash_addr);          // flash地址
	Check_Busy_SFI_DMA(); 
	Start_SFI_DMA();
	Check_Busy_SFI_DMA();
	Memory_XY_Mode();
}

void LT768_DMA_32bit_Linear
(
 unsigned char SCS              // 选择外挂的SPI   : SCS：0       SCS：1
,unsigned char Clk              // SPI时钟分频参数 : SPI Clock = System Clock /{(Clk+1)*2}
,unsigned long flash_addr       // 要从flash读取数据的起始地址   
,unsigned long memory_addr      // 数据要传输到SDRAM的起始地址
,unsigned long data_num         // 传输的数据量
)
{
	Enable_SFlash_SPI();									             // 使能SPI功能
	if(SCS == 0)		Select_SFI_0();								     // 选择外挂的SPI0
	if(SCS == 1)		Select_SFI_1();										 // 选择外挂的SPI1
	
	Memory_Linear_Mode();
	Select_SFI_DMA_Mode();								            // 设置SPI的DMA模式
	Select_SFI_32bit_Address();
	
	SPI_Clock_Period(Clk);                             // SPI速率 
	SFI_DMA_Destination_Start_Address(memory_addr);  	 // 指定的内存的开始地址
	SFI_DMA_Transfer_Number(data_num);                 // DMA传输的数量
	SFI_DMA_Source_Start_Address(flash_addr);          // flash地址
	Check_Busy_SFI_DMA(); 
	Start_SFI_DMA();
	Check_Busy_SFI_DMA();
	Memory_XY_Mode();
}

void LT768_DMA_24bit_Block
(
 unsigned char SCS         // 选择外挂的SPI   : SCS：0       SCS：1
,unsigned char Clk         // SPI时钟分频参数 : SPI Clock = System Clock /{(Clk+1)*2}
,unsigned short X1         // 传输到内存X1的位置
,unsigned short Y1         // 传输到内存Y1的位置
,unsigned short X_W        // DMA传输数据的宽度
,unsigned short Y_H        // DMA传输数据的高度
,unsigned short P_W        // 图片的宽度
,unsigned long Addr        // Flash的地址
)
{  

	Enable_SFlash_SPI();									          // 使能SPI功能
	if(SCS == 0)	Select_SFI_0();										// 选择外挂的SPI0
	if(SCS == 1)	Select_SFI_1();									  // 选择外挂的SPI1

										   
	Select_SFI_DMA_Mode();								          // 设置SPI的DMA模式
	SPI_Clock_Period(Clk);                          // 设置SPI的分频系数

	Goto_Pixel_XY(X1,Y1);									          // 在图形模式中设置内存的位置
	SFI_DMA_Destination_Upper_Left_Corner(X1,Y1);		// DMA传输的目的地（内存的位置）
	SFI_DMA_Transfer_Width_Height(X_W,Y_H);				  // 设置块数据的宽度和高度
	SFI_DMA_Source_Width(P_W);							        // 设置源数据的宽度
	SFI_DMA_Source_Start_Address(Addr); 					  // 设置源数据在Flash的地址

	Start_SFI_DMA();									              // 开始DMA传输
	Check_Busy_SFI_DMA();								            // 检测DMA是否传输完成
}



void LT768_DMA_32bit_Block
(
 unsigned char SCS         // 选择外挂的SPI   : SCS：0       SCS：1
,unsigned char Clk         // SPI时钟分频参数 : SPI Clock = System Clock /{(Clk+1)*2}
,unsigned short X1         // 内存X1的位置
,unsigned short Y1         // 内存Y1的位置
,unsigned short X_W        // DMA传输数据的宽度
,unsigned short Y_H        // DMA传输数据的高度
,unsigned short P_W        // 图片的宽度
,unsigned long Addr        // Flash的地址
)
{  

	Enable_SFlash_SPI();									
	if(SCS == 0)	Select_SFI_0();										       
	if(SCS == 1)	Select_SFI_1();										      

	Select_SFI_DMA_Mode();								  
	SPI_Clock_Period(Clk);

	Select_SFI_32bit_Address();							  

	Goto_Pixel_XY(X1,Y1);									  
	SFI_DMA_Destination_Upper_Left_Corner(X1,Y1);			
	SFI_DMA_Transfer_Width_Height(X_W,Y_H);				  
	SFI_DMA_Source_Width(P_W);							  
	SFI_DMA_Source_Start_Address(Addr); 						 

	Start_SFI_DMA();									  
	Check_Busy_SFI_DMA();								 
	Select_SFI_24bit_Address();
}

//--------------------------------------------------------------------------------------------------------------------------------------------
/* 选择内部集成字库初始化 */
void LT768_Select_Internal_Font_Init
(
 unsigned char Size         // 设置字体大小  16：16*16     24:24*24    32:32*32
,unsigned char XxN          // 字体的宽度放大倍数：1~4
,unsigned char YxN          // 字体的高度放大倍数：1~4
,unsigned char ChromaKey    // 0：字体背景色透明    1：可以设置字体的背景色
,unsigned char Alignment    // 0：不字体不对齐      1：字体对齐
)
{
	if(Size==16)	Font_Select_8x16_16x16();
	if(Size==24)	Font_Select_12x24_24x24();
	if(Size==32)	Font_Select_16x32_32x32();

	//(*)
	if(XxN==1)	Font_Width_X1();
	if(XxN==2)	Font_Width_X2();
	if(XxN==3)	Font_Width_X3();
	if(XxN==4)	Font_Width_X4();

	//(*)	
	if(YxN==1)	Font_Height_X1();
	if(YxN==2)	Font_Height_X2();
	if(YxN==3)	Font_Height_X3();
	if(YxN==4)	Font_Height_X4();

	//(*)
	if(ChromaKey==0)	Font_Background_select_Color();	
	if(ChromaKey==1)	Font_Background_select_Transparency();	

	//(*)
	if(Alignment==0)	Disable_Font_Alignment();
	if(Alignment==1)	Enable_Font_Alignment();
}


/* 显示内部集成字体 */
void LT768_Print_Internal_Font_String
(
 unsigned short x               // 字体开始显示的x位置
,unsigned short y               // 字体开始显示的y位置
,unsigned long FontColor        // 字体的颜色
,unsigned long BackGroundColor  // 字体的背景色（注意：当字体背景初始化成透明时，设置该值无效）
,char *c                        // 数据缓冲的首地址
)
{
	Text_Mode();
	CGROM_Select_Internal_CGROM();
	Foreground_color_65k(FontColor);
	Background_color_65k(BackGroundColor);
	Goto_Text_XY(x,y);
	Show_String(c);
}


/* 选择外部集成字库初始化 */
void LT768_Select_Outside_Font_Init
(
 unsigned char SCS           // 选择外挂的SPI   : SCS：0       SCS：1
,unsigned char Clk           // SPI时钟分频参数 : SPI Clock = System Clock /{(Clk+1)*2}
,unsigned long FlashAddr     // 源地址(Flash)
,unsigned long MemoryAddr    // 目的地址(SDRAM)
,unsigned long Num           // 字库的数据量大小
,unsigned char Size          // 设置字体大小  16：16*16     24:24*24    32:32*32
,unsigned char XxN           // 字体的宽度放大倍数：1~4
,unsigned char YxN           // 字体的高度放大倍数：1~4
,unsigned char ChromaKey     // 0：字体背景色透明    1：可以设置字体的背景色
,unsigned char Alignment     // 0：不字体不对齐      1：字体对齐
)
{
	if(Size==16)	Font_Select_8x16_16x16();
	if(Size==24)	Font_Select_12x24_24x24();
	if(Size==32)	Font_Select_16x32_32x32();

	//(*)
	if(XxN==1)	Font_Width_X1();
	if(XxN==2)	Font_Width_X2();
	if(XxN==3)	Font_Width_X3();
	if(XxN==4)	Font_Width_X4();

	//(*)	
	if(YxN==1)	Font_Height_X1();
	if(YxN==2)	Font_Height_X2();
	if(YxN==3)	Font_Height_X3();
	if(YxN==4)	Font_Height_X4();

	//(*)
	if(ChromaKey==0)	Font_Background_select_Color();	
	if(ChromaKey==1)	Font_Background_select_Transparency();	

	//(*)
	if(Alignment==0)	Disable_Font_Alignment();
	if(Alignment==1)	Enable_Font_Alignment();	
	
	LT768_DMA_24bit_Linear(SCS,Clk,FlashAddr,MemoryAddr,Num);
	CGRAM_Start_address(MemoryAddr);        
}

/* 显示外部及内部集成字体 */
void LT768_Print_Outside_Font_String
(
 unsigned short x               // 字体开始显示的x位置
,unsigned short y               // 字体开始显示的y位置
,unsigned long FontColor        // 字体的颜色
,unsigned long BackGroundColor  // 字体的背景色（注意：当字体背景初始化成透明时，设置该值无效）
,unsigned char *c               // 数据缓冲的首地址
)
{
	unsigned short temp_H = 0;
	unsigned short temp_L = 0;
	unsigned short temp = 0;
	unsigned long i = 0;
	
	Text_Mode();
	Font_Select_UserDefine_Mode();
	Foreground_color_65k(FontColor);
	Background_color_65k(BackGroundColor);
	Goto_Text_XY(x,y);
	
	while(c[i] != '\0')
	{ 
		if(c[i] < 0xa1)
		{
			CGROM_Select_Internal_CGROM();   // 内部CGROM为字符来源
			LCD_CmdWrite(0x04);
			LCD_DataWrite(c[i]);
			Check_Mem_WR_FIFO_not_Full();  
			i += 1;
		}
		else
		{
			Font_Select_UserDefine_Mode();   // 自定义字库
			LCD_CmdWrite(0x04);
			temp_H = ((c[i] - 0xa1) & 0x00ff) * 94;
			temp_L = c[i+1] - 0xa1;
			temp = temp_H + temp_L + 0x8000;
			LCD_DataWrite((temp>>8)&0xff);
			Check_Mem_WR_FIFO_not_Full();
			LCD_DataWrite(temp&0xff);
			Check_Mem_WR_FIFO_not_Full();
			i += 2;		
		}
	}

	Check_2D_Busy();

	Graphic_Mode(); //back to graphic mode;图形模式
}

/* 显示外部GBK及内部集成字体 */
void LT768_Print_Outside_Font_GBK_String
(
 unsigned short x               // 字体开始显示的x位置
,unsigned short y               // 字体开始显示的y位置
,unsigned long FontColor        // 字体的颜色
,unsigned long BackGroundColor  // 字体的背景色（注意：当字体背景初始化成透明时，设置该值无效）
,unsigned char *c               // 数据缓冲的首地址
)
{
	unsigned short temp_H = 0;
	unsigned short temp_L = 0;
	unsigned short temp = 0;
	unsigned long i = 0;
	
	Text_Mode();
	Font_Select_UserDefine_Mode();
  Foreground_color_65k(FontColor);
	Background_color_65k(BackGroundColor);
	Goto_Text_XY(x,y);
	
	while(c[i] != '\0')
  { 
		if(c[i] < 0x81)
		{
			CGROM_Select_Internal_CGROM();   // 内部CGROM为字符来源
			LCD_CmdWrite(0x04);
			LCD_DataWrite(c[i]);
			Check_Mem_WR_FIFO_not_Full();  
			i += 1;
		}
		else
		{
			Font_Select_UserDefine_Mode();   // 自定义字库
			LCD_CmdWrite(0x04);
			temp_H = ((c[i] - 0x81) & 0x00ff) * 191;
			temp_L = c[i+1] - 0x40;
			temp = temp_H + temp_L + 0x8000;
			LCD_DataWrite((temp>>8)&0xff);
			Check_Mem_WR_FIFO_not_Full();
			LCD_DataWrite(temp&0xff);
			Check_Mem_WR_FIFO_not_Full();
			i += 2;		
		}
	}
	
  Check_2D_Busy();

  Graphic_Mode(); //back to graphic mode;图形模式
}

/*显示48*48、72*72字库*/
void LT768_BTE_Memory_Copy_ColorExpansion_8
(
 unsigned long S0_Addr             // SO图像的内存起始地址
,unsigned short YS0                // S0图像的左上方Y坐标
,unsigned long Des_Addr            // 目的图像的内存起始地址
,unsigned short Des_W              // 目的图像的宽度
,unsigned short XDes               // 目的图像的左上方X坐标(显示窗口的起始x坐标)
,unsigned short YDes               // 目的图像的左上方Y坐标(显示窗口的起始y坐标)
,unsigned short X_W                // 显示窗口的宽度
,unsigned short Y_H                // 显示窗口的长度
,unsigned long Foreground_color
,unsigned long Background_color
)
{
	Foreground_color_256(Foreground_color);
	Background_color_256(Background_color);
	BTE_ROP_Code(7);
	
	BTE_S0_Color_8bpp();
	BTE_S0_Memory_Start_Address(S0_Addr);
	BTE_S0_Image_Width(Des_W);
	BTE_S0_Window_Start_XY(YS0,0);

	BTE_Destination_Color_16bpp();
	BTE_Destination_Memory_Start_Address(Des_Addr);
	BTE_Destination_Image_Width(Des_W);
	BTE_Destination_Window_Start_XY(XDes,YDes);
   
	BTE_Operation_Code(0x0e);	//BTE Operation: Memory copy (move) with chroma keying (w/o ROP)
	BTE_Window_Size(X_W,Y_H); 
	BTE_Enable();
	Check_BTE_Busy();
}

void LT768_BTE_Memory_Copy_ColorExpansion_Chroma_key_8
(
 unsigned long S0_Addr             // SO图像的内存起始地址
,unsigned short YS0                // S0图像的左上方Y坐标
,unsigned long Des_Addr            // 目的图像的内存起始地址
,unsigned short Des_W              // 目的图像的宽度
,unsigned short XDes               // 目的图像的左上方X坐标(显示窗口的起始x坐标)
,unsigned short YDes               // 目的图像的左上方Y坐标(显示窗口的起始y坐标)
,unsigned short X_W                // 显示窗口的宽度
,unsigned short Y_H                // 显示窗口的长度
,unsigned long Foreground_color
)
{
	Foreground_color_256(Foreground_color);
	BTE_ROP_Code(7);
	
	BTE_S0_Color_8bpp();
	BTE_S0_Memory_Start_Address(S0_Addr);
	BTE_S0_Image_Width(Des_W);
	BTE_S0_Window_Start_XY(YS0,0);	

	BTE_Destination_Color_16bpp();
	BTE_Destination_Memory_Start_Address(Des_Addr);
	BTE_Destination_Image_Width(Des_W);
	BTE_Destination_Window_Start_XY(XDes,YDes);
   
	BTE_Operation_Code(0x0f);	//BTE Operation: Memory copy (move) with chroma keying (w/o ROP)
	BTE_Window_Size(X_W,Y_H); 
	BTE_Enable();
	Check_BTE_Busy();
}

void LT768_Print_Outside_Font_GB2312_48_72
(
 unsigned char SCS           		// 选择外挂的SPI   : SCS：0       SCS：1
,unsigned char Clk           		// SPI时钟分频参数 : SPI Clock = System Clock /{(Clk+1)*2}
,unsigned long FlashAddr     		// 字库源地址(Flash)
,unsigned long MemoryAddr    		// 目的地址(SDRAM)
,unsigned long ShowAddr             // 显示层的地址
,unsigned short width               // 显示层的宽度
,unsigned char Size          		// 设置字体大小  48：48*48     72:72*72
,unsigned char ChromaKey     		// 0：字体背景色透明    1：可以设置字体的背景色
,unsigned short x                   // 字体开始显示的x位置
,unsigned short y                   // 字体开始显示的y位置
,unsigned long FontColor            // 字体的颜色
,unsigned long BackGroundColor      // 字体的背景色（注意：当字体背景初始化成透明时，设置该值无效）
,unsigned short w					// 字体粗细：0：不加粗  1：加粗1级  2：加粗2级
,unsigned short s                   // 行距
,unsigned char *c                   // 数据缓冲的首地址
)
{
	unsigned short temp_H = 0;
	unsigned short temp_L = 0;
	unsigned short temp = 0;
	unsigned long i = 0;
	unsigned short j = 0;
	unsigned short k = 0;
	unsigned short h = 0;
	unsigned short n = 0;
	unsigned short m = 0;
	unsigned short g = 0;
	unsigned short f = 0;
	
	h = x; k = y;
	Memory_8bpp_Mode();//使用8位色深来存储图片
 	Canvas_Image_Start_address(MemoryAddr);
 	Canvas_image_width(width);
	while(c[i] != '\0')
	{
		temp_H = (c[i] - 0xa1) * 94;
		temp_L = c[i+1] - 0xa1;
		temp = temp_H + temp_L;
		LT768_DMA_24bit_Block(SCS,Clk,Size*j/8,0,Size/8,Size,Size/8,FlashAddr+temp*((Size*Size)/8));
		i+=2;
		j++;
	}
	
	Memory_16bpp_Mode();
	Canvas_Image_Start_address(ShowAddr);
	Canvas_image_width(width);
	j = 0; i = 0;
	
	if(w>2)	w = 2;
	for(g=0;g<w+1;g++)
	{
		while(c[i] != '\0')
		{
			if((f == m)&&((x+Size*j+Size)>(width*(n+1)))) {m++;n++;y=y+Size-1+s;x = x+((width*n)-(x+Size*j))+g;f=n;}
				
			if(ChromaKey==1)
			{
				LT768_BTE_Memory_Copy_ColorExpansion_8(MemoryAddr,Size*j/8,
										   ShowAddr,width,x+Size*j,y,
										   Size,Size,FontColor,BackGroundColor
										   );
			}
			if(ChromaKey==0)
			{
				LT768_BTE_Memory_Copy_ColorExpansion_Chroma_key_8(MemoryAddr,Size*j/8,   
												  ShowAddr,width,x+Size*j,y,
													Size,Size,FontColor
													);
			}
			i+=2;
			j++;
		}
		ChromaKey=0;i=0;j=0;m=0;n=0;f=0;x=h+g+1;y=k+g+1;
	}
}


void LT768_Print_Outside_Font_BIG5_48_72
(
 unsigned char SCS           		    // 选择外挂的SPI   : SCS：0       SCS：1
,unsigned char Clk           		    // SPI时钟分频参数 : SPI Clock = System Clock /{(Clk+1)*2}
,unsigned long FlashAddr     		    // 字库源地址(Flash)
,unsigned long MemoryAddr    		    // 目的地址(SDRAM)
,unsigned long ShowAddr             // 显示层的地址
,unsigned short width               // 显示层的宽度
,unsigned char Size          		    // 设置字体大小  48：48*48     72:72*72
,unsigned char ChromaKey     		    // 0：字体背景色透明    1：可以设置字体的背景色
,unsigned short x                   // 字体开始显示的x位置
,unsigned short y                   // 字体开始显示的y位置
,unsigned long FontColor            // 字体的颜色
,unsigned long BackGroundColor      // 字体的背景色（注意：当字体背景初始化成透明时，设置该值无效）
,unsigned short w				            // 字体粗细：0：不加粗  1：加粗1级  2：加粗2级
,unsigned short s                   // 行距
,unsigned char *c                   // 数据缓冲的首地址
)
{
	unsigned short temp_H = 0;
	unsigned short temp_L = 0;
	unsigned short temp = 0;
	unsigned long i = 0;
	unsigned short j = 0;
	unsigned short k = 0;
	unsigned short h = 0;
	unsigned short n = 0;
	unsigned short m = 0;
	unsigned short g = 0;
	unsigned short f = 0;
	h = x; k = y;
	Memory_8bpp_Mode();//使用8位色深来存储图片
 	Canvas_Image_Start_address(MemoryAddr);
 	Canvas_image_width(width);
	while(c[i] != '\0')
	{
		temp_H = (c[i] - 0xa1) * 160;
		if(c[i+1]<0x7f)
		{
			temp_L = c[i+1] - 0x40;
		}
		else
		{
			temp_L = c[i+1] - 0xa0 + 0x40;
		}
		temp = temp_H + temp_L;
		LT768_DMA_24bit_Block(SCS,Clk,Size*j/8,0,Size/8,Size,Size/8,FlashAddr+temp*((Size*Size)/8));
		i+=2;
		j++;
	}
	Memory_16bpp_Mode();   // 因为显示是16位的色深，所以需要转为16位色深	
	Canvas_Image_Start_address(ShowAddr);
	Canvas_image_width(width);
	j = 0; i = 0;
	for(g=0;g<w;g++)
	{
		while(c[i] != '\0')
		{
			if((f == m)&&((x+Size*j+Size)>(1024*(n+1)))) {m++;n++;y=y+Size-1+s;x = x+((1024*n)-(x+Size*j))+g;f=n;}
			if(ChromaKey==1)
			{
				LT768_BTE_Memory_Copy_ColorExpansion_8(MemoryAddr,Size*j/8,
										 ShowAddr,LCD_XSIZE_TFT,x+Size*j,y,
										 Size,Size,FontColor,BackGroundColor
										);
			}
			if(ChromaKey==0)
			{
				LT768_BTE_Memory_Copy_ColorExpansion_Chroma_key_8(MemoryAddr,Size*j/8,   
												 ShowAddr,LCD_XSIZE_TFT,x+Size*j,y,
												 Size,Size,FontColor
												);
			}
			i+=2;
			j++;
		}
		ChromaKey=0;i=0;j=0;m=0;n=0;f=0;x=h+g+1;y=k+g+1;
	}
}

void LT768_Print_Outside_Font_GBK_48_72
(
 unsigned char SCS           		// 选择外挂的SPI   : SCS：0       SCS：1
,unsigned char Clk           		// SPI时钟分频参数 : SPI Clock = System Clock /{(Clk+1)*2}
,unsigned long FlashAddr     		// 字库源地址(Flash)
,unsigned long MemoryAddr    		// 目的地址(SDRAM)
,unsigned long ShowAddr             // 显示层的地址
,unsigned short width               // 显示层的宽度
,unsigned char Size          		// 设置字体大小  48：48*48     72:72*72
,unsigned char ChromaKey     		// 0：字体背景色透明    1：可以设置字体的背景色
,unsigned short x                   // 字体开始显示的x位置
,unsigned short y                   // 字体开始显示的y位置
,unsigned long FontColor            // 字体的颜色
,unsigned long BackGroundColor      // 字体的背景色（注意：当字体背景初始化成透明时，设置该值无效）
,unsigned short w				// 字体粗细：0：不加粗  1：加粗1级  2：加粗2级
,unsigned short s                   // 行距
,unsigned char *c                   // 数据缓冲的首地址
)
{
	unsigned short temp_H = 0;
	unsigned short temp_L = 0;
	unsigned short temp = 0;
	unsigned long i = 0;
	unsigned short j = 0;
	unsigned short k = 0;
	unsigned short h = 0;
	unsigned short n = 0;
	unsigned short m = 0;
	unsigned short g = 0;
	unsigned short f = 0;
	
	h = x; k = y;
	Memory_8bpp_Mode();//使用8位色深来存储图片
 	Canvas_Image_Start_address(MemoryAddr);
 	Canvas_image_width(width);
	while(c[i] != '\0')
	{
		temp_H = (c[i] - 0x81) * 191;	
		temp_L = c[i+1] - 0x40;
		temp = temp_H + temp_L;
		LT768_DMA_24bit_Block(SCS,Clk,Size*j/8,0,Size/8,Size,Size/8,FlashAddr+temp*((Size*Size)/8));
		i+=2;
		j++;
	}
	
	Memory_16bpp_Mode();
	Canvas_Image_Start_address(ShowAddr);
	Canvas_image_width(width);
	j = 0; i = 0;
	
	if(w>2)	w = 2;
	for(g=0;g<w+1;g++)
	{
		while(c[i] != '\0')
		{
			if((f == m)&&((x+Size*j+Size)>(width*(n+1)))) {m++;n++;y=y+Size-1+s;x = x+((width*n)-(x+Size*j))+g;f=n;}
				
			if(ChromaKey==1)
			{
				LT768_BTE_Memory_Copy_ColorExpansion_8(MemoryAddr,Size*j/8,
										   ShowAddr,width,x+Size*j,y,
										   Size,Size,FontColor,BackGroundColor
										   );
			}
			if(ChromaKey==0)
			{
				LT768_BTE_Memory_Copy_ColorExpansion_Chroma_key_8(MemoryAddr,Size*j/8,   
												  ShowAddr,width,x+Size*j,y,
													Size,Size,FontColor
													);
			}
			i+=2;
			j++;
		}
		ChromaKey=0;i=0;j=0;m=0;n=0;f=0;x=h+g+1;y=k+g+1;
	}
}

/*自定义字库*/
//只能输入中文字
#define Font_size 200//定义字库大小，200：最多200个字
#define Font_BYTE Font_size*2
unsigned char Font_1[Font_BYTE];

void clear_Font_1(void)
{
	u16 i;
	for(i = 0; i < Font_BYTE;i++)
	{
		Font_1[i] = 0;
	}
}

int Get_User_Font_P(char chH,char chL)
{
	int i;
	
	for(i=0;i<(sizeof(Font_1)/2);i++)
	{
		if((chH == Font_1[i*2])&&(chL == Font_1[i*2+1]))
		{
			return i;
		}
	}
	
	return -1;
}

void LT768_Print_Outside_UserDefineFont_GB2312
(
 unsigned char SCS           		// 选择外挂的SPI   : SCS：0       SCS：1
,unsigned char Clk           		// SPI时钟分频参数 : SPI Clock = System Clock /{(Clk+1)*2}
,unsigned long FlashAddr     		// 字库源地址(Flash)
,unsigned long MemoryAddr    		// 目的地址(SDRAM)
,unsigned long ShowAddr             // 显示层的地址
,unsigned short width               // 显示层的宽度
,unsigned char Size          		// 设置字体大小  48：48*48     72:72*72
,unsigned char ChromaKey     		// 0：字体背景色透明    1：可以设置字体的背景色
,unsigned short x                   // 字体开始显示的x位置
,unsigned short y                   // 字体开始显示的y位置
,unsigned long FontColor            // 字体的颜色
,unsigned long BackGroundColor      // 字体的背景色（注意：当字体背景初始化成透明时，设置该值无效）
,unsigned short w				    // 字体粗细：0：不加粗  1：加粗1级  2：加粗2级
,unsigned short s                   // 行距
,unsigned char *c                   // 数据缓冲的首地址
)
{
    short temp = 0;
	unsigned long i = 0;
	unsigned short j = 0;
	unsigned short k = 0;
	unsigned short h = 0;
	unsigned short n = 0;
	unsigned short m = 0;
	unsigned short g = 0;
	unsigned short f = 0;
	unsigned char temp1[2] = {0};
	unsigned int fontNum = 0;
	
	clear_Font_1();
	LT768_SPI_Init(SCS,1);
	LT_W25QXX_Read(temp1,FlashAddr+8,2);
	fontNum = (temp1[0]<<8)+temp1[1];
	LT_W25QXX_Read(Font_1,FlashAddr + 0xc,fontNum * 2);
	
	h = x; k = y;
	Memory_8bpp_Mode();//使用8位色深来存储图片
 	Canvas_Image_Start_address(MemoryAddr);
 	Canvas_image_width(width);
	
	while(c[i] != '\0')
	{
		temp = Get_User_Font_P(c[i],c[i+1]);
		if(temp>-1)
		{
			LT768_DMA_24bit_Block(SCS,Clk,Size*j/8,0,Size/8,Size,Size/8,FlashAddr+temp*((Size*Size)/8)+0x0c+fontNum*2);
		}
		i+=2;
		j++;
	}
	
	Memory_16bpp_Mode();
	Canvas_Image_Start_address(ShowAddr);
	Canvas_image_width(width);
	j = 0; i = 0;
	
	if(w>2)	w = 2;
	for(g=0;g<w+1;g++)
	{
		while(c[i] != '\0')
		{
			if((f == m)&&((x+Size*j+Size)>(width*(n+1)))) {m++;n++;y=y+Size-1+s;x = x+((width*n)-(x+Size*j))+g;f=n;}
				
			if(ChromaKey==1)
			{
				LT768_BTE_Memory_Copy_ColorExpansion_8(MemoryAddr,Size*j/8,
										   ShowAddr,width,x+Size*j,y,
										   Size,Size,FontColor,BackGroundColor
										   );
			}
			if(ChromaKey==0)
			{
				LT768_BTE_Memory_Copy_ColorExpansion_Chroma_key_8(MemoryAddr,Size*j/8,   
												  ShowAddr,width,x+Size*j,y,
													Size,Size,FontColor
													);
			}
			i+=2;
			j++;
		}
		ChromaKey=0;i=0;j=0;m=0;n=0;f=0;x=h+g+1;y=k+g+1;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------------------

void LT768_Text_cursor_Init
(
 unsigned char On_Off_Blinking         // 0：禁止光标闪烁   1：使能光标闪烁
,unsigned short Blinking_Time          // 设置文字光标闪烁时间
,unsigned short X_W                    // 文字光标水平大小
,unsigned short Y_W                    // 文字光标垂直大小
)
{
	if(On_Off_Blinking == 0)	Disable_Text_Cursor_Blinking();
	if(On_Off_Blinking == 1)	Enable_Text_Cursor_Blinking();

	Blinking_Time_Frames(Blinking_Time); 

	//[3E][3Fh]
	Text_Cursor_H_V(X_W,Y_W);

	Enable_Text_Cursor();
}


void LT768_Enable_Text_Cursor(void)
{
	Enable_Text_Cursor();
}


void LT768_Disable_Text_Cursor(void)
{
	Disable_Text_Cursor();
}


void LT768_Graphic_cursor_Init
(
 unsigned char Cursor_N                  // 选择光标   1:光标1   2:光标2   3:光标3  4:光标4
,unsigned char Color1                    // 颜色1
,unsigned char Color2                    // 颜色2
,unsigned short X_Pos                    // 显示坐标X
,unsigned short Y_Pos                    // 显示坐标Y
,unsigned char *Cursor_Buf               // 光标数据的缓冲首地址
)
{
	unsigned int i ;
	
	Memory_Select_Graphic_Cursor_RAM(); 
	Graphic_Mode();
	
	switch(Cursor_N)
	{
		case 1:	Select_Graphic_Cursor_1();	break;
		case 2:	Select_Graphic_Cursor_2();	break;
		case 3:	Select_Graphic_Cursor_3();	break;
		case 4:	Select_Graphic_Cursor_4();	break;
		default:break;
	}
	
	LCD_CmdWrite(0x04);
	for(i=0;i<256;i++)
	{					 
		LCD_DataWrite(Cursor_Buf[i]);
	}

	Memory_Select_SDRAM();//写完后切回SDRAM
	Set_Graphic_Cursor_Color_1(Color1);
	Set_Graphic_Cursor_Color_2(Color2);
	Graphic_Cursor_XY(X_Pos,Y_Pos);
	
	Enable_Graphic_Cursor();
}


void LT768_Set_Graphic_cursor_Pos
(
 unsigned char Cursor_N                  // 选择光标   1:光标1   2:光标2   3:光标3  4:光标4
,unsigned short X_Pos                    // 显示坐标X
,unsigned short Y_Pos                    // 显示坐标Y
)
{
	Graphic_Cursor_XY(X_Pos,Y_Pos);
	switch(Cursor_N)
	{
		case 1:	Select_Graphic_Cursor_1();	break;
		case 2:	Select_Graphic_Cursor_2();	break;
		case 3:	Select_Graphic_Cursor_3();	break;
		case 4:	Select_Graphic_Cursor_4();	break;
		default:
		break;
	}
}


void LT768_Enable_Graphic_Cursor(void)
{
	Enable_Graphic_Cursor();
}


void LT768_Disable_Graphic_Cursor(void)
{
	Disable_Graphic_Cursor();
}


//-----------------------------------------------------------------------------------------------------------------------------

void LT768_PIP_Init
(
 unsigned char On_Off         // 0 : 禁止 PIP    1 : 使能 PIP    2 : 保持原来的状态
,unsigned char Select_PIP     // 1 : 使用 PIP1   2 : 使用 PIP2
,unsigned long PAddr          // PIP的开始地址
,unsigned short XP            // PIP窗口的X坐标,必须被4整除
,unsigned short YP            // PIP窗口的Y坐标,必须被4整除
,unsigned long ImageWidth     // 底图的宽度
,unsigned short X_Dis         // 显示窗口的X坐标
,unsigned short Y_Dis         // 显示窗口的Y坐标
,unsigned short X_W           // 显示窗口的宽度，必须被4整除
,unsigned short Y_H           // 显示窗口的长度，必须被4整除
)
{
	if(Select_PIP == 1 )  
	{
		Select_PIP1_Window_16bpp();
		Select_PIP1_Parameter();
	}
	if(Select_PIP == 2 )  
	{
		Select_PIP2_Window_16bpp();
		Select_PIP2_Parameter();
	}
	
	PIP_Display_Start_XY(X_Dis,Y_Dis);
	PIP_Image_Start_Address(PAddr);
	PIP_Image_Width(ImageWidth);
	PIP_Window_Image_Start_XY(XP,YP);
	PIP_Window_Width_Height(X_W,Y_H);
	

	if(On_Off == 0)
	{
		if(Select_PIP == 1 )  Disable_PIP1();	
		if(Select_PIP == 2 )  Disable_PIP2();
	}

	if(On_Off == 1)
	{
		if(Select_PIP == 1 )  Enable_PIP1();	
		if(Select_PIP == 2 )  Enable_PIP2();
	}
}


void LT768_Set_DisWindowPos
(
 unsigned char On_Off         // 0 : 禁止 PIP, 1 : 使能 PIP, 2 : 保持原来的状态
,unsigned char Select_PIP     // 1 : 使用 PIP1 , 2 : 使用 PIP2
,unsigned short X_Dis         // 显示窗口的X坐标
,unsigned short Y_Dis         // 显示窗口的Y坐标
)
{
	if(Select_PIP == 1 )  Select_PIP1_Parameter();
	if(Select_PIP == 2 )  Select_PIP2_Parameter();
	
	if(On_Off == 0)
	{
		if(Select_PIP == 1 )  Disable_PIP1();	
		if(Select_PIP == 2 )  Disable_PIP2();
	}

	if(On_Off == 1)
	{
		if(Select_PIP == 1 )  Enable_PIP1();	
		if(Select_PIP == 2 )  Enable_PIP2();
	}
	
	PIP_Display_Start_XY(X_Dis,Y_Dis);
	
}

//-----------------------------------------------------------------------------------------------------------------------------

void BTE_Solid_Fill
(
 unsigned long Des_Addr           // 填充的目的地址 
,unsigned short Des_W             // 目的地址图片宽度
,unsigned short XDes              // x坐标 
,unsigned short YDes              // y坐标 
,unsigned short color             // 填充的颜色 
,unsigned short X_W               // 填充的长度 
,unsigned short Y_H               // 填充的宽度 
)            
{
	
	BTE_Destination_Color_16bpp();

	BTE_Destination_Memory_Start_Address(Des_Addr);

	BTE_Destination_Image_Width(Des_W);
	BTE_Destination_Window_Start_XY(XDes,YDes);
	BTE_Window_Size(X_W,Y_H);

	Foreground_color_65k(color);
	BTE_Operation_Code(0x0c);
	BTE_Enable();
	Check_BTE_Busy();     
}

/*  结合光栅操作的BTE内存复制 */
void LT768_BTE_Memory_Copy
(
 unsigned long S0_Addr     // SO图像的内存起始地址
,unsigned short S0_W       // S0图像的宽度
,unsigned short XS0        // S0图像的左上方X坐标
,unsigned short YS0        // S0图像的左上方Y坐标
,unsigned long S1_Addr     // S1图像的内存起始地址
,unsigned short S1_W       // S1图像的宽度
,unsigned short XS1        // S1图像的左上方X坐标
,unsigned short YS1        // S1图像的左上方Y坐标
,unsigned long Des_Addr    // 目的图像的内存起始地址
,unsigned short Des_W      // 目的图像的宽度
,unsigned short XDes       // 目的图像的左上方X坐标
,unsigned short YDes       // 目的图像的左上方Y坐标
,unsigned int ROP_Code     // 光栅操作模式
/*ROP_Code :
   0000b		0(Blackness)
   0001b		~S0!E~S1 or ~(S0+S1)
   0010b		~S0!ES1
   0011b		~S0
   0100b		S0!E~S1
   0101b		~S1
   0110b		S0^S1
   0111b		~S0 + ~S1 or ~(S0 + S1)
   1000b		S0!ES1
   1001b		~(S0^S1)
   1010b		S1
   1011b		~S0+S1
   1100b		S0
   1101b		S0+~S1
   1110b		S0+S1
   1111b		1(whiteness)*/
,unsigned short X_W       // 活动窗口的宽度
,unsigned short Y_H       // 活动窗口的长度
)
{
	BTE_S0_Color_16bpp();
	BTE_S0_Memory_Start_Address(S0_Addr);
	BTE_S0_Image_Width(S0_W);
	BTE_S0_Window_Start_XY(XS0,YS0);

	BTE_S1_Color_16bpp();
	BTE_S1_Memory_Start_Address(S1_Addr);
	BTE_S1_Image_Width(S1_W); 
	BTE_S1_Window_Start_XY(XS1,YS1);

	BTE_Destination_Color_16bpp();
	BTE_Destination_Memory_Start_Address(Des_Addr);
	BTE_Destination_Image_Width(Des_W);
	BTE_Destination_Window_Start_XY(XDes,YDes);	

	BTE_ROP_Code(ROP_Code);	
	BTE_Operation_Code(0x02); //BTE Operation: Memory copy (move) with ROP.
	BTE_Window_Size(X_W,Y_H); 
	BTE_Enable();
	Check_BTE_Busy();
}


/*  结合 Chroma Key 的内存复制（不含 ROP） */
void LT768_BTE_Memory_Copy_Chroma_key
(
 unsigned long S0_Addr             // SO图像的内存起始地址
,unsigned short S0_W               // S0图像的宽度
,unsigned short XS0                // S0图像的左上方X坐标
,unsigned short YS0                // S0图像的左上方Y坐标
,unsigned long Des_Addr            // 目的图像的内存起始地址
,unsigned short Des_W              // 目的图像的宽度
,unsigned short XDes               // 目的图像的左上方X坐标
,unsigned short YDes               // 目的图像的左上方X坐标
,unsigned long Background_color    // 透明色
,unsigned short X_W                // 活动窗口的宽度
,unsigned short Y_H                // 活动窗口的长度
)
{
	Background_color_65k(Background_color); 

	BTE_S0_Color_16bpp();
	BTE_S0_Memory_Start_Address(S0_Addr);
	BTE_S0_Image_Width(S0_W);
	BTE_S0_Window_Start_XY(XS0,YS0);	

	BTE_Destination_Color_16bpp();
	BTE_Destination_Memory_Start_Address(Des_Addr);
	BTE_Destination_Image_Width(Des_W);
	BTE_Destination_Window_Start_XY(XDes,YDes);

	BTE_Operation_Code(0x05);	//BTE Operation: Memory copy (move) with chroma keying (w/o ROP)
	BTE_Window_Size(X_W,Y_H); 
	BTE_Enable();
	Check_BTE_Busy();
}


void LT768_BTE_Pattern_Fill
(
 unsigned char P_8x8_or_16x16      // 0 : use 8x8 Icon , 1 : use 16x16 Icon.
,unsigned long S0_Addr             // SO图像的内存起始地址
,unsigned short S0_W               // S0图像的宽度
,unsigned short XS0                // S0图像的左上方X坐标
,unsigned short YS0                // S0图像的左上方Y坐标
,unsigned long Des_Addr            // 目的图像的内存起始地址
,unsigned short Des_W              // 目的图像的宽度
, unsigned short XDes              // 目的图像的左上方X坐标
,unsigned short YDes               // 目的图像的左上方X坐标
,unsigned int ROP_Code             // 光栅操作模式
/*ROP_Code :
   0000b		0(Blackness)
   0001b		~S0!E~S1 or ~(S0+S1)
   0010b		~S0!ES1
   0011b		~S0
   0100b		S0!E~S1
   0101b		~S1
   0110b		S0^S1
   0111b		~S0 + ~S1 or ~(S0 + S1)
   1000b		S0!ES1
   1001b		~(S0^S1)
   1010b		S1
   1011b		~S0+S1
   1100b		S0
   1101b		S0+~S1
   1110b		S0+S1
   1111b		1(whiteness)*/
,unsigned short X_W                // 活动窗口的宽度
,unsigned short Y_H                // 活动窗口的长度
)
{
	if(P_8x8_or_16x16 == 0)
	{
		Pattern_Format_8X8();
	}
	if(P_8x8_or_16x16 == 1)
	{														    
		Pattern_Format_16X16();
	}	

	BTE_S0_Color_16bpp();
	BTE_S0_Memory_Start_Address(S0_Addr);
	BTE_S0_Image_Width(S0_W);
	BTE_S0_Window_Start_XY(XS0,YS0);

	BTE_Destination_Color_16bpp();
	BTE_Destination_Memory_Start_Address(Des_Addr);
	BTE_Destination_Image_Width(Des_W);
	BTE_Destination_Window_Start_XY(XDes,YDes);	

	BTE_ROP_Code(ROP_Code);	
	BTE_Operation_Code(0x06); //BTE Operation: Pattern Fill with ROP.
	BTE_Window_Size(X_W,Y_H); 
	BTE_Enable();
	Check_BTE_Busy();
}



void LT768_BTE_Pattern_Fill_With_Chroma_key
(
 unsigned char P_8x8_or_16x16      // 0 : use 8x8 Icon , 1 : use 16x16 Icon.
,unsigned long S0_Addr             // SO图像的内存起始地址
,unsigned short S0_W               // S0图像的宽度
,unsigned short XS0                // S0图像的左上方X坐标
,unsigned short YS0                // S0图像的左上方Y坐标
,unsigned long Des_Addr            // 目的图像的内存起始地址
,unsigned short Des_W              // 目的图像的宽度
,unsigned short XDes               // 目的图像的左上方X坐标
,unsigned short YDes               // 目的图像的左上方Y坐标
,unsigned int ROP_Code             // 光栅操作模式
/*ROP_Code :
   0000b		0(Blackness)
   0001b		~S0!E~S1 or ~(S0+S1)
   0010b		~S0!ES1
   0011b		~S0
   0100b		S0!E~S1
   0101b		~S1
   0110b		S0^S1
   0111b		~S0 + ~S1 or ~(S0 + S1)
   1000b		S0!ES1
   1001b		~(S0^S1)
   1010b		S1
   1011b		~S0+S1
   1100b		S0
   1101b		S0+~S1
   1110b		S0+S1
   1111b		1(whiteness)*/
,unsigned long Background_color   // 透明色
,unsigned short X_W               // 活动窗口的宽度
,unsigned short Y_H               // 活动窗口的长度
)
{
	Background_color_65k(Background_color);

	if(P_8x8_or_16x16 == 0)
	{
	Pattern_Format_8X8();
	}
	if(P_8x8_or_16x16 == 1)
	{														    
	Pattern_Format_16X16();
	}	  

	BTE_S0_Color_16bpp();
	BTE_S0_Memory_Start_Address(S0_Addr);
	BTE_S0_Image_Width(S0_W);
	BTE_S0_Window_Start_XY(XS0,YS0);

	BTE_Destination_Color_16bpp();
	BTE_Destination_Memory_Start_Address(Des_Addr);
	BTE_Destination_Image_Width(Des_W);
	BTE_Destination_Window_Start_XY(XDes,YDes);	

	BTE_ROP_Code(ROP_Code);	
	BTE_Operation_Code(0x07); //BTE Operation: Pattern Fill with Chroma key.
	BTE_Window_Size(X_W,Y_H); 
	BTE_Enable();
	Check_BTE_Busy();
}



void LT768_BTE_MCU_Write_MCU_16bit
(
 unsigned long S1_Addr              // S1图像的内存起始地址
,unsigned short S1_W                // S1图像的宽度
,unsigned short XS1                 // S1图像的左上方X坐标
,unsigned short YS1                 // S1图像的左上方Y坐标
,unsigned long Des_Addr             // 目的图像的内存起始地址
,unsigned short Des_W               // 目的图像的宽度
,unsigned short XDes                // 目的图像的左上方X坐标
,unsigned short YDes                // 目的图像的左上方Y坐标
,unsigned int ROP_Code              // 光栅操作模式 
/*ROP_Code :
   0000b		0(Blackness)
   0001b		~S0!E~S1 or ~(S0+S1)
   0010b		~S0!ES1
   0011b		~S0
   0100b		S0!E~S1
   0101b		~S1
   0110b		S0^S1
   0111b		~S0 + ~S1 or ~(S0 + S1)
   1000b		S0!ES1
   1001b		~(S0^S1)
   1010b		S1
   1011b		~S0+S1
   1100b		S0
   1101b		S0+~S1
   1110b		S0+S1
   1111b		1(whiteness)*/
,unsigned short X_W                 // 活动窗口的宽度
,unsigned short Y_H                 // 活动窗口的长度
,const unsigned short *data         // S0的数据首地址
)
{
	unsigned short i,j;

	BTE_S1_Color_16bpp();
	BTE_S1_Memory_Start_Address(S1_Addr);
	BTE_S1_Image_Width(S1_W); 
	BTE_S1_Window_Start_XY(XS1,YS1);

	BTE_Destination_Color_16bpp();
	BTE_Destination_Memory_Start_Address(Des_Addr);
	BTE_Destination_Image_Width(Des_W);
	BTE_Destination_Window_Start_XY(XDes,YDes);

	BTE_Window_Size(X_W,Y_H);
	BTE_ROP_Code(ROP_Code);
	BTE_Operation_Code(0x00);		//BTE Operation: MPU Write with ROP.
	BTE_Enable();

	BTE_S0_Color_16bpp();
	LCD_CmdWrite(0x04);				 		//Memory Data Read/Write Port

	//MCU_16bit_ColorDepth_16bpp
	for(i=0;i< Y_H;i++)
	{	
		for(j=0;j< (X_W);j++)
	  {
			Check_Mem_WR_FIFO_not_Full();
			LCD_DataWrite_Pixel((*data));
			data++;
	  }
	}
	Check_Mem_WR_FIFO_Empty();
	Check_BTE_Busy();
}



void LT768_BTE_MCU_Write_Chroma_key_MCU_16bit
(
 unsigned long Des_Addr                 // 目的图像的内存起始地址
,unsigned short Des_W                   // 目的图像的宽度
,unsigned short XDes                    // 目的图像的左上方X坐标
,unsigned short YDes                    // 目的图像的左上方Y坐标
,unsigned long Background_color         // 透明色
,unsigned short X_W                     // 活动窗口的宽度
,unsigned short Y_H                     // 活动窗口的长度
,const unsigned short *data             // S0的数据收地址
)
{
	unsigned int i,j;

	Background_color_65k(Background_color);

	BTE_Destination_Color_16bpp();
	BTE_Destination_Memory_Start_Address(Des_Addr);
	BTE_Destination_Image_Width(Des_W);
	BTE_Destination_Window_Start_XY(XDes,YDes);

	BTE_Window_Size(X_W,Y_H);
	BTE_Operation_Code(0x04);		//BTE Operation: MPU Write with chroma keying (w/o ROP)
	BTE_Enable();

	BTE_S0_Color_16bpp();
	LCD_CmdWrite(0x04);			//Memory Data Read/Write Port

	//MCU_16bit_ColorDepth_16bpp
	for(i=0;i< Y_H;i++)
	{	
		for(j=0;j< (X_W);j++)
	  {
			Check_Mem_WR_FIFO_not_Full();
			LCD_DataWrite_Pixel((*data));
			data++;
	  }
	}
	Check_Mem_WR_FIFO_Empty();
	Check_BTE_Busy();
}


/* 结合扩展色彩的 MPU 寫入 */
void LT768_BTE_MCU_Write_ColorExpansion_MCU_16bit
(
 unsigned long Des_Addr               // 目的图像的内存起始地址
,unsigned short Des_W                 // 目的图像的宽度
,unsigned short XDes                  // 目的图像的左上方X坐标
,unsigned short YDes                  // 目的图像的左上方Y坐标
,unsigned short X_W                   // 活动窗口的宽度
,unsigned short Y_H                   // 活动窗口的长度
,unsigned long Foreground_color       // 前景色
/*Foreground_color : The source (1bit map picture) map data 1 translate to Foreground color by color expansion*/
,unsigned long Background_color       // 背景色
/*Background_color : The source (1bit map picture) map data 0 translate to Background color by color expansion*/
,const unsigned short *data           // 数据缓存首地址
)
{
	unsigned short i,j;

	RGB_16b_16bpp();
	Foreground_color_65k(Foreground_color);
	Background_color_65k(Background_color);
	BTE_ROP_Code(15);

	BTE_Destination_Color_16bpp();
	BTE_Destination_Memory_Start_Address(Des_Addr);
	BTE_Destination_Image_Width(Des_W);
	BTE_Destination_Window_Start_XY(XDes,YDes);


	BTE_Window_Size(X_W,Y_H);
	BTE_Operation_Code(0x8);		//BTE Operation: MPU Write with Color Expansion (w/o ROP)
	BTE_Enable();

	LCD_CmdWrite(0x04);				 		//Memory Data Read/Write Port  
	for(i=0;i< Y_H;i++)
	{	
		for(j=0;j< X_W/16;j++)
		{
			Check_Mem_WR_FIFO_not_Full();
			LCD_DataWrite_Pixel(*data);  
			data++;
		}
	}
	Check_Mem_WR_FIFO_Empty();
	Check_BTE_Busy();
}

/* 结合扩展色彩与 Chroma key 的 MPU 寫入 */
void LT768_BTE_MCU_Write_ColorExpansion_Chroma_key_MCU_16bit
(
 unsigned long Des_Addr            // 目的图像的内存起始地址
,unsigned short Des_W              // 目的图像的宽度
,unsigned short XDes               // 目的图像的左上方X坐标
,unsigned short YDes               // 目的图像的左上方Y坐标
,unsigned short X_W                // 活动窗口的宽度
,unsigned short Y_H                // 活动窗口的长度
,unsigned long Foreground_color    // 前景色
/*Foreground_color : The source (1bit map picture) map data 1 translate to Foreground color by color expansion*/
,const unsigned short *data        // 数据缓存首地址
)
{
	unsigned short i,j;

	RGB_16b_16bpp();
	Foreground_color_65k(Foreground_color);
	BTE_ROP_Code(15);

	BTE_Destination_Color_16bpp();
	BTE_Destination_Memory_Start_Address(Des_Addr);
	BTE_Destination_Image_Width(Des_W);
	BTE_Destination_Window_Start_XY(XDes,YDes);


	BTE_Window_Size(X_W,Y_H);
	BTE_Operation_Code(0x9);		//BTE Operation: MPU Write with Color Expansion and chroma keying (w/o ROP)
	BTE_Enable();

	LCD_CmdWrite(0x04);				 		//Memory Data Read/Write Port  
	for(i=0;i< Y_H;i++)
	{	
		for(j=0;j< X_W/16;j++)
		{
			Check_Mem_WR_FIFO_not_Full();
			LCD_DataWrite_Pixel(*data);  
			data++;
		}
	}
	Check_Mem_WR_FIFO_Empty();
	Check_BTE_Busy();
}

/* 结合透明度的内存复制 */
void BTE_Alpha_Blending
(
 unsigned long S0_Addr         // SO图像的内存起始地址
 ,unsigned short S0_W          // S0图像的宽度
 ,unsigned short XS0           // S0图像的左上方X坐标
 ,unsigned short YS0           // S0图像的左上方Y坐标
 ,unsigned long S1_Addr        // S1图像的内存起始地址
 ,unsigned short S1_W          // S1图像的宽度
 ,unsigned short XS1           // S1图像的左上方X坐标
 ,unsigned short YS1           // S1图像的左上方Y坐标
 ,unsigned long Des_Addr       // 目的图像的内存起始地址
 ,unsigned short Des_W         // 目的图像的宽度
 ,unsigned short XDes          // 目的图像的左上方X坐标
 ,unsigned short YDes          // 目的图像的左上方X坐标
 ,unsigned short X_W           // 活动窗口的宽度
 ,unsigned short Y_H           // 活动窗口的长度
 ,unsigned char alpha          // 透明度等级（32等级）
)
{	
	BTE_S0_Color_16bpp();
	BTE_S0_Memory_Start_Address(S0_Addr);
	BTE_S0_Image_Width(S0_W);
	BTE_S0_Window_Start_XY(XS0,YS0);

	BTE_S1_Color_16bpp();
	BTE_S1_Memory_Start_Address(S1_Addr);
	BTE_S1_Image_Width(S1_W); 
	BTE_S1_Window_Start_XY(XS1,YS1);

	BTE_Destination_Color_16bpp();
	BTE_Destination_Memory_Start_Address(Des_Addr);
	BTE_Destination_Image_Width(Des_W);
	BTE_Destination_Window_Start_XY(XDes,YDes);

	BTE_Window_Size(X_W,Y_H);
	BTE_Operation_Code(0x0A);		//BTE Operation: Memory write with opacity (w/o ROP)
	BTE_Alpha_Blending_Effect(alpha);
	BTE_Enable();
	Check_BTE_Busy();
}

void BTE_Pixel_8bpp_Alpha_Blending
(
 unsigned long S0_Addr         // SO图像的内存起始地址
 ,unsigned short S0_W          // S0图像的宽度
 ,unsigned short XS0           // S0图像的左上方X坐标
 ,unsigned short YS0           // S0图像的左上方Y坐标
 ,unsigned long S1_Addr        // S1图像的内存起始地址
 ,unsigned short S1_W          // S1图像的宽度
 ,unsigned short XS1           // S1图像的左上方X坐标
 ,unsigned short YS1           // S1图像的左上方Y坐标
 ,unsigned long Des_Addr       // 目的图像的内存起始地址
 ,unsigned short Des_W         // 目的图像的宽度
 ,unsigned short XDes          // 目的图像的左上方X坐标
 ,unsigned short YDes          // 目的图像的左上方X坐标
 ,unsigned short X_W           // 活动窗口的宽度
 ,unsigned short Y_H           // 活动窗口的长度
)
{	
	BTE_S0_Color_8bpp();
	BTE_S0_Memory_Start_Address(S0_Addr);
	BTE_S0_Image_Width(S0_W);
	BTE_S0_Window_Start_XY(XS0,YS0);

	BTE_S1_Color_8bit_Alpha();
	BTE_S1_Memory_Start_Address(S1_Addr);
	BTE_S1_Image_Width(S1_W); 
	BTE_S1_Window_Start_XY(XS1,YS1);

	BTE_Destination_Color_8bpp();
	BTE_Destination_Memory_Start_Address(Des_Addr);
	BTE_Destination_Image_Width(Des_W);
	BTE_Destination_Window_Start_XY(XDes,YDes);

	BTE_Window_Size(X_W,Y_H);
	BTE_Operation_Code(0x0A);		//BTE Operation: Memory write with opacity (w/o ROP)
	BTE_Enable();
	Check_BTE_Busy();
}

void BTE_Pixel_16bpp_Alpha_Blending
(
 unsigned long S0_Addr         // SO图像的内存起始地址
 ,unsigned short S0_W          // S0图像的宽度
 ,unsigned short XS0           // S0图像的左上方X坐标
 ,unsigned short YS0           // S0图像的左上方Y坐标
 ,unsigned long S1_Addr        // S1图像的内存起始地址
 ,unsigned short S1_W          // S1图像的宽度
 ,unsigned short XS1           // S1图像的左上方X坐标
 ,unsigned short YS1           // S1图像的左上方Y坐标
 ,unsigned long Des_Addr       // 目的图像的内存起始地址
 ,unsigned short Des_W         // 目的图像的宽度
 ,unsigned short XDes          // 目的图像的左上方X坐标
 ,unsigned short YDes          // 目的图像的左上方X坐标
 ,unsigned short X_W           // 活动窗口的宽度
 ,unsigned short Y_H           // 活动窗口的长度
)
{	
	BTE_S0_Color_16bpp();
	BTE_S0_Memory_Start_Address(S0_Addr);
	BTE_S0_Image_Width(S0_W);
	BTE_S0_Window_Start_XY(XS0,YS0);

	BTE_S1_Color_16bit_Alpha();
	BTE_S1_Memory_Start_Address(S1_Addr);
	BTE_S1_Image_Width(S1_W); 
	BTE_S1_Window_Start_XY(XS1,YS1);

	BTE_Destination_Color_16bpp();
	BTE_Destination_Memory_Start_Address(Des_Addr);
	BTE_Destination_Image_Width(Des_W);
	BTE_Destination_Window_Start_XY(XDes,YDes);

	BTE_Window_Size(X_W,Y_H);
	BTE_Operation_Code(0x0A);		//BTE Operation: Memory write with opacity (w/o ROP)
	BTE_Enable();
	Check_BTE_Busy();
}

//----------------------------------------------------------------------------------------------------------------------------------

void LT768_PWM0_Init
(
 unsigned char on_off                       // 0：禁止PWM0    1：使能PWM0
,unsigned char Clock_Divided                // PWM时钟分频  取值范围 0~3(1,1/2,1/4,1/8)
,unsigned char Prescalar                    // 时钟分频     取值范围 1~256
,unsigned short Count_Buffer                // 设置PWM的输出周期
,unsigned short Compare_Buffer              // 设置占空比
)
{
	Select_PWM0();
	Set_PWM_Prescaler_1_to_256(Prescalar);

	if(Clock_Divided ==0)	Select_PWM0_Clock_Divided_By_1();
	if(Clock_Divided ==1)	Select_PWM0_Clock_Divided_By_2();
	if(Clock_Divided ==2)	Select_PWM0_Clock_Divided_By_4();
	if(Clock_Divided ==3) Select_PWM0_Clock_Divided_By_8();

	Set_Timer0_Count_Buffer(Count_Buffer);  
	Set_Timer0_Compare_Buffer(Compare_Buffer);	

	if (on_off == 1)	Start_PWM0(); 
	if (on_off == 0)	Stop_PWM0();
}


void LT768_PWM0_Duty(unsigned short Compare_Buffer)
{
	Set_Timer0_Compare_Buffer(Compare_Buffer);
}



void LT768_PWM1_Init
(
 unsigned char on_off                       // 0：禁止PWM0    1：使能PWM0
,unsigned char Clock_Divided                // PWM时钟分频  取值范围 0~3(1,1/2,1/4,1/8)
,unsigned char Prescalar                    // 时钟分频     取值范围 1~256
,unsigned short Count_Buffer                // 设置PWM的输出周期
,unsigned short Compare_Buffer              // 设置占空比
)
{
	Select_PWM1();
	Set_PWM_Prescaler_1_to_256(Prescalar);
 
	if(Clock_Divided ==0)	Select_PWM1_Clock_Divided_By_1();
	if(Clock_Divided ==1)	Select_PWM1_Clock_Divided_By_2();
	if(Clock_Divided ==2) Select_PWM1_Clock_Divided_By_4();
	if(Clock_Divided ==3)	Select_PWM1_Clock_Divided_By_8();

	Set_Timer1_Count_Buffer(Count_Buffer); 
	Set_Timer1_Compare_Buffer(Compare_Buffer); 

	if (on_off == 1)	Start_PWM1(); 
	if (on_off == 0)	Stop_PWM1();
}


void LT768_PWM1_Duty(unsigned short Compare_Buffer)
{
	Set_Timer1_Compare_Buffer(Compare_Buffer);
}

//----------------------------------------------------------------------------------------------------------------------------------

// LT768进入待命模式
void LT768_Standby(void)
{
	Power_Saving_Standby_Mode();
	Check_Power_is_Saving();
}
// 从待命模式中唤醒
void LT768_Wkup_Standby(void)
{
	Power_Normal_Mode();
	Check_Power_is_Normal();
}


// LT768进入暂停模式
void LT768_Suspend(void)
{
	LT768_SDRAM_initail(10);
	Power_Saving_Suspend_Mode();
	Check_Power_is_Saving();
}
// 从暂停模式中唤醒
void LT768_Wkup_Suspend(void)
{
	Power_Normal_Mode();
	Check_Power_is_Normal();
	LT768_SDRAM_initail(MCLK);
}


// LT768进入休眠模式
void LT768_SleepMode(void)
{
	Power_Saving_Sleep_Mode();
	Check_Power_is_Saving();
}
// 从休眠模式中唤醒
void LT768_Wkup_Sleep(void)
{
	Power_Normal_Mode();
	Check_Power_is_Normal();
}

//--------------------------------------------------------------------------------------------------------------------------------------------

void LT768_SPI_Init(u8 cs,u8 div)
{
	if(cs == 0)
	{
		Select_SFI_0();
		Select_nSS_drive_on_xnsfcs0();
	}
	else
	{
		Select_SFI_1();
		Select_nSS_drive_on_xnsfcs1();
	}
	SPI_Clock_Period(div);
	Enable_SFlash_SPI();
}

void W25QXX_Enter_4Byte_AddressMode(void)//val:选择外挂的SPI: SCS：0  SCS：1
{
	nSS_Active();
	SPI_Master_FIFO_Data_Put(0xB7);
	nSS_Inactive();
}

void LT_W25QXX_Read(u8* pBuffer,u32 ReadAddr,u16 NumByteToRead)
{
	u16 i;
	nSS_Active();
	SPI_Master_FIFO_Data_Put(0x03);
	SPI_Master_FIFO_Data_Put((uint8_t)((ReadAddr)>>16));
	SPI_Master_FIFO_Data_Put((uint8_t)((ReadAddr)>>8));
	SPI_Master_FIFO_Data_Put((uint8_t)ReadAddr);
	for(i=0;i<NumByteToRead;i++)
	{
		pBuffer[i]=SPI_Master_FIFO_Data_Put(0XFF);
	}
	nSS_Inactive();
}

//--------------------------------------------------------------------------------------------------------------------------------------------

u8 W25N01GV_ReadSR(u8 reg)
{
	u8 byte = 0;
	nSS_Active();
	SPI_Master_FIFO_Data_Put(0x0f);
	SPI_Master_FIFO_Data_Put(reg);
	byte = SPI_Master_FIFO_Data_Put(0Xff);
	nSS_Inactive();
	return byte;
}

void W25N01GV_Write_SR(u8 reg,u8 val)   
{
	nSS_Active();
	SPI_Master_FIFO_Data_Put(0x1f);
	SPI_Master_FIFO_Data_Put(reg);
	SPI_Master_FIFO_Data_Put(val);
	nSS_Inactive();
}

void W25N01GV_Wait_Busy(void)   
{   
	while((W25N01GV_ReadSR(0xC0)&0x01)==0x01);
} 

void W25N01GV_ContinuousRead_Mode(void)
{
	u8 statut = 0;
	statut = W25N01GV_ReadSR(0xb0);
	statut &= cClrb3;
	statut &= cClrb4;
	statut |= cSetb4;
	W25N01GV_Write_SR(0xb0,statut);
}

void W25N01GV_Write_Page(u16 page)
{
	nSS_Active();
	SPI_Master_FIFO_Data_Put(0x13);
	SPI_Master_FIFO_Data_Put(0x00);
	SPI_Master_FIFO_Data_Put((u8)(page>>8));
	SPI_Master_FIFO_Data_Put((u8)page);
	nSS_Inactive();
	W25N01GV_Wait_Busy();
}

void W25N01GV_ReadPageAddr_Data(u8* pBuffer,u32 PageNum,u32 PageAddr,u16 NumByteToRead)
{ 
	u16 i; 

	//------从flash读数据到BUFF-----	
	nSS_Active();                            	    //使能器件   
	SPI_Master_FIFO_Data_Put(0x13);
	SPI_Master_FIFO_Data_Put(0x00);
	SPI_Master_FIFO_Data_Put((u8)((PageNum)>>8));  	//发送16bit地址    
	SPI_Master_FIFO_Data_Put((u8)(PageNum));   
	nSS_Inactive();  	
	W25N01GV_Wait_Busy();
	W25N01GV_Write_SR(0xb0,W25N01GV_ReadSR(0xb0) | 0x08);	//设定从BUFF的哪个位置开始读

	//------再从BUFF读数据到单片机内-----
	nSS_Active();   	
	SPI_Master_FIFO_Data_Put(0x03); 
	SPI_Master_FIFO_Data_Put((PageAddr)>>8);  
	SPI_Master_FIFO_Data_Put(PageAddr); 	
	SPI_Master_FIFO_Data_Put(0x00);
	for(i=0;i<NumByteToRead;i++)
	{ 
		pBuffer[i]=SPI_Master_FIFO_Data_Put(0XFF);   	//循环读数  
	}
	nSS_Active();
	W25N01GV_Wait_Busy();
}

void LT_W25N01GV_Read(u8* pBuffer,u32 ReadAddr,u16 NumByteToRead)//读2k以内数据，从页0位置开始读
{
	unsigned short page,endpage,remainder;
	remainder = ReadAddr%2048;//图片在一页中的起始位置
	page = (ReadAddr/2048);//图片起始地址所在的页编号（页地址）
	endpage = ((ReadAddr+NumByteToRead)/2048);//图片终止地址所在的页编号（页地址）
	LT768_SPI_Init(1,1);
	if(page == endpage)
	{
		W25N01GV_ReadPageAddr_Data(pBuffer,page,remainder,NumByteToRead);
	}
	else
	{
		W25N01GV_ReadPageAddr_Data(pBuffer,page,remainder,2048 - remainder);
		W25N01GV_ReadPageAddr_Data(&pBuffer[2048 - remainder],endpage,0,NumByteToRead - (2048 - remainder));
	}
}

//参数：X1：   在显示区域里指定显示的起始坐标x
//      Y1:    在显示区域里指定显示的起始坐标y
//      X_W:    图片的宽度
//      Y_H:    图片的高度
//      Addr: 图片在FLASH的存储起始地址
//      lay0: 显示区   (用于显示最终处理好的图片)
//      lay1: 缓存区1  (用于存放最终处理好的图片)

void LT768_Nand_Pic
(
 unsigned char SCS
,unsigned short X1
,unsigned short Y1
,unsigned short X_W
,unsigned short Y_H
,unsigned long Addr
,unsigned long lay0
,unsigned long lay1
)
{
	unsigned short page,endpage,remainder,X_W4 = 0;
	remainder = Addr%2048;//图片在一页中的起始位置
	page = (Addr/2048);//图片起始地址所在的页编号（页地址）
	if(X_W % 4 != 0)
	{
		X_W4 = (X_W / 4) * 4 + 4;
	}
	else
	{
		X_W4 = X_W;
	}
	endpage = ((Addr+X_W4 * Y_H * 2)/2048);//图片终止地址所在的页编号（页地址）
	LT768_SPI_Init(SCS,1);
	W25N01GV_ContinuousRead_Mode();
	W25N01GV_Write_Page(page);
	LT768_DMA_24bit_Linear(1,1,0,lay1 - remainder,((endpage-page+1)*2048));//往图层2传入图片所在的页的所有数据
	Canvas_Image_Start_address(lay0);
	Canvas_image_width(LCD_XSIZE_TFT);
	  LT768_BTE_Memory_Copy(lay1,X_W4,0,0,
							lay1,X_W4,0,0,
							lay0,LCD_XSIZE_TFT,X1,Y1,
							0x0c,X_W,Y_H);
}

void NandFlash_Select_Outside_Font_Init
(
 unsigned char SCS           // 选择外挂的SPI   : SCS：0       SCS：1
,unsigned char Clk           // SPI时钟分频参数 : SPI Clock = System Clock /{(Clk+1)*2}
,unsigned long FlashAddr     // 源地址(Flash)
,unsigned long MemoryAddr    // 目的地址(SDRAM)
,unsigned long Num           // 字库的数据量大小
,unsigned char Size          // 设置字体大小  16：16*16     24:24*24    32:32*32
,unsigned char XxN           // 字体的宽度放大倍数：1~4
,unsigned char YxN           // 字体的高度放大倍数：1~4
,unsigned char ChromaKey     // 0：字体背景色透明    1：可以设置字体的背景色
,unsigned char Alignment     // 0：不字体不对齐      1：字体对齐
)
{
	u16 page,endpage,remainder;
	remainder = FlashAddr%2048;//字库在一页中的起始位置
	page = (FlashAddr/2048);//字库起始地址所在的页编号（页地址）
	endpage = ((FlashAddr+Num)/2048);//字库终止地址所在的页编号（页地址）
	LT768_SPI_Init(1,1);
	W25N01GV_ContinuousRead_Mode();
	W25N01GV_Write_Page(page);
	
	if(Size==16)	Font_Select_8x16_16x16();
	if(Size==24)	Font_Select_12x24_24x24();
	if(Size==32)	Font_Select_16x32_32x32();

	//(*)
	if(XxN==1)	Font_Width_X1();
	if(XxN==2)	Font_Width_X2();
	if(XxN==3)	Font_Width_X3();
	if(XxN==4)	Font_Width_X4();

	//(*)	
	if(YxN==1)	Font_Height_X1();
	if(YxN==2)	Font_Height_X2();
	if(YxN==3)	Font_Height_X3();
	if(YxN==4)	Font_Height_X4();

	//(*)
	if(ChromaKey==0)	Font_Background_select_Color();	
	if(ChromaKey==1)	Font_Background_select_Transparency();	

	//(*)
	if(Alignment==0)	Disable_Font_Alignment();
	if(Alignment==1)	Enable_Font_Alignment();	
	
	LT768_DMA_24bit_Linear(SCS,Clk,FlashAddr,MemoryAddr,(endpage-page+1)*2048);
	CGRAM_Start_address(MemoryAddr+remainder);
}

void LT768_BTE_Memory_Copy_8
(
 unsigned long S0_Addr		// SO图像的内存起始地址
,unsigned short S0_W		// S0图像的宽度
,unsigned short XS0			// S0图像的左上方X坐标
,unsigned short YS0			// S0图像的左上方Y坐标
,unsigned long S1_Addr		// S1图像的内存起始地址
,unsigned short S1_W		// S1图像的宽度
,unsigned short XS1			// S1图像的左上方X坐标
,unsigned short YS1			// S1图像的左上方Y坐标
,unsigned long Des_Addr		// 目的图像的内存起始地址
,unsigned short Des_W		// 目的图像的宽度
,unsigned short XDes		// 目的图像的左上方X坐标
,unsigned short YDes		// 目的图像的左上方Y坐标
,unsigned int ROP_Code		// 光栅操作模式
,unsigned short X_W			// 活动窗口的宽度
,unsigned short Y_H			// 活动窗口的长度
)
{
	BTE_S0_Color_8bpp();
	BTE_S0_Memory_Start_Address(S0_Addr);
	BTE_S0_Image_Width(S0_W);
	BTE_S0_Window_Start_XY(XS0,YS0);

	BTE_S1_Color_8bpp();
	BTE_S1_Memory_Start_Address(S1_Addr);
	BTE_S1_Image_Width(S1_W); 
	BTE_S1_Window_Start_XY(XS1,YS1);

	BTE_Destination_Color_8bpp();
	BTE_Destination_Memory_Start_Address(Des_Addr);
	BTE_Destination_Image_Width(Des_W);
	BTE_Destination_Window_Start_XY(XDes,YDes);	

	BTE_ROP_Code(ROP_Code);	
	BTE_Operation_Code(0x02); //BTE Operation: Memory copy (move) with ROP.
	BTE_Window_Size(X_W,Y_H); 
	BTE_Enable();
	Check_BTE_Busy();
}


void LT768_Nand_8bpp_font
(
 unsigned char SCS         // 选择外挂的SPI   : SCS：0       SCS：1
,unsigned char Clk         // SPI时钟分频参数 : SPI Clock = System Clock /{(Clk+1)*2}
,unsigned short X1          // 传输到内存X1的位置
,unsigned short Y1          // 传输到内存Y1的位置
,unsigned short X_W        // DMA传输数据的宽度
,unsigned short Y_H        // DMA传输数据的高度
,unsigned long Addr        // Flash的地址
,unsigned long lay1        // 缓冲层1的地址
,unsigned long lay2        // 缓冲层2的地址
)
{
	unsigned short i,page,endpage,remainder,x1,y1;
	remainder = Addr%2048;//图片在一页中的起始位置
	page = (Addr/2048);//图片起始地址所在的页编号（页地址）
	endpage = ((Addr+X_W * Y_H)/2048);//图片终止地址所在的页编号（页地址）
	LT768_SPI_Init(1,1);
	W25N01GV_ContinuousRead_Mode();
	W25N01GV_Write_Page(page);
	LT768_DMA_24bit_Linear(1,1,0,lay1,((endpage-page+1)*2048));//传入图片所在的页的所有数据
	x1 = (remainder) % LCD_XSIZE_TFT;
	y1 = (remainder) / LCD_XSIZE_TFT;
	//重组
	for(i = 0 ; i < Y_H ; i++)
	{
		LT768_BTE_Memory_Copy_8(lay1,LCD_XSIZE_TFT,x1,y1,
								lay1,LCD_XSIZE_TFT,x1,y1,
								lay2,LCD_XSIZE_TFT,X1,i,
								0x0c,X_W,1);
		x1=x1+X_W;
		if(x1>LCD_XSIZE_TFT)
		{
			x1 = x1-LCD_XSIZE_TFT;
			y1++;
		}
	}
}

void LT768_Print_Outside_Font_GB2312_48_72_Nand
(
 unsigned char SCS           		    // 选择外挂的SPI   : SCS：0       SCS：1
,unsigned char Clk           		    // SPI时钟分频参数 : SPI Clock = System Clock /{(Clk+1)*2}
,unsigned long FlashAddr     		    // 字库源地址(Flash)
,unsigned long MemoryAddr1    		    // 缓冲层1的地址(SDRAM)
,unsigned long MemoryAddr2    		    // 缓冲层1的地址(SDRAM)
,unsigned long ShowAddr             // 显示层的地址
,unsigned short width               // 显示层的宽度
,unsigned char Size          		    // 设置字体大小  48：48*48     72:72*72
,unsigned char ChromaKey     		    // 0：字体背景色透明    1：可以设置字体的背景色
,unsigned short x                   // 字体开始显示的x位置
,unsigned short y                   // 字体开始显示的y位置
,unsigned long FontColor            // 字体的颜色
,unsigned long BackGroundColor      // 字体的背景色（注意：当字体背景初始化成透明时，设置该值无效）
,unsigned short w				            // 字体粗细：0：不加粗  1：加粗1级  2：加粗2级
,unsigned short s                   // 行距
,unsigned char *c                   // 数据缓冲的首地址
)
{
	unsigned short temp_H = 0;
	unsigned short temp_L = 0;
	unsigned short temp = 0;
	unsigned long i = 0;
	unsigned short j = 0;
	unsigned short k = 0;
	unsigned short h = 0;
	unsigned short n = 0;
	unsigned short m = 0;
	unsigned short g = 0;
	unsigned short f = 0;
	
	h = x; k = y;
	Memory_8bpp_Mode();		                       //使用8位色深来存储图片
 	Canvas_Image_Start_address(MemoryAddr1);
 	Canvas_image_width(width);
	while((c[i] != '\0')&&(c[i]>=0xa0))  // 遇见不是汉字编码的自动退出
	{
		temp_H = (c[i] - 0xa1) * 94;
		temp_L = c[i+1] - 0xa1;
		temp = temp_H + temp_L;
		LT768_Nand_8bpp_font(1,0,Size*j/8,0,Size/8,Size,FlashAddr+temp*((Size*Size)/8),MemoryAddr1,MemoryAddr2);				//2019
		i+=2;
		j++;
	}
	
	Memory_16bpp_Mode();   
	Canvas_Image_Start_address(ShowAddr);
	Canvas_image_width(width);
	j = 0; i = 0;
	
	if(w>2)	w = 2;
	for(g=0;g<w+1;g++)
	{
		while((c[i] != '\0')&&(c[i]>=0xa0))   // 遇见不是汉字编码的自动退出
		{
			if((f == m)&&((x+Size*j+Size)>(width*(n+1)))) {m++;n++;y=y+Size-1+s;x = x+((width*n)-(x+Size*j))+g;f=n;}
				
			if(ChromaKey==0)
			{
				LT768_BTE_Memory_Copy_ColorExpansion_8(MemoryAddr2,Size*j/8,
										                           ShowAddr,width,x+Size*j,y,
										                           Size,Size,FontColor,BackGroundColor
										                          );
			}
			if(ChromaKey==1)
			{
				  LT768_BTE_Memory_Copy_ColorExpansion_Chroma_key_8(MemoryAddr2,Size*j/8,
																	ShowAddr,width,x+Size*j,y,
																	Size,Size,FontColor);
			}
			i+=2;
			j++;
		}
		i=0;j=0;m=0;n=0;f=0;x=h+g+1;y=k+g+1;
	}
}

void LT768_Print_Outside_Font_GBK_48_72_Nand
(
 unsigned char SCS           		    // 选择外挂的SPI   : SCS：0       SCS：1
,unsigned char Clk           		    // SPI时钟分频参数 : SPI Clock = System Clock /{(Clk+1)*2}
,unsigned long FlashAddr     		    // 字库源地址(Flash)
,unsigned long MemoryAddr1    		    // 缓冲层1的地址(SDRAM)
,unsigned long MemoryAddr2    		    // 缓冲层1的地址(SDRAM)
,unsigned long ShowAddr             // 显示层的地址
,unsigned short width               // 显示层的宽度
,unsigned char Size          		    // 设置字体大小  48：48*48     72:72*72
,unsigned char ChromaKey     		    // 0：字体背景色透明    1：可以设置字体的背景色
,unsigned short x                   // 字体开始显示的x位置
,unsigned short y                   // 字体开始显示的y位置
,unsigned long FontColor            // 字体的颜色
,unsigned long BackGroundColor      // 字体的背景色（注意：当字体背景初始化成透明时，设置该值无效）
,unsigned short w				            // 字体粗细：0：不加粗  1：加粗1级  2：加粗2级
,unsigned short s                   // 行距
,unsigned char *c                   // 数据缓冲的首地址
)
{
	unsigned short temp_H = 0;
	unsigned short temp_L = 0;
	unsigned short temp = 0;
	unsigned long i = 0;
	unsigned short j = 0;
	unsigned short k = 0;
	unsigned short h = 0;
	unsigned short n = 0;
	unsigned short m = 0;
	unsigned short g = 0;
	unsigned short f = 0;
	
	h = x; k = y;
	Memory_8bpp_Mode();		                       //使用8位色深来存储图片
 	Canvas_Image_Start_address(MemoryAddr1);
 	Canvas_image_width(width);
	while((c[i] != '\0')&&(c[i]>=0xa0))  // 遇见不是汉字编码的自动退出
	{
		temp_H = (c[i] - 0x81) * 191;
		temp_L = c[i+1] - 0x40;
		temp = temp_H + temp_L;
		LT768_Nand_8bpp_font(1,0,Size*j/8,0,Size/8,Size,FlashAddr+temp*((Size*Size)/8),MemoryAddr1,MemoryAddr2);				//2019
		i+=2;
		j++;
	}
	
	Memory_16bpp_Mode();   
	Canvas_Image_Start_address(ShowAddr);
	Canvas_image_width(width);
	j = 0; i = 0;
	
	if(w>2)	w = 2;
	for(g=0;g<w+1;g++)
	{
		while((c[i] != '\0')&&(c[i]>=0xa0))   // 遇见不是汉字编码的自动退出
		{
			if((f == m)&&((x+Size*j+Size)>(width*(n+1)))) {m++;n++;y=y+Size-1+s;x = x+((width*n)-(x+Size*j))+g;f=n;}
				
			if(ChromaKey==0)
			{
				LT768_BTE_Memory_Copy_ColorExpansion_8(MemoryAddr2,Size*j/8,
										                           ShowAddr,width,x+Size*j,y,
										                           Size,Size,FontColor,BackGroundColor
										                          );
			}
			if(ChromaKey==1)
			{
				  LT768_BTE_Memory_Copy_ColorExpansion_Chroma_key_8(MemoryAddr2,Size*j/8,
																	ShowAddr,width,x+Size*j,y,
																	Size,Size,FontColor);
			}
			i+=2;
			j++;
		}
		i=0;j=0;m=0;n=0;f=0;x=h+g+1;y=k+g+1;
	}
}

void LT768_Print_Outside_UserDefineFont_GB2312_Nand
(
 unsigned char SCS           		// 选择外挂的SPI   : SCS：0       SCS：1
,unsigned char Clk           		// SPI时钟分频参数 : SPI Clock = System Clock /{(Clk+1)*2}
,unsigned long FlashAddr     		// 字库源地址(Flash)
,unsigned long MemoryAddr1    		    // 缓冲层1的地址(SDRAM)
,unsigned long MemoryAddr2    		    // 缓冲层1的地址(SDRAM)
,unsigned long ShowAddr             // 显示层的地址
,unsigned short width               // 显示层的宽度
,unsigned char Size          		// 设置字体大小  48：48*48     72:72*72
,unsigned char ChromaKey     		// 0：字体背景色透明    1：可以设置字体的背景色
,unsigned short x                   // 字体开始显示的x位置
,unsigned short y                   // 字体开始显示的y位置
,unsigned long FontColor            // 字体的颜色
,unsigned long BackGroundColor      // 字体的背景色（注意：当字体背景初始化成透明时，设置该值无效）
,unsigned short w				    // 字体粗细：0：不加粗  1：加粗1级  2：加粗2级
,unsigned short s                   // 行距
,unsigned char *c                   // 数据缓冲的首地址
)
{
    short temp = 0;
	unsigned long i = 0;
	unsigned short j = 0;
	unsigned short k = 0;
	unsigned short h = 0;
	unsigned short n = 0;
	unsigned short m = 0;
	unsigned short g = 0;
	unsigned short f = 0;
	unsigned char temp1[2] = {0};
	unsigned int fontNum = 0;
	
	clear_Font_1();
	LT768_SPI_Init(SCS,1);
	LT_W25N01GV_Read(temp1,FlashAddr+8,2);
	fontNum = (temp1[0]<<8)+temp1[1];
	LT_W25N01GV_Read(Font_1,FlashAddr + 0xc,fontNum * 2);
	h = x; k = y;
	Memory_8bpp_Mode();//使用8位色深来存储图片
 	Canvas_Image_Start_address(MemoryAddr1);
 	Canvas_image_width(width);
	
	while(c[i + 1] != '\0')
	{
		temp = Get_User_Font_P(c[i],c[i+1]);
		if(temp != -1)
		{
			LT768_Nand_8bpp_font(SCS,Clk,Size*j/8,0,Size/8,Size,FlashAddr+temp*((Size*Size)/8)+0x0c+fontNum*2,MemoryAddr1,MemoryAddr2);				//2019
		}
		else
		{
			break;
		}
		i+=2;
		j++;
	}
	
	Memory_16bpp_Mode();
	Canvas_Image_Start_address(ShowAddr);
	Canvas_image_width(width);
	j = 0; i = 0;
	
	if(w>2)	w = 2;
	for(g=0;g<w+1;g++)
	{
		while(c[i + 1] != '\0')
		{
			temp = Get_User_Font_P(c[i],c[i+1]);
			if(temp == -1)				break;
			
			if((f == m)&&((x+Size*j+Size)>(width*(n+1)))) {m++;n++;y=y+Size-1+s;x = x+((width*n)-(x+Size*j))+g;f=n;}
			if(ChromaKey==1)
			{
				LT768_BTE_Memory_Copy_ColorExpansion_8(MemoryAddr2,Size*j/8,
										   ShowAddr,width,x+Size*j,y,
										   Size,Size,FontColor,BackGroundColor
										   );
			}
			if(ChromaKey==0)
			{
				LT768_BTE_Memory_Copy_ColorExpansion_Chroma_key_8(MemoryAddr2,Size*j/8,
												  ShowAddr,width,x+Size*j,y,
													Size,Size,FontColor
													);
			}
			i+=2;
			j++;
		}
		ChromaKey=0;i=0;j=0;m=0;n=0;f=0;x=h+g+1;y=k+g+1;
	}
}

//-------------------------------------------------------------------------------------------------------------------------------





