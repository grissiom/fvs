#include <em_msc.h>

#include <fvs.h>

rt_err_t fvs_begin_write(struct fvs_page *p)
{
	MSC_Init();
	return RT_EOK;
}

rt_err_t fvs_native_write(void *addr, fvs_native_t data)
{
	MSC_Init();
	msc_Return_TypeDef res;
	fvs_debug("FVS: write %X to 0x%p\n", data, addr);
	res = MSC_WriteWord(addr, &data, sizeof(data));
	if (res != mscReturnOk)
		return -RT_ERROR;

	return RT_EOK;
}

rt_err_t fvs_end_write(struct fvs_page *p)
{
	MSC_Deinit();
	return RT_EOK;
}

rt_err_t fvs_erase_page(struct fvs_page *page)
{
	MSC_ErasePage(page->base_addr);
	return RT_EOK;
}

