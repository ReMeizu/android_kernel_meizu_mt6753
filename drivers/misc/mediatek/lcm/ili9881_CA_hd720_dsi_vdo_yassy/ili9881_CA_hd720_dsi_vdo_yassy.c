//modify@zte.com.cn 20140813 end
/* BEGIN PN: , Added by guohongjin, 2014.08.13*/
#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK

#include <platform/gpio_const.h>
#include <platform/mt_gpio.h>
#include <platform/upmu_common.h>
#include <cust_gpio_usage.h>
#else
#include <mach/gpio_const.h>
#include <mt-plat/mt_gpio.h>
//#include <cust_gpio_usage.h>
#include <linux/string.h>
#endif


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define LCM_DSI_CMD_MODE									0
#define FRAME_WIDTH  										(720)
#define FRAME_HEIGHT 										(1280)
#define LCM_ID_ILI9881                                                              (0x0)

#define REGFLAG_DELAY             								0xFC
#define REGFLAG_END_OF_TABLE      							0xFD   // END OF REGISTERS MARKER

#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

//static unsigned int lcm_esd_test = FALSE;      ///only for ESD test
// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------
#define GPIO_LCM_PWR_EN   (119 | 0x80000000) /* M5s M1612: bias enable = GPIO119. FACT: stock vmlinux drives it via pinctrl states "gpio119_enable_output1/0" (disp_dts_gpio_select_state 6/5) in init/suspend; stock lcm init/suspend/resume/compare_id contain NO i2c bias-chip writes. Reset via lcm_util=DCT GPIO_LCM_RST(146). */
static LCM_UTIL_FUNCS lcm_util;

#define __SAME_IC_COMPATIBLE__

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))
#define MDELAY(n) 											(lcm_util.mdelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

 struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};

