#ifndef __MISC_H__
#define __MISC_H__

#define ELF_SEGMENT_X	1
#define ELF_SEGMENT_W	2
#define ELF_SEGMENT_R	3

#define ALIGN_UP(_num, _align)    (_align * ((_num + _align - 1) / _align))

#endif
