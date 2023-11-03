

#pragma once

#include <3ds.h>
#include <3ds/types.h>
#include <3ds/os.h>
#include <3ds/services/hid.h>
#include "menu.h"
#include "ifile.h"
#include "utils.h"

#define MAP_BASE_1		0x08000000
#define MAP_BASE_SIZE	  0x200000

#define MEMOP_FREE 		1
#define MEMOP_ALLOC 	3
#define MEMPERM_READ 	1
#define MEMPERM_WRITE 	2

typedef struct {
    char fileNames[128][128];
	char Path[512];
}Path_Firm;

u32 copy_bootonce(FS_ArchiveID sdmcArchive, const char *Path);
void chainloader(void);
