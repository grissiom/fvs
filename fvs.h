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
 * vnode.
 *
 * The struct fvs_page is to represent the flash page on chip. We expose the
 * data structure detail here for static usage. But the implementation detail
 * might be changed in the future.
 *
 * The FVS project is released to Public Domain.
 */

#ifndef FVS_H
#define FVS_H

#include <stdint.h>
#include <stdlib.h>

#if FVS_LOG_LVL >= FVS_VERBOSE
#define fvs_verbose rt_kprintf
#if FVS_LOG_LVL >= FVS_DEBUG
#define fvs_debug rt_kprintf
#else
#define fvs_debug(...)
#endif
#else
#define fvs_verbose(...)
#endif

typedef uint16_t fvs_id_t;
typedef uint16_t fvs_size_t;
typedef uint16_t fvs_native_t;

/** the values after erase */
#define FVS_END_OF_ID  ((fvs_id_t)0xFFFF)

struct fvs_page {
	void *base_addr;
	size_t size;
	// TODO: lock the page, maybe read-write lock is good.
};

/** initialize the fvs_page pointed by page
 */
void fvs_page_init(struct fvs_page *page, void *base_addr, size_t size);

/** create a page dynamically
 */
struct fvs_page *fvs_page_create(void *base_addr, size_t size);


/** return how many byte are used in the page.
 *
 * Note that the meta-data is not counted.
 */
size_t fvs_page_used_size(struct fvs_page *page);


/** get vnode (id, size) from page
 *
 * @return the pointer to the data. You can cast the pointer to the pointer
 * type of your real variable.
 */
void *fvs_vnode_get(struct fvs_page *page, fvs_id_t id, size_t size);

/** update the the vnode (id, size) on page with the data pointed by data
 *
 * @return the error code on write failure.
 */
rt_err_t fvs_vnode_write(struct fvs_page *page, fvs_id_t id, fvs_size_t size, void *data);

/** delete the vnode (id, size) on page
 */
void fvs_vnode_delete(struct fvs_page *page, fvs_id_t id, fvs_size_t size);

#endif /* end of include guard: FVS_H */
