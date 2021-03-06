/*
 * init.c
 *
 *  Created on: Mar 7, 2016
 *      Author: zhongwei
 */

#include "includes.h"

void initALL(void)
{
	disableWatchdog();
	initModesAndClock();
	enableIrq();
	initEMIOS_0MotorAndSteer();
	initEMIOS_0ModulusCounter();
	initEMIOS_0Image();
	init_serial_port_0();//wifi
	//	init_supersonic();
	initAD();                     //ADC
	//	initKeys_Switchs_Infrared();

	initTestIO();
	OLED_Init();
	
	g_device_NO=1;
	g_device_NO_Hex=0x01;
	SET_steer(RIGHT);
	delay_ms(500);
	SET_steer(LEFT);
	delay_ms(500);
	SET_steer(CENTER);
	
	g_f_enable_speed_control = 1;
//	LCD_P8x16Str(0, 4, (BYTE*)"S.T=0");
//	set_speed_target(0);
	delay_ms(1000);
	//SD_Init();
	//SD_Test();
}

/*********************************************************************************************/
/*********************************  关闭看门狗 **********************************************/
/*********************************************************************************************/
void disableWatchdog(void)
{
	SWT.SR.R = 0x0000c520; /* Write keys to clear soft lock bit */
	SWT.SR.R = 0x0000d928;
	SWT.CR.R = 0x8000010A; /* Clear watchdog enable (WEN) */
}

//*****************************************************************************************************************
//*			 *************************时钟初始化 *******************************************************    	  *
//*****************************************************************************************************************
void initModesAndClock(void) 
{
	ME.MER.R = 0x0000001D;          /* Enable DRUN, RUN0, SAFE, RESET modes */
	//Initialize PLL before turning it on                    
	//设置sysclk
	//CGM.FMPLL_CR.R = 0x02400100;    /* 8 MHz xtal: Set PLL0 to 64 MHz */  
	CGM.FMPLL_CR.R = 0x01280000;      /* 8 MHz xtal: Set PLL0 to 80 MHz */
	//CGM.FMPLL_CR.R = 0x013C0000;    /* 8 MHz xtal: Set PLL0 to 120 MHz */   
	ME.RUN[0].R = 0x001F0064;       /* RUN0 cfg: 16MHzIRCON,OSC0ON,PLL0ON,syclk=PLL     sysclk选择锁相环时钟*/
	ME.RUNPC[0].R = 0x00000010;    /* Peri. Cfg. 1 settings: only run in RUN0 mode      选择RUN0模式*/
	/*************************PCTL[?] 选择需要时钟模块****************************/
	ME.PCTL[32].R = 0x00; 			/* MPC56xxB/P/S ADC 0: select ME.RUNPC[0] */
	ME.PCTL[48].R = 0x00;           /* MPC56xxB/P/S LINFlex 0: select ME.RUNPC[0] */
	ME.PCTL[68].R = 0x00;           /* MPC56xxB/S SIUL:  select ME.RUNPC[0] */ 
	ME.PCTL[72].R = 0x00;           /* MPC56xxB/S EMIOS 0:  select ME.RUNPC[0] */ 
	//Mode Transition to enter RUN0 mode      
	ME.MCTL.R = 0x40005AF0;         /* Enter RUN0 Mode & Key */
	ME.MCTL.R = 0x4000A50F;         /* Enter RUN0 Mode & Inverted Key */ 

	while (ME.GS.B.S_MTRANS) {}     // Wait for mode transition to complete 等待模式转换完成    
	/********************************** Note: could wait here using timer and/or I_TC IRQ*/                          
	while(ME.GS.B.S_CURRENTMODE != 4) {} // Verify RUN0 is the current mode 等待选择RUN0模式
	//开peri0、1、2
	CGM.SC_DC[0].R = 0x84;//LIN
	CGM.SC_DC[1].R = 0x80;//FLEXCAN,DSPI
	CGM.SC_DC[2].R = 0x80;//eMIOS,CTU,ADC
}

