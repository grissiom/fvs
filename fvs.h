#ifndef FVS_H
#define FVS_H

#include <stdint.h>
#include <stdlib.h>

typedef uint16_t fvs_id_t;
typedef uint16_t fvs_size_t;
typedef uint16_t fvs_native_t;

#define FVS_END_OF_ID  ((fvs_id_t)0xFFFF)

struct fvs_page {
	void *base_addr;
	size_t size;
	// TODO: lock the page, maybe read-write lock is good.
};

void fvs_page_init(struct fvs_page *page, void *base_addr, size_t size);
struct fvs_page *fvs_page_create(void *base_addr, size_t size);
size_t fvs_page_used_size(struct fvs_page *page);

void *fvs_vnode_get(struct fvs_page *page, fvs_id_t id, size_t size);
rt_err_t fvs_vnode_write(struct fvs_page *page, fvs_id_t id, fvs_size_t size, void *data);
void fvs_vnode_delete(struct fvs_page *page, fvs_id_t id, fvs_size_t size);

#endif /* end of include guard: FVS_H */
