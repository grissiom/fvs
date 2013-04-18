/** Flash Variable System
 *
 * Test cases. This is part of FVS project
 */

#include <rtthread.h>

#include "fvs.h"

// don't need the whole physical page, 512 bytes should be enough to go
#define _PAGE_SZ 512

#define _RETURN_ON_FAIL(exp) \
do { \
    res = exp; \
    if (res != RT_EOK) \
        return res; \
} while (0)

static rt_err_t _test_vnode_get(struct fvs_page *pg)
{
    int i;
	char *ppn = (char*)pg->base_addr;
	for (i = 1; i < _PAGE_SZ; i++) {
		const int dsz = 4;
		const int nsz = 3*sizeof(fvs_native_t)+dsz;
		char *pn = fvs_vnode_get(pg, i, dsz);
        // one preserved vnode
		if (i > _PAGE_SZ/nsz-1) {
			if (pn != NULL) {
				rt_kprintf("fvs_vnode_get fail on i = %d\n", i);
				rt_kprintf("expect node %p, get node %p\n", NULL, pn);
				return -RT_ERROR;
			} else {
				break;
			}
		} else {
			if (pn == NULL) {
				rt_kprintf("fvs_vnode_get fail on i = %d\n", i);
				return -RT_ERROR;
			}
			if (i != 1 && (pn - ppn) != nsz) {
				rt_kprintf("fvs_vnode_get fail on wrong node size\n");
				rt_kprintf("expect %d, get %d\n", nsz, (pn-ppn));
				return -RT_ERROR;
			}
		}

		ppn = pn;
	}
	rt_kprintf("fvs_vnode_get test pass\n");
    return RT_EOK;
}

static rt_err_t _test_simple_write(struct fvs_page *pg)
{
	int i = 0x12345678;
	rt_err_t err = fvs_vnode_write(pg, 1, 4, &i);
	if (*(int*)fvs_vnode_get(pg, 1, 4) != i) {
		rt_kprintf("fvs write data fail, err: %d\n", err);
		rt_kprintf("expect %d, get %d\n", i, *(int*)fvs_vnode_get(pg, 1, 4));
		return -RT_ERROR;
	} else {
		rt_kprintf("fvs simple write data pass\n");
        return RT_EOK;
	}
}

static rt_err_t _test_rewrite(struct fvs_page *pg)
{
	int i = 0x12345678;
	rt_err_t err = fvs_vnode_write(pg, 1, 4, &i);
	if (*(int*)fvs_vnode_get(pg, 1, 4) != i) {
		rt_kprintf("fvs rewrite data fail, err: %d\n", err);
		rt_kprintf("expect %d, get %d\n", i, *(int*)fvs_vnode_get(pg, 1, 4));
		return -RT_ERROR;
	} else {
		rt_kprintf("fvs rewrite whole page pass\n");
		return -RT_EOK;
	}
}

static rt_err_t _test_del(struct fvs_page *pg)
{
	fvs_vnode_delete(pg, 1, 4);
	// note the page is full
	if (fvs_vnode_get(pg, 1, 4) != NULL) {
		rt_kprintf("fvs del node fail\n");
		rt_kprintf("expect %X, get %X\n", NULL, fvs_vnode_get(pg, 1, 4));
		return -RT_ERROR;
	} else {
		rt_kprintf("fvs del node pass\n");
		return -RT_EOK;
	}
}

rt_err_t fvs_test(void)
{
    rt_err_t res;
    struct fvs_page tst_pg;
	rt_kprintf("fvs test begin\n");

	// stm32f10x
	/*fvs_page_init(&tst_pg, (void*)0x0807E000, _PAGE_SZ);*/
	// efm32gg980
	fvs_page_init(&tst_pg, (void*)0x7FE00, _PAGE_SZ);
	/*rt_kprintf("base content %X\n", *(uint32_t*)0x0FE00000);*/
	/* reset the page */
	fvs_erase_page(&tst_pg);

    _RETURN_ON_FAIL(_test_vnode_get(&tst_pg));
    _RETURN_ON_FAIL(_test_simple_write(&tst_pg));
    _RETURN_ON_FAIL(_test_rewrite(&tst_pg));
    _RETURN_ON_FAIL(_test_del(&tst_pg));

    return res;
}

#ifdef RT_USING_FINSH
#include <finsh.h>
FINSH_FUNCTION_EXPORT(fvs_test, test the fvs);
#endif
