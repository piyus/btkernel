#ifndef BTMOD_H
#define BTMOD_H

#include <linux/ioctl.h>

#define MODULE_NAME "bt"
#define MAJOR_NUM 100
#define INIT_BT  _IOR(MAJOR_NUM, 1, int)
#define STOP_BT  _IOR(MAJOR_NUM, 2, int)
#define TRANSLATE_IDT  _IOR(MAJOR_NUM, 3, int)
#define LOAD_NATIVE_IDT _IOR(MAJOR_NUM, 4, int)
#define TRANSLATE_DEBUG_CODE _IOR(MAJOR_NUM, 5, int)
#define BT_SET_MODE _IOR(MAJOR_NUM, 6, int)
#define READ_SHADOW_REG _IOR(MAJOR_NUM, 7, unsigned long)
#define INIT_EXTABLE _IOR(MAJOR_NUM, 8, unsigned long)

#define BT_MODE_NULL 0
#define BT_MODE_ICOUNT 1
#define BT_MODE_ICOUNT_OPT 2
#define BT_MODE_KADDRCHECK 3

struct extable_ioctl {
  unsigned len, addr;
};

#endif
