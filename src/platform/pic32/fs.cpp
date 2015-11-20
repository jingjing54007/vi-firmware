#ifdef CROSSCHASM_C5 //to be tested on other platforms

#include "interface/fs.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <plib.h>
#include <stdbool.h>
#include <ctype.h>
#include "util/log.h"
#include "util/timer.h"
#include "fsman.h"
#include <stdarg.h>
#include "lights.h"


static uint32_t file_elapsed_timer=0;
static uint32_t file_flush_timer=0;
 
using openxc::util::log::debug;
namespace lights = openxc::lights;

static FS_STATE fs_mode = FS_STATE::NONE_CONNECTED;

extern "C" {
	
#include "fsman.h"
#ifdef RTCC_SUPPORT
	#include "rtcc.h"
#ifdef DEBUG
	#undef RTCC_SUPPORT
#endif
#endif

extern uint16_t adc_get_pval(void);

void __debug(const char* format, ...){
	va_list args;
	va_start(args, format);

	char buffer[128];
	vsnprintf(buffer, 128, format, args);

	debug(buffer);
	va_end(args);
}
void _GetTimestamp (struct tm * t){
#ifndef RTCC_SUPPORT
	return;
#endif	
	RTCC_STATUS r = RTCCGetTimeDateDecimal(t);
	if(r != RTCC_NO_ERROR){
		//todo add a psuedo time here??
	}
}

}

void Init_RTC(void)
{
#ifndef RTCC_SUPPORT
	debug("Skipping RTC Init");
	return;
#endif	
	struct tm TimeDate; 
	
	RTCC_STATUS r = I2C_Initialize();
	
	if(r != RTCC_NO_ERROR)
	{
		debug("RTC_INIT FAILED %d",r);
	}
}

	
FS_STATE GetDevicePowerState(void)
{
	if(adc_get_pval() < 160){
		__debug("Device USB powered");
		return FS_STATE::USB_CONNECTED;
	}
	__debug("Device battery powered");
	return FS_STATE::VI_CONNECTED;
}
FS_STATE openxc::interface::fs::getmode(void){
	return fs_mode;
}

bool openxc::interface::fs::connected(FsDevice* device){

	if(getmode() == FS_STATE::USB_CONNECTED){
		return false;
	}
	else{
		return device->configured; //remove all SD logging features
	}
}

bool openxc::interface::fs::setRTC(uint32_t *unixtime){
	debug("Set RTC Time %d", *unixtime);
#ifndef RTCC_SUPPORT
	return false;
#endif		
	RTCC_STATUS status  = RTCCSetTimeDateUnix(*unixtime);

	return (status == RTCC_STATUS::RTCC_NO_ERROR)? true: false;
}


bool openxc::interface::fs::initialize(FsDevice* device){
	

	uint8_t err;

	device->configured = false;
	
	fs_mode = GetDevicePowerState();
	
	//Initialize RTCC module for filetimestamping
	Init_RTC();
	
	if(fsmanInit(&err)){
		debug("SD Card Initialized");
	}
	else{
		debug("Unable to Init SD Card");
		debug(fsmanGetErrStr(err));
		return false;
	}
	if(getmode() == FS_STATE::USB_CONNECTED){
		return true;
	}
		
	device->configured = true;
	initializeCommon(device);
	return device->configured;
}



void openxc::interface::fs::manager(FsDevice* device){ //session manager for FS
	uint8_t ret;
	uint32_t secs_elapsed;
	
	if(getmode() == FS_STATE::VI_CONNECTED)
	{
		secs_elapsed = millis()/1000;
		
		if(device->configured == false)
			return;
		
		if(fsmanSessionIsActive()){
			//flush session based on timeout of data to write entries to the FAT
			if(secs_elapsed > file_flush_timer + FILE_FLUSH_DATA_TIMEOUT_SEC){
				if(!fsmanSessionFlush(&ret)){
					debug("Unable to flush session");
				}
				file_flush_timer = secs_elapsed;
			}
		}
	}
}

void openxc::interface::fs::write(FsDevice* device, uint8_t *data, uint32_t len) 
{
	uint8_t ret;
	uint32_t secs_elapsed;
	
	if(device->configured == false ||
			getmode() == FS_STATE::VI_CONNECTED){
				
		debug("USB Mode SD Card Uninitialized");
		return;
	}
	
	
	if(!fsmanSessionIsActive()){
		debug("Starting Session");
		if(!fsmanSessionStart(&ret)){
			debug(fsmanGetErrStr(ret));
		}
	}
	secs_elapsed = millis()/1000;
	debug("SE:%d",secs_elapsed);
	if(secs_elapsed > file_elapsed_timer + FILE_WRITE_RATE_SEC){
		file_elapsed_timer = secs_elapsed;
		if(!fsmanSessionReset(&ret)){
			debug("Unable to reset session");
			debug(fsmanGetErrStr(ret));
			return;
		}
	}
	if(!fsmanSessionWrite(&ret, data, len)){
		debug("Unable to write data");
		debug(fsmanGetErrStr(ret));
	}

}	

void openxc::interface::fs::deinitialize(FsDevice* device){
	uint8_t ret;
	if(getmode() == FS_STATE::VI_CONNECTED){
		if(device->configured == true){
			if(fsmanSessionIsActive()){
				if(fsmanSessionEnd(&ret)){
					debug("Unable to end session");
					debug(fsmanGetErrStr(ret));
				}
			}
		}
	}else if(getmode() == FS_STATE::USB_CONNECTED){
		
		//we should not be called here in this mode as we are always powered
	}
	
	//flush existing data
	deinitializeCommon(device);
	debug("de-initialized disk");
}	
#endif