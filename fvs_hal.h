#ifndef __FVS_HAL_H_
#define __FVS_HAL_H_

#include <rtthread.h>

#include "fvs/hw.h"

struct fvs_page;

rt_err_t fvs_begin_write(struct fvs_page*);
rt_err_t fvs_native_write(void* addr, fvs_native_t data);
rt_err_t fvs_end_write(struct fvs_page*);
rt_err_t fvs_erase_page(struct fvs_page*);

#endif /* end of include guard: __FVS_HAL_H_ */
