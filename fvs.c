/** Flash Variable System
 *
 * Implementations. This is part of FVS project
 */

#include <string.h>
#include <rtthread.h>

#include "fvs.h"

#define malloc  rt_malloc
#define free    rt_free
#define realloc rt_realloc

#define ASSERT RT_ASSERT

#define FVS_VN_STATUS_EMPTY   ((fvs_native_t)-1)
#define FVS_VN_STATUS_WRITTEN ((fvs_native_t)0x0)

/* the struct is reside on the flash in most of the times. The content of
 * base_addr of a fvs_page should a fvs_vnode. */
struct fvs_vnode {
	/* variable id */
	fvs_id_t id;
	/* the size of variable(without this head struct) */
	fvs_size_t size;
	fvs_native_t status;
	/* there should be size of bytes of data followed */
} __attribute__((packed));

void fvs_page_init(struct fvs_page *page, void *base_addr, size_t size)
{
	ASSERT(page);

	page->base_addr = base_addr;
	/* FVS assume we are in a memory space filled with 0xFF. So we have to
	 * preserve one information block to hold at least the FVS_END_OF_ID */
	page->size = size - sizeof(struct fvs_vnode);
}

struct fvs_page *fvs_page_create(void *base_addr, size_t size)
{
	struct fvs_page *page = malloc(sizeof(*page));

	if (page == NULL)
		return NULL;

	fvs_page_init(page, base_addr, size);
	return page;
}

static rt_err_t vn_do_create(
		struct fvs_page *page,
		struct fvs_vnode *node,
		fvs_id_t id,
		fvs_size_t size)
{
	fvs_begin_write(page);

	fvs_verbose("FVS: do create vnode on 0x%p, ", node);
	fvs_verbose("id: %d, size %d\n", id, size);

	fvs_native_write((void*)&node->id, id);
	fvs_native_write((void*)&node->size, size);
	fvs_native_write((void*)&node->status, FVS_VN_STATUS_EMPTY);

	fvs_end_write(page);

	return RT_EOK;
}

static void vn_mark_written(
		struct fvs_page *page,
		struct fvs_vnode *node)
{
	fvs_begin_write(page);

	fvs_verbose("FVS: mark 0x%p as written, ", node);
	fvs_verbose("id: %d, size %d\n", node->id, node->size);

	fvs_native_write((void*)&node->status, FVS_VN_STATUS_WRITTEN);

	fvs_end_write(page);
}

static int vn_is_empty(struct fvs_vnode *node)
{
	return node->status == FVS_VN_STATUS_EMPTY;
}

static void vn_mark_invalid(
		struct fvs_page *page,
		struct fvs_vnode *node)
{
	fvs_begin_write(page);

	fvs_verbose("FVS: mark 0x%p as invalid, ", node);
	fvs_verbose("id: %d, size %d\n", node->id, node->size);
	fvs_native_write((void*)&node->id, 0);

	fvs_end_write(page);
}

rt_inline int vn_is_valid(struct fvs_vnode *node)
{
	return node->id != 0;
}

rt_inline struct fvs_vnode* vn_next(struct fvs_vnode *node)
{
	return (struct fvs_vnode*)((char*)node + sizeof(*node) + node->size);
}

static struct fvs_vnode *vn_find(struct fvs_page *page, fvs_id_t id, size_t size)
{
	struct fvs_vnode *node;

	ASSERT(page);

	/* return the pointer to data if it has been created. */
	for (node = (struct fvs_vnode*)page->base_addr;
	     node->id != FVS_END_OF_ID;
	     node = vn_next(node)) {
		fvs_debug("FVS: vn_found node id:%d, size: %d\n", node->id, node->size);
		ASSERT((char*)node < (char*)(page->base_addr) + page->size);

		if ((node->id) == id && node->size == size)
			return node;
	}
	return node;
}

/* return a result in void* and receive (current_vnode, last_result)*/
typedef void* (*vn_lmap_func)(struct fvs_vnode*, void*);
/* linear map, not the real map, since it is not parallel. */
static void *vn_lmap(struct fvs_vnode *cur_node, vn_lmap_func func, void *init_res)
{
	if (cur_node->id == FVS_END_OF_ID)
		return init_res;

	return vn_lmap(vn_next(cur_node), func, func(cur_node, init_res));
}

static void *vn_acc_size(struct fvs_vnode *node, void *psize)
{
	if (!vn_is_valid(node))
		return psize;
	return (void*)((size_t)psize + node->size);
}

static void *vn_acc_size_all(struct fvs_vnode *node, void *psize)
{
	if (!vn_is_valid(node))
		return psize;
	return (void*)((size_t)psize + node->size + sizeof(struct fvs_vnode));
}

static void *vn_cpy2ram(struct fvs_vnode *node, void* buf)
{
	size_t sz = sizeof(*node) + node->size;

	if (!vn_is_valid(node))
		return buf;

	fvs_verbose("FVS: cpy node 0x%p to RAM 0x%p, ", node, buf);
	fvs_verbose("id: %d, size %d\n", node->id, node->size);
	memcpy(buf, node, sz);
	return (char*)buf + sz;
}

