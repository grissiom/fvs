#include <stm32f10x.h>

#include <fvs.h>

rt_err_t fvs_begin_write(void *addr)
{
	FLASH_UnlockBank1();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	return RT_EOK;
}

rt_err_t fvs_native_write_r(void *addr, fvs_native_t data)
{
	FLASH_Status res;
	/*FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);*/
	fvs_debug("FVS: write %X to 0x%p\n", data, addr);
	res = FLASH_ProgramHalfWord((uint32_t)addr, data);
	if (res != FLASH_COMPLETE)
		return -RT_ERROR;

	return RT_EOK;
}

rt_err_t fvs_native_write_m(void *addr, rt_uint8_t *data, rt_size_t len)
{
    rt_size_t i;

	fvs_debug("FVS: write %X to 0x%p\n", data, addr);

    for (i = 0; i < len; i += sizeof(fvs_native_t))
    {
        FLASH_Status res;
        res = FLASH_ProgramHalfWord((uint32_t)((char*)addr+i),
                *(fvs_native_t*)(data+i));
        if (res != FLASH_COMPLETE)
            return -RT_ERROR;
    }

	return RT_EOK;
}

rt_err_t fvs_end_write(void *addr)
{
	FLASH_LockBank1();
	return RT_EOK;
}

rt_err_t fvs_erase_page(void *addr)
{
	FLASH_ErasePage((rt_uint32_t)addr);
	return RT_EOK;
}