//*****************************************************************************************************************
//*************************eMIOS初始化:电机正反转E3、E4、E5、E6；舵机输出A4  **********************************************
//*****************************************************************************************************************
/*************************PWM初始化***********************/
void initEMIOS_0MotorAndSteer(void)
{
	//eMIOS0初始化80MHz分为1MHz
	EMIOS_0.MCR.B.GPRE= 31;   //GPRE+1=分频系数；/* Divide 80 MHz sysclk by 31+1 = 32 for 2.5MHz(0.4us) eMIOS clk*/
	EMIOS_0.MCR.B.GPREN = 1;	/* Enable eMIOS clock */
	EMIOS_0.MCR.B.GTBE = 1;   /* Enable global time base */
	EMIOS_0.MCR.B.FRZ = 1;    /* Enable stopping channels when in debug mode */	
	/**********电机PWM 5kHZ E3、E4、E5、E6*********************************************************************************/ 
	//eMIOS0D通道16设置/* EMIOS 0 CH 16: Modulus Up Counter */
	EMIOS_0.CH[16].CCR.B.UCPRE=0;	    /* Set channel prescaler to divide by 1 */
	EMIOS_0.CH[16].CCR.B.UCPEN = 1;   /* Enable prescaler; uses default divide by 1 */
	EMIOS_0.CH[16].CCR.B.FREN = 1; 	/* Freeze channel counting when in debug mode */
	EMIOS_0.CH[16].CADR.R = 500;	/********设置周期200us  5kHZ********/
	EMIOS_0.CH[16].CCR.B.MODE = 0x50; /* Modulus Counter Buffered (MCB) */
	EMIOS_0.CH[16].CCR.B.BSL = 0x3;	/* Use internal counter */
	/*E3(EMIOS 0 CH 19)左前进输出: Output Pulse Width Modulation */
	EMIOS_0.CH[19].CCR.B.BSL = 0x1;//use counter bus D
	EMIOS_0.CH[19].CCR.B.MODE = 0x60;//Mode is OPWM Buffered
	EMIOS_0.CH[19].CCR.B.EDPOL = 1;//Polarity-leading edge sets output/trailing clears
	EMIOS_0.CH[19].CADR.R = 0;//Leading edge when channel counter bus=250
	EMIOS_0.CH[19].CBDR.R = 0;//Trailing edge when channel counter bus=500
	SIU.PCR[67].R = 0x0600;//Assign EMIOS_0 CH19 to Pad
	/*E4(EMIOS 0 CH 20)左后退输出: Output Pulse Width Modulation */
	EMIOS_0.CH[20].CCR.B.BSL = 0x1;//use counter bus D
	EMIOS_0.CH[20].CCR.B.MODE = 0x60;//Mode is OPWM Buffered
	EMIOS_0.CH[20].CCR.B.EDPOL = 1;//Polarity-leading edge sets output/trailing clears
	EMIOS_0.CH[20].CADR.R = 0;//Leading edge when channel counter bus=250
	EMIOS_0.CH[20].CBDR.R = 0;//Trailing edge when channel counter bus=500
	SIU.PCR[68].R = 0x0600;//Assign EMIOS_0 CH20 to Pad
	/* E5(EMIOS 0 CH 21)右前进输出: Output Pulse Width Modulation */
	EMIOS_0.CH[21].CCR.B.BSL = 0x1;	/* Use counter bus D (default) */
	EMIOS_0.CH[21].CCR.B.MODE = 0x60; /* Mode is OPWM Buffered */
	EMIOS_0.CH[21].CCR.B.EDPOL = 1;	/* Polarity-leading edge sets output/trailing clears*/
	EMIOS_0.CH[21].CADR.R = 0;     /* Leading edge when channel counter bus=250*/
	EMIOS_0.CH[21].CBDR.R = 0;      /* Trailing edge when channel counter bus=500*/
	SIU.PCR[69].R = 0x0600;    //[11:10]选择AFx 此处AF1 /* MPC56xxS: Assign EMIOS_0 ch 21 to pad */
	/* E6（EMIOS 0 CH 22）右后退输出: Output Pulse Width Modulation */
	EMIOS_0.CH[22].CCR.B.BSL = 0x1;	/* Use counter bus D (default) */
	EMIOS_0.CH[22].CCR.B.MODE = 0x60; /* Mode is OPWM Buffered */
	EMIOS_0.CH[22].CCR.B.EDPOL = 1;	/* Polarity-leading edge sets output/trailing clears*/
	EMIOS_0.CH[22].CADR.R = 0;     /* Leading edge when channel counter bus=250*/
	EMIOS_0.CH[22].CBDR.R = 0;     /* Trailing edge when channel counter bus=500*/
	SIU.PCR[70].R = 0x0600;   //[11:10]选择AFx 此处AF1 /* MPC56xxS: Assign EMIOS_0 ch 21 to pad */

	/**********舵机PWM 50HZ A4口输出50000*7.5%=3750中位**********/
	//eMIOS0 A通道23设置/* EMIOS 0 CH 0: Modulus Counter */
	EMIOS_0.CH[0].CCR.B.UCPRE=0;	    /* Set channel prescaler to divide by 1 */
	EMIOS_0.CH[0].CCR.B.UCPEN = 1;   /* Enable prescaler; uses default divide by 1 */
	EMIOS_0.CH[0].CCR.B.FREN = 1; 	/* Freeze channel counting when in debug mode */
	EMIOS_0.CH[0].CADR.R = 50000;/********设置周期20ms  50HZ*******/
	EMIOS_0.CH[0].CCR.B.MODE = 0x50; /* Modulus Counter Buffered (MCB) */
	EMIOS_0.CH[0].CCR.B.BSL = 0x3;	/* Use internal counter */

	/* EMIOS 0 CH 4: Output Pulse Width Modulation */
	EMIOS_0.CH[4].CCR.B.BSL = 1;	/* Use counter bus A (default) */
	EMIOS_0.CH[4].CCR.B.MODE = 0x60; /* Mode is OPWM Buffered */  
	EMIOS_0.CH[4].CCR.B.EDPOL = 1;	/* Polarity-leading edge sets output/trailing clears*/
	EMIOS_0.CH[4].CADR.R = 0;//半占空比/* Leading edge when channel counter bus=250*/
	EMIOS_0.CH[4].CBDR.R = CENTER;            /* Trailing edge when channel counter bus=500*/
	SIU.PCR[4].R = 0x0600;    //[11:10]选择AFx 此处AF1   A4口舵机输出
}
/*************************光编初始化***********************/
void initEMIOS_0ModulusCounter(void) //A8,D12模数计数器入口，上升沿，C15,C9光编正反转
{
	//D12
	//EMIOS_0.CH[24].CCR.B.MODE = 0x51; // Mode is MCB, 
	EMIOS_0.CH[24].CCR.B.MODE = 0x13; // Mode is MCB, 
	EMIOS_0.CH[24].CCR.B.BSL = 0x3; // Use internal counter
	EMIOS_0.CH[24].CCR.B.UCPRE=0; // Set channel prescaler to divide by 1
	EMIOS_0.CH[24].CCR.B.UCPEN = 1; // Enable prescaler; uses default divide by 1 
	//EMIOS_0.CH[24].CCR.B.FREN = 0;// Freeze channel counting when in debug mode 
	EMIOS_0.CH[24].CCR.B.EDPOL=1; //Edge Select rising edge
	EMIOS_0.CH[24].CADR.R=0xffff;
	SIU.PCR[60].R = 0x0102;  // Initialize pad for eMIOS channel Initialize pad for input
	SIU.PCR[41].R = 0x0100;  //C9右轮光编正反转 ，高电平正转，低电平反转

	//A8 
	//	EMIOS_0.CH[8].CCR.B.MODE = 0x51; // Mode is MCB, 
	EMIOS_0.CH[8].CCR.B.MODE = 0x13; // Mode is MCB, 
	EMIOS_0.CH[8].CCR.B.BSL = 0x3; // Use internal counter
	EMIOS_0.CH[8].CCR.B.UCPRE=0; // Set channel prescaler to divide by 1
	EMIOS_0.CH[8].CCR.B.UCPEN = 1; // Enable prescaler; uses default divide by 1 
	//EMIOS_0.CH[8].CCR.B.FREN = 0;// Freeze channel counting when in debug mode 
	EMIOS_0.CH[8].CCR.B.EDPOL=1; //Edge Select rising edge
	EMIOS_0.CH[8].CADR.R=0xffff;

	SIU.PCR[8].R = 0x0102;  // Initialize pad for eMIOS channel Initialize pad for input
	SIU.PCR[47].R=0x0100;  //C15左轮光编正反转，高电平反转，低电平正转
}