size_t fvs_page_used_size(struct fvs_page *page)
{
	return (size_t)vn_lmap(page->base_addr, vn_acc_size, 0);
}

void *fvs_vnode_get(struct fvs_page *page, fvs_id_t id, size_t size)
{
	struct fvs_vnode *node;

	ASSERT(page);
	ASSERT(id != FVS_END_OF_ID);

	/* return the pointer to data if it has been created. */
	node = vn_find(page, id, size);
	if (node->id != FVS_END_OF_ID)
		return node+1;

	/* no enough space */
	if ((char*)(node+1) + size > (char*)page->base_addr + page->size)
		return NULL;

	/* there is enough space to create the node we need. */
	vn_do_create(page, node, id, size);

	return node+1;
}

static rt_err_t vn_fill_data(
		struct fvs_page *page,
		struct fvs_vnode *node,
		void* data)
{
	fvs_size_t i;
	uint8_t *pdata = data;

	ASSERT(node);
	ASSERT(data);
	ASSERT(node->status == FVS_VN_STATUS_EMPTY);

	fvs_verbose("FVS: fill node 0x%p with data from 0x%p, ",
		  node, data);
	fvs_verbose("id: %d, size: %d\n", node->id, node->size);

	fvs_begin_write(page);

	for (i = 0; i < node->size; i += sizeof(fvs_native_t))
		fvs_native_write((char*)(node+1) + i,
				   *(fvs_native_t*)(pdata + i));
	vn_mark_written(page, node);

	fvs_end_write(page);
	return RT_EOK;
}

void fvs_vnode_delete(struct fvs_page *page, fvs_id_t id, fvs_size_t size)
{
	struct fvs_vnode *node;

	ASSERT(page);
	ASSERT(id != FVS_END_OF_ID);

	node = vn_find(page, id, size);
	if (node->id == FVS_END_OF_ID)
		return;

	fvs_verbose("FVS: delete node 0x%p, ", node);
	fvs_verbose("id: %d, size: %d\n", id, size);

	vn_mark_invalid(page, node);
}

rt_err_t fvs_vnode_write(struct fvs_page *page, fvs_id_t id, fvs_size_t size, void *data)
{
	struct fvs_vnode *node, *new_node;

	ASSERT(page);
	ASSERT(id != FVS_END_OF_ID);

	node = vn_find(page, id, size);
	if (node->id == FVS_END_OF_ID)
		return -RT_ERROR;

	/* find the fresh node if possible. */
	if (vn_is_empty(node)) {
		fvs_verbose("FVS: first write on node 0x%p, ", node);
		fvs_verbose("id: %d, size: %d\n", id, size);

		vn_fill_data(page, node, data);

		return RT_EOK;
	}

	new_node = vn_find(page, FVS_END_OF_ID, (fvs_size_t)-1);
	/* we need rewrite the whole page since there is no free node
	 * left. The current page will be able to contain all the nodes since
	 * we have had that node in this page. (We will return -RT_ERROR on
	 * node not found.) */
	if ((char*)(new_node+1) + size > (char*)page->base_addr + page->size) {
		uint8_t *buf;
		size_t buf_sz, i;

		fvs_verbose("FVS: rewrite whole page 0x%p, ", page->base_addr);
		fvs_verbose("id: %d, size: %d\n", id, size);

		/* copy the vs to ram buffer. */
		buf_sz = (size_t)vn_lmap(node, vn_acc_size_all, 0);
		buf = malloc(buf_sz);
		if (buf == NULL) {
			fvs_end_write(page);
			return -RT_ENOMEM;
		}
		buf = memset(buf, -1, buf_sz);
		fvs_verbose("FVS: malloced %d byte buf\n", buf_sz);
		vn_lmap(node, vn_cpy2ram, buf);

		/* refresh the node to be written. */
		for (node = (struct fvs_vnode*)buf;
		     (char*)node < (char*)buf + buf_sz;
		     node = vn_next(node)) {
			if (node->id == id && node->size == size) {
				memcpy(node+1, data, size);
				break;
			}
			vn_mark_written(page, node);
		}

		fvs_begin_write(page);

		/* erase the page and write back. */
		fvs_erase_page(page);
		for (i = 0; i < buf_sz; i += sizeof(fvs_native_t)) {
			fvs_native_write((char*)page->base_addr + i,
					*(fvs_native_t*)((char*)buf + i));
		}

		fvs_end_write(page);

		free(buf);
	} else {
		fvs_verbose("FVS: write to new node:0x%p, old node:0x%p, ",
				new_node, node);
		fvs_verbose("id: %d, size: %d\n", id, size);

		/* mark the current node invalid and write it to the
		 * new node. */
		vn_mark_invalid(page, node);
		vn_do_create(page, new_node, id, size);
		vn_fill_data(page, new_node, data);
	}
	return RT_EOK;
}

