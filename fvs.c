/** Flash Variable System
 *
 * Implementations. This is part of FVS project
 */

#include <string.h>
#include <rtthread.h>

#include "fvs.h"

#define ASSERT RT_ASSERT

#define FVS_VN_STATUS_EMPTY   ((fvs_native_t)-1)
#define FVS_VN_STATUS_WRITTEN ((fvs_native_t)0x0)

static rt_err_t vn_do_create(
		rt_uint8_t *base_addr,
		struct fvs_vnode *node,
		fvs_id_t id,
		fvs_size_t size);

static rt_err_t vn_fill_data(
		rt_uint8_t *base_addr,
		struct fvs_vnode *node,
		void* data);

rt_inline struct fvs_vnode* vn_next(struct fvs_vnode *node)
{
	return (struct fvs_vnode*)((char*)node + sizeof(*node) + node->size);
}

rt_inline int vn_is_valid(struct fvs_vnode *node)
{
	return node->id != 0;
}

rt_inline void blk_mark_as_using(
		rt_uint8_t *base_addr,
		size_t size)
{
	struct fvs_vnode *node = (struct fvs_vnode*)(base_addr + size);

	fvs_verbose("FVS: mark page 0x%p as using\n", base_addr);
	fvs_begin_write(base_addr);
	fvs_native_write_r((void*)&node->status, 0);
	fvs_end_write(base_addr);
}

rt_inline rt_bool_t blk_page_inuse(
		const struct fvs_block *page,
		int idx)
{
	struct fvs_vnode *node = (struct fvs_vnode*)(page->pages[idx] + page->size);
	return node->status == 0;
}

rt_inline rt_uint8_t* blk_find_using(
		const struct fvs_block *page)
{
	int i;
	for (i = 0; i < FVS_BLK_PAGE_NR; i++)
	{
		if (blk_page_inuse(page, i))
			return page->pages[i];
	}
	return RT_NULL;
}

rt_err_t blk_roll_pages(const struct fvs_block *blk)
{
	struct fvs_vnode *node;
	rt_uint8_t *using_page, *empty_page, *ptr;

	using_page = blk_find_using(blk);
	ASSERT(using_page);

	if (using_page == blk->pages[0])
		empty_page = blk->pages[1];
	else
		empty_page = blk->pages[0];

	fvs_verbose("FVS: rolling pages: from(0x%x), to(0x%x)\n",
			using_page, empty_page);

	ptr = empty_page;
	for (node = (struct fvs_vnode*)using_page;
			node->id != FVS_END_OF_ID;
			node = vn_next(node)) {
		if (!vn_is_valid(node))
			continue;

		vn_do_create((rt_uint8_t*)empty_page,
				(struct fvs_vnode*)ptr, node->id, node->size);
		vn_fill_data((rt_uint8_t*)empty_page,
				(struct fvs_vnode*)ptr, node+1);
		ptr += sizeof(struct fvs_vnode) + node->size;
	}
	/* mark the empty page as using */
	blk_mark_as_using(empty_page, blk->size);

	/* erase the old page in the last */
	fvs_begin_write(using_page);
	fvs_erase_page(using_page);
	fvs_end_write(using_page);

	return RT_EOK;
}

static rt_err_t vn_do_create(
		rt_uint8_t *base_addr,
		struct fvs_vnode *node,
		fvs_id_t id,
		fvs_size_t size)
{
	fvs_begin_write(base_addr);

	fvs_verbose("FVS: do create vnode on 0x%p, ", node);
	fvs_verbose("id: %d, size %d\n", id, size);

	fvs_native_write_r((void*)&node->id, id);
	fvs_native_write_r((void*)&node->size, size);
	fvs_native_write_r((void*)&node->status, FVS_VN_STATUS_EMPTY);

	fvs_end_write(base_addr);

	return RT_EOK;
}

static void vn_mark_written(
		rt_uint8_t *base_addr,
		struct fvs_vnode *node)
{
	fvs_begin_write(base_addr);

	fvs_verbose("FVS: mark 0x%p as written, ", node);
	fvs_verbose("id: %d, size %d\n", node->id, node->size);

	fvs_native_write_r((void*)&node->status, FVS_VN_STATUS_WRITTEN);

	fvs_end_write(base_addr);
}

static int vn_is_empty(struct fvs_vnode *node)
{
	return node->status == FVS_VN_STATUS_EMPTY;
}

static void vn_mark_invalid(
		rt_uint8_t *base_addr,
		struct fvs_vnode *node)
{
	fvs_begin_write(base_addr);

	fvs_verbose("FVS: mark 0x%p as invalid, ", node);
	fvs_verbose("id: %d, size %d\n", node->id, node->size);
	fvs_native_write_r((void*)&node->id, 0);

	fvs_end_write(base_addr);
}

static struct fvs_vnode *vn_find(
		rt_uint8_t *base_addr,
		size_t page_sz,
		fvs_id_t id,
		size_t size)
{
	struct fvs_vnode *node;

	ASSERT(base_addr);

	/* return the pointer to data if it has been created. */
	for (node = (struct fvs_vnode*)base_addr;
			node->id != FVS_END_OF_ID;
			node = vn_next(node)) {
		fvs_debug("FVS: vn_found node id:%d, size: %d\n", node->id, node->size);
		ASSERT((char*)node < (char*)(base_addr) + page_sz);

		if ((node->id) == id && node->size == size)
			return node;
	}
	return node;
}

size_t fvs_page_used_size(const struct fvs_block *blk)
{
	size_t s = 0;
	struct fvs_vnode *node;
	for (node = (struct fvs_vnode*)blk->pages;
			node->id != FVS_END_OF_ID;
			node = vn_next(node))
	{
		if (!vn_is_valid(node))
			continue;
		s += node->size;
	}
	return s;
}