void initEMIOS_0Image(void) 
{
	//eMIOS0 B通道0设置/* EMIOS 0 CH 0: Modulus Counter */
	EMIOS_0.CH[0].CCR.B.UCPRE=0;	    /* Set channel prescaler to divide by 1 */
	EMIOS_0.CH[0].CCR.B.UCPEN = 1;   /* Enable prescaler; uses default divide by 1 */
	//EMIOS_0.CH[0].CCR.B.FREN = 1; 	/* Freeze channel counting when in debug mode */
	EMIOS_0.CH[0].CADR.R = 50000;/********设置周期20ms  50HZ*******/
	EMIOS_0.CH[0].CCR.B.MODE = 0x50; /* Modulus Counter Buffered (MCB) */
	EMIOS_0.CH[0].CCR.B.BSL = 0x3;	/* Use internal counter */

	//B14场中断捕捉下降沿
	EMIOS_0.CH[6].CCR.B.MODE = 0x02; // Mode is SAIC, continuous 
	EMIOS_0.CH[6].CCR.B.BSL = 0x01; /* Use counter bus B (default) */
	EMIOS_0.CH[6].CCR.B.EDSEL = 1;  //Both edges
	//	EMIOS_0.CH[5].CCR.B.EDPOL=0; //Edge Select falling edge
	//	EMIOS_0.CH[5].CCR.B.FEN=1;  //interupt enbale
	SIU.PCR[30].R = 0x0102;  // Initialize pad for eMIOS channel Initialize pad for input
	SIU.PSMI[16].R=1;//E0UC[6]选择B14
	INTC_InstallINTCInterruptHandler(FieldInputCapture,144,13);

	//B13行中断捕捉上升沿
	EMIOS_0.CH[5].CCR.B.MODE = 0x02; // Mode is SAIC, continuous 
	EMIOS_0.CH[5].CCR.B.BSL = 0x01; /* Use counter bus B (default) */
	EMIOS_0.CH[5].CCR.B.EDPOL=1; //Edge Select rising edge
	//EMIOS_0.CH[7].CCR.B.FEN=1;  //interupt enbale
	SIU.PCR[29].R = 0x0102;  // Initialize pad for eMIOS channel Initialize pad for input 
	SIU.PSMI[15].R=1;//E0UC[5]选择B13
	INTC_InstallINTCInterruptHandler(RowInputCapture,143,12); 
}

