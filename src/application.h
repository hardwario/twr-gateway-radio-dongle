#ifndef _APPLICATION_H
#define _APPLICATION_H

#ifndef CORE_MODULE
#define CORE_MODULE 0
#endif

#if CORE_MODULE
#define FIRMWARE "twr-gateway-core-module"
#define TALK_OVER_CDC 1
#define GPIO_LED TWR_GPIO_LED
#else
#define FIRMWARE "twr-gateway-radio-dongle"
#define GPIO_LED 19
#endif

#include <twr.h>

#endif
