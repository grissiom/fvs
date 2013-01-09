#include "fvs.h"

#define   BOOT_TIME_ID    1
#define   USER_INPUT_ID   2

static struct fvs_page the_page;

uint32_t boot_time;
uint16_t user_input;

void init(void)
{
	fvs_page_init(&the_page, (void*)0x0807E000, 512);

	boot_time = *(uint32_t*)fvs_vnode_get(&the_page,
			                      BOOT_TIME_ID, sizeof(boot_time));
	if (boot_time == 0xFFFF) {
		boot_time = 1;
	} else {
		boot_time++;
	}
	fvs_vnode_write(&the_page, BOOT_TIME_ID, sizeof(boot_time), &boot_time);

	user_input = *(uint32_t*)fvs_vnode_get(&the_page,
			                       USER_INPUT_ID, sizeof(user_input));
	if (user_input == 0xFFFF) {
		// do something
	} else {
		// do other thing
	}

}