//*****************************************************************************************************************
//****************************************中断初始化******************************************************    	  *
//*****************************************************************************************************************
void enableIrq(void) 
{
	INTC.CPR.B.PRI = 0;          /* Single Core: Lower INTC's current priority */
	asm(" wrteei 1");	    	   /* Enable external interrupts */
}

//*****************************************************************************************************************
//************************************************调试模块************************************************    	  *
//*****************************************************************************************************************
/*************************蓝牙初始化***********************/
void init_serial_port_0(void)
{
	LINFLEX_0.LINCR1.B.INIT=1;  //进入初始化模式
	LINFLEX_0.LINCR1.R=0x00000015; 
	LINFLEX_0.LINIER.B.DRIE=1; //允许接收中断
	/*波特率算法baud=Fperiph_clk/(16*LFDIV)
	DIV_M=LFDIV整数部分
	DIV_F=LFDIV小数部分*16  */ 	
//#ifndef PERIPH_SET_1_CLK_16M	//80M
	
	LINFLEX_0.LINFBRR.B.DIV_F = 6;		//38400:64M-104+3
	LINFLEX_0.LINIBRR.B.DIV_M= 17;  //115200	//波特率设置38400:80M-130+3 57600:80M-86+13 115200:80M-43+6  9600:80M-520+13
   
//#else
//	LINFLEX_0.LINIBRR.B.DIV_M= 17;	//波特率设置 2400:416+11, 9600:104+3, 10417:96+0, 19200:52+1, 57600:17+6
//    LINFLEX_0.LINFBRR.B.DIV_F = 6;	//波特率设置 115200:8+11, 230400:4+5, 460800:2+3, 921600:1+1
//#endif
    LINFLEX_0.UARTCR.B.UART=1;
	LINFLEX_0.UARTCR.R=0x00000033;//8-bit data、UART mode
	LINFLEX_0.LINCR1.B.INIT=0; //退出初始化模式
	
	SIU.PCR[18].R = 0x0400;    /* MPC56xxB: Configure port B2 as LIN0TX */
    SIU.PCR[19].R = 0x0103;    /* MPC56xxB: Configure port B3 as LIN0RX */
  	INTC_InstallINTCInterruptHandler(intc_serial_port_0_RX,79,5); 
}


void serial_port_0_TX(BYTE data)
{
	int i = 0;
	
	LINFLEX_0.BDRL.B.DATA0 = data;	//发送语句
	while(!LINFLEX_0.UARTSR.B.DTF)
	{
		if (i++ >= 10000)
		{
			break;	/* 防止DTF置位失败 */
		}
	}
	LINFLEX_0.UARTSR.B.DTF=1;
}


void serial_port_0_TX_array(const BYTE data[], WORD n)
{
	int i = 0;
	
	for (i=0; i<n; i++)
	{
		serial_port_0_TX(data[i]);
	}
}


