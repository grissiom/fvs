#include <string.h>
#include <rtthread.h>

#include <stm32f10x.h>

#include "fvs.h"

#define ASSERT RT_ASSERT

#define FVS_QUIET  0
#define FVS_VERBOSE 1
#define FVS_DEBUG  2

#define FVS_LOG_LVL FVS_VERBOSE

static void dummy_log(const char *fmt, ...)
{
}

#if FVS_LOG_LVL >= FVS_VERBOSE
#define fvs_verbose rt_kprintf
#if FVS_LOG_LVL >= FVS_DEBUG
#define fvs_debug rt_kprintf
#else
#define fvs_debug dummy_log
#endif
#else
#define fvs_verbose dummy_log
#endif

void fvs_page_init(struct fvs_page *page, void *base_addr, size_t size)
{
	ASSERT(page);

	page->base_addr = base_addr;
	page->size      = size;
}

struct fvs_page *fvs_page_create(void *base_addr, size_t size)
{
	struct fvs_page *page = malloc(sizeof(*page));

	if (page == NULL)
		return NULL;

	fvs_page_init(page, base_addr, size);
	return page;
}

#define FVS_VN_STATUS_EMPTY   ((fvs_native_t)0xFFFF)
#define FVS_VN_STATUS_WRITTEN ((fvs_native_t)0x0)

/* the struct is reside on the flash in most of the times. The content of
 * base_addr of a fvs_page should a fvs_vnode. */
struct fvs_vnode {
	/* variable id */
	fvs_id_t id;
	/* the size of variable(without this head struct) */
	fvs_size_t size;
	fvs_native_t status;
	/* the data of variable */
	uint8_t data[0];
} __attribute__((packed));

static rt_err_t flash_write_uint16(void* addr, uint16_t data)
{
	FLASH_Status res;
	/*FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);*/
	fvs_debug("FVS: write %X to 0x%p\n", data, addr);
	res = FLASH_ProgramHalfWord((uint32_t)addr, data);
	if (res != FLASH_COMPLETE)
		return -RT_ERROR;

	return RT_EOK;
}

static rt_err_t vn_do_create(struct fvs_vnode *node, fvs_id_t id, fvs_size_t size)
{
	FLASH_UnlockBank1();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

	fvs_verbose("FVS: do create vnode on 0x%p, ", node);
	fvs_verbose("id: %d, size %d\n", id, size);

	flash_write_uint16(&node->id, id);
	flash_write_uint16(&node->size, size);
	flash_write_uint16(&node->status, FVS_VN_STATUS_EMPTY);

	FLASH_LockBank1();

	return RT_EOK;
}

static void vn_mark_written(struct fvs_vnode *node)
{
	FLASH_UnlockBank1();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

	fvs_verbose("FVS: mark 0x%p as written, ", node);
	fvs_verbose("id: %d, size %d\n", node->id, node->size);

	flash_write_uint16(&node->status, FVS_VN_STATUS_WRITTEN);

	FLASH_LockBank1();
}

static int vn_is_empty(struct fvs_vnode *node)
{
	return node->status == FVS_VN_STATUS_EMPTY;
}

static void vn_mark_invalid(struct fvs_vnode *node)
{
	FLASH_UnlockBank1();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

	fvs_verbose("FVS: mark 0x%p as invalid, ", node);
	fvs_verbose("id: %d, size %d\n", node->id, node->size);
	flash_write_uint16(&node->id, 0);

	FLASH_LockBank1();
}

static inline int vn_is_valid(struct fvs_vnode *node)
{
	return node->id != 0;
}

static inline struct fvs_vnode* vn_next(struct fvs_vnode *node)
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
		return &node->data;

	/* no enough space */
	if ((char*)&node->data + size > (char*)page->base_addr + page->size)
		return NULL;

	/* there is enough space to create the node we need. */
	vn_do_create(node, id, size);

	return node->data;
}

static rt_err_t vn_fill_data(struct fvs_vnode *node, void* data)
{
	fvs_size_t i;
	uint8_t *pdata = data;

	ASSERT(node);
	ASSERT(data);
	ASSERT(node->status == FVS_VN_STATUS_EMPTY);

	fvs_verbose("FVS: fill node 0x%p with data from 0x%p, ",
		  node, data);
	fvs_verbose("id: %d, size: %d\n", node->id, node->size);

	FLASH_UnlockBank1();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

	for (i = 0; i < node->size; i += 2)
		flash_write_uint16((char*)&node->data + i,
				   *(uint16_t*)(pdata + i));
	vn_mark_written(node);

	FLASH_LockBank1();
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

	vn_mark_invalid(node);
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

		vn_fill_data(node, data);

		return RT_EOK;
	}

	new_node = vn_find(page, FVS_END_OF_ID, -1);
	/* we need rewrite the whole page since there is no free node
	 * left. The current page will be able to contain all the nodes since
	 * we have had that node in this page. (We will return -RT_ERROR on
	 * node not found.) */
	if ((char*)&new_node->data + size > (char*)page->base_addr + page->size) {
		uint8_t *buf;
		size_t buf_sz, i;

		fvs_verbose("FVS: rewrite whole page 0x%p, ", page->base_addr);
		fvs_verbose("id: %d, size: %d\n", id, size);

		/* copy the vs to ram buffer. */
		buf_sz = (size_t)vn_lmap(node, vn_acc_size_all, 0);
		buf = malloc(buf_sz);
		if (buf == NULL) {
			FLASH_LockBank1();
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
				memcpy(&node->data, data, size);
				break;
			}
			vn_mark_written(node);
		}

		FLASH_UnlockBank1();
		FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

		/* erase the page and write back. */
		FLASH_ErasePage((uint32_t)page->base_addr);
		for (i = 0; i < buf_sz; i += 2) {
			flash_write_uint16((char*)page->base_addr + i,
					*(uint16_t*)((char*)buf + i));
		}

		FLASH_LockBank1();

		free(buf);
	} else {
		fvs_verbose("FVS: write to new node:0x%p, old node:0x%p, ",
				new_node, node);
		fvs_verbose("id: %d, size: %d\n", id, size);

		/* mark the current node invalid and write it to the
		 * new node. */
		vn_mark_invalid(node);
		vn_do_create(new_node, id, size);
		vn_fill_data(new_node, data);
	}
	return RT_EOK;
}

