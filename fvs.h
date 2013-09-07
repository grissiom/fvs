/** Flash Variable System
 *
 * FVS is short for Flash Variable System. It does not tend to be as cumbersome
 * as a full-featured File System. It's main goal is to store the *variables*
 * on the flash on chip efficiently and safely.
 *
 * We call each variable a "vnode"(variable node) and we use the combination of
 * two elements to identify a vnode:(id, size), which id is an arbitrary number
 * used by the application to identify the variable and size is the size of the
 * variable.  For example, (1, 4), (1, 8) and (2, 4) will represent different
 * vnode. Be aware that 0 and -1 are not valid ids.
 *
 * The struct fvs_block is to represent a flash block on chip. Usaully it
 * consist by FVS_BLK_PAGE_NR physical pages. But the implementation detail
 * might be changed in the future.
 *
 * The FVS project is released to Public Domain.
 */

#ifndef FVS_H
#define FVS_H

#include <stdint.h>
#include <stdlib.h>

#define FVS_QUIET  0
#define FVS_VERBOSE 1
#define FVS_DEBUG  2

#define FVS_LOG_LVL 0

#if FVS_LOG_LVL >= FVS_VERBOSE
#define fvs_verbose rt_kprintf

#if FVS_LOG_LVL >= FVS_DEBUG
#define fvs_debug rt_kprintf
#else
#define fvs_debug(...)
#endif

#else
#define fvs_verbose(...)
#define fvs_debug(...)
#endif

#include "fvs_hal.h"

typedef fvs_native_t fvs_id_t;
typedef fvs_native_t fvs_size_t;

/** the values after erase */
#define FVS_END_OF_ID  ((fvs_id_t)(-1))

/* number of physical page involved with page rolling.
 * Due to implementation details, this is not configurable. I just use micro to
 * avoid magic numbers.
 */
#define FVS_BLK_PAGE_NR 2

struct fvs_block {
	rt_uint8_t *pages[FVS_BLK_PAGE_NR];
	/* the usable size of the page. This is smaller than the actual size of the
	 * page. */
	size_t size;
	// TODO: lock the page, maybe read-write lock is good.
};

#define FVS_DEFINE_BLOCK(name, base1, base2, size) \
	struct fvs_block name = {(rt_uint8_t*)base1, (rt_uint8_t*)base2,  \
		/* FVS assume we are in a memory space filled with 0xFF. So we have to
		 * preserve one information block to hold at least the FVS_END_OF_ID.
		 * The last fvs_native_t of the page is used to store the page
		 * status(empty(-1) or using(0)). */ \
		(size_t)size - sizeof(struct fvs_vnode)}

/* the struct is reside on the flash in most of the times. The content of
 * base_addr of a fvs_block should a fvs_vnode. */
struct fvs_vnode {
	/* variable id */
	fvs_id_t id;
	/* the size of variable(without this head struct) */
	fvs_size_t size;
	fvs_native_t status;
	/* there should be size of bytes of data followed */
} __attribute__((packed));

/** return how many byte are used in the page.
 *
 * Note that the meta-data is not counted.
 */
size_t fvs_page_used_size(const struct fvs_block *page);

/** return whether the page is used
 *
 * A used page is a page that we have created(get) vnode on.
 */
rt_bool_t fvs_page_used(const struct fvs_block *page);


/** get vnode (id, size) from page
 *
 * @return the pointer to the data. You can cast the pointer to the pointer
 * type of your real variable.
 */
void *fvs_vnode_get(const struct fvs_block *page, fvs_id_t id, size_t size);

/** update the the vnode (id, size) on page with the data pointed by data
 *
 * @return the error code on write failure.
 */
rt_err_t fvs_vnode_write(const struct fvs_block *page, fvs_id_t id, fvs_size_t size, void *data);

/** delete the vnode (id, size) on page */
void fvs_vnode_delete(const struct fvs_block *page, fvs_id_t id, fvs_size_t size);

#endif /* end of include guard: FVS_H */