void intc_serial_port_0_RX(void)
{
	BYTE rev_ch;
//	D3=~D3;
	while(!LINFLEX_0.UARTSR.B.DRF){}
	rev_ch = (BYTE)LINFLEX_0.BDRM.B.DATA4;
//	g_serial_port_0_f = 1;
//	g_serial_port_0_data = rev_ch;
	LINFLEX_0.UARTSR.B.DRF=1;      //清空标志位
	rev_remote_frame_2(rev_ch);
}
/*************************超声波初始化***********************/
void init_supersonic(void)
{	
	EMIOS_0.CH[7].CCR.B.MODE = 0x04; // Mode is Input Pulse Width Measurement 
	EMIOS_0.CH[7].CCR.B.BSL = 0x3;   // Use internal counter
	EMIOS_0.CH[7].CCR.B.UCPRE=3; //Set channel prescaler to divide by 1
	EMIOS_0.CH[7].CCR.B.UCPEN = 1;	//Enable prescaler; uses default divide by 1
	EMIOS_0.CH[7].CCR.B.FREN = 0;	//Freeze channel counting when in debug mode
	EMIOS_0.CH[7].CCR.B.EDSEL=0; //Edge Select rising edge
	EMIOS_0.CH[7].CCR.B.EDPOL=1;
	EMIOS_0.CH[7].CCR.B.FEN=1;  //interupt enbale

	SIU.PCR[7].R = 0x0100;  //
	SIU.PSMI[17].R=1;
//	INTC_InstallINTCInterruptHandler(intc_get_supersonic_time_2, 144, 9);

}

void initAD(void)
{
	ADC.MCR.R = 0x20000000;       /*未读转换数据不能被覆盖；右对齐；连续转换序列模式；引起当前链转换结束并终止操作；对通道注入的外触发器使不能；模拟时钟频率为40MHz；CTU触发转换使不能；自动时钟关闭使不能；正常模式*/
	ADC.NCMR[1].R = 0x00000004;   /*使能CH34、33通道（标准通道）的位正常采样*/
	ADC.CTR[1].R = 0x00008606;    /*转换时间寄存器 与标准通道相关联*/
	ADC.MCR.B.NSTART=1;         /* Trigger normal conversions for ADC0 */

	SIU.PCR[26].R = 0x2100;//CCD AO B10
	SIU.PCR[36].R = 0x0200;//CCD CLK B10
	SIU.PCR[34].R = 0x0200;//CCD SI B10
	//	SIU.PCR[25].R = 0x2100;//SMOK5 B9
//	SIU.PCR[26].R = 0x2100;//SMOK6 B10
}



/*********************测试IO初始化***********************/
void initTestIO(void)
{
	//调试模块（OLED、蜂鸣器、超声、拨码开关、按键、LED）

//	//人体检测模块
//	SIU.PCR[34].R = 0x0102;//C2
//
//	//烟雾检测模块
//	SIU.PCR[5].R = 0x0102;//SMOK1 A5
//	SIU.PCR[76].R = 0x0102;//SMOK2 E12

	//OLED
	SIU.PCR[72].R = 0x0200;//OLED     E8
	SIU.PCR[74].R = 0x0200;//OLED     E10
	SIU.PCR[75].R = 0x0200;//OLED     E11
	SIU.PCR[42].R = 0x0200;//OLED     C10
	SIU.PCR[17].R = 0x0200;//OLED     B1

//	//蜂鸣器
	SIU.PCR[73].R = 0x0100; //碰撞开关     E9
//	BEE = 0;

	//按键
	SIU.PCR[15].R = 0x0103;//KEY S6   A15
	SIU.PCR[39].R = 0x0103;//KEY S5   C7
	SIU.PCR[14].R = 0x0103;//KEY S4   A14
	SIU.PCR[13].R = 0x0103;//KEY S3   A13



	SIU.PCR[31].R = 0x0200;//     B15  超声1 触发
	SIU.GPDO[31].R = 0;
	//SWITCH
	SIU.PCR[0].R = 0x0100;//SWITCH   A0
	SIU.PCR[43].R = 0x0100;//SWITCH   C11
	SIU.PCR[16].R = 0x0100;//SWITCH   B0
	SIU.PCR[38].R = 0x0100;//SWITCH   C6

	//输入模块（摄像头、光编）

	//摄像头1
	SIU.PCR[49].R = 0x0102;//   D1_Y3
	SIU.PCR[50].R = 0x0102;//   D2_Y2
	SIU.PCR[51].R = 0x0102;//   D3_Y5
	SIU.PCR[52].R = 0x0102;//   D4_Y4
	SIU.PCR[53].R = 0x0102;//   D5_Y7
	SIU.PCR[54].R = 0x0102;//   D6_Y6
	SIU.PCR[55].R = 0x0102;//   D7_Y1
	SIU.PCR[56].R = 0x0102;//   D8_Y0

}


