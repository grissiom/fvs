#include <stm32f10x.h>

#include <fvs.h>

rt_err_t fvs_begin_write(struct fvs_page *p)
{
	FLASH_UnlockBank1();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	return RT_EOK;
}

rt_err_t fvs_native_write(void *addr, fvs_native_t data)
{
	FLASH_Status res;
	/*FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);*/
	fvs_debug("FVS: write %X to 0x%p\n", data, addr);
	res = FLASH_ProgramHalfWord((uint32_t)addr, data);
	if (res != FLASH_COMPLETE)
		return -RT_ERROR;

	return RT_EOK;
}

rt_err_t fvs_end_write(struct fvs_page *p)
{
	FLASH_LockBank1();
	return RT_EOK;
}

rt_err_t fvs_erase_page(struct fvs_page *page)
{
	FLASH_ErasePage((uint32_t)page->base_addr);
	return RT_EOK;
}

