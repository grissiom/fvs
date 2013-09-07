#include <em_msc.h>

#include <fvs.h>

rt_err_t fvs_begin_write(void *addr)
{
	MSC_Init();
	return RT_EOK;
}

rt_err_t fvs_native_write_r(void *addr, fvs_native_t data)
{
	msc_Return_TypeDef res;
	fvs_debug("FVS: write %X to 0x%p\n", data, addr);
	res = MSC_WriteWord(addr, &data, sizeof(data));
	if (res != mscReturnOk)
		return -RT_ERROR;

	return RT_EOK;
}

rt_err_t fvs_native_write_m(void *addr, rt_uint8_t *data, rt_size_t len)
{
	msc_Return_TypeDef res;
	fvs_debug("FVS: write %d bytes of data to 0x%p\n", len, addr);
	res = MSC_WriteWord(addr, data, len);
	if (res != mscReturnOk)
		return -RT_ERROR;

	return RT_EOK;
}

rt_err_t fvs_end_write(void *addr)
{
	MSC_Deinit();
	return RT_EOK;
}

rt_err_t fvs_erase_page(void *addr)
{
	MSC_ErasePage(addr);
	return RT_EOK;
}

