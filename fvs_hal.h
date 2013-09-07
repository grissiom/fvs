#ifndef __FVS_HAL_H_
#define __FVS_HAL_H_

#include <rtthread.h>

#include "fvs/hw.h"

struct fvs_block;

rt_err_t fvs_begin_write(void *base_addr);
/* write the value in memory */
rt_err_t fvs_native_write_m(void* addr, rt_uint8_t *data, rt_size_t len);
/* write the value in register */
rt_err_t fvs_native_write_r(void* addr, fvs_native_t data);
rt_err_t fvs_end_write(void *base_addr);
rt_err_t fvs_erase_page(void *base_addr);

#endif /* end of include guard: __FVS_HAL_H_ */
