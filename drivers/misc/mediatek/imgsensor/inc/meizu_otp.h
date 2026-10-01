/*****************************************************************************
 *
 * Filename:
 * ---------
 *   meizu_otp.h
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   Meizu OTP struct definitions for m2note camera modules.
 *
 ****************************************************************************/
#ifndef _MEIZU_OTP_H
#define _MEIZU_OTP_H

typedef struct sunny_otp_struct {
	int flag;
	int module_id;
	int lens_id;
	int IR_id;
	int production_year;
	int production_month;
	int production_day;
	int awb_rg_msb;
	int awb_bg_msb;
	int awb_lsb;
} sunny_otp_struct;

typedef struct film_otp_struct {
	int flag;
	int module_id;
	int lens_id;
	int production_year;
	int production_month;
	int production_day;
	int awb_rg_msb;
	int awb_bg_msb;
	int awb_lsb;
} ofilm_otp_struct;

typedef struct tech_otp_struct {
	int flag;
	int module_id;
	int IR_id;
	int production_year;
	int production_month;
	int production_day;
	int awb_rg_msb;
	int awb_bg_msb;
	int awb_lsb;
} qtech_otp_struct;

typedef struct avc_otp_struct {
	int flag;
	int module_id;
	int lens_id;
	int production_year;
	int production_month;
	int production_day;
	int awb_rg_msb;
	int awb_bg_msb;
	int awb_lsb;
} avc_otp_struct;

#endif
