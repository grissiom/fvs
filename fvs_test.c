/** Flash Variable System
 *
 * Test cases. This is part of FVS project
 */

#include <rtthread.h>

#include "fvs.h"

// don't need the whole physical page, 128 bytes should be enough to go
#define _PAGE_SZ 128

#define _DATA_SZ 4
#define _NODE_SZ (sizeof(struct fvs_vnode)+_DATA_SZ)
#define _NODE_PER_PAGE ((_PAGE_SZ+_NODE_SZ/2)/_NODE_SZ-1)

#define _RETURN_ON_FAIL(exp) \
	do { \
		res = exp; \
		if (res != RT_EOK) \
		return res; \
	} while (0)

static rt_err_t _test_vnode_get(const struct fvs_block *pg)
{
	int i;
	/* it should be the first page */
	char *ppn = (char*)pg->pages[0];

	// this should assert fail
	/*fvs_vnode_get(pg, 0, 1);*/

	for (i = 1; i < _PAGE_SZ; i++) {
		char *pn = fvs_vnode_get(pg, i, _DATA_SZ);
		// one preserved vnode
		if (i > _NODE_PER_PAGE) {
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
			if (i != 1 && (pn - ppn) != _NODE_SZ) {
				rt_kprintf("fvs_vnode_get fail on wrong node size\n");
				rt_kprintf("expect %d, get %d\n", _NODE_SZ, (pn-ppn));
				return -RT_ERROR;
			}
		}

		ppn = pn;
	}
	rt_kprintf("fvs_vnode_get test pass\n");
	return RT_EOK;
}

static rt_err_t _test_simple_write(const struct fvs_block *pg)
{
	int i = 0x12345678;
	int *p, *np;

	rt_err_t err = fvs_vnode_write(pg, 2, _DATA_SZ, &i);
	p = (int*)fvs_vnode_get(pg, 2, _DATA_SZ);
	if (*p != i) {
		rt_kprintf("fvs write data fail, err: %d\n", err);
		rt_kprintf("expect %d, get %d\n", i, *(int*)fvs_vnode_get(pg, 2, _DATA_SZ));
		return -RT_ERROR;
	}
	if (*(int*)fvs_vnode_get(pg, 1, _DATA_SZ) != -1) {
		rt_kprintf("fvs write data fail, err: %d\n", err);
		rt_kprintf("expect %d, get %d\n", -1, *(int*)fvs_vnode_get(pg, 1, _DATA_SZ));
		return -RT_ERROR;
	}
	if (*(int*)fvs_vnode_get(pg, 3, _DATA_SZ) != -1) {
		rt_kprintf("fvs write data fail, err: %d\n", err);
		rt_kprintf("expect %d, get %d\n", -1, *(int*)fvs_vnode_get(pg, 3, _DATA_SZ));
		return -RT_ERROR;
	}
	rt_kprintf("fvs simple write data pass\n");

	err = fvs_vnode_write(pg, 2, _DATA_SZ, &i);
	np = (int*)fvs_vnode_get(pg, 2, _DATA_SZ);
	if (np != p) {
		rt_kprintf("fvs write same data fail\n");
		rt_kprintf("expect 0x%p, get 0x%p\n", p, np);
		return -RT_ERROR;
	}
	rt_kprintf("fvs write same data pass\n");

	return RT_EOK;
}

static rt_err_t _test_rewrite(const struct fvs_block *pg)
{
	int i = 0x56781234;
	rt_err_t err = fvs_vnode_write(pg, 2, _DATA_SZ, &i);
	if (*(int*)fvs_vnode_get(pg, 2, _DATA_SZ) != i) {
		rt_kprintf("fvs rewrite data fail, err: %d\n", err);
		rt_kprintf("expect %d, get %d\n", i, *(int*)fvs_vnode_get(pg, 2, _DATA_SZ));
		return -RT_ERROR;
	}

	if (*(int*)fvs_vnode_get(pg, 1, _DATA_SZ) != -1) {
		rt_kprintf("fvs rewrite data fail, err: %d\n", err);
		rt_kprintf("expect %d, get %d\n", -1, *(int*)fvs_vnode_get(pg, 1, _DATA_SZ));
		return -RT_ERROR;
	}
	if (*(int*)fvs_vnode_get(pg, 3, _DATA_SZ) != -1) {
		rt_kprintf("fvs rewrite data fail, err: %d\n", err);
		rt_kprintf("expect %d, get %d\n", -1, *(int*)fvs_vnode_get(pg, 3, _DATA_SZ));
		return -RT_ERROR;
	}

	rt_kprintf("fvs rewrite whole page pass\n");
	return -RT_EOK;
}

static rt_err_t _test_del(const struct fvs_block *pg)
{
	int *p;

	fvs_vnode_delete(pg, 1, _DATA_SZ);
	p = fvs_vnode_get(pg, _NODE_PER_PAGE+1, _DATA_SZ);

	if (p == NULL) {
		rt_kprintf("fvs del node fail\n");
		rt_kprintf("expect get node after deletion\n");
		return -RT_ERROR;
	}

	// note the page is full
	if (*p != -1) {
		rt_kprintf("fvs del node fail\n");
		rt_kprintf("expect the data to be 0x%X, get 0x%X\n", -1, *p);
		return -RT_ERROR;
	} else {
		rt_kprintf("fvs del node pass\n");
		return -RT_EOK;
	}
}

rt_err_t fvs_test(void)
{
	rt_err_t res;
	int i;

	// stm32f10x
	/*
	 *FVS_DEFINE_BLOCK(tst_pg,
	 *        (void*)0x0807E000,
	 *        (void*)0x0807F000,
	 *        _PAGE_SZ);
	 */

	// efm32gg980
	const FVS_DEFINE_BLOCK(tst_pg,
			(void*)0x7F000,
			(void*)0x7E000,
			_PAGE_SZ);


	rt_kprintf("fvs test begin\n");
	/* reset the block */
	for (i = 0; i < FVS_BLK_PAGE_NR; i++)
	{
		fvs_begin_write((void*)tst_pg.pages[i]);
		fvs_erase_page((void*)tst_pg.pages[i]);
		fvs_end_write((void*)tst_pg.pages[i]);
	}

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