rt_bool_t fvs_page_used(const struct fvs_block *blk)
{
    /* if there no blk we are using, the page would be never be written. */
	return blk_find_using(blk) != RT_NULL;
}

void *fvs_vnode_get(const struct fvs_block *blk, fvs_id_t id, size_t size)
{
	struct fvs_vnode *node;
	rt_uint8_t *base_addr;
	int s;

	ASSERT(blk);
	ASSERT(id);
	ASSERT(id != FVS_END_OF_ID);
	/* the size of the data should be multiple of fvs_native_t */
	ASSERT((size & (sizeof(fvs_native_t)-1)) == 0);

	base_addr = blk_find_using(blk);
	/* empty block, use the first page */
	if (base_addr == RT_NULL)
	{
		base_addr = blk->pages[0];
		blk_mark_as_using(base_addr, blk->size);
	}

	/* return the pointer to data if it has been created. */
	node = vn_find(base_addr, blk->size, id, size);
	if (node->id != FVS_END_OF_ID)
		return node+1;

	/* there is enough space to create the node we need. */
	if ((rt_uint8_t*)(node+1) + size <= base_addr + blk->size)
	{
		vn_do_create(base_addr, node, id, size);
		return node+1;
	}

	s = 0;
	for (node = (struct fvs_vnode*)base_addr;
			node->id != FVS_END_OF_ID;
			node = vn_next(node))
	{
		if (!vn_is_valid(node))
			continue;
		s += sizeof(*node) + node->size;
	}

	if (s + sizeof(*node) + size > blk->size)
		/* we run out of luck */
		return NULL;

	blk_roll_pages(blk);

	/* refresh the base_addr as the using page is changed */
	base_addr = blk_find_using(blk);
	ASSERT(base_addr);

	/* return the pointer to data if it has been created. */
	node = vn_find(base_addr, blk->size, id, size);
	ASSERT(node->id == FVS_END_OF_ID);
	ASSERT((rt_uint8_t*)(node+1) + size <= base_addr + blk->size);

	vn_do_create(base_addr, node, id, size);

	return node+1;
}

static rt_err_t vn_fill_data(
		rt_uint8_t *base_addr,
		struct fvs_vnode *node,
		void* data)
{
	ASSERT(node);
	ASSERT(data);
	ASSERT(node->status == FVS_VN_STATUS_EMPTY);

	fvs_verbose("FVS: fill node 0x%p with data from 0x%p, ",
			node, data);
	fvs_verbose("id: %d, size: %d\n", node->id, node->size);

	fvs_begin_write(base_addr);

	fvs_native_write_m(node+1, data, node->size);
	vn_mark_written(base_addr, node);

	fvs_end_write(base_addr);
	return RT_EOK;
}

void fvs_vnode_delete(const struct fvs_block *blk, fvs_id_t id, fvs_size_t size)
{
	struct fvs_vnode *node;
	rt_uint8_t *base_addr;

	ASSERT(blk);
	ASSERT(id != FVS_END_OF_ID);

	base_addr = blk_find_using(blk);
	if (!base_addr)
		return;

	node = vn_find(base_addr, blk->size, id, size);
	if (node->id == FVS_END_OF_ID)
		return;

	fvs_verbose("FVS: delete node 0x%p, ", node);
	fvs_verbose("id: %d, size: %d\n", id, size);

	vn_mark_invalid(base_addr, node);
}

rt_err_t fvs_vnode_write(const struct fvs_block *blk, fvs_id_t id, fvs_size_t size, void *data)
{
	struct fvs_vnode *node, *new_node;
	rt_uint8_t *base_addr;

	ASSERT(blk);
	ASSERT(id != FVS_END_OF_ID);

	base_addr = blk_find_using(blk);
	if (!base_addr)
		return -RT_ERROR;

	node = vn_find(base_addr, blk->size, id, size);
	if (node->id == FVS_END_OF_ID)
		return -RT_ERROR;

	/* find the fresh node if possible. */
	if (vn_is_empty(node)) {
		fvs_verbose("FVS: first write on node 0x%p, ", node);
		fvs_verbose("id: %d, size: %d\n", id, size);

		vn_fill_data(base_addr, node, data);

		return RT_EOK;
	}

	/* if the content does not change, there is nothing to do. */
	if (rt_memcmp(node+1, data, size) == 0) {
		fvs_verbose("FVS: write old data on node 0x%p\n", node);
		return RT_EOK;
	}

	new_node = vn_find(base_addr, blk->size, FVS_END_OF_ID, (fvs_size_t)-1);
	/* we need rewrite the whole blk since there is no free node left. The
	 * other page will be able to contain all the nodes since we have had that
	 * node in this page. (We will return -RT_ERROR on node not found.) */
	if ((rt_uint8_t*)(new_node+1) + size > base_addr + blk->size) {
		fvs_verbose("FVS: rewrite whole blk 0x%p, page 0x%p ", blk, base_addr);
		fvs_verbose("id: %d, size: %d\n", id, size);

		/* delete and write back */
		fvs_vnode_delete(blk, id, size);
		blk_roll_pages(blk);
		new_node = fvs_vnode_get(blk, id, size);
		ASSERT(new_node);
		/* fvs_vnode_get return the data address and we need to rewind it */
		new_node -= 1;
		vn_fill_data(base_addr, new_node, data);
	} else {
		fvs_verbose("FVS: write to new node:0x%p, old node:0x%p, ",
				new_node, node);
		fvs_verbose("id: %d, size: %d\n", id, size);

		/* create the new node before mark the old one as invalid */
		vn_do_create(base_addr, new_node, id, size);
		vn_fill_data(base_addr, new_node, data);
		vn_mark_invalid(base_addr, node);
	}
	return RT_EOK;
}

