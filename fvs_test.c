/** Flash Variable System
 *
 * Test cases. This is part of FVS project
 */

#include <rtthread.h>

#include "fvs.h"

static struct fvs_page tst_pg;

void fvs_test(void)
{
	rt_kprintf("fvs test begin\n");

	// don't need the whole physical page, 512 bytes should be enough to go
	fvs_page_init(&tst_pg, (void*)0x0807E000, 512);

	void *ppn = tst_pg.base_addr;
	for (int i = 1; i < 512; i++) {
		// the node header is 6 bytes, so a node is 10 bytes
		// the loop should fall when i == 52
		void *pn = fvs_vnode_get(&tst_pg, i, 4);
		if (i == 52) {
			if (pn != NULL) {
				rt_kprintf("fvs_vnode_get fail on i = %d", i);
				rt_kprintf("expect node %p, get node %p", NULL, pn);
				return;
			} else {
				break;
			}
		} else {
			if (pn == NULL) {
				rt_kprintf("fvs_vnode_get fail on i = %d", i);
				return;
			}
			if (i != 1 && (pn - ppn) != 10) {
				rt_kprintf("fvs_vnode_get fail on wrong node size\n");
				rt_kprintf("expect %d, get %d", 10, (pn-ppn));
				return;
			}
		}

		ppn = pn;
	}
	rt_kprintf("fvs_vnode_get test pass\n");

	rt_kprintf("fvs test simple write data\n");
	int i = 0x12345678;
	rt_err_t err = fvs_vnode_write(&tst_pg, 1, 4, &i);
	if (*(int*)fvs_vnode_get(&tst_pg, 1, 4) != i) {
		rt_kprintf("fvs write data fail, err: %d\n", err);
		rt_kprintf("expect %d, get %d", i, *(int*)fvs_vnode_get(&tst_pg, 1, 4));
		return;
	} else {
		rt_kprintf("fvs simple write data pass\n");
	}

	rt_kprintf("fvs rewrite whole page test\n");
	i = 0x13572468;
	err = fvs_vnode_write(&tst_pg, 1, 4, &i);
	if (*(int*)fvs_vnode_get(&tst_pg, 1, 4) != i) {
		rt_kprintf("fvs write data fail, err: %d\n", err);
		rt_kprintf("expect %d, get %d", i, *(int*)fvs_vnode_get(&tst_pg, 1, 4));
		return;
	} else {
		rt_kprintf("fvs rewrite whole page pass\n");
	}

	rt_kprintf("fvs del node test\n");
	fvs_vnode_delete(&tst_pg, 1, 4);
	// note the page is full
	if (fvs_vnode_get(&tst_pg, 1, 4) != NULL) {
		rt_kprintf("fvs del node fail\n");
		rt_kprintf("expect %X, get %X", NULL, fvs_vnode_get(&tst_pg, 1, 4));
		return;
	} else {
		rt_kprintf("fvs del node pass\n");
	}
}

#ifdef RT_USING_FINSH
#include <finsh.h>
FINSH_FUNCTION_EXPORT(fvs_test, test the fvs);
#endif