static struct LCM_setting_table lcm_initialization_setting1[] = {
	/* Init table extracted verbatim from STOCK Meizu M5s kernel
	 * (boot.img @ Flyme 6.3.1.0G), LCM_DRIVER "ili9881_CA_hd720_dsi_vdo_yassy"
	 * init table @file 0xd78f88 (185 register writes). Replaces the ZTE
	 * Blade A610 _E169F values which differed at 109/185 writes (wrong panel
	 * module tuning). FACT: verified against stock vmlinux 2026-08-19. */
	{0xFF,3,{0x98,0x81,0x03}},
	{0x01,1,{0x00}},
	{0x02,1,{0x00}},
	{0x03,1,{0x53}},
	{0x04,1,{0x53}},
	{0x05,1,{0x13}},
	{0x06,1,{0x04}},
	{0x07,1,{0x02}},
	{0x08,1,{0x02}},
	{0x09,1,{0x00}},
	{0x0A,1,{0x00}},
	{0x0B,1,{0x00}},
	{0x0C,1,{0x00}},
	{0x0D,1,{0x00}},
	{0x0E,1,{0x00}},
	{0x0F,1,{0x00}},
	{0x10,1,{0x00}},
	{0x11,1,{0x00}},
	{0x12,1,{0x00}},
	{0x13,1,{0x00}},
	{0x14,1,{0x00}},
	{0x15,1,{0x00}},
	{0x16,1,{0x00}},
	{0x17,1,{0x00}},
	{0x18,1,{0x00}},
	{0x19,1,{0x00}},
	{0x1A,1,{0x00}},
	{0x1B,1,{0x00}},
	{0x1C,1,{0x00}},
	{0x1D,1,{0x00}},
	{0x1E,1,{0xC0}},
	{0x1F,1,{0x80}},
	{0x20,1,{0x02}},
	{0x21,1,{0x09}},
	{0x22,1,{0x00}},
	{0x23,1,{0x00}},
	{0x24,1,{0x00}},
	{0x25,1,{0x00}},
	{0x26,1,{0x00}},
	{0x27,1,{0x00}},
	{0x28,1,{0x55}},
	{0x29,1,{0x03}},
	{0x2A,1,{0x00}},
	{0x2B,1,{0x00}},
	{0x2C,1,{0x00}},
	{0x2D,1,{0x00}},
	{0x2E,1,{0x00}},
	{0x2F,1,{0x00}},
	{0x30,1,{0x00}},
	{0x31,1,{0x00}},
	{0x32,1,{0x00}},
	{0x33,1,{0x00}},
	{0x34,1,{0x03}},
	{0x35,1,{0x00}},
	{0x36,1,{0x05}},
	{0x37,1,{0x00}},
	{0x38,1,{0x3C}},
	{0x39,1,{0x00}},
	{0x3A,1,{0x00}},
	{0x3B,1,{0x00}},
	{0x3C,1,{0x00}},
	{0x3D,1,{0x00}},
	{0x3E,1,{0x00}},
	{0x3F,1,{0x00}},
	{0x40,1,{0x00}},
	{0x41,1,{0x00}},
	{0x42,1,{0x00}},
	{0x43,1,{0x00}},
	{0x44,1,{0x00}},
	{0x50,1,{0x01}},
	{0x51,1,{0x23}},
	{0x52,1,{0x45}},
	{0x53,1,{0x67}},
	{0x54,1,{0x89}},
	{0x55,1,{0xAB}},
	{0x56,1,{0x01}},
	{0x57,1,{0x23}},
	{0x58,1,{0x45}},
	{0x59,1,{0x67}},
	{0x5A,1,{0x89}},
	{0x5B,1,{0xAB}},
	{0x5C,1,{0xCD}},
	{0x5D,1,{0xEF}},
	{0x5E,1,{0x01}},
	{0x5F,1,{0x14}},
	{0x60,1,{0x15}},
	{0x61,1,{0x0C}},
	{0x62,1,{0x0D}},
	{0x63,1,{0x0E}},
	{0x64,1,{0x0F}},
	{0x65,1,{0x10}},
	{0x66,1,{0x11}},
	{0x67,1,{0x08}},
	{0x68,1,{0x02}},
	{0x69,1,{0x0A}},
	{0x6A,1,{0x02}},
	{0x6B,1,{0x02}},
	{0x6C,1,{0x02}},
	{0x6D,1,{0x02}},
	{0x6E,1,{0x02}},
	{0x6F,1,{0x02}},
	{0x70,1,{0x02}},
	{0x71,1,{0x02}},
	{0x72,1,{0x06}},
	{0x73,1,{0x02}},
	{0x74,1,{0x02}},
	{0x75,1,{0x14}},
	{0x76,1,{0x15}},
	{0x77,1,{0x0F}},
	{0x78,1,{0x0E}},
	{0x79,1,{0x0D}},
	{0x7A,1,{0x0C}},
	{0x7B,1,{0x11}},
	{0x7C,1,{0x10}},
	{0x7D,1,{0x06}},
	{0x7E,1,{0x02}},
	{0x7F,1,{0x0A}},
	{0x80,1,{0x02}},
	{0x81,1,{0x02}},
	{0x82,1,{0x02}},
	{0x83,1,{0x02}},
	{0x84,1,{0x02}},
	{0x85,1,{0x02}},
	{0x86,1,{0x02}},
	{0x87,1,{0x02}},
	{0x88,1,{0x08}},
	{0x89,1,{0x02}},
	{0x8A,1,{0x02}},
	{0xFF,3,{0x98,0x81,0x04}},
	{0x6C,1,{0x15}},
	{0x6E,1,{0x2F}},
	{0x6F,1,{0x55}},
	{0x3A,1,{0xA4}},
	{0x8D,1,{0x1F}},
	{0x87,1,{0xBA}},
	{0x26,1,{0x76}},
	{0xB2,1,{0xD1}},
	{0x88,1,{0x0B}},
	{0xFF,3,{0x98,0x81,0x01}},
	{0x22,1,{0x09}},
	{0x31,1,{0x00}},
	{0x53,1,{0x77}},
	{0x55,1,{0x77}},
	{0x50,1,{0xA6}},
	{0x51,1,{0xA6}},
	{0x60,1,{0x14}},
	{0xA0,1,{0x00}},
	{0xA1,1,{0x0B}},
	{0xA2,1,{0x2A}},
	{0xA3,1,{0x14}},
	{0xA4,1,{0x17}},
	{0xA5,1,{0x2A}},
	{0xA6,1,{0x1E}},
	{0xA7,1,{0x20}},
	{0xA8,1,{0x8C}},
	{0xA9,1,{0x1C}},
	{0xAA,1,{0x28}},
	{0xAB,1,{0x78}},
	{0xAC,1,{0x1A}},
	{0xAD,1,{0x15}},
	{0xAE,1,{0x4C}},
	{0xAF,1,{0x20}},
	{0xB0,1,{0x27}},
	{0xB1,1,{0x52}},
	{0xB2,1,{0x65}},
	{0xB3,1,{0x3F}},
	{0xC0,1,{0x00}},
	{0xC1,1,{0x1B}},
	{0xC2,1,{0x2A}},
	{0xC3,1,{0x14}},
	{0xC4,1,{0x17}},
	{0xC5,1,{0x2A}},
	{0xC6,1,{0x1E}},
	{0xC7,1,{0x20}},
	{0xC8,1,{0x8D}},
	{0xC9,1,{0x1C}},
	{0xCA,1,{0x28}},
	{0xCB,1,{0x78}},
	{0xCC,1,{0x1B}},
	{0xCD,1,{0x15}},
	{0xCE,1,{0x4C}},
	{0xCF,1,{0x21}},
	{0xD0,1,{0x27}},
	{0xD1,1,{0x52}},
	{0xD2,1,{0x65}},
	{0xD3,1,{0x3F}},
	{0xFF,3,{0x98,0x81,0x00}},
	{0x11,1,{0x00}},
	{REGFLAG_DELAY, 120, {}},
	{0x29,1,{0x00}},
	{REGFLAG_DELAY, 20, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#if 0
static struct LCM_setting_table lcm_sleep_out_setting[] = {
    //Sleep Out
    {0x11, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
    {0x29, 1, {0x00}},
    {REGFLAG_DELAY, 20, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif
static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
    // Display off sequence
    {0x28, 1, {0x00}},
    {REGFLAG_DELAY, 20, {}},	/* stock vmlinux suspend table @0xd78e20: 20ms (was ZTE 40) */

    // Sleep Mode On
    {0x10, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},
	//modify@zte.com.cn begin
   // {0xFF,	3,		{0x98,0x81,0x01}},
   // {0x58, 1, {0x01}},
//{REGFLAG_DELAY, 20, {}},
		//modify@zte.com.cn end
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
    unsigned int i;

    for(i = 0; i < count; i++)
    {
        unsigned cmd;
        cmd = table[i].cmd;

        switch (cmd) {
			
            case REGFLAG_DELAY :
                MDELAY(table[i].count);
                break;
				
            case REGFLAG_END_OF_TABLE :
                break;

            default:
                dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
        }
    }
}

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{

		memset(params, 0, sizeof(LCM_PARAMS));
		params->type   = LCM_TYPE_DSI;

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

		params->dbi.te_mode				= LCM_DBI_TE_MODE_VSYNC_ONLY;
		//LCM_DBI_TE_MODE_DISABLED;
		//LCM_DBI_TE_MODE_VSYNC_ONLY;  
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING; 
		/////////////////////   
		//if(params->dsi.lcm_int_te_monitor)  
		//params->dsi.vertical_frontporch *=2;  
		//params->dsi.lcm_ext_te_monitor= 0;//TRUE; 
	//	params->dsi.noncont_clock= TRUE;//FALSE;   
	//	params->dsi.noncont_clock_period=2;
		params->dsi.cont_clock=1;    //modify  yudengwu
		////////////////////          
		params->dsi.mode   = SYNC_PULSE_VDO_MODE;  
		// DSI    /* Command mode setting */  
		params->dsi.LANE_NUM				= LCM_FOUR_LANE;      
		//The following defined the fomat for data coming from LCD engine.  
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;   
		params->dsi.data_format.trans_seq	= LCM_DSI_TRANS_SEQ_MSB_FIRST; 
		params->dsi.data_format.padding 	= LCM_DSI_PADDING_ON_LSB;    
		params->dsi.data_format.format	  = LCM_DSI_FORMAT_RGB888;       
		// Video mode setting		   
		params->dsi.intermediat_buffer_num = 2;  
		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;  
		params->dsi.packet_size=256;    
		// params->dsi.word_count=480*3;	
		//DSI CMD mode need set these two bellow params, different to 6577   
		// params->dsi.vertical_active_line=800;   
		params->dsi.vertical_sync_active				= 8; //4   
		params->dsi.vertical_backporch				       = 24;  /* stock vmlinux get_params @0x3dcd48: 24 (was ZTE 20) */
		params->dsi.vertical_frontporch				       = 16;  /* stock vmlinux: 16 (was ZTE 10) */
		params->dsi.vertical_active_line				       = FRAME_HEIGHT;
		params->dsi.horizontal_sync_active				= 40;   /* stock vmlinux: 40 (was ZTE 50) */
		params->dsi.horizontal_backporch				= 40;  /* stock vmlinux: 40 (was ZTE 90) */
		params->dsi.horizontal_frontporch				= 60;    /* stock vmlinux: 60 (was ZTE 70) */
		params->dsi.horizontal_blanking_pixel				= 60;   
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;  
		
	//	params->dsi.pll_div1=1;		   
	//	params->dsi.pll_div2=1;		   
	//	params->dsi.fbk_div =28;//28	
		params->dsi.PLL_CLOCK = 207;	/* stock vmlinux get_params: 207 unconditionally (ZTE ifdef 215/212 removed) */

		params->dsi.ssc_disable = 1;	/* stock vmlinux: offset 568 = 1 (SSC off) */

		params->dsi.HS_TRAIL = 10;
			
//modify@zte.com.cn 20140820 end
		params->dsi.esd_check_enable = 1;
                params->dsi.customization_esd_check_enable = 1;
		/* ESD check table from stock vmlinux get_params: read 0x0A (power mode),
		 * expect single byte 0x9C. Replaces ZTE 0x09/3-byte variant. */
		params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
		params->dsi.lcm_esd_check_table[0].count = 1;
		params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
}


static void lcm_init(void)
{

	/* Stock lcm_init @0x3dcf3c drives the SAME pin via pinctrl state
	 * "gpio119_enable_output1" (disp_dts_gpio_select_state(6)) — confirms
	 * GPIO119 as bias enable. Stock then waits 2 x mdelay(10) before reset. */
	mt_set_gpio_mode(GPIO_LCM_PWR_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCM_PWR_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM_PWR_EN, GPIO_OUT_ONE);
	MDELAY(10);	/* stock vmlinux init: mdelay(10) x2 after bias enable */
	MDELAY(10);

		//modify@zte.com.cn begin
	#ifdef BUILD_LK
		  //  ili9881_write_byte(0x03,0x01);
#endif
/*
#ifdef BUILD_LK
	ret=ili9881_write_byte(cmd,data);
    if(ret)    	
    dprintf(0, "[LK]ili9881----tps6132----cmd=%0x--i2c write error----\n",cmd);    	
	else
	dprintf(0, "[LK]ili9881----tps6132----cmd=%0x--i2c write success----\n",cmd);    		
#else
	ret=ili9881_write_bytes(cmd,data);
	if(ret<0)
	printk("[KERNEL]ili9881----tps6132---cmd=%0x-- i2c write error-----\n",cmd);
	else
	printk("[KERNEL]ili9881----tps6132---cmd=%0x-- i2c write success-----\n",cmd);
#endif
	//modify@zte.com.cn end
*/
	SET_RESET_PIN(1);
	MDELAY(1);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(20);	/* stock vmlinux init @0x3dcf3c: 20ms after reset release (was ZTE 120) */

	      push_table(lcm_initialization_setting1, sizeof(lcm_initialization_setting1) / sizeof(struct LCM_setting_table), 1);

}

static void lcm_suspend(void) 
{
     
	// when phone sleep , config output low, disable backlight drv chip  
	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
	
	/* Stock suspend @0x3dced4: push sleep-in table, then pinctrl state
	 * "gpio119_enable_output0" (bias off), then mdelay(10) x2. */
	mt_set_gpio_mode(GPIO_LCM_PWR_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCM_PWR_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM_PWR_EN, GPIO_OUT_ZERO);
	MDELAY(10);	/* stock vmlinux suspend: mdelay(10) x2 after bias off */
	MDELAY(10);

}


static void lcm_resume(void)
{
	lcm_init();
}

static unsigned int lcm_compare_id(void)
{
	unsigned int id=0,id1=0,id2=0;
	unsigned char buffer[3];
	unsigned int array[16];  
	//modify@zte.com.cn modify at 20150921 begin
	mt_set_gpio_mode(GPIO_LCM_PWR_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCM_PWR_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM_PWR_EN, GPIO_OUT_ONE);
	MDELAY(10);
//modify@zte.com.cn modify at 20150921 begin
	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);
	
	SET_RESET_PIN(1);
	MDELAY(120); 

array[0]=0x00043902;
    array[1]=0x078198FF;
   // array[2]=0x00000007;
    dsi_set_cmdq(array, 2, 1);
    MDELAY(10);  
    array[0] = 0x00013700;// read id return two byte,version and id
    dsi_set_cmdq(array, 1, 1);
    MDELAY(10); 
       
    read_reg_v2(0x03, buffer, 1);
    id1 = buffer[0]; 
    
    read_reg_v2(0x04, buffer, 1);
    id2 = buffer[1];  
    
    id2 = buffer[1]; //we test buffer 1
	id = (id1 << 8) | id2;

    #ifdef BUILD_LK
		printf("%s, LK ili9881 debug: ili9881 id = %x,%x,%x\n", __func__, id,id1,id2);
    #else
		printk("%s, kernel ili9881 horse debug: ili9881 id = 0x%08x\n", __func__, id);
    #endif
//return 1;
    if(id1 == LCM_ID_ILI9881)
    	return 1;
    else
        return 0;
}
#if 0
static int err_count = 0;
static unsigned int lcm_esd_check(void)
{
  #ifndef BUILD_LK
    unsigned char buffer[8] = {0};

    unsigned int array[4];
    int i =0;

	if(lcm_esd_test)
	{
		lcm_esd_test = FALSE;
		return TRUE;
	}

    array[0] = 0x00013700;    

    dsi_set_cmdq(array, 1,1);

    read_reg_v2(0x0A, buffer,8);

	printk( "ili9881 lcm_esd_check: buffer[0] = %d,buffer[1] = %d,buffer[2] = %d,buffer[3] = %d,buffer[4] = %d,buffer[5] = %d,buffer[6] = %d,buffer[7] = %d\n",buffer[0],buffer[1],buffer[2],buffer[3],buffer[4],buffer[5],buffer[6],buffer[7]);

    if((buffer[0] != 0x9C))/*LCD work status error,need re-initalize*/

    {

        printk( "ili9881 lcm_esd_check buffer[0] = %d\n",buffer[0]);

        return TRUE;

    }

    else

    {

#if 0
        if(buffer[3] != 0x02) //error data type is 0x02

        {
		//  is not 02, 
             err_count = 0;

        }

        else

        {
		// is 02, so ,
             if((buffer[4] == 0x40) || (buffer[5] == 0x80))
             {
			 // buffer[4] is not 0, || (huo),buffer[5] is 0x80.
			   err_count = 0;
             }
             else

             {
			// is  0,0x80,  
			   err_count++;
             }             

             if(err_count >=2 )
             {
			
                 err_count = 0;

                 printk( "ili9881 lcm_esd_check buffer[4] = %d , buffer[5] = %d\n",buffer[4],buffer[5]);

                 return TRUE;

             }

        }
#endif
        return FALSE;

    }
#endif
	
}
static unsigned int lcm_esd_recover(void)
{
    #ifndef BUILD_LK
    printk( "ili9881 lcm_esd_recover\n");
    #endif
	lcm_init();
//	lcm_resume();

	return TRUE;
}
#endif
//modify@zte.com.cn modify at 20150921 begin
#ifdef CONFIG_WIND_LCD_POWER_SUPPLY_SUPPORT
extern void lcm_init_power(void);
extern void lcm_resume_power(void);
extern void lcm_suspend_power(void);
#endif
LCM_DRIVER ili9881_CA_hd720_dsi_vdo_yassy_lcm_drv =
{
    .name           	= "ili9881_CA_hd720_dsi_vdo_yassy",
    .set_util_funcs 	= lcm_set_util_funcs,
    .get_params     	= lcm_get_params,
    .init           	= lcm_init,
    .suspend        	= lcm_suspend,
    .resume         	= lcm_resume,
    .compare_id     	= lcm_compare_id,
	//.esd_check = lcm_esd_check,
	//.esd_recover = lcm_esd_recover,
#ifdef CONFIG_WIND_LCD_POWER_SUPPLY_SUPPORT
	.init_power		= lcm_init_power,
	.resume_power   = lcm_resume_power,
	.suspend_power  = lcm_suspend_power,
#endif    
};
//modify@zte.com.cn modify at 20150921 end
//late_initcall(lcm_init);
/* END PN: , Added by guohongjin, 2014.08.13*/
//modify@zte.com.cn 20140813 end
