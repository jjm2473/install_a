#include <sys/ioctl.h>
#include <rtk_common.h>
#include <rtk_def.h>
#include <rtk_mtd.h>
#include <rtk_fwdesc.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#define READ_SETTING_TXT

extern char gsettingpath[128];
extern int pc_get_parameter_string(const char* keystring, char *string, int str_maxlen);
extern int pc_get_parameter_long_value(const char* keystring, unsigned long long *keyval);
extern int pc_get_parameter_value(const char* keystring, unsigned int *keyval);

extern "C" {
int MCP_AES_DataEncryption(unsigned char EnDe, unsigned char Mode, unsigned char Key[16], unsigned char IV[16], unsigned char* pDataIn, unsigned char* pDataOut, unsigned long Len)
{
	printf("----- %s -----\n", __func__);
	*pDataOut = 0xab;
	return 0;
}
} // end of extern "C"

int ioctl_pc(int d, int request, ...)
{
	mtd_info_user *p_mtd;
	int *p_int;
	char str_tmp[32];
	unsigned long long *blkSize64;
	unsigned long long flash_reserved_area_size;
	//unsigned long *blkSize;
	va_list vl;

	printf("----- %s ----- ", __func__);

	switch(request) {
	case MEMGETINFO:
		printf("MEMGETINFO\n");
		va_start(vl,request);
		p_mtd=va_arg(vl,mtd_info_user*);
		va_end(vl);
#ifdef READ_SETTING_TXT
		if( ! gsettingpath[0] ) {
			sprintf(gsettingpath, "%s", DEFAULT_SETTING_FILE );
			printf("setting file: %s\n", gsettingpath);
		}
		pc_get_parameter_string("boot_flash", str_tmp, sizeof(str_tmp));

		if (!strcmp(str_tmp, "nand")) {
		   p_mtd->type = MTD_NANDFLASH;
		   pc_get_parameter_long_value("flash_size", (unsigned long long*)&p_mtd->size);
		   pc_get_parameter_value("flash_oob_size", &p_mtd->oobblock);
		   pc_get_parameter_value("flash_erase_size", &p_mtd->erasesize);
		   #ifndef RESERVED_AREA
		   //flash_reserved_area_size is a block less than the real reserved area
		   flash_reserved_area_size = SIZE_ALIGN_BOUNDARY_LESS(p_mtd->size/20, p_mtd->erasesize);
		   p_mtd->size -= flash_reserved_area_size;
		   #endif
		}
		else {
		   printf("[ERROR]mtd device and setting.txt not match\n\n");
		   assert(0);
		}
		break;
#else
		p_mtd->size = 8*1024*1024*1024ULL;
		p_mtd->oobblock = 8*1024;	// page size, write size
		p_mtd->erasesize = 2*1024*1024;// block size
		p_mtd->type = MTD_NANDFLASH;
	#ifndef RESERVED_AREA
		flash_reserved_area_size = SIZE_ALIGN_BOUNDARY_LESS(p_mtd->size/20, p_mtd->erasesize);
		p_mtd->size -= flash_reserved_area_size;
	#endif
		break;
#endif
	case GETRBAPERCENTAGE:
		printf("GETRBAPERCENTAGE\n");
		va_start(vl,request);
		p_int=va_arg(vl,int*);
		va_end(vl);
		*p_int = 5;
		break;
	case BLKGETSIZE64:
		printf("BLKGETSIZE64\n");
		va_start(vl,request);
		blkSize64=va_arg(vl,unsigned long long*);
		va_end(vl);
#ifdef READ_SETTING_TXT
		if( ! gsettingpath[0] ) {
			sprintf(gsettingpath, "%s", DEFAULT_SETTING_FILE );
			printf("setting file: %s\n", gsettingpath);
		}
		pc_get_parameter_string("boot_flash", str_tmp, sizeof(str_tmp));

		if (!strcmp(str_tmp, "emmc")) {
		   pc_get_parameter_long_value("flash_size", blkSize64);
		}
		else {
		   printf("[ERROR]mtd device and setting.txt not match\n\n");
		   assert(0);
		}
		break;
#else

		*blkSize64 = 7376*1024*1024ULL;	// 8G
		//*blkSize64 = 3656*1024*1024ULL;	// 4G
#endif
		break;
		/*
	case BLKGETSIZE:
		printf("BLKGETSIZE\n");
		va_start(vl,request);
		blkSize=va_arg(vl,unsigned long*);
		va_end(vl);
		*blkSize = 1024*1024*1024ULL;
		break;
		*/
	case BLKRRPART:
		printf("BLKRRPART\n");
		break;

	default:
		printf("NOT SUPPORTED(%d)\n", request);
		return -1;
	}

	return 0;
}
