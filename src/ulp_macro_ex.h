#pragma once



#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp32/ulp.h"

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"



esp_err_t ulp_process_macros_and_load_ex(uint32_t load_addr, const ulp_insn_t* program, size_t* psize);
