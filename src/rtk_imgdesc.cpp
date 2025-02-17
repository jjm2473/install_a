#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

#include <rtk_common.h>
#include <rtk_fwdesc.h>
#include <rtk_mtd.h>
#include <rtk_imgdesc.h>
#include <rtk_config.h>
#include <rtk_parameter.h>
#include <rtk_def.h>
#include <rtk_boottable.h>
#include <rtk_factory.h>

struct t_DYNAMIC_PARTDESC dynamic_part_tbl[NUM_SUPPORT_DYNAMIC_PART_TBL];


#ifdef ENABLE_ERASE_CHECK
struct t_PARTDESC rtk_part_list[NUM_RTKPART]=
{
    // FS_TYPE_YAFFS2 need nandwrite &flash_erase
    // LABEL, partition_name, mount_point, filesystem, filename, min_size, bErase

    // default setting
    {FW_ROOTFS        ,"rootfs"             ,""                  ,FS_TYPE_UBIFS   ,""               ,0, 1},
    {FW_USR_LOCAL_ETC ,"etc"                ,""                  ,FS_TYPE_UBIFS    ,""               ,0, 1},
    {FW_CACHE         ,"cache"              ,""                  ,FS_TYPE_UBIFS    ,""               ,0, 1},
    {FW_DATA          ,"data"               ,""                  ,FS_TYPE_UBIFS   ,""                ,0, 1},
    {FW_SYSTEM        ,"system"             ,""                  ,FS_TYPE_UBIFS   ,""                ,0, 1},
    {FW_RES           ,"res"                ,""                  ,FS_TYPE_UBIFS   ,""                ,0, 1},
    // reserve for configuration.xml
    // reserve for config.txt
    {P_PARTITION1     ,""                   ,""                  ,FS_TYPE_NONE     ,""               ,0, 1},
    {P_PARTITION2     ,""                   ,""                  ,FS_TYPE_NONE     ,""               ,0, 1},
    {P_PARTITION3     ,""                   ,""                  ,FS_TYPE_NONE     ,""               ,0, 1},
    {P_PARTITION4     ,""                   ,""                  ,FS_TYPE_NONE     ,""               ,0, 1},
    {P_PARTITION5     ,""                   ,""                  ,FS_TYPE_NONE     ,""               ,0, 1},
    {P_PARTITION6     ,""                   ,""                  ,FS_TYPE_NONE     ,""               ,0, 1},
    {P_PARTITION7     ,""                   ,""                  ,FS_TYPE_NONE     ,""               ,0, 1},
    {P_PARTITION8     ,""                   ,""                  ,FS_TYPE_NONE     ,""               ,0, 1},
};
#else
struct t_PARTDESC rtk_part_list[NUM_RTKPART]=
{
    // FS_TYPE_YAFFS2 need nandwrite &flash_erase
    // LABEL, partition_name, mount_point, filesystem, filename, min_size

    // default setting
    {FW_ROOTFS        ,"rootfs"             ,""                  ,FS_TYPE_UBIFS   ,""               ,0},
    {FW_USR_LOCAL_ETC ,"etc"                ,""                  ,FS_TYPE_UBIFS   ,""               ,0},
    {FW_CACHE         ,"cache"              ,""                  ,FS_TYPE_UBIFS   ,""                ,0},
    {FW_DATA          ,"data"               ,""                  ,FS_TYPE_UBIFS   ,""                ,0},
    {FW_SYSTEM        ,"system"             ,""                  ,FS_TYPE_UBIFS   ,""                ,0},
    {FW_RES           ,"res"                ,""                  ,FS_TYPE_UBIFS   ,""                ,0},
    // reserve for configuration.xml
    // reserve for config.txt
    {P_PARTITION1     ,""                   ,""                  ,FS_TYPE_NONE     ,""               ,0},
    {P_PARTITION2     ,""                   ,""                  ,FS_TYPE_NONE     ,""               ,0},
    {P_PARTITION3     ,""                   ,""                  ,FS_TYPE_NONE     ,""               ,0},
    {P_PARTITION4     ,""                   ,""                  ,FS_TYPE_NONE     ,""               ,0},
    {P_PARTITION5     ,""                   ,""                  ,FS_TYPE_NONE     ,""               ,0},
    {P_PARTITION6     ,""                   ,""                  ,FS_TYPE_NONE     ,""               ,0},
    {P_PARTITION7     ,""                   ,""                  ,FS_TYPE_NONE     ,""               ,0},
    {P_PARTITION8     ,""                   ,""                  ,FS_TYPE_NONE     ,""               ,0},
};
#endif
struct t_PARTDESC *rtk_part_list_sort[NUM_RTKPART];


#if 0
struct t_FWDESC rtk_fw_list[]=
{
   {FW_KERNEL         , "linuxKernel"      ,"package1/vmlinux.develop.simple.tv.macarthur.nonet.bin.lzma" , 0x80100000},
   {FW_AUDIO          , "audioKernel"      ,"package1/bluecore.audio.lzma"                                , 0x81c00000},
   {FW_VIDEO          , "videoKernel"      ,"package1/bluecore.video.lzma"                                , 0x81e00000},
   {FW_AUDIO_BOOTFILE , "audioFile"        ,"package1/bootfile.audio"                                     , 0x83000000},
   {FW_VIDEO_BOOTFILE , "videoFile"        ,"package1/bootfile.video"                                     , 0x83100000},
};
#else
struct t_FWDESC rtk_fw_list[]=
{
	// LABEL, firmware_name, filename, target_address

   {FW_KERNEL         , "linuxKernel"      , ""                        , 0x0},
#ifdef TEE_ENABLE
   {FW_TEE         , "tee"      , ""                        , 0x0},
   {FW_BL31         , "BL31"      , ""                        , 0x0},
#endif
   {FW_RESCUE_DT      , "rescueDT"		   , ""						, 0x0},
   {FW_KERNEL_DT      , "kernelDT"		   , ""						, 0x0},
   {FW_RESCUE_ROOTFS  , "rescueRootFS"   	, ""					 , 0x0},
   {FW_KERNEL_ROOTFS  , "kernelRootFS" 		, ""					 , 0x0},
#ifdef NAS_ENABLE
   {FW_NAS_KERNEL         , "NASrescueKernel"      , ""                        , 0x0},
   {FW_NAS_RESCUE_DT      , "NASrescueDT"		   , ""						, 0x0},
   {FW_NAS_RESCUE_ROOTFS  , "NASrescueRootFS"   	, ""					 , 0x0},
#ifdef HYPERVISOR
   {FW_XEN                , "XenOS"                 , ""                    , 0x0},
#endif
#endif
#ifdef NFLASH_LAOUT
   {FW_GOLD_KERNEL		 , "GOLDKernel"	  , ""						  , 0x0},
   {FW_GOLD_RESCUE_DT 	 , "GOLDrescueDT"		  , ""					   , 0x0},
   {FW_GOLD_RESCUE_ROOTFS  , "GOLDRootFS"	   , "" 					, 0x0},
   {FW_GOLD_AUDIO        , "GOLDaudio"	   , "" 					, 0x0},
   {FW_UBOOT 			 , "UBOOT"	   , "" 					, 0x0},
#endif

   {FW_AUDIO          , "audioKernel"      , ""                        , 0x0},
   {FW_VIDEO          , "videoKernel"      , ""                        , 0x0},
   {FW_VIDEO2         , "video2Kernel"     , ""                        , 0x0},
   {FW_ECPU           , "ecpuKernel"       , ""                        , 0x0},
   {FW_AUDIO_BOOTFILE , "audioFile"        , ""                        , 0x0},
   {FW_IMAGE_BOOTFILE , "imageFile"        , ""                        , 0x0},
   {FW_VIDEO_BOOTFILE , "videoFile"        , ""                        , 0x0},
   {FW_AUDIO_CLOGO1 , "audioFile1"         , ""                        , 0x0},
   {FW_IMAGE_CLOGO1 , "imageFile1" 	       , "" 					   , 0x0},
   {FW_VIDEO_CLOGO1 , "videoFile1"        , ""                        , 0x0},
   {FW_AUDIO_CLOGO2 , "audioFile2"        , ""                        , 0x0},
   {FW_IMAGE_CLOGO2 , "imageFile2" 	      , "" 					      , 0x0},
   {FW_VIDEO_CLOGO2 , "videoFile2"        , ""                        , 0x0},
};
#endif

struct t_UTILDESC rtk_util_list[]=
{
    // LABEL,               file pattern        bin file path
   {UTIL_FLASHERASE,        "flash_erase",      FLASHERASE_BIN},
   {UTIL_NANDWRITE,         "nandwrite",        NANDWRITE_BIN},
   {UTIL_UBIFORMAT,         "ubiformat",        UBIFORMAT_BIN},
   {UTIL_MKE2FS,            "mke2fs",           MKE2FS_BIN},
   {UTIL_RESIZE2FS,         "resize2fs",        RESIZE2FS_BIN},
   {UTIL_MKYAFFS2,          "mkyaffs2image",    YAFFS2_BIN},
   {UTIL_MKJFFS2,           "mkfs.jffs2",       JFFS2_BIN}
};

struct t_KEYDESC rtk_key_list[]=
{
    // LABEL,                   file pattern                                 bin file path
   {KEY_RSA_PUBLIC,      "rsa_key_2048.pem.bin.rev",    KEY_RSA_BIN},
   {KEY_AES_KEY_SEED,    "aes_128bit_seed.bin",         KEY_KSEED_BIN},
   {KEY_AES_KEY,         "aes_128bit_key.bin",          KEY_K_BIN},
   {KEY_AES_KEY1,        "aes_128bit_key_1.bin",        KEY_K1_BIN},
   {KEY_AES_KEY2,        "aes_128bit_key_2.bin",        KEY_K2_BIN},
   {KEY_AES_KEY3,        "aes_128bit_key_3.bin",        KEY_K3_BIN},
};

#if defined(NAS_ENABLE) && defined(CONFIG_BOOT_FROM_SPI)
#define NOR_NORMAL_DTS_START_ADDR	0x30000
#define NOR_RESCUE_DTS_START_ADDR	0x40000
#define NOR_KERNEL_START_ADDR		0x50000
#endif

#ifdef ENABLE_ERASE_CHECK
int nErasePartCnt = 0;
int nErasePartIdxList[8] = {0};
#endif

#ifdef DEBUG_FUNC
void dump_rtk_part_list(struct t_PARTDESC* _rtk_part_list)
{
   int i;

   printf("\r\n[Part list]\r\n");
   printf("%-5s  %-10s%-30s%-3s  %-40s%-8s\r\n"
      , " efs"
      , "part_name"
      , "mount_point"
      , "efs"
      , "filename"
      , "min_size");
   for(i = 0; i < NUM_RTKPART; i++)
   {
      printf(" (%02d)  %-10s%-30s(%d)  %-40s%10llu\r\n"
         , _rtk_part_list[i].efwtype
         , _rtk_part_list[i].partition_name
         , _rtk_part_list[i].mount_point
         , _rtk_part_list[i].efs
         , _rtk_part_list[i].filename
         , _rtk_part_list[i].min_size);
   }
   printf("\r\n\r\n");
}

static void dump_rtk_fw_list(void)
{
   int i;

   printf("\r\n[FW list]\r\n");
   printf("%-5s  %-15s %-60s %-10s\r\n"
      , " efs"
      , "fw_name"
      , "filename"
      , "target");
   for(i = 0; i < (int)(sizeof(rtk_fw_list)/sizeof(struct t_FWDESC)); i++)
   {
      printf(" (%02d)  %-15s %-60s 0x%08x\r\n"
         , rtk_fw_list[i].efwtype
         , rtk_fw_list[i].firmware_name
         , rtk_fw_list[i].filename
         , rtk_fw_list[i].target);
   }
   printf("\r\n\r\n");
}
#endif	//DEBUG_FUNC


// find part
//
struct t_PARTDESC* find_empty_part(struct t_PARTDESC* _rtk_part_list)
{
   int i;
   // return empty if there no exist
   for(i = 0; i < NUM_RTKPART; i++)
      if(0 == strlen(_rtk_part_list[i].partition_name))
         return &_rtk_part_list[i];
   return NULL;
}

struct t_PARTDESC* find_part_by_part_name(struct t_PARTDESC* _rtk_part_list,const char* part_name)
{
   int i;
   // return element directly if exist
   for(i = 0; i < NUM_RTKPART; i++)
      if(0 == strcmp(_rtk_part_list[i].partition_name, part_name))
         return &(_rtk_part_list[i]);
   // return empty if there no exist
   for(i = 0; i < NUM_RTKPART; i++)
      if(0 == strlen(_rtk_part_list[i].partition_name))
         return &(_rtk_part_list[i]);
   return NULL;
}

struct t_PARTDESC* find_part_by_efwtype(struct t_PARTDESC* _rtk_part_list,enum FWTYPE efwtype)
{
   int i;
   for(i = 0;i<NUM_RTKPART; i++)
      if(efwtype == _rtk_part_list[i].efwtype)
         return &_rtk_part_list[i];
   return NULL;
}

static struct t_PARTDESC* find_part_by_filename(struct t_PARTDESC* _rtk_part_list,const char* fn)
{
   int i;
   for(i = 0;i < NUM_RTKPART; i++)
      if(strlen(_rtk_part_list[i].filename) != 0 && 0 == strcmp(fn, _rtk_part_list[i].filename))
         return &_rtk_part_list[i];
   return NULL;
}

// find fw
//
struct t_FWDESC* find_fw_by_fw_name(const char* fw_name)
{
   int i;
   // return element directly if exist
   for(i = 0; i < (int)(sizeof(rtk_fw_list)/sizeof(struct t_FWDESC)); i++)
      if(0 == strcmp(rtk_fw_list[i].firmware_name, fw_name))
         return &rtk_fw_list[i];
   return NULL;
}

struct t_FWDESC* find_fw_by_efwtype(enum FWTYPE efwtype)
{
   int i;
   for(i = 0; i < (int)(sizeof(rtk_fw_list)/sizeof(struct t_FWDESC)); i++)
      if(efwtype == rtk_fw_list[i].efwtype)
         return &rtk_fw_list[i];
   return NULL;
}

static struct t_FWDESC* find_fw_by_filename(const char* fn)
{
   int i;
   for(i = 0; i < (int)(sizeof(rtk_fw_list)/sizeof(struct t_FWDESC)); i++)
      if((strlen(rtk_fw_list[i].filename) == strlen(fn)) && !strncmp(fn, rtk_fw_list[i].filename, strlen(rtk_fw_list[i].filename)))
         return &rtk_fw_list[i];
   return NULL;
}

const char* inv_by_fwtype(enum FWTYPE efwtype)
{
   int i;
   // firmware name
   for(i = 0; i < (int)(sizeof(rtk_fw_list)/sizeof(struct t_FWDESC)); i++)
      if(efwtype == rtk_fw_list[i].efwtype)
         return rtk_fw_list[i].firmware_name;
   // partition name
   for(i = 0; i < (int)(sizeof(rtk_part_list)/sizeof(struct t_PARTDESC)); i++)
      if(efwtype == rtk_part_list[i].efwtype && strlen(rtk_part_list[i].partition_name) != 0)
         return rtk_part_list[i].partition_name;
   switch(efwtype)
   {
      case FW_BOOTCODE:
         return "bootcode";
      case FW_FACTORY:
         return "factory";
      case FW_FW_TBL:
         return "fw table";
      case FW_RESCUE:
         return "Rescue";
      case P_SROOTFS:
         return "srootfs";
      case FW_AUDIO_CLOGO1:
         return "caLogo1";
      case FW_VIDEO_CLOGO1:
         return "cvLogo1";
      case FW_AUDIO_CLOGO2:
         return "caLogo2";
      case FW_VIDEO_CLOGO2:
         return "cvLogo2";
      case FW_P_FREE_SPACE:
         return "FREE SPACE";
      case FW_P_SSU_WORKING:
         return "SSU WORKING";
	  case FW_SE_STORAGE:
         return "SE_STORAGE";
      case FW_GOLD_FW_TBL:
         return "GoldFwTable";
	  default:
		return "Unkonwn";
   }
   return "Unkonwn";
}

#define PAGE_ALIGN(x)      (((x) + 0xFE) & ~0xFF)

static int check_spi_bootcode_size(struct t_rtkimgdesc* prtkimg)
{
   //Check bootcode & rescue from install.img file
   char cmd[128] = {0}, path[128] = {0}, rescue_file_name[128] = {0};
   unsigned int file_len = 0, bootcode_len = 0;
   int ret = 0;

   snprintf(path, sizeof(path), "%s.tar", BOOTCODE_TEMP);
   if(rtk_extract_file(prtkimg, &prtkimg->bootloader_tar, path) < 0)
   {
      install_fail("Can't extract bootcode\r\n");
      return -_eFILL_RTK_IMGDESC_LAYOUT_FAIL_BOOTCODE_EXTRACT;
   }

   snprintf(cmd, sizeof(cmd), "rm -rf %s;mkdir -p %s;tar -xf %s.tar -C %s/", BOOTCODE_TEMP, BOOTCODE_TEMP, BOOTCODE_TEMP, BOOTCODE_TEMP);

   if((ret = rtk_command(cmd , __LINE__, __FILE__, 0)) < 0)
   {
      install_fail("untar %s.tar fail\r\n", BOOTCODE_TEMP);
      return -_eFILL_RTK_IMGDESC_LAYOUT_FAIL_BOOTCODE_EXTRACT;
   }

   //calculate bootcode size from bootloader.tar
   //starts from hw_setting default start address
   //bootcode+size = hw_setting_start_addr + hw_setting + 4 + hash_target.bin
   // 4 : An unsigned int variable to store length of hash_target.bin
   bootcode_len = NOR_HWSETTING_START_ADDR;
   snprintf(path, sizeof(path), "%s/%s", BOOTCODE_TEMP, HWSETTING_FILENAME);
   ret = rtk_get_size_of_file(path, &file_len);
   //20121119 HWSETTING is alligned to 256bytes in bootcode, fix it to meet it
   bootcode_len += PAGE_ALIGN(file_len);
   install_debug("hwsetting size = 0x%x, alligned to 0x%x\r\n", file_len, PAGE_ALIGN(file_len));
   snprintf(path, sizeof(path), "%s/%s", BOOTCODE_TEMP, HASHTARGET_FILENAME);
   ret = rtk_get_size_of_file(path, &file_len);
   bootcode_len += file_len + 4;
   if ((ret = rtk_find_file_in_dir(BOOTCODE_TEMP, FIND_RESCUE_FILENAME, rescue_file_name, sizeof(rescue_file_name))) == 0) {
      snprintf(path, sizeof(path), "%s/%s", BOOTCODE_TEMP, rescue_file_name);
      rtk_get_size_of_file(path, &(prtkimg->rescue_size));
   } else {
      prtkimg->rescue_size = 0;
      install_debug("Can't find file, rescue_size = 0\r\n");
   }

   install_debug("\r\nfrom system parameter: bootcode_size = 0x%x, rescue_start = 0x%x.\r\n", prtkimg->bootcode_size, prtkimg->rescue_start);

   prtkimg->rescue_start = SIZE_ALIGN_BOUNDARY_MORE(bootcode_len, 0x1000);

   install_debug("from bootloader.tar: bootcode_size = 0x%x, rescue_start = 0x%x.\r\n\r\n", bootcode_len, prtkimg->rescue_start);

   if(prtkimg->ignore_native_rescue == 1)
   {
      prtkimg->rescue_size = 0;
   }

   return 0;
}

int fill_rtkpartdesc_by_dynamicTbl(struct t_rtkimgdesc* prtkimg)
{
	int i;
#ifndef __OFFLINE_GENERATE_BIN__
	int lessThenIdx = -1, largerThenIdx = -1;
#else
	int hitIdx = -1;
#endif
	unsigned long long cal_size = 0;

	// sansity check
	for (i=0; i< NUM_SUPPORT_DYNAMIC_PART_TBL; i++) {
		cal_size |= dynamic_part_tbl[i].flash_sizeM;
	}
	if (cal_size == 0) {
		install_log("We don't need handle dynamic partiton table!");
		return 0;
	}

#ifndef __OFFLINE_GENERATE_BIN__
	for (i=0; i< NUM_SUPPORT_DYNAMIC_PART_TBL; i++) {
#ifdef DEBUG_FUNC
		install_debug("======%d dynamic table!======\n\r", i);
		cal_size = (unsigned long long)dynamic_part_tbl[i].flash_sizeM;
		install_debug("flash_sizeM: %u M ( %llu bytes) / %llu bytes",dynamic_part_tbl[i].flash_sizeM,(unsigned long long)(cal_size*1024*1024), (unsigned long long)(prtkimg->flash_size+536870912));
		dump_rtk_part_list((struct t_PARTDESC *)&(dynamic_part_tbl[i].partDescTbl));
		printf("============\n\r");
#endif
		if (dynamic_part_tbl[i].flash_sizeM !=0 ) {
			if (prtkimg->flash_size > (unsigned long long)(cal_size*1024*1024))
				largerThenIdx = i;
			if (prtkimg->flash_size < (unsigned long long)(cal_size*1024*1024))
				lessThenIdx = i;
		} else {
			install_debug("Encounter dynamic_part_tbl[%d] flash_sizeM = 0, break!", i);
			break;
		}
	}
	if (lessThenIdx != -1) {
		install_info("Hit part table [%d]th to do the following (lessthan)", lessThenIdx);
		memcpy(&rtk_part_list, &(dynamic_part_tbl[lessThenIdx].partDescTbl), sizeof(struct t_PARTDESC) * NUM_RTKPART);
	} else if (largerThenIdx != -1) {
		install_info("Hit part table [%d]th to do the following (largerthen)", largerThenIdx);
		memcpy(&rtk_part_list, &(dynamic_part_tbl[largerThenIdx].partDescTbl), sizeof(struct t_PARTDESC) * NUM_RTKPART);
	} else {
		install_info("strange! no part table hit! Use 0th to do the following");
		memcpy(&rtk_part_list, &(dynamic_part_tbl[0].partDescTbl), sizeof(struct t_PARTDESC) * NUM_RTKPART);
	}
#else

	for (i=0; i< NUM_SUPPORT_DYNAMIC_PART_TBL; i++) {
		if (dynamic_part_tbl[i].flash_sizeM !=0 && (dynamic_part_tbl[i].flash_sizeM == prtkimg->target_flashM)) {
			hitIdx = i;
			break;
		}
	}
	if (hitIdx != -1) {
		install_info("Hit part table [%d] to do the following", hitIdx);
		memcpy(&rtk_part_list, &(dynamic_part_tbl[hitIdx].partDescTbl), sizeof(struct t_PARTDESC) * NUM_RTKPART);
	} else {
		install_info("strange! no part table hit! Use 0th to do the following");
		memcpy(&rtk_part_list, &(dynamic_part_tbl[0].partDescTbl), sizeof(struct t_PARTDESC) * NUM_RTKPART);
	}
#endif

#ifdef DEBUG_FUNC
	dump_rtk_part_list((struct t_PARTDESC *)rtk_part_list);
#endif
	return 0;
}


#ifndef __OFFLINE_GENERATE_BIN__
/* For Platform installation */
int fill_rtkimgdesc_meminfo(struct t_rtkimgdesc* prtkimg)
{
   char* dev_path;
   int dev_fd;
   struct mtd_info_user meminfo;
   unsigned int part_inv;
   int ret, rba_percentage;
   struct stat stat_buf;
 
   if (strstr(prtkimg->fw[FW_KERNEL].filename, "emmc") == NULL) {
       if (get_sata_block_name(&dev_path) == 0) {
          prtkimg->mtdblock_path = dev_path;
          prtkimg->hdd_dev_name= dev_path;
          prtkimg->flash_type = MTD_SATA;
          prtkimg->mtd_erasesize = 512;
          prtkimg->flash_size = (unsigned long long)prtkimg->mtd_erasesize* (rtk_get_size_emmc());
          printf("[Installer_D]: SATA flash size = %llu Bytes.\n", prtkimg->flash_size );
          printf("[Installer_D]: SATA prtkimg->mtdblock_path = %s.\n", prtkimg->mtdblock_path );
          return 0;
       }
   }
   
   //open mtd block device
   dev_fd = rtk_open_mtd_block(&dev_path);
   if(-1 == dev_fd)
      return -1;
   close(dev_fd);
   prtkimg->mtdblock_path = dev_path;
   prtkimg->isNandAndNor = 0;

   	if (strstr(prtkimg->mtdblock_path, "mmcblk")) {
		prtkimg->flash_type = MTD_EMMC;
		prtkimg->mtd_erasesize = 512;	// 512 byte per block
#ifndef PC_SIMULATE
        if (rtk_get_size_emmc() > 0x2200000)
            prtkimg->flash_size = (unsigned long long)prtkimg->mtd_erasesize*0x3900000; //32GB
        else if ((rtk_get_size_emmc() > 0x1200000) && (rtk_get_size_emmc() < 0x2200000))
            prtkimg->flash_size = (unsigned long long)prtkimg->mtd_erasesize*0x1C80000; //16GB
        else if ((rtk_get_size_emmc() > 0xb40000) && (rtk_get_size_emmc() < 0x1200000))
            prtkimg->flash_size = (unsigned long long)prtkimg->mtd_erasesize*0xe40000; //8GB
        else
            prtkimg->flash_size = (unsigned long long)prtkimg->mtd_erasesize*0x720000; //4GB
#else
        if (strcmp(prtkimg->offline_flashSize, "32gb") == 0)
            prtkimg->flash_size = (unsigned long long)prtkimg->mtd_erasesize*0x3900000; //32GB
        else if (strcmp(prtkimg->offline_flashSize, "16gb") == 0)
            prtkimg->flash_size = (unsigned long long)prtkimg->mtd_erasesize*0x1C80000; //16GB
        else if (strcmp(prtkimg->offline_flashSize, "8gb") == 0)
            prtkimg->flash_size = (unsigned long long)prtkimg->mtd_erasesize*0xe40000; //8GB
        else if (strcmp(prtkimg->offline_flashSize, "4gb") == 0)
            prtkimg->flash_size = (unsigned long long)prtkimg->mtd_erasesize*0x720000; //4GB
#endif
	}
	else {
   		//open mtd char device
   		dev_fd = rtk_open_mtd_char(&dev_path);
   		if(-1 == dev_fd)
      		return -1;
   		prtkimg->mtd_path = dev_path;

   		/* Get MTD device capability structure */
mem_getinfo:
   		memset(&meminfo, 0, sizeof(struct mtd_info_user));
   		if (ioctl(dev_fd, MEMGETINFO, &meminfo) == 0)
   		{
      		// flash info
      		prtkimg->flash_type = meminfo.type;
	   		if(prtkimg->flash_type == MTD_DATAFLASH)
			{
				meminfo.size = 0x01000000;//WST: get memory size failed, so set default value 16M.
				prtkimg->norflash_size = meminfo.size;
				prtkimg->norflash_page_size = meminfo.oobblock;
				prtkimg->norflash_mtd_erasesize = meminfo.erasesize;
				install_info("Get nor flash size : 0x%08x\r\n",prtkimg->norflash_size);
				close(dev_fd);

				sleep(1);
				if (stat(DEV_PATH_NAND_MTD_SP, &stat_buf) == 0)
				{
					dev_fd = open(DEV_PATH_NAND_MTD_SP, O_RDWR);
					if(-1 == dev_fd)
	      				return -1;
					install_info("It's NandAndNor Case!\n");
					prtkimg->isNandAndNor = 1;
	   				prtkimg->mtd_path = DEV_PATH_NAND_MTD_SP;
					prtkimg->mtdblock_path = DEV_PATH_NAND_BLOCK_SP;
					goto mem_getinfo;
				}
			}
			else
				prtkimg->flash_size = meminfo.size;
			prtkimg->page_size = meminfo.oobblock;
			prtkimg->mtd_erasesize = meminfo.erasesize;
		}
   		else
   		{
			install_fail("Get flash info error!, errno(%d)[%s]\r\n", errno, strerror(errno));
      		close(dev_fd);
      		return -1;
   		}

   		if(prtkimg->flash_type == MTD_NANDFLASH)
   		{
      		if (ioctl(dev_fd, GETRBAPERCENTAGE, &rba_percentage) == 0)
      		{
         		if (prtkimg->rba_percentage != rba_percentage)
         		{
            		install_warn("RBA percentage setting is different between kernel (%d) and install package (%d)!\r\n", rba_percentage, prtkimg->rba_percentage);
         		}
      		}
      		install_log("set RBA percentage to %d\r\n", prtkimg->rba_percentage);
   		}

   		close(dev_fd);
	}

#ifndef USING_SYSTEM_FILE
	if( set_system_param( prtkimg ) ) {
		printf("Unknown flash size...\n");
		return -1;
	}
#endif

//skip_mtd_open:
   install_info( "MEMINFO flash_type:%d flash_size:0x%08llx (%llu KB = %lluMB) mtd_erasesize:0x%08x (%u KB), page_size:0x%08x\r\n"
         , prtkimg->flash_type
         , prtkimg->flash_size
         , prtkimg->flash_size>>10
         , prtkimg->flash_size>>20
         , prtkimg->mtd_erasesize
         , prtkimg->mtd_erasesize>>10
         , prtkimg->page_size);

#ifdef EMMC_SUPPORT
   if(prtkimg->flash_type == MTD_NANDFLASH || prtkimg->flash_type == MTD_EMMC)
#else
   if(prtkimg->flash_type == MTD_NANDFLASH)
#endif
   {
      if((ret = get_parameter_value("bootcode_start", &prtkimg->bootcode_start)) < 0)
      {
         install_fail("bootcode_start not found!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_BOOTCODE;
      }
      if (prtkimg->bootcode_start >= prtkimg->flash_size)
      {
         install_fail("bootcode_start parameter error!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_BOOTCODE;
      }

      if((ret = get_parameter_value("bootcode_size", &prtkimg->bootcode_size)) < 0)
      {
         install_fail("bootcode_size not found!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_BOOTCODE;
      }
      if (prtkimg->bootcode_size >= prtkimg->flash_size)
      {
         install_fail("bootcode_size parameter error!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_BOOTCODE;
      }

      if((ret = get_parameter_value("factory_start",  &prtkimg->factory_start)) < 0)
      {
         install_fail("factory_start not found!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_FACTORY;
      }
      if (prtkimg->factory_start >= prtkimg->flash_size)
      {
         install_fail("factory_start parameter error!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_FACTORY;
      }

      if((ret = get_parameter_value("factory_size",  &prtkimg->factory_size)) < 0)
      {
         install_fail("factory_size not found!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_FACTORY;
      }
      if (prtkimg->factory_size >= prtkimg->flash_size)
      {
         install_fail("factory_size parameter error!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_FACTORY;
      }

#ifdef NFLASH_LAOUT
#ifdef USE_SE_STORAGE
      if((ret = get_parameter_value("se_storage_Start",  &prtkimg->se_storage_Start)) < 0)
      {
         install_fail("se_storage_Start not found!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_SE_STORAGE;
      }
      if (prtkimg->se_storage_Start >= prtkimg->flash_size)
      {
         install_fail("se_storage_Start parameter error!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_SE_STORAGE;
      }

      if((ret = get_parameter_value("se_storageSize",  &prtkimg->se_storageSize)) < 0)
      {
         install_fail("se_storageSize not found!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_SE_STORAGE;
      }
      if (prtkimg->se_storageSize >= prtkimg->flash_size)
      {
         install_fail("se_storageSize parameter error!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_SE_STORAGE;
      }
#endif
#endif
	  get_parameter_value("boot_part",  &prtkimg->bootpart);

	  if (prtkimg->backup == 1) {
      	 if (prtkimg->bootpart != 1) {
			prtkimg->bootpart = 0;
		 }
	  }
	  else
         prtkimg->bootpart = 0;
   }
   else
   {
      // for NOR flash info
      //
      if (prtkimg->flash_size >= 32*1024*1024)
      {
         install_debug("flash size = %#llx, set norflash_size_32MB \r\n", prtkimg->flash_size);
         prtkimg->norflash_size_32MB = 1;
      }
      else
         prtkimg->norflash_size_32MB = 0;

      if(((ret = get_parameter_value("part_inv",  &part_inv)) == 0) && (prtkimg->only_factory == 0))
      {
         if (part_inv == 1) {
            install_log("part_inv set!\r\n");
            if (prtkimg->partition_inverse != 1) {
               install_fail("Please add partition_inverse=1 option when make image.\r\n");
               return -1;
            }
         }
      }

      if((ret = get_parameter_value("norbootcode_start", &prtkimg->norbootcode_start)) < 0)
      {
         install_fail("norbootcode_start not found!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_NORBOOTCODE;
      }
      if (prtkimg->norbootcode_start >= prtkimg->norflash_size)
      {
         install_fail("norbootcode_start parameter error!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_NORBOOTCODE;
      }

      if((ret = get_parameter_value("norbootcode_size", &prtkimg->norbootcode_size)) < 0)
      {
         install_fail("norbootcode_size not found!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_NORBOOTCODE;
      }
      if (prtkimg->norbootcode_size >= prtkimg->norflash_size)
      {
         install_fail("norbootcode_size parameter error!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_NORBOOTCODE;
      }
      if((ret = get_parameter_value("norfactory_start",  &prtkimg->norfactory_start)) < 0)
      {
         install_fail("norfactory_start not found!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_NORFACTORY;
      }
      if (prtkimg->norfactory_start >= prtkimg->norflash_size)
      {
         install_fail("norfactory_start parameter error!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_NORFACTORY;
      }

      if((ret = get_parameter_value("norfactory_size",  &prtkimg->norfactory_size)) < 0)
      {
         install_fail("norfactory_size not found!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_NORFACTORY;
      }
      if (prtkimg->norfactory_size >= prtkimg->norflash_size)
      {
         install_fail("norfactory_size parameter error!\r\n");
         return -_eFILL_RTKIMGDESC_MEMINFO_FAIL_NORFACTORY;
      }
   }
   return 0;
}
#else /* else of ifndef __OFFLINE_GENERATE_BIN__ */
/* For Offiline generate bin */
static inline void progressbar_long(unsigned long long len, unsigned long long length, const char* filename)
{
	int i, starcount;
	if(len == 0) install_ui("\n");
	install_ui("\r");
	if(filename != NULL) install_ui("%s", filename);
	install_ui("%3d |", (int) (100.0*len/length));
	starcount = (int) (30.0*len/length);
	for(i=0;i<starcount;i++) install_ui("*");
	for(i=0;i<30.0-starcount;i++) install_ui(" ");
	install_ui("| %8d KB", len>>10);
	if(len == length) install_ui("\n");
}

static inline int check_power_of_2(const unsigned int check){

   if (check&(check-1)) {
      return 0;
   }
   else {
      //power of 2
      return 1;
   }

}

int prepare_empty_file(char *filename, unsigned long long filesize, unsigned char value)
{
	FILE *fd;
	char buffer[8192] = {0};
	int write_count = 0, write_unit;
	unsigned long long write_bytes = 0;

	if (filename == NULL || filesize == 0) {
		install_fail("parameters are not correct\n");
		return -1;
	}

	if ((fd = fopen(filename, "wb")) == NULL) {
      install_fail("Cannot open file: %s", filename);
      return -1;
   }

	install_info("%s file is generating...", filename);
   memset(buffer, value, sizeof(buffer));

   progressbar_long(0, filesize, NULL);

	while (write_bytes < filesize) {
		if ((filesize - write_bytes) >= sizeof(buffer))
			write_unit = sizeof(buffer);
		else
			write_unit = filesize;

		write_bytes += fwrite(buffer, 1, write_unit, fd);
		if (write_count++%4096 == 0) {
         progressbar_long(write_bytes, filesize, NULL);
         fflush(stdout);
      }
	}

   progressbar_long(filesize, filesize, NULL);

   fclose(fd);
	return 0;
}

int fill_rtkimgdesc_meminfo(struct t_rtkimgdesc* prtkimg)
{
   char *file_path = NULL;
   char *output_path = NULL;
   char str_tmp[128] = {0};

   unsigned int factory_size = 0, flash_reserved_area_size = 0;
   unsigned long long virt_flash_size_w_oob = 0, write_bytes = 0;
   int ret = 0, write_count = 0;
   int rba_div;
	unsigned int flash_bp1_size = 0, flash_bp2_size = 0;

   get_parameter_string("boot_flash", str_tmp, sizeof(str_tmp));

   if (!strcmp(str_tmp, "spi")) {
      prtkimg->flash_type = MTD_NORFLASH;
      prtkimg->page_size = 0x100;
      prtkimg->oob_size = 0;
      get_parameter_long_value("flash_size", &prtkimg->flash_size);
      get_parameter_value("flash_erase_size", &prtkimg->mtd_erasesize);
      virt_flash_size_w_oob = prtkimg->flash_size;
   }
   else if (!strcmp(str_tmp, "nand")) {
      prtkimg->flash_type = MTD_NANDFLASH;
      get_parameter_long_value("flash_size", &prtkimg->flash_size);
      get_parameter_value("flash_page_size", &prtkimg->page_size);
      get_parameter_value("flash_oob_size", &prtkimg->oob_size);
      get_parameter_value("flash_erase_size", &prtkimg->mtd_erasesize);
      get_parameter_string("flash_programmer_model", str_tmp, sizeof(str_tmp));
      snprintf(prtkimg->flash_programmer_model , sizeof(prtkimg->flash_programmer_model), "%s",str_tmp);
      get_parameter_string("flash_programmer_name", str_tmp, sizeof(str_tmp));
      snprintf(prtkimg->flash_programmer_name , sizeof(prtkimg->flash_programmer_name), "%s",str_tmp);
      if (prtkimg->rba_percentage)
         rba_div = 100/prtkimg->rba_percentage;
      else
         rba_div = 1;

      //flash_reserved_area_size is a block less than the real reserved area
      flash_reserved_area_size = SIZE_ALIGN_BOUNDARY_LESS(prtkimg->flash_size/rba_div, prtkimg->mtd_erasesize);
      if (prtkimg->whole_image_on == 1) {
         virt_flash_size_w_oob = prtkimg->flash_size/RTK_UNIT_PAGE_SIZE*(RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE);
      }
      else {
         virt_flash_size_w_oob = (prtkimg->flash_size-flash_reserved_area_size)/RTK_UNIT_PAGE_SIZE*(RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE);
      }
      prtkimg->flash_reserved_area_block_count = flash_reserved_area_size/prtkimg->mtd_erasesize;
      prtkimg->flash_block_count = prtkimg->flash_size/prtkimg->mtd_erasesize;
      install_log("RBA percentage %d\r\n", prtkimg->rba_percentage);
      install_debug("flash_reserved_area_size=%#x, virt_flash_size_w_oob=%#x\r\n", flash_reserved_area_size, virt_flash_size_w_oob);
      install_debug("prtkimg->flash_reserved_area_block_count=%#x, prtkimg->flash_block_count=%#x\r\n", prtkimg->flash_reserved_area_block_count, prtkimg->flash_block_count);
   }
   else if (!strcmp(str_tmp, "emmc")) {
      prtkimg->flash_type = MTD_EMMC;
      prtkimg->page_size = 512;
      prtkimg->oob_size = 0;
      get_parameter_long_value("flash_size", &prtkimg->flash_size);
      get_parameter_value("flash_bp1_size", &flash_bp1_size);
      get_parameter_value("flash_bp2_size", &flash_bp2_size);
      get_parameter_value("flash_erased_content", &prtkimg->erased_content);
	   prtkimg->mtd_erasesize = 512;
      virt_flash_size_w_oob = prtkimg->flash_size;
   }
   else
		return -1;

   get_parameter_string("flash_partname", str_tmp, sizeof(str_tmp));
   snprintf(prtkimg->flash_partname , sizeof(prtkimg->flash_partname), "%s",str_tmp);

   if (prtkimg->output_path == NULL) {
      output_path = (char *)malloc(128);
      get_parameter_string("output_path", output_path, 128);
      if (strlen(output_path) == 0) {
         snprintf(output_path, 128, DEFAULT_OUTPUT);
      }
      prtkimg->output_path = output_path;
   }
   install_log("output file:%s\r\n", prtkimg->output_path);

   file_path = (char *)malloc(128);
   snprintf(file_path, 128, DEFAULT_TEMP_OUTPUT);

   //sanity-check
   if (prtkimg->flash_type == MTD_NANDFLASH) {
      if (!check_power_of_2(prtkimg->page_size) || prtkimg->page_size == 0) {
         install_fail("Flash oobblock size error, it is not power of 2!!! oobblock size = %x\r\n", prtkimg->page_size);
         return -1;
      }
      if (!check_power_of_2(prtkimg->oob_size) || prtkimg->oob_size == 0) {
         install_fail("Flash oobsize size error, it is not power of 2!!! oobsize size = %x\r\n", prtkimg->oob_size);
         return -1;
      }
   }
	if (prtkimg->flash_type != MTD_EMMC) {
   	if (!check_power_of_2(prtkimg->mtd_erasesize) || prtkimg->mtd_erasesize == 0) {
      	install_fail("Flash erase size error, it is not power of 2!!! erase size = %x\r\n", prtkimg->mtd_erasesize);
      	return -1;
   	}
   	if (!check_power_of_2(prtkimg->flash_size) || prtkimg->flash_size == 0) {
      	install_fail("Flash size error, it is not power of 2!!! flash size = %x\r\n", prtkimg->flash_size);
      	return -1;
   	}
	}

	rtk_open_mtd_block();

   prtkimg->mtd_path = file_path;
   prtkimg->mtdblock_path = file_path;

   install_log("prtkimg->mtd_path = %s, prtkimg->mtdblock_path = %s\r\n", prtkimg->mtd_path, prtkimg->mtdblock_path);

	// create a 0xFF file with flash size
	if (prtkimg->all_in_one)
		prepare_empty_file(prtkimg->mtd_path, virt_flash_size_w_oob, (unsigned char)(prtkimg->erased_content & 0xff));

	if (prtkimg->flash_type == MTD_EMMC && (prtkimg->erased_content & 0xff) != 0xff) {
		prepare_empty_file((char *)DEFAULT_BP1_OUTPUT, flash_bp1_size, 0xff);
		prepare_empty_file((char *)DEFAULT_BP2_OUTPUT, flash_bp2_size, 0xff);
	}

   // flash info
   install_log("\r\n\r\nMEMINFO flash_type:%d flash_size:0x%08llx (%uKB = %uMB) mtd_erasesize:%#x (%u KB) page_size:%#x oob_size=%#x\r\n\r\n"
         , prtkimg->flash_type
         , prtkimg->flash_size
         , prtkimg->flash_size>>10
         , prtkimg->flash_size>>20
         , prtkimg->mtd_erasesize
         , prtkimg->mtd_erasesize>>10
         , prtkimg->page_size
         , prtkimg->oob_size);


   if(prtkimg->flash_type == MTD_NANDFLASH)
   {
      // in NAND, factory starts from 12MB
      prtkimg->factory_start = NAND_DEFAULT_FACTORY_START;
      // in NAND, factory size is twice of block size.
      // get factory size from bootcode
      if ((ret = get_value_from_project_config(prtkimg, PROJECT_CONFIG_STRING_CONFIG_FACTORY_SIZE, &prtkimg->factory_size)) < 0) {
         prtkimg->factory_size = NAND_DEFAULT_CONFIG_FACTORY_SIZE;
         install_warn("Get factory size fail, set to default %#x\r\n", prtkimg->factory_size);
      }
      prtkimg->bootcode_start = 0x0;
   }
   else if (prtkimg->flash_type == MTD_NORFLASH)
   {
      // get factory size from bootcode
      if ((ret = get_factory_size_from_bootcode(prtkimg)) < 0) {
         return -1;
      }
		prtkimg->factory_size = prtkimg->factory_size/2;
      // in NOR, factory size is the same as block size.
      prtkimg->factory_start = prtkimg->flash_size - 2*prtkimg->factory_size;
      prtkimg->bootcode_start = 0x0;
		prtkimg->bootcode_size = prtkimg->rescue_start = 0x43000;
      prtkimg->rescue_size = 0x0;
   }
   else if (prtkimg->flash_type == MTD_EMMC) {
      if ((ret = get_value_from_project_config(prtkimg, PROJECT_CONFIG_STRING_CONFIG_FACTORY_SIZE, &prtkimg->factory_size)) < 0)
         return -1;

		prtkimg->factory_size = prtkimg->factory_size/2;

      if ((ret = get_value_from_project_config(prtkimg, PROJECT_CONFIG_STRING_CONFIG_FACTORY_START, &prtkimg->factory_start)) < 0)
         return -1;

      if ((ret = get_value_from_project_config(prtkimg, PROJECT_CONFIG_STRING_CONFIG_BOOTCODE_START, &prtkimg->bootcode_start)) < 0)
         return -1;
   }

   return 0;
}
#endif /* end of ifndef __OFFLINE_GENERATE_BIN__ */

int fill_rtk_part_list_by_config(struct t_rtkimgdesc* prtkimg)
{
   int ret;

   // TODO: load partition info into rtk_part_list info from config.txt
   ret = rtk_load_config(prtkimg);

   //dump_rtk_part_list();
   //dump_rtk_fw_list();

   return ret;
}

int fill_rtk_part(struct t_rtkimgdesc* prtkimg)
{
   enum FWTYPE efwtype;
   struct t_PARTDESC* rtk_part;
   char *dotpos = NULL;
   unsigned long long tmpSize;

      // partition mount point, size from rtk_part_list
      for(efwtype=FW_ROOTFS;efwtype<=FW_USR_LOCAL_ETC;efwtype=FWTYPE(efwtype+1)) {
         rtk_part = find_part_by_efwtype((struct t_PARTDESC*)&rtk_part_list, efwtype);
         if(rtk_part == NULL) continue;

         // partition part name & mount point & filesystem name
         snprintf(prtkimg->fw[efwtype].part_name, sizeof(prtkimg->fw[efwtype].part_name), "%s", rtk_part->partition_name);
         snprintf(prtkimg->fw[efwtype].mount_point, sizeof(prtkimg->fw[efwtype].mount_point), "%s", rtk_part->mount_point);
         snprintf(prtkimg->fw[efwtype].fs_name, sizeof(prtkimg->fw[efwtype].fs_name), "%s", inv_efs_to_str(rtk_part->efs));

#ifdef ENABLE_ERASE_CHECK
         prtkimg->fw[efwtype].erased = rtk_part->bErase;
         if (rtk_part->bErase == 0) {
            install_debug("ERASE_CHECK: [%d] partIdx: %d will not beErased!\n", nErasePartCnt, efwtype);

            nErasePartIdxList[nErasePartCnt] = efwtype;
            nErasePartCnt++;
         }
#endif


         //install_test("part_name=%s, mount_point=%s, fs_name=%s\r\n"
         //   , prtkimg->fw[efwtype].part_name
         //   , prtkimg->fw[efwtype].mount_point
         //   , prtkimg->fw[efwtype].fs_name);

         // partition filesystem and flash_allo_size
         switch(rtk_part->efs) {
            case FS_TYPE_YAFFS2:
            case FS_TYPE_JFFS2:
               // empty partition
               if(0 == strlen(rtk_part->filename)) {
                  prtkimg->fw[efwtype].flash_allo_size = rtk_part->min_size;
                  prtkimg->fw[efwtype].img_size = rtk_part->min_size;
               }
               // img partition
               else if(strstr(rtk_part->filename, ".img")) {
                  if(rtk_part->min_size > SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[efwtype].img_size, prtkimg->mtd_erasesize))
                     prtkimg->fw[efwtype].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(rtk_part->min_size, prtkimg->mtd_erasesize);
               }
               // tar.bz2 partition
               else if(strstr(rtk_part->filename, ".bz2")) {
                  prtkimg->fw[efwtype].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(rtk_part->min_size, prtkimg->mtd_erasesize);
               }
               break;
            case FS_TYPE_UBIFS:
                if( (prtkimg->fw[efwtype].img_size==0) && (rtk_part->min_size==0))
                    break;
                if( (unsigned long long)prtkimg->fw[efwtype].img_size > rtk_part->min_size )
                    tmpSize = prtkimg->fw[efwtype].img_size;
                else
                    tmpSize = rtk_part->min_size;
                prtkimg->fw[efwtype].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(tmpSize, prtkimg->mtd_erasesize);
                break;
            case FS_TYPE_SQUASH:
               prtkimg->fw[efwtype].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[efwtype].img_size+MBR_RESERVE_SIZE, prtkimg->mtd_erasesize);
               if(rtk_part->min_size > prtkimg->fw[efwtype].flash_allo_size)
                     prtkimg->fw[efwtype].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(rtk_part->min_size, prtkimg->mtd_erasesize);
               break;
            case FS_TYPE_RAWFILE:
               prtkimg->fw[efwtype].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[efwtype].img_size+MBR_RESERVE_SIZE, prtkimg->mtd_erasesize);
               if(rtk_part->min_size > prtkimg->fw[efwtype].flash_allo_size)
                     prtkimg->fw[efwtype].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(rtk_part->min_size, prtkimg->mtd_erasesize);
               break;
            case FS_TYPE_NONE:
               prtkimg->fw[efwtype].flash_allo_size= rtk_part->min_size;
               prtkimg->fw[efwtype].img_size= rtk_part->min_size;
               break;
#ifdef EMMC_SUPPORT
			case FS_TYPE_EXT4:
			   prtkimg->fw[efwtype].flash_allo_size = rtk_part->min_size;
			   break;
#endif
			default:
			   break;
         }
         prtkimg->fw[efwtype].sector = prtkimg->fw[efwtype].flash_allo_size/prtkimg->mtd_erasesize;

         if ((dotpos = strrchr(prtkimg->fw[efwtype].filename, '.')))
            snprintf(prtkimg->fw[efwtype].compress_type, sizeof(prtkimg->fw[efwtype].compress_type), "%s", dotpos+1);
         install_debug("rtk_fw[%d](%s)->filename[%s] compress_type[%s]\n",
            efwtype, inv_by_fwtype(efwtype),
            prtkimg->fw[efwtype].filename, prtkimg->fw[efwtype].compress_type);
#ifdef ENABLE_ERASE_CHECK
         install_debug("prtkimg->fw[%d].bErase=%d\r\n", efwtype, prtkimg->fw[efwtype].erased);
#endif
      }
   return 0;
}

int fill_rtk_fw(struct t_rtkimgdesc* prtkimg)
{
   enum FWTYPE efwtype;
   struct t_FWDESC* rtk_fw;
   //char *dotpos = NULL;

   // firmware mem_offset from rtk_fw_list
   //for(efwtype=FW_KERNEL;efwtype<FW_ROOTFS;efwtype=FWTYPE(efwtype+1))
   for(efwtype=FW_KERNEL;efwtype<FW_UNKNOWN;efwtype=FWTYPE(efwtype+1))
   {
      if (((efwtype >= FW_ROOTFS) && (efwtype<FW_NORBOOTCODE)) || (efwtype == FW_UBOOT)) 
        continue;
        
      rtk_fw = find_fw_by_efwtype(efwtype);
      if((rtk_fw == NULL) || (prtkimg->fw[efwtype].img_size == 0)) continue; 

      // firmware target
      prtkimg->fw[efwtype].mem_offset = rtk_fw->target;

	  // So far, we only support lzma
	  if (strstr(prtkimg->fw[efwtype].filename, "lzma"))
		strcpy(prtkimg->fw[efwtype].compress_type, "lzma");
	  else if (strlen(prtkimg->fw[efwtype].filename) != 0)
		strcpy(prtkimg->fw[efwtype].compress_type, "bin");
#if 0
      install_debug("rtk_fw[%d](%s)filename=[%s] compress_type[%s]\n",
        efwtype, inv_by_fwtype(efwtype),
        prtkimg->fw[efwtype].filename, prtkimg->fw[efwtype].compress_type);
#else
      install_debug("rtk_fw[%d](%s)filename=[%s] target address[0x%x]\n",
        efwtype, inv_by_fwtype(efwtype),
        prtkimg->fw[efwtype].filename, prtkimg->fw[efwtype].mem_offset);
#endif
   }
   
   return 0;
}

int fill_rtk_install_av_count(struct t_rtkimgdesc* prtkimg)
{
   switch (prtkimg->install_avfile_count) {
      case 0:
         prtkimg->fw[FW_AUDIO_BOOTFILE].img_size = prtkimg->fw[FW_AUDIO_BOOTFILE].flash_allo_size = 0;
         prtkimg->fw[FW_IMAGE_BOOTFILE].img_size = prtkimg->fw[FW_IMAGE_BOOTFILE].flash_allo_size = 0;
         prtkimg->fw[FW_VIDEO_BOOTFILE].img_size = prtkimg->fw[FW_VIDEO_BOOTFILE].flash_allo_size = 0;
         /* falling through */
      case 1:
         prtkimg->fw[FW_AUDIO_CLOGO1].img_size = prtkimg->fw[FW_AUDIO_CLOGO1].flash_allo_size = 0;
         prtkimg->fw[FW_IMAGE_CLOGO1].img_size = prtkimg->fw[FW_IMAGE_CLOGO1].flash_allo_size = 0;
         prtkimg->fw[FW_VIDEO_CLOGO1].img_size = prtkimg->fw[FW_VIDEO_CLOGO1].flash_allo_size = 0;
         /* falling through */
      case 2:
         prtkimg->fw[FW_AUDIO_CLOGO2].img_size = prtkimg->fw[FW_AUDIO_CLOGO2].flash_allo_size = 0;
         prtkimg->fw[FW_IMAGE_CLOGO2].img_size = prtkimg->fw[FW_IMAGE_CLOGO2].flash_allo_size = 0;
         prtkimg->fw[FW_VIDEO_CLOGO2].img_size = prtkimg->fw[FW_VIDEO_CLOGO2].flash_allo_size = 0;
         break;
      case 3:
         break;

      default:
         install_fail("error invalid install_avfile_count=%d.\r\n", prtkimg->install_avfile_count);
         return -1;
         break;

   }
   return 0;
}

int fill_rtkimgdesc_layout(struct t_rtkimgdesc* prtkimg)
{
   int i, j, ret, rba_div;
   int etc_index = 0, vssu_work_part = 0;
   enum FWTYPE efwtype;
   S_BOOTTABLE boottable, *pboottable = NULL;
   char cmd[128] = {0}, path[128] = {0};

   unsigned long long flash_top_low_limit=0, flash_bottom_high_limit=0;
	unsigned long long flash_total_need_size=0;
   unsigned long long fw_part_flash_bottom_start=0, fw_part_flash_top_end=0;
   unsigned long long flash_start=0xffffffffffffffffLL;

   // sanity check
   // not implemented

   // initial data structure
   // firmware and partition
   for(efwtype=FWTYPE(FW_KERNEL);efwtype<FW_UNKNOWN;efwtype=FWTYPE(efwtype+1))
         prtkimg->fw[efwtype].efwtype = efwtype;

   // from rtk_part
   // flash_allo_size img_size
   fill_rtk_part(prtkimg);

   // from rtk_fw
   // flash_allo_size mem_offset
   fill_rtk_fw(prtkimg);

   //dump_rtk_part_list();
   //dump_rtk_fw_list();

   memset(&boottable, 0, sizeof(boottable));
   // we need flash related info of prtkimg
   if (prtkimg->flash_type != MTD_SATA)
      pboottable = read_boottable(&boottable, prtkimg);

   if (prtkimg->safe_upgrade == 1)
   {
      if (prtkimg->bootcode == 1)
      {
         install_warn("Warning!!! SSU doesn't guarantee upgrade bootcode.\r\n");
      }
      if (pboottable)
      {
         install_debug("SSU WORK PART = %d\r\n", pboottable->ssu_work_part);
         vssu_work_part = pboottable->ssu_work_part;
      }
      else
      {
         install_debug("boottable not found!\r\n");
         vssu_work_part = 0;
      }
      snprintf(cmd, sizeof(cmd), "cp %s/%s %s/%s_bak"
                              , get_factory_tmp_dir()
                              , LAYOUT_FILENAME
                              , PKG_TEMP
                              , LAYOUT_FILENAME);
      ret = rtk_command(cmd, __LINE__, __FILE__, 0);
   }
   else
   {
      if (pboottable)
      {
         if (pboottable->ssu_work_part != 0)
            install_warn("SSU flag detected, but SSU is not set in this package. Falling back to normal installation!\r\n");
         prtkimg->next_ssu_work_part = 0;
      }
   }
#ifdef NAS_ENABLE
   /* Use NAS rescue info from boottable layout
    * if nas_rescue is not enabled in install image. */
   if (0 == prtkimg->nas_rescue && pboottable){
      install_log("Read NAS rescue info from boottable layout\r\n");
      for(efwtype=FW_NAS_KERNEL; efwtype<=FW_NAS_RESCUE_ROOTFS; efwtype=FWTYPE(efwtype+1)){
         if(pboottable->fw.list[efwtype].loc.size != 0){
            install_log("Read fw info: %s\r\n", inv_fwtype(E_FWTYPE(efwtype)));
            strncpy(prtkimg->fw[efwtype].filename, pboottable->fw.list[efwtype].imgname, sizeof(prtkimg->fw[efwtype].filename));
            prtkimg->fw[efwtype].img_size = pboottable->fw.list[efwtype].loc.size;
            prtkimg->fw[efwtype].mem_offset = pboottable->fw.list[efwtype].loc.target;
            //prtkimg->fw[efwtype].flash_offset = pboottable->fw.list[efwtype].loc.offset;
            strncpy(prtkimg->fw[efwtype].compress_type, pboottable->fw.list[efwtype].loc.type, sizeof(prtkimg->fw[efwtype].compress_type));
         }
      }
   }
#endif

   if(prtkimg->flash_type == MTD_NANDFLASH)
   {

#ifdef __OFFLINE_GENERATE_BIN__
      prtkimg->bootcode_size = prtkimg->factory_start;
#endif

      prtkimg->fw[FW_BOOTCODE].img_size = prtkimg->bootcode_size;
      prtkimg->fw[FW_BOOTCODE].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_BOOTCODE].img_size, prtkimg->mtd_erasesize);
      prtkimg->fw[FW_BOOTCODE].sector = prtkimg->fw[FW_BOOTCODE].flash_allo_size/prtkimg->mtd_erasesize;
      for(efwtype=FW_KERNEL; efwtype<FW_ROOTFS; efwtype=FWTYPE(efwtype+1)) {
         prtkimg->fw[efwtype].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[efwtype].img_size, prtkimg->mtd_erasesize);
         prtkimg->fw[efwtype].sector = prtkimg->fw[efwtype].flash_allo_size/prtkimg->mtd_erasesize;
      }
      
#ifdef NAS_ENABLE
#ifdef HYPERVISOR
      prtkimg->fw[FW_XEN].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_XEN].img_size, prtkimg->mtd_erasesize);
      prtkimg->fw[FW_XEN].sector = prtkimg->fw[FW_XEN].flash_allo_size/prtkimg->mtd_erasesize;
#endif
#endif

      // fill install av count firmware desc
      if ((ret = fill_rtk_install_av_count(prtkimg)) < 0) {
         return -1;
      }

	  // we use ping-pong for factory partition
      prtkimg->factory_section_size = prtkimg->factory_size * 2;
      prtkimg->fw[FW_FACTORY].flash_offset = prtkimg->factory_start;
      prtkimg->fw[FW_FACTORY].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->factory_section_size,prtkimg->mtd_erasesize);
      prtkimg->fw[FW_FACTORY].sector = prtkimg->fw[FW_FACTORY].flash_allo_size/prtkimg->mtd_erasesize;
      prtkimg->fw[FW_FACTORY].img_size = prtkimg->fw[FW_FACTORY].flash_allo_size;

      if (prtkimg->rba_percentage)
         rba_div = 100/prtkimg->rba_percentage;
      else
         rba_div = 1;

      //reserve x% of FLASH for remapping
      prtkimg->reserved_boot_size = prtkimg->bootcode_size + prtkimg->factory_section_size;
      prtkimg->reserved_boottable_size = prtkimg->mtd_erasesize;
      #ifdef RESERVED_AREA
      prtkimg->reserved_remapping_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->flash_size / rba_div, prtkimg->mtd_erasesize) + prtkimg->mtd_erasesize;
      prtkimg->reserved_remapping_sector = prtkimg->reserved_remapping_size / prtkimg->mtd_erasesize;
      prtkimg->reserved_remapping_offset = prtkimg->flash_size - prtkimg->reserved_remapping_size;
      install_log("reserved remapping size: 0x%x (0x%x sectors).\r\n", prtkimg->reserved_remapping_size, prtkimg->reserved_remapping_size/prtkimg->mtd_erasesize);
      #else
      prtkimg->reserved_remapping_size = 0;
      prtkimg->reserved_remapping_sector = 0;
      prtkimg->reserved_remapping_offset = 0;
      install_log("reserved remapping size is excluded!\n");
      #endif

      //firmware desc table, only for NAND
      prtkimg->fw[FW_FW_TBL].flash_offset = prtkimg->reserved_boot_size;
      prtkimg->fw[FW_FW_TBL].sector = 1;
      prtkimg->fw[FW_FW_TBL].flash_allo_size = prtkimg->mtd_erasesize * prtkimg->fw[FW_FW_TBL].sector;

      //firmware & partition flash start offset and flash end offset
      if (prtkimg->safe_upgrade == 1)
      {
         install_fail("SSU function not implentment in NAND yet.\r\n");
         return -1;
      }
      else
      {
         fw_part_flash_bottom_start = prtkimg->fw[FW_FW_TBL].flash_offset + prtkimg->fw[FW_FW_TBL].flash_allo_size;
         fw_part_flash_top_end = prtkimg->flash_size - prtkimg->reserved_remapping_size;
      }

      // layout buttom up
      prtkimg->fw[FW_BOOTCODE].flash_offset = prtkimg->bootcode_start;
#ifdef NAS_ENABLE
      /* Add one block for rescue fwdesc table */
      prtkimg->fw[FW_NAS_RESCUE_DT].flash_offset = fw_part_flash_bottom_start + prtkimg->mtd_erasesize;
      prtkimg->fw[FW_NAS_KERNEL].flash_offset = prtkimg->fw[FW_NAS_RESCUE_DT].flash_offset + prtkimg->fw[FW_NAS_RESCUE_DT].flash_allo_size;
      prtkimg->fw[FW_NAS_RESCUE_ROOTFS].flash_offset = prtkimg->fw[FW_NAS_KERNEL].flash_offset + prtkimg->fw[FW_NAS_KERNEL].flash_allo_size;
      prtkimg->fw[FW_RESCUE_DT].flash_offset = prtkimg->fw[FW_NAS_RESCUE_ROOTFS].flash_offset + prtkimg->fw[FW_NAS_RESCUE_ROOTFS].flash_allo_size;
      /* no rescue fwdesc if there's no rescue DTB/kernel/rootfs */
      if(prtkimg->fw[FW_RESCUE_DT].flash_offset == prtkimg->fw[FW_NAS_RESCUE_DT].flash_offset)
#endif
      prtkimg->fw[FW_RESCUE_DT].flash_offset = fw_part_flash_bottom_start;
      prtkimg->fw[FW_KERNEL_DT].flash_offset = prtkimg->fw[FW_RESCUE_DT].flash_offset + prtkimg->fw[FW_RESCUE_DT].flash_allo_size;
      prtkimg->fw[FW_AUDIO].flash_offset = prtkimg->fw[FW_KERNEL_DT].flash_offset + prtkimg->fw[FW_KERNEL_DT].flash_allo_size;;
      prtkimg->fw[FW_KERNEL].flash_offset = prtkimg->fw[FW_AUDIO].flash_offset + prtkimg->fw[FW_AUDIO].flash_allo_size;
#ifdef TEE_ENABLE
      prtkimg->fw[FW_TEE].flash_offset = prtkimg->fw[FW_KERNEL].flash_offset + prtkimg->fw[FW_KERNEL].flash_allo_size;
      prtkimg->fw[FW_BL31].flash_offset = prtkimg->fw[FW_TEE].flash_offset + prtkimg->fw[FW_TEE].flash_allo_size;
      prtkimg->fw[FW_RESCUE_ROOTFS].flash_offset = prtkimg->fw[FW_BL31].flash_offset + prtkimg->fw[FW_BL31].flash_allo_size;
#else
      prtkimg->fw[FW_RESCUE_ROOTFS].flash_offset = prtkimg->fw[FW_KERNEL].flash_offset + prtkimg->fw[FW_KERNEL].flash_allo_size;

#endif
      prtkimg->fw[FW_KERNEL_ROOTFS].flash_offset = prtkimg->fw[FW_RESCUE_ROOTFS].flash_offset + prtkimg->fw[FW_RESCUE_ROOTFS].flash_allo_size;
      prtkimg->fw[FW_AUDIO_BOOTFILE].flash_offset = prtkimg->fw[FW_KERNEL_ROOTFS].flash_offset + prtkimg->fw[FW_KERNEL_ROOTFS].flash_allo_size;
      prtkimg->fw[FW_AUDIO_CLOGO1].flash_offset = prtkimg->fw[FW_AUDIO_BOOTFILE].flash_offset + prtkimg->fw[FW_AUDIO_BOOTFILE].flash_allo_size;
      prtkimg->fw[FW_AUDIO_CLOGO2].flash_offset = prtkimg->fw[FW_AUDIO_CLOGO1].flash_offset + prtkimg->fw[FW_AUDIO_CLOGO1].flash_allo_size;
      prtkimg->fw[FW_IMAGE_BOOTFILE].flash_offset = prtkimg->fw[FW_AUDIO_CLOGO2].flash_offset + prtkimg->fw[FW_AUDIO_CLOGO2].flash_allo_size;
      prtkimg->fw[FW_IMAGE_CLOGO1].flash_offset = prtkimg->fw[FW_IMAGE_BOOTFILE].flash_offset + prtkimg->fw[FW_IMAGE_BOOTFILE].flash_allo_size;
      prtkimg->fw[FW_IMAGE_CLOGO2].flash_offset = prtkimg->fw[FW_IMAGE_CLOGO1].flash_offset + prtkimg->fw[FW_IMAGE_CLOGO1].flash_allo_size;
      prtkimg->fw[FW_VIDEO_BOOTFILE].flash_offset = prtkimg->fw[FW_IMAGE_CLOGO2].flash_offset + prtkimg->fw[FW_IMAGE_CLOGO2].flash_allo_size;
      prtkimg->fw[FW_VIDEO_CLOGO1].flash_offset = prtkimg->fw[FW_VIDEO_BOOTFILE].flash_offset + prtkimg->fw[FW_VIDEO_BOOTFILE].flash_allo_size;
      prtkimg->fw[FW_VIDEO_CLOGO2].flash_offset = prtkimg->fw[FW_VIDEO_CLOGO1].flash_offset + prtkimg->fw[FW_VIDEO_CLOGO1].flash_allo_size;
#ifdef NAS_ENABLE
#ifdef HYPERVISOR
      prtkimg->fw[FW_XEN].flash_offset = prtkimg->fw[FW_VIDEO_CLOGO2].flash_offset + prtkimg->fw[FW_VIDEO_CLOGO2].flash_allo_size;
      flash_bottom_high_limit = prtkimg->fw[FW_XEN].flash_offset + prtkimg->fw[FW_XEN].flash_allo_size;
#else
      flash_bottom_high_limit = prtkimg->fw[FW_VIDEO_CLOGO2].flash_offset + prtkimg->fw[FW_VIDEO_CLOGO2].flash_allo_size;
#endif
#else
      flash_bottom_high_limit = prtkimg->fw[FW_VIDEO_CLOGO2].flash_offset + prtkimg->fw[FW_VIDEO_CLOGO2].flash_allo_size;
#endif


      // layout top down
      FWTYPE prevFW = FW_UNKNOWN;
      for(i=NUM_RTKPART-1; i>=0; i--) {
         if( ! rtk_part_list_sort[i] )
            continue;

         efwtype = rtk_part_list_sort[i]->efwtype;
         if( prevFW == FW_UNKNOWN ) {
            prtkimg->fw[efwtype].flash_offset = fw_part_flash_top_end - prtkimg->fw[efwtype].flash_allo_size;
         } else {
            prtkimg->fw[efwtype].flash_offset = prtkimg->fw[prevFW].flash_offset - prtkimg->fw[efwtype].flash_allo_size;
         }
         prevFW = efwtype;
      }
      flash_top_low_limit = prtkimg->fw[prevFW].flash_offset;

      // add dev_path[], for recovery mode.
      for(i=0; i<NUM_RTKPART; i++) {
         if( ! rtk_part_list_sort[i] )
            break;

         efwtype = rtk_part_list_sort[i]->efwtype;
         sprintf(prtkimg->fw[efwtype].dev_path, "/dev/mtd/mtd%d", i+1);
      }

      // burn firmware/partition method
      prtkimg->eburn = BURN_BY_NANDWRITE;
   }
#if defined(NAS_ENABLE) && defined(CONFIG_BOOT_FROM_SPI)
   else if(prtkimg->flash_type == MTD_DATAFLASH)
   {
      const unsigned int _64K_BYTE = 64*1024;

      prtkimg->reserved_boot_size = prtkimg->norbootcode_size;
      prtkimg->reserved_boottable_size = _64K_BYTE;

      prtkimg->fw[FW_BOOTCODE].img_size = prtkimg->norbootcode_size;
      prtkimg->fw[FW_BOOTCODE].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_BOOTCODE].img_size, prtkimg->mtd_erasesize);
      prtkimg->fw[FW_BOOTCODE].sector = prtkimg->fw[FW_BOOTCODE].flash_allo_size/prtkimg->mtd_erasesize;

      for(efwtype=FW_KERNEL; efwtype<FW_TEE; efwtype=FWTYPE(efwtype+1)) {
         prtkimg->fw[efwtype].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[efwtype].img_size, prtkimg->mtd_erasesize);
         prtkimg->fw[efwtype].sector = prtkimg->fw[efwtype].flash_allo_size/prtkimg->mtd_erasesize;
      }

      prtkimg->fw[FW_FW_TBL].flash_offset = prtkimg->reserved_boot_size;
      prtkimg->fw[FW_FW_TBL].flash_allo_size = prtkimg->reserved_boottable_size;

      //prtkimg->factory_section_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->factory_size * 2, prtkimg->mtd_erasesize);
      prtkimg->factory_section_size = prtkimg->norfactory_size;
      prtkimg->fw[FW_FACTORY].flash_allo_size = prtkimg->factory_section_size;
      prtkimg->fw[FW_FACTORY].flash_offset = prtkimg->norfactory_start;
      prtkimg->fw[FW_FACTORY].img_size = prtkimg->fw[FW_FACTORY].flash_allo_size;

      //bootcode flash offset
      prtkimg->fw[FW_BOOTCODE].flash_offset = prtkimg->norbootcode_start;

      prtkimg->fw[FW_KERNEL_DT].flash_offset = NOR_NORMAL_DTS_START_ADDR;
      prtkimg->fw[FW_KERNEL_DT].flash_allo_size = NOR_RESCUE_DTS_START_ADDR - NOR_NORMAL_DTS_START_ADDR;
      prtkimg->fw[FW_RESCUE_DT].flash_offset = NOR_RESCUE_DTS_START_ADDR;
      prtkimg->fw[FW_RESCUE_DT].flash_allo_size = NOR_KERNEL_START_ADDR - NOR_RESCUE_DTS_START_ADDR;
      prtkimg->fw[FW_KERNEL].flash_offset = NOR_KERNEL_START_ADDR;
      prtkimg->fw[FW_KERNEL].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_KERNEL].img_size, prtkimg->mtd_erasesize);
      prtkimg->fw[FW_AUDIO].flash_offset = prtkimg->fw[FW_KERNEL].flash_offset + prtkimg->fw[FW_KERNEL].flash_allo_size;
      prtkimg->fw[FW_AUDIO].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_AUDIO].img_size, prtkimg->mtd_erasesize);
      prtkimg->fw[FW_RESCUE_ROOTFS].flash_offset = prtkimg->fw[FW_AUDIO].flash_offset + prtkimg->fw[FW_AUDIO].flash_allo_size;
      prtkimg->fw[FW_RESCUE_ROOTFS].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_RESCUE_ROOTFS].img_size, prtkimg->mtd_erasesize);

      // burn firmware/partition method
      prtkimg->eburn = BURN_BY_MTDBLOCK;

      return _eRTK_SUCCESS;
   }
#endif
   else if(prtkimg->flash_type == MTD_NORFLASH || prtkimg->flash_type == MTD_DATAFLASH)
   {

      //If install_bootloader is set, we calcuate bootcode_size from bootloader.tar.
      if(prtkimg->bootcode == 1) {
         if ((ret = check_spi_bootcode_size(prtkimg)) < 0) {
            return ret;
         }
      }

      prtkimg->bootcode_size = prtkimg->rescue_start + prtkimg->rescue_size;

      prtkimg->fw[FW_BOOTCODE].img_size = prtkimg->bootcode_size;
      prtkimg->fw[FW_BOOTCODE].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_BOOTCODE].img_size, prtkimg->mtd_erasesize);
      prtkimg->fw[FW_KERNEL].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_KERNEL].img_size, prtkimg->mtd_erasesize);
#ifdef TEE_ENABLE
      prtkimg->fw[FW_TEE].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_TEE].img_size, prtkimg->mtd_erasesize);
      prtkimg->fw[FW_BL31].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_BL31].img_size, prtkimg->mtd_erasesize);
#endif
#ifdef NAS_ENABLE
#ifdef HYPERVISOR
      prtkimg->fw[FW_XEN].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_XEN].img_size, prtkimg->mtd_erasesize);
#endif
#endif
      prtkimg->fw[FW_AUDIO_BOOTFILE].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_AUDIO_BOOTFILE].img_size, prtkimg->mtd_erasesize);
      prtkimg->fw[FW_VIDEO_BOOTFILE].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_VIDEO_BOOTFILE].img_size, prtkimg->mtd_erasesize);
      prtkimg->fw[FW_AUDIO].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_AUDIO].img_size, prtkimg->mtd_erasesize);
      prtkimg->fw[FW_VIDEO].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_VIDEO].img_size, prtkimg->mtd_erasesize);
      prtkimg->fw[FW_VIDEO2].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_VIDEO2].img_size, prtkimg->mtd_erasesize);

      // fill install av count firmware desc
      if ((ret = fill_rtk_install_av_count(prtkimg)) < 0) {
         return -1;
      }

      //In SPI NOR FLASH factory section size is the twice of factory size.
      prtkimg->factory_section_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->factory_size * 2, prtkimg->mtd_erasesize);
      prtkimg->fw[FW_FACTORY].flash_offset = prtkimg->factory_start;
      prtkimg->fw[FW_FACTORY].flash_allo_size = prtkimg->factory_section_size;
      prtkimg->fw[FW_FACTORY].img_size = prtkimg->fw[FW_FACTORY].flash_allo_size;

      //bootcode flash offset
      prtkimg->fw[FW_BOOTCODE].flash_offset = prtkimg->bootcode_start;

      //firmware & partition flash start offset and flash end offset
      if (prtkimg->safe_upgrade == 1)
      {
         switch(vssu_work_part)
         {
            case 0:
               /* working part as 0, means first use of ssu */
               /* falling through */
            case 2:
               /* working part as 2, protection part as 1 */
               /* write to protection part */
               fw_part_flash_bottom_start = prtkimg->fw[FW_BOOTCODE].flash_offset + prtkimg->fw[FW_BOOTCODE].flash_allo_size;
               fw_part_flash_top_end = SIZE_ALIGN_BOUNDARY_LESS(prtkimg->flash_size/2, prtkimg->mtd_erasesize);
               prtkimg->fw[FW_P_SSU_WORKING].flash_offset = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->flash_size/2, prtkimg->mtd_erasesize);
               prtkimg->fw[FW_P_SSU_WORKING].flash_allo_size = prtkimg->flash_size - prtkimg->factory_section_size - prtkimg->fw[FW_P_SSU_WORKING].flash_offset;
               prtkimg->next_ssu_work_part = 1;
               break;
            case 1:
               /* working part as 1, protection part as 2 */
               /* write to protection part */
               fw_part_flash_bottom_start = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->flash_size/2, prtkimg->mtd_erasesize);
               fw_part_flash_top_end = prtkimg->flash_size - prtkimg->factory_section_size;
               prtkimg->fw[FW_P_SSU_WORKING].flash_offset = prtkimg->fw[FW_BOOTCODE].flash_offset + prtkimg->fw[FW_BOOTCODE].flash_allo_size;
               prtkimg->fw[FW_P_SSU_WORKING].flash_allo_size = SIZE_ALIGN_BOUNDARY_LESS(prtkimg->flash_size/2, prtkimg->mtd_erasesize) - prtkimg->fw[FW_P_SSU_WORKING].flash_offset;
               prtkimg->next_ssu_work_part = 2;
               break;
            default:
               install_fail("vssu_work_part = %d, not defined!!!\r\n", vssu_work_part);
               return -1;
         }
         install_log("SSU now work part = %d, next work part = %d\r\nfw part: bottom = 0x%llx, top = 0x%llx.\r\n"
            , vssu_work_part
            , prtkimg->next_ssu_work_part
            , fw_part_flash_bottom_start
            , fw_part_flash_top_end);
      }
      else
      {
         fw_part_flash_bottom_start = prtkimg->fw[FW_BOOTCODE].flash_offset + prtkimg->fw[FW_BOOTCODE].flash_allo_size;
         fw_part_flash_top_end = prtkimg->flash_size - prtkimg->factory_section_size;
         install_log("fw part: bottom = 0x%llx, top = 0x%llx.\r\n", fw_part_flash_bottom_start, fw_part_flash_top_end);
      }

      if (prtkimg->partition_inverse == 1) {
         install_warn("====== Warning!!! partition_inverse is set ======\r\n");
         // layout top down
         prtkimg->fw[FW_USR_LOCAL_ETC].flash_offset = prtkimg->flash_size - prtkimg->factory_section_size - prtkimg->fw[FW_USR_LOCAL_ETC].flash_allo_size;
         prtkimg->fw[FW_ROOTFS].flash_offset = prtkimg->fw[FW_USR_LOCAL_ETC].flash_offset - prtkimg->fw[FW_ROOTFS].flash_allo_size;
         flash_top_low_limit = prtkimg->fw[FW_ROOTFS].flash_offset;

         // layout bottom up
         //prtkimg->fw[FW_BOOTCODE].flash_offset = 0;
         prtkimg->fw[FW_VIDEO].flash_offset = fw_part_flash_bottom_start;
         prtkimg->fw[FW_AUDIO].flash_offset = prtkimg->fw[FW_VIDEO].flash_offset + prtkimg->fw[FW_VIDEO].flash_allo_size;
         prtkimg->fw[FW_KERNEL].flash_offset = prtkimg->fw[FW_AUDIO].flash_offset + prtkimg->fw[FW_AUDIO].flash_allo_size;
#ifdef TEE_ENABLE
         prtkimg->fw[FW_TEE].flash_offset = prtkimg->fw[FW_KERNEL].flash_offset + prtkimg->fw[FW_KERNEL].flash_allo_size;
         prtkimg->fw[FW_BL31].flash_offset = prtkimg->fw[FW_TEE].flash_offset + prtkimg->fw[FW_TEE].flash_allo_size;
         prtkimg->fw[FW_VIDEO_BOOTFILE].flash_offset = prtkimg->fw[FW_BL31].flash_offset + prtkimg->fw[FW_BL31].flash_allo_size;
#else
         prtkimg->fw[FW_VIDEO_BOOTFILE].flash_offset = prtkimg->fw[FW_KERNEL].flash_offset + prtkimg->fw[FW_KERNEL].flash_allo_size;
#endif
         prtkimg->fw[FW_AUDIO_BOOTFILE].flash_offset = prtkimg->fw[FW_VIDEO_BOOTFILE].flash_offset + prtkimg->fw[FW_VIDEO_BOOTFILE].flash_allo_size;
         prtkimg->fw[FW_VIDEO_CLOGO1].flash_offset = prtkimg->fw[FW_AUDIO_BOOTFILE].flash_offset + prtkimg->fw[FW_AUDIO_BOOTFILE].flash_allo_size;
         prtkimg->fw[FW_AUDIO_CLOGO1].flash_offset = prtkimg->fw[FW_VIDEO_CLOGO1].flash_offset + prtkimg->fw[FW_VIDEO_CLOGO1].flash_allo_size;
         prtkimg->fw[FW_VIDEO_CLOGO2].flash_offset = prtkimg->fw[FW_AUDIO_CLOGO1].flash_offset + prtkimg->fw[FW_AUDIO_CLOGO1].flash_allo_size;
         prtkimg->fw[FW_AUDIO_CLOGO2].flash_offset = prtkimg->fw[FW_VIDEO_CLOGO2].flash_offset + prtkimg->fw[FW_VIDEO_CLOGO2].flash_allo_size;
         prtkimg->fw[P_PARTITION1].flash_offset = prtkimg->fw[FW_AUDIO_CLOGO2].flash_offset + prtkimg->fw[FW_AUDIO_CLOGO2].flash_allo_size;
         prtkimg->fw[P_PARTITION2].flash_offset = prtkimg->fw[P_PARTITION1].flash_offset + prtkimg->fw[P_PARTITION1].flash_allo_size;
         prtkimg->fw[P_PARTITION3].flash_offset = prtkimg->fw[P_PARTITION2].flash_offset + prtkimg->fw[P_PARTITION2].flash_allo_size;
         prtkimg->fw[P_PARTITION4].flash_offset = prtkimg->fw[P_PARTITION3].flash_offset + prtkimg->fw[P_PARTITION3].flash_allo_size;
         prtkimg->fw[P_PARTITION5].flash_offset = prtkimg->fw[P_PARTITION4].flash_offset + prtkimg->fw[P_PARTITION4].flash_allo_size;
         flash_bottom_high_limit = prtkimg->fw[P_PARTITION5].flash_offset + prtkimg->fw[P_PARTITION5].flash_allo_size;

      }
      else {
         // old partition layout
         prtkimg->fw[FW_VIDEO].flash_offset = fw_part_flash_top_end - prtkimg->fw[FW_VIDEO].flash_allo_size;
         prtkimg->fw[FW_AUDIO].flash_offset = prtkimg->fw[FW_VIDEO].flash_offset - prtkimg->fw[FW_AUDIO].flash_allo_size;
         prtkimg->fw[FW_KERNEL].flash_offset = prtkimg->fw[FW_AUDIO].flash_offset - prtkimg->fw[FW_KERNEL].flash_allo_size;
#ifdef TEE_ENABLE
         prtkimg->fw[FW_TEE].flash_offset = prtkimg->fw[FW_KERNEL].flash_offset - prtkimg->fw[FW_TEE].flash_allo_size;
         prtkimg->fw[FW_BL31].flash_offset = prtkimg->fw[FW_TEE].flash_offset - prtkimg->fw[FW_BL31].flash_allo_size;
         prtkimg->fw[FW_VIDEO_BOOTFILE].flash_offset = prtkimg->fw[FW_BL31].flash_offset - prtkimg->fw[FW_VIDEO_BOOTFILE].flash_allo_size;
#else
         prtkimg->fw[FW_VIDEO_BOOTFILE].flash_offset = prtkimg->fw[FW_KERNEL].flash_offset - prtkimg->fw[FW_VIDEO_BOOTFILE].flash_allo_size;
#endif
         prtkimg->fw[FW_AUDIO_BOOTFILE].flash_offset = prtkimg->fw[FW_VIDEO_BOOTFILE].flash_offset - prtkimg->fw[FW_AUDIO_BOOTFILE].flash_allo_size;
         prtkimg->fw[FW_VIDEO_CLOGO1].flash_offset = prtkimg->fw[FW_AUDIO_BOOTFILE].flash_offset - prtkimg->fw[FW_VIDEO_CLOGO1].flash_allo_size;
         prtkimg->fw[FW_AUDIO_CLOGO1].flash_offset = prtkimg->fw[FW_VIDEO_CLOGO1].flash_offset - prtkimg->fw[FW_AUDIO_CLOGO1].flash_allo_size;
         prtkimg->fw[FW_VIDEO_CLOGO2].flash_offset = prtkimg->fw[FW_AUDIO_CLOGO1].flash_offset - prtkimg->fw[FW_VIDEO_CLOGO2].flash_allo_size;
         prtkimg->fw[FW_AUDIO_CLOGO2].flash_offset = prtkimg->fw[FW_VIDEO_CLOGO2].flash_offset - prtkimg->fw[FW_AUDIO_CLOGO2].flash_allo_size;
         prtkimg->fw[P_PARTITION5].flash_offset = prtkimg->fw[FW_AUDIO_CLOGO2].flash_offset - prtkimg->fw[P_PARTITION5].flash_allo_size;
         prtkimg->fw[P_PARTITION4].flash_offset = prtkimg->fw[P_PARTITION5].flash_offset - prtkimg->fw[P_PARTITION4].flash_allo_size;
         prtkimg->fw[P_PARTITION3].flash_offset = prtkimg->fw[P_PARTITION4].flash_offset - prtkimg->fw[P_PARTITION3].flash_allo_size;
         prtkimg->fw[P_PARTITION2].flash_offset = prtkimg->fw[P_PARTITION3].flash_offset - prtkimg->fw[P_PARTITION2].flash_allo_size;
         prtkimg->fw[P_PARTITION1].flash_offset = prtkimg->fw[P_PARTITION2].flash_offset - prtkimg->fw[P_PARTITION1].flash_allo_size;
         flash_top_low_limit = prtkimg->fw[P_PARTITION1].flash_offset;

         // partition layout: etc, rootfs
         // layout buttom up
         //prtkimg->fw[FW_BOOTCODE].flash_offset = 0;
         prtkimg->fw[FW_USR_LOCAL_ETC].flash_offset = fw_part_flash_bottom_start;
         prtkimg->fw[FW_ROOTFS].flash_offset = prtkimg->fw[FW_USR_LOCAL_ETC].flash_offset + prtkimg->fw[FW_USR_LOCAL_ETC].flash_allo_size;
         flash_bottom_high_limit = prtkimg->fw[FW_ROOTFS].flash_offset + prtkimg->fw[FW_ROOTFS].flash_allo_size;

      }

      // burn firmware/partition method
      prtkimg->eburn = BURN_BY_MTDBLOCK;
#ifdef NAS_ENABLE
      return _eRTK_SUCCESS;
#endif
   }
#ifdef EMMC_SUPPORT
   else if (prtkimg->flash_type == MTD_EMMC)
   {
#ifdef __OFFLINE_GENERATE_BIN__
      prtkimg->bootcode_size = prtkimg->factory_start;
#endif

      const unsigned int _8K_BYTE = 8*1024;
      const unsigned int _32K_BYTE = 32*1024;
      const unsigned int _256K_BYTE = 256*1024;
      const unsigned int _4M_BYTE =	4*1024*1024U;
      const unsigned int _5M_BYTE =	5*1024*1024U;
	  const unsigned int _13M_BYTE = 13*1024*1024U;     
	  const unsigned int _16M_BYTE = 16*1024*1024U;

	  // there is no boundary issue for eMMC, but it is easy to r/w with 512-byte aligned
      prtkimg->fw[FW_BOOTCODE].img_size = prtkimg->bootcode_size;
      prtkimg->fw[FW_BOOTCODE].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_BOOTCODE].img_size,512);
      for(efwtype=FW_KERNEL; efwtype<FW_ROOTFS; efwtype=FWTYPE(efwtype+1)) {
        prtkimg->fw[efwtype].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[efwtype].img_size, prtkimg->mtd_erasesize);
      }
      // reserve 40K byte for RESCUE_DT/KERNEL_DT.
      prtkimg->fw[FW_RESCUE_DT].flash_allo_size = _256K_BYTE;
      prtkimg->fw[FW_KERNEL_DT].flash_allo_size = _256K_BYTE;

#ifdef NFLASH_LAOUT
      // reserve fw_size for Golden fw.
      prtkimg->fw[FW_UBOOT].flash_allo_size = _4M_BYTE;
      prtkimg->fw[FW_IMAGE_BOOTFILE].flash_allo_size = _16M_BYTE;
      prtkimg->fw[FW_GOLD_RESCUE_DT].flash_allo_size = _256K_BYTE;
      prtkimg->fw[FW_GOLD_AUDIO].flash_allo_size = _5M_BYTE;
      prtkimg->fw[FW_GOLD_RESCUE_ROOTFS].flash_allo_size = _13M_BYTE;
      prtkimg->fw[FW_GOLD_KERNEL].flash_allo_size = _16M_BYTE;
#ifdef TEE_ENABLE
      if (prtkimg->fw[FW_BL31].img_size > 0)
         prtkimg->fw[FW_BL31].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_BL31].img_size,512);
#endif
#endif
#ifdef NAS_ENABLE
#ifdef HYPERVISOR
      if (prtkimg->fw[FW_XEN].img_size > 0)
         prtkimg->fw[FW_XEN].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[FW_XEN].img_size, 512);
#endif
#endif
      // fill install av count firmware desc
      if ((ret = fill_rtk_install_av_count(prtkimg)) < 0) {
         return -1;
      }

#ifdef NFLASH_LAOUT
#ifdef USE_SE_STORAGE
      prtkimg->se_storage_section_size = prtkimg->se_storageSize; //prtkimg->se_storageSize * 2;
      prtkimg->fw[FW_SE_STORAGE].flash_offset = prtkimg->se_storage_Start;
      prtkimg->fw[FW_SE_STORAGE].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->se_storage_section_size, 512);
      prtkimg->fw[FW_SE_STORAGE].img_size = prtkimg->fw[FW_SE_STORAGE].flash_allo_size;
#endif
#endif

	  // we use ping-pong for factory partition
      prtkimg->factory_section_size = prtkimg->factory_size * 2;
      prtkimg->fw[FW_FACTORY].flash_offset = prtkimg->factory_start;
      prtkimg->fw[FW_FACTORY].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->factory_section_size, 512);
      prtkimg->fw[FW_FACTORY].img_size = prtkimg->fw[FW_FACTORY].flash_allo_size;

	  if (prtkimg->backup == 1) {
		// if backup is enabled, we program another partition except current bootpart
		if (prtkimg->bootpart == 1)
			prtkimg->reserved_boot_size = prtkimg->fw[FW_FACTORY].flash_offset + prtkimg->fw[FW_FACTORY].flash_allo_size;
		else
			prtkimg->reserved_boot_size = prtkimg->fw[FW_FACTORY].flash_offset + prtkimg->fw[FW_FACTORY].flash_allo_size + 0x08000000;
	  }
	  else
      	prtkimg->reserved_boot_size = prtkimg->fw[FW_FACTORY].flash_offset + prtkimg->fw[FW_FACTORY].flash_allo_size;

      prtkimg->reserved_boottable_size = _32K_BYTE;

      //firmware desc table, only for EMMC
      prtkimg->fw[FW_FW_TBL].flash_offset = prtkimg->reserved_boot_size;
      prtkimg->fw[FW_FW_TBL].flash_allo_size = prtkimg->reserved_boottable_size;

      prtkimg->fw[FW_GOLD_FW_TBL].flash_offset = prtkimg->fw[FW_FW_TBL].flash_offset + prtkimg->reserved_boottable_size;
      prtkimg->fw[FW_GOLD_FW_TBL].flash_allo_size = prtkimg->reserved_boottable_size;

      fw_part_flash_bottom_start = prtkimg->fw[FW_GOLD_FW_TBL].flash_offset + prtkimg->fw[FW_GOLD_FW_TBL].flash_allo_size;
      fw_part_flash_top_end = prtkimg->flash_size;

      // layout buttom up
      prtkimg->fw[FW_BOOTCODE].flash_offset = prtkimg->bootcode_start;

#ifdef NFLASH_LAOUT
      prtkimg->fw[FW_GOLD_RESCUE_DT].flash_offset = fw_part_flash_bottom_start + prtkimg->mtd_erasesize;
      prtkimg->fw[FW_GOLD_AUDIO].flash_offset = prtkimg->fw[FW_GOLD_RESCUE_DT].flash_offset + prtkimg->fw[FW_GOLD_RESCUE_DT].flash_allo_size;
      prtkimg->fw[FW_GOLD_KERNEL].flash_offset = prtkimg->fw[FW_GOLD_AUDIO].flash_offset + prtkimg->fw[FW_GOLD_AUDIO].flash_allo_size;
      prtkimg->fw[FW_GOLD_RESCUE_ROOTFS].flash_offset = prtkimg->fw[FW_GOLD_KERNEL].flash_offset + prtkimg->fw[FW_GOLD_KERNEL].flash_allo_size;
      //prtkimg->fw[FW_UBOOT].flash_offset = prtkimg->fw[FW_GOLD_RESCUE_ROOTFS].flash_offset + prtkimg->fw[FW_GOLD_RESCUE_ROOTFS].flash_allo_size; + _8K_BYTE;
      //prtkimg->fw[FW_IMAGE_BOOTFILE].flash_offset = prtkimg->fw[FW_UBOOT].flash_offset + prtkimg->fw[FW_UBOOT].flash_allo_size + _8K_BYTE;
#endif

#ifdef NAS_ENABLE
      /* Add one block for rescue fwdesc table */
#ifdef NFLASH_LAOUT
      prtkimg->fw[FW_NAS_RESCUE_DT].flash_offset = prtkimg->fw[FW_GOLD_RESCUE_ROOTFS].flash_offset + prtkimg->fw[FW_GOLD_RESCUE_ROOTFS].flash_allo_size;
#else
      prtkimg->fw[FW_NAS_RESCUE_DT].flash_offset = fw_part_flash_bottom_start + prtkimg->mtd_erasesize;
#endif
      prtkimg->fw[FW_NAS_KERNEL].flash_offset = prtkimg->fw[FW_NAS_RESCUE_DT].flash_offset + prtkimg->fw[FW_NAS_RESCUE_DT].flash_allo_size;
      prtkimg->fw[FW_NAS_RESCUE_ROOTFS].flash_offset = prtkimg->fw[FW_NAS_KERNEL].flash_offset + prtkimg->fw[FW_NAS_KERNEL].flash_allo_size;
      prtkimg->fw[FW_RESCUE_DT].flash_offset = prtkimg->fw[FW_NAS_RESCUE_ROOTFS].flash_offset + prtkimg->fw[FW_NAS_RESCUE_ROOTFS].flash_allo_size;
      /* no rescue fwdesc if there's no rescue DTB/kernel/rootfs */
      if(prtkimg->fw[FW_RESCUE_DT].flash_offset == prtkimg->fw[FW_NAS_RESCUE_DT].flash_offset)
#endif
#ifdef NFLASH_LAOUT
	  prtkimg->fw[FW_RESCUE_DT].flash_offset = prtkimg->fw[FW_GOLD_RESCUE_ROOTFS].flash_offset + prtkimg->fw[FW_GOLD_RESCUE_ROOTFS].flash_allo_size;
      //prtkimg->fw[FW_RESCUE_DT].flash_offset = prtkimg->fw[FW_IMAGE_BOOTFILE].flash_offset + prtkimg->fw[FW_IMAGE_BOOTFILE].flash_allo_size;
      //prtkimg->fw[FW_RESCUE_DT].flash_offset = prtkimg->fw[FW_UBOOT].flash_offset + prtkimg->fw[FW_UBOOT].flash_allo_size;
#else
      prtkimg->fw[FW_RESCUE_DT].flash_offset = fw_part_flash_bottom_start;
#endif
      prtkimg->fw[FW_KERNEL_DT].flash_offset = prtkimg->fw[FW_RESCUE_DT].flash_offset + prtkimg->fw[FW_RESCUE_DT].flash_allo_size;
      prtkimg->fw[FW_AUDIO].flash_offset = prtkimg->fw[FW_KERNEL_DT].flash_offset + prtkimg->fw[FW_KERNEL_DT].flash_allo_size;
      prtkimg->fw[FW_KERNEL].flash_offset = prtkimg->fw[FW_AUDIO].flash_offset + prtkimg->fw[FW_AUDIO].flash_allo_size;
#ifdef TEE_ENABLE
      prtkimg->fw[FW_TEE].flash_offset = prtkimg->fw[FW_KERNEL].flash_offset + prtkimg->fw[FW_KERNEL].flash_allo_size;
      prtkimg->fw[FW_BL31].flash_offset = prtkimg->fw[FW_TEE].flash_offset + prtkimg->fw[FW_TEE].flash_allo_size;
      prtkimg->fw[FW_RESCUE_ROOTFS].flash_offset = prtkimg->fw[FW_BL31].flash_offset + prtkimg->fw[FW_BL31].flash_allo_size;
#else
      prtkimg->fw[FW_RESCUE_ROOTFS].flash_offset = prtkimg->fw[FW_KERNEL].flash_offset + prtkimg->fw[FW_KERNEL].flash_allo_size;
#endif
      prtkimg->fw[FW_KERNEL_ROOTFS].flash_offset = prtkimg->fw[FW_RESCUE_ROOTFS].flash_offset + prtkimg->fw[FW_RESCUE_ROOTFS].flash_allo_size;
      prtkimg->fw[FW_AUDIO_BOOTFILE].flash_offset = prtkimg->fw[FW_KERNEL_ROOTFS].flash_offset + prtkimg->fw[FW_KERNEL_ROOTFS].flash_allo_size;
      prtkimg->fw[FW_AUDIO_CLOGO1].flash_offset = prtkimg->fw[FW_AUDIO_BOOTFILE].flash_offset + prtkimg->fw[FW_AUDIO_BOOTFILE].flash_allo_size;
      prtkimg->fw[FW_AUDIO_CLOGO2].flash_offset = prtkimg->fw[FW_AUDIO_CLOGO1].flash_offset + prtkimg->fw[FW_AUDIO_CLOGO1].flash_allo_size;
#ifndef NFLASH_LAOUT
	  prtkimg->fw[FW_IMAGE_BOOTFILE].flash_offset = prtkimg->fw[FW_AUDIO_CLOGO2].flash_offset + prtkimg->fw[FW_AUDIO_CLOGO2].flash_allo_size;
	  prtkimg->fw[FW_IMAGE_CLOGO1].flash_offset = prtkimg->fw[FW_IMAGE_BOOTFILE].flash_offset + prtkimg->fw[FW_IMAGE_BOOTFILE].flash_allo_size;
#else
      prtkimg->fw[FW_IMAGE_CLOGO1].flash_offset = prtkimg->fw[FW_AUDIO_CLOGO2].flash_offset + prtkimg->fw[FW_AUDIO_CLOGO2].flash_allo_size;
#endif
      prtkimg->fw[FW_IMAGE_CLOGO2].flash_offset = prtkimg->fw[FW_IMAGE_CLOGO1].flash_offset + prtkimg->fw[FW_IMAGE_CLOGO1].flash_allo_size;
      prtkimg->fw[FW_VIDEO_BOOTFILE].flash_offset = prtkimg->fw[FW_IMAGE_CLOGO2].flash_offset + prtkimg->fw[FW_IMAGE_CLOGO2].flash_allo_size;
      prtkimg->fw[FW_VIDEO_CLOGO1].flash_offset = prtkimg->fw[FW_VIDEO_BOOTFILE].flash_offset + prtkimg->fw[FW_VIDEO_BOOTFILE].flash_allo_size;
      prtkimg->fw[FW_VIDEO_CLOGO2].flash_offset = prtkimg->fw[FW_VIDEO_CLOGO1].flash_offset + prtkimg->fw[FW_VIDEO_CLOGO1].flash_allo_size;
#ifdef NAS_ENABLE
#ifdef HYPERVISOR
      prtkimg->fw[FW_XEN].flash_offset = prtkimg->fw[FW_VIDEO_CLOGO2].flash_offset + prtkimg->fw[FW_VIDEO_CLOGO2].flash_allo_size;
      flash_bottom_high_limit = prtkimg->fw[FW_XEN].flash_offset + prtkimg->fw[FW_XEN].flash_allo_size;
#else
      flash_bottom_high_limit = prtkimg->fw[FW_VIDEO_CLOGO2].flash_offset + prtkimg->fw[FW_VIDEO_CLOGO2].flash_allo_size;
#endif
#else
      flash_bottom_high_limit = prtkimg->fw[FW_VIDEO_CLOGO2].flash_offset + prtkimg->fw[FW_VIDEO_CLOGO2].flash_allo_size;
#endif

#if defined(GENERIC_LINUX)
      if (prtkimg->fw[FW_UBOOT].img_size != 0)
         prtkimg->fw[FW_UBOOT].flash_offset = 0x5024A00;
#endif

      // layout top down
      FWTYPE prevFW = FW_UNKNOWN;
      for(i=NUM_RTKPART-1;  i>=0; i--) {
        if( ! rtk_part_list_sort[i] )
            continue;

        efwtype = rtk_part_list_sort[i]->efwtype;
		/*
		install_ui("\r\n[WST_D]==> efwtype=[%d].\r\n", efwtype);
		if (efwtype == P_PARTITION1){
			install_ui("\r\n[WST_D]==> efwtype=[P_PARTITION1].\r\n");
			prtkimg->fw[efwtype].flash_offset = prtkimg->fw[FW_UBOOT].flash_offset - _8K_BYTE;
			continue;
		}
		
		else if (efwtype == P_PARTITION2){
			install_ui("\r\n[WST_D]==> efwtype=[P_PARTITION2].\r\n");
			prtkimg->fw[efwtype].flash_offset = prtkimg->fw[FW_IMAGE_BOOTFILE].flash_offset - _8K_BYTE;
			continue;
		}
		*/
        if( prevFW == FW_UNKNOWN ) {
            prtkimg->fw[efwtype].flash_offset = fw_part_flash_top_end - prtkimg->fw[efwtype].flash_allo_size;
			if (strcmp(prtkimg->fw[efwtype].part_name, "logo") == 0)
				prtkimg->fw[FW_IMAGE_BOOTFILE].flash_offset = prtkimg->fw[efwtype].flash_offset+_8K_BYTE; //8kB is mbr length
        } else {
            install_ui("\r\n[Installer_D]==> prtkimg->fw[efwtype].part_name=[%s].\r\n", prtkimg->fw[efwtype].part_name);
            #if 0
            if (strcmp(prtkimg->fw[efwtype].part_name, "data") == 0) {
                if (prtkimg->flash_size == (unsigned long long)prtkimg->mtd_erasesize*0x720000) //emmc 4GB
                    prtkimg->fw[efwtype].flash_allo_size = (unsigned long long)0x7E300000;
                else if (prtkimg->flash_size == (unsigned long long)prtkimg->mtd_erasesize*0xe40000) { //emmc 8GB
                #ifdef NAS_ENABLE
                    prtkimg->fw[efwtype].flash_allo_size = (unsigned long long)0x142600000;
                #else
                    prtkimg->fw[efwtype].flash_allo_size = (unsigned long long)0x14EE00000;
                #endif
                }
                else if (prtkimg->flash_size == (unsigned long long)prtkimg->mtd_erasesize*0x1C80000) //emmc 16GB
                    prtkimg->fw[efwtype].flash_allo_size = (unsigned long long)0x272100000;
                else if (prtkimg->flash_size == (unsigned long long)prtkimg->mtd_erasesize*0x3900000) //emmc 32GB
                    prtkimg->fw[efwtype].flash_allo_size = (unsigned long long)0x5B4D00000;
            }
            #endif
            prtkimg->fw[efwtype].flash_offset = prtkimg->fw[prevFW].flash_offset - prtkimg->fw[efwtype].flash_allo_size;
			if (strcmp(prtkimg->fw[efwtype].part_name, "uboot") == 0)
				prtkimg->fw[FW_UBOOT].flash_offset = 0x5024A00;//prtkimg->fw[efwtype].flash_offset+_8K_BYTE; //8kB is mbr length
			else if (strcmp(prtkimg->fw[efwtype].part_name, "logo") == 0)
				prtkimg->fw[FW_IMAGE_BOOTFILE].flash_offset = prtkimg->fw[efwtype].flash_offset+_8K_BYTE; //8kB is mbr length
        }
        prevFW = efwtype;
      }
      flash_top_low_limit = prtkimg->fw[prevFW].flash_offset;

      // burn firmware/partition method
      prtkimg->eburn = BURN_BY_MMCBLOCK;
   }
#endif

    else if (prtkimg->flash_type == MTD_SATA) {
        const unsigned int _512_BYTE = 512; // 1 sector = 512B

        for(efwtype=FW_KERNEL; efwtype<FW_ROOTFS; efwtype=FWTYPE(efwtype+1)) {
            prtkimg->fw[efwtype].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[efwtype].img_size, prtkimg->mtd_erasesize);
        }

        for(efwtype=FW_GOLD_RESCUE_DT; efwtype<FW_UNKNOWN; efwtype=FWTYPE(efwtype+1)) {
            prtkimg->fw[efwtype].flash_allo_size = SIZE_ALIGN_BOUNDARY_MORE(prtkimg->fw[efwtype].img_size, prtkimg->mtd_erasesize);
        }
        
        prtkimg->fw[FW_FW_TBL].flash_offset = 0x22 * _512_BYTE;
        prtkimg->fw[FW_FW_TBL].flash_allo_size = 0x3DE * _512_BYTE;
        prtkimg->fw[FW_GOLD_FW_TBL].flash_offset = 0x400 * _512_BYTE;
        prtkimg->fw[FW_GOLD_FW_TBL].flash_allo_size = 0x400 * _512_BYTE;
        prtkimg->fw[FW_KERNEL].flash_offset = 0x800 * _512_BYTE;
        prtkimg->fw[FW_KERNEL_ROOTFS].flash_offset = 0x10800 * _512_BYTE;
        prtkimg->fw[FW_RESCUE_ROOTFS].flash_offset = 0x20800 * _512_BYTE;
        prtkimg->fw[FW_KERNEL_DT].flash_offset = 0x30800 * _512_BYTE;
        prtkimg->fw[FW_RESCUE_DT].flash_offset = 0x31000 * _512_BYTE;
        prtkimg->fw[FW_AUDIO].flash_offset = 0x31800 * _512_BYTE;
        prtkimg->fw[FW_GOLD_KERNEL].flash_offset = 0x33800 * _512_BYTE;
        prtkimg->fw[FW_GOLD_RESCUE_ROOTFS].flash_offset = 0x43800 * _512_BYTE;
        prtkimg->fw[FW_GOLD_RESCUE_DT].flash_offset = 0x64000 * _512_BYTE;
        prtkimg->fw[FW_GOLD_AUDIO].flash_offset = 0x64800 * _512_BYTE;
        prtkimg->fw[FW_BOOTCODE].flash_offset = 0x66800 * _512_BYTE; //uboot32 on SATA HDD
        prtkimg->fw[FW_UBOOT].flash_offset = 0x67000 * _512_BYTE; //uboot64 on SATA HDD
        prtkimg->fw[FW_BL31].flash_offset = 0x67800 * _512_BYTE;
        prtkimg->fw[FW_TEE].flash_offset = 0x68000 * _512_BYTE;

        for(i=NUM_RTKPART-1;  i>=0; i--) {
            if( ! rtk_part_list_sort[i] )
                continue;

            efwtype = rtk_part_list_sort[i]->efwtype;
            if (efwtype == FW_SYSTEM) {               
                prtkimg->fw[FW_SYSTEM].flash_offset = (unsigned long long)0xCC800 * _512_BYTE;
            }
            else if (efwtype == FW_DATA) {               
                prtkimg->fw[FW_DATA].flash_offset = (unsigned long long)0x2CC800 * _512_BYTE;
            }
            else if (efwtype == FW_CACHE) {               
                prtkimg->fw[FW_CACHE].flash_offset = (unsigned long long)0x12CC800 * _512_BYTE;
            }
            else if (strcmp(prtkimg->fw[efwtype].part_name, "logo") == 0) {
                prtkimg->fw[FW_IMAGE_BOOTFILE].flash_offset = (unsigned long long)0x13CC800 * _512_BYTE;
                prtkimg->fw[efwtype].flash_offset = (unsigned long long)0x13CC800 * _512_BYTE;
            }
            else if (strcmp(prtkimg->fw[efwtype].part_name, "backup") == 0) {
                prtkimg->fw[efwtype].flash_offset = (unsigned long long)0x13D4800 * _512_BYTE;
            }
            else if (strcmp(prtkimg->fw[efwtype].part_name, "verify") == 0) {
                prtkimg->fw[efwtype].flash_offset = (unsigned long long)0x14CE800 * _512_BYTE;
            }
            else if (strcmp(prtkimg->fw[efwtype].part_name, "uboot") == 0) {
                prtkimg->fw[efwtype].flash_offset = prtkimg->fw[FW_UBOOT].flash_offset;
            }
        }

        prtkimg->eburn = BURN_BY_MMCBLOCK;
        flash_bottom_high_limit = 0x68800 * _512_BYTE;
        flash_top_low_limit = 0xCC800 * _512_BYTE;

        //return _eRTK_SUCCESS;
    }

   if (flash_top_low_limit > flash_bottom_high_limit) {
      prtkimg->fw[FW_P_FREE_SPACE].flash_offset = flash_bottom_high_limit;
      prtkimg->fw[FW_P_FREE_SPACE].flash_allo_size = flash_top_low_limit - flash_bottom_high_limit;

      if (prtkimg->fw[FW_P_FREE_SPACE].flash_offset&(prtkimg->mtd_erasesize-1)) {
         install_warn("Warning!!! Free space start address is not on erase boundary.\r\n");
      }

      if (prtkimg->fw[FW_P_FREE_SPACE].flash_allo_size&(prtkimg->mtd_erasesize-1)) {
         install_warn("Warning!!! Free space end address is not on erase boundary.\r\n");
      }
   }

   install_log("\r\n[system info] flash_type=0x%08x, flash_size=0x%08llx (%llu KB), mtd_erasesize=0x%08x (%u KB), page_size=0x%08x, total_sector=%lld\r\n\r\n" \
      , prtkimg->flash_type, prtkimg->flash_size, prtkimg->flash_size/1024, prtkimg->mtd_erasesize, prtkimg->mtd_erasesize/1024, prtkimg->page_size, prtkimg->flash_size/prtkimg->mtd_erasesize);

   if (prtkimg->only_factory == 1) {
      install_info("[only install factory]\r\n");
      install_ui("\r\n");
      install_info("%15s flash_ofs:0x%08llx ~ flash_end:0x%08llx size_align:0x%08llx(%05llu sectors) img_size:0x%08x (%8u Bytes = %6u KBytes)\r\n"
      , inv_by_fwtype(FW_FACTORY)
      , prtkimg->fw[FW_FACTORY].flash_offset
      , (unsigned long long)prtkimg->fw[FW_FACTORY].flash_offset + prtkimg->fw[FW_FACTORY].flash_allo_size - 1
      , prtkimg->fw[FW_FACTORY].flash_allo_size
      , prtkimg->fw[FW_FACTORY].sector
      , prtkimg->fw[FW_FACTORY].img_size
      , prtkimg->fw[FW_FACTORY].img_size
      , prtkimg->fw[FW_FACTORY].img_size/1024
      );
      return 0;
   }

   unsigned int temp_max, _temp, _sort[FW_UNKNOWN];

   for (i=FW_KERNEL; i<FW_UNKNOWN; i=FWTYPE(i+1)) {
      _sort[i] = i;
   }

   for (i=FW_KERNEL; i<FW_UNKNOWN;i=FWTYPE(i+1)) {
      temp_max = i;
      for (j=i+1;j<FW_UNKNOWN;j=FWTYPE(j+1)) {
         if (prtkimg->fw[_sort[temp_max]].flash_offset < prtkimg->fw[_sort[j]].flash_offset) {
            temp_max = j;
         }
      }
      {
         _temp = _sort[i];
         _sort[i] = _sort[temp_max];
         _sort[temp_max] = _temp;
      }
   }

   flash_total_need_size = 0;

   if(prtkimg->flash_type == MTD_NANDFLASH) {
      flash_total_need_size += prtkimg->reserved_remapping_size;
      install_debug("reserved size=%llx\r\n", flash_total_need_size);
   }

   for(efwtype=FW_KERNEL; efwtype<FW_UNKNOWN; efwtype=FWTYPE(efwtype+1)) {
      if(0 == prtkimg->fw[FWTYPE(_sort[efwtype])].flash_allo_size) continue;
      install_debug("%15s flash_ofs:0x%010llx ~ flash_end:0x%010llx size_align:0x%010llx(%05llu sectors) img_size:0x%010x(%6u KBytes)\r\n"
      , inv_by_fwtype(FWTYPE(_sort[efwtype]))
      , prtkimg->fw[FWTYPE(_sort[efwtype])].flash_offset
      , prtkimg->fw[FWTYPE(_sort[efwtype])].flash_offset + prtkimg->fw[FWTYPE(_sort[efwtype])].flash_allo_size - 1
      , prtkimg->fw[FWTYPE(_sort[efwtype])].flash_allo_size
      , prtkimg->fw[FWTYPE(_sort[efwtype])].sector
      , prtkimg->fw[FWTYPE(_sort[efwtype])].img_size
      , prtkimg->fw[FWTYPE(_sort[efwtype])].img_size/1024
      );

      if(_sort[efwtype] == FW_P_FREE_SPACE || _sort[efwtype] == FW_P_SSU_WORKING || _sort[efwtype] == FW_UBOOT) continue;
      flash_total_need_size += prtkimg->fw[FWTYPE(_sort[efwtype])].flash_allo_size;
      if( efwtype <= FW_USR_LOCAL_ETC ){
#ifdef NAS_ENABLE
          if( _sort[efwtype] >= FW_ROOTFS && _sort[efwtype] <= FW_USR_LOCAL_ETC){
            prtkimg->total_alloc_size += prtkimg->fw[FWTYPE(_sort[efwtype])].img_size;
          }
          else
#endif
          prtkimg->total_alloc_size += prtkimg->fw[FWTYPE(_sort[efwtype])].flash_allo_size;//prtkimg->fw[FWTYPE(_sort[efwtype])].img_size;
      //install_test("flash_total_need_size = %u (%uKBytes) 0x%X.\r\n", flash_total_need_size, flash_total_need_size/1024, flash_total_need_size);
      }
   }

   install_ui("\r\n");
   for(efwtype=FW_KERNEL; efwtype<=FW_BOOTCODE; efwtype=FWTYPE(efwtype+1)) {
      if(0 == prtkimg->fw[efwtype].mem_offset) continue;
      install_info("%15s target_offset:0x%08x tarfile_offset:0x%08x\r\n"
            , inv_by_fwtype(efwtype)
            , prtkimg->fw[efwtype].mem_offset
            , prtkimg->fw[efwtype].tarfile_offset);
   }
   fflush(stdout);
   fflush(stderr);

   //SSU layout size check
   if (flash_total_need_size > (fw_part_flash_top_end - fw_part_flash_bottom_start + prtkimg->fw[FW_BOOTCODE].flash_allo_size + prtkimg->fw[FW_FACTORY].flash_allo_size) && prtkimg->safe_upgrade == 1)
   {
      install_fail("\r\nspace is not enough in SSU layout.\r\n" \
                   "total is %llu bytes (%lluKbytes). SSU size is %llu bytes (%lluKbytes).\r\n" \
                   "need %llu bytes (%lluKbytes) more.\r\n\r\n" \
                   , flash_total_need_size
                   , flash_total_need_size/1024
                   , fw_part_flash_top_end - fw_part_flash_bottom_start + prtkimg->fw[FW_BOOTCODE].flash_allo_size + prtkimg->fw[FW_FACTORY].flash_allo_size
                   , (fw_part_flash_top_end - fw_part_flash_bottom_start + prtkimg->fw[FW_BOOTCODE].flash_allo_size + prtkimg->fw[FW_FACTORY].flash_allo_size)/1024
                   , flash_total_need_size - (fw_part_flash_top_end - fw_part_flash_bottom_start + prtkimg->fw[FW_BOOTCODE].flash_allo_size + prtkimg->fw[FW_FACTORY].flash_allo_size)
                   , (flash_total_need_size - (fw_part_flash_top_end - fw_part_flash_bottom_start + prtkimg->fw[FW_BOOTCODE].flash_allo_size + prtkimg->fw[FW_FACTORY].flash_allo_size))/1024);
         return -_eFILL_RTK_IMGDESC_LAYOUT_FAIL_SIZE_NOT_ENOUGH;
   }

   // layout space check
   if (flash_total_need_size > prtkimg->flash_size && prtkimg->safe_upgrade == 0)
   {
      install_fail("\r\nspace is not enough in layout.\r\n" \
                   "total is %llu bytes (%lluKbytes). flash size is %llu bytes (%lluKbytes).\r\n" \
                   "need %llu bytes (%lluKbytes) more.\r\n\r\n" \
                   , flash_total_need_size
                   , flash_total_need_size/1024
                   , prtkimg->flash_size
                   , prtkimg->flash_size/1024
                   , flash_total_need_size - prtkimg->flash_size
                   , (flash_total_need_size - prtkimg->flash_size)/1024);
      return -_eFILL_RTK_IMGDESC_LAYOUT_FAIL_SIZE_NOT_ENOUGH;
   }

   install_ui("\r\n[Layout View]\r\n");

   #ifdef RESERVED_AREA
   if(prtkimg->flash_type == MTD_NANDFLASH) {
       install_ui("   +---------------------------------+ 0x%012llx\r\n", prtkimg->flash_size);
       install_ui("   |%12s (%10lluKBytes)  |\r\n", "RESERVED", prtkimg->reserved_remapping_size/1024);
   }
   #endif
   for(efwtype = FW_KERNEL; efwtype < FW_UNKNOWN; efwtype = FWTYPE(efwtype+1))
   {
		if (0 == prtkimg->fw[FWTYPE(_sort[efwtype])].flash_allo_size) continue;

		if ((FWTYPE(_sort[efwtype]) == FW_UBOOT) || (FWTYPE(_sort[efwtype]) == FW_IMAGE_BOOTFILE)) continue;

		if (flash_start == 0xffffffffffffffffLL) {
		install_ui("   +---------------------------------+ 0x%012llx\r\n",
			prtkimg->fw[FWTYPE(_sort[efwtype])].flash_offset + prtkimg->fw[FWTYPE(_sort[efwtype])].flash_allo_size);
		}
		else
			install_ui("   +---------------------------------+ 0x%012llx\r\n", flash_start);

		unsigned int install = 1;
		switch(FWTYPE(_sort[efwtype])) {
		case FW_RESCUE_DT:
		case FW_KERNEL_DT:
			install = prtkimg->install_dtb;
			break;
		case FW_FACTORY:
			install = prtkimg->install_factory;
			break;
		case FW_BOOTCODE:
			install = prtkimg->bootcode;
			break;
		default:
			break;
		}
		if( install == 1 ) {
			install_ui("   |%12s (%10lluKBytes)  |\r\n", inv_by_fwtype(FWTYPE(_sort[efwtype])), prtkimg->fw[FWTYPE(_sort[efwtype])].flash_allo_size/1024);
		} else {
			install_ui("   |##%10s (%10lluKBytes)  |\r\n", inv_by_fwtype(FWTYPE(_sort[efwtype])), prtkimg->fw[FWTYPE(_sort[efwtype])].flash_allo_size/1024);
		}

		flash_start = prtkimg->fw[FWTYPE(_sort[efwtype])].flash_offset;
   }
   install_ui("   +---------------------------------+ 0x%012x\r\n", 0);

   install_info("\r\ntotal layout is %llu bytes (%lluKbytes).\r\n" \
               "free space %llu bytes (%lluKbytes).\r\n\r\n" \
                , flash_bottom_high_limit + prtkimg->flash_size - flash_top_low_limit
                , (flash_bottom_high_limit + prtkimg->flash_size - flash_top_low_limit)/1024
                , flash_top_low_limit - flash_bottom_high_limit
                , (flash_top_low_limit - flash_bottom_high_limit)/1024);

#ifdef ENABLE_ERASE_CHECK
    install_debug("So far we get %d need not erase!\n", nErasePartCnt);
    bool eraseAll = false, bFound;
    if (factory_load(NULL, prtkimg) == 0) {
        char *parsedStrPtr = NULL, *pch = NULL;
        char *offset = NULL, *size = NULL, *partName = NULL;
        FILE* fp = fopen("/tmp/factory/layout.txt", "rb");

        char buffer[512];
        if (fp) {
            for (i=0; i<nErasePartCnt; i++) {
                bFound = false;
                install_debug("ERASE_CHECK: Check [%d] part(id:%d/ %s), flash_offset:(0x%016llx), size(0x%010llx)\n", i,nErasePartIdxList[i], prtkimg->fw[nErasePartIdxList[i]].part_name,prtkimg->fw[nErasePartIdxList[i]].flash_offset,prtkimg->fw[nErasePartIdxList[i]].flash_allo_size);
                while (fgets(buffer, sizeof(buffer)-1, fp)) {
                     if(0 == strncmp("#define PART", buffer, 12)) {
                        parsedStrPtr = strchr(buffer, '"');
                        if (parsedStrPtr) {
                            pch = strtok (parsedStrPtr+1," ");
                            offset = strchr(pch, '=')+1;
                            pch = strtok (NULL, " ");
                            size = strchr(pch, '=')+1;
                            pch = strtok (NULL, " "); //mount_point
                            pch = strtok (NULL, " "); //mount_dev
                            pch = strtok (NULL, " "); //filesystem
                            pch = strtok (NULL, " "); //part_name
                            partName =  strchr(pch, '=')+1;
                            if (partName && size && offset) {
                                install_debug("ERASE_CHECK: Read part(%s), flash_offset(%s/0x%016llx), size(%s/ 0x%010llx)\n", partName, offset, strtoull(offset, NULL, 16),  size,strtoull(size, NULL,16));
                                if (strcmp(prtkimg->fw[nErasePartIdxList[i]].part_name, partName) == 0) {
                                    bFound = true;
                                    if (prtkimg->fw[nErasePartIdxList[i]].flash_offset  != strtoull(offset, NULL, 16) || prtkimg->fw[nErasePartIdxList[i]].flash_allo_size != strtoull(size, NULL,16)) {
                                        prtkimg->fw[nErasePartIdxList[i]].erased = 1;
                                        install_log("ERASE_CHECK: Part [%s] Should Change to erase\n",partName);
                                    } else {
                                        install_log("ERASE_CHECK: Part [%s] is the same as previous(erase_bit:%d)\n", partName,prtkimg->fw[nErasePartIdxList[i]].erased);
                                        break;
                                    }
                                }
                            }
                        }
                     }
                }
                if (!bFound) {
                    // means you add new partition this time
                    prtkimg->fw[nErasePartIdxList[i]].erased = 1;
                     install_log("ERASE_CHECK: New Partition (%s)!Should Change to erase\n",prtkimg->fw[nErasePartIdxList[i]].part_name);
                }
                rewind(fp);
            }
        } else {
            install_log("ERASE_CHECK: Load layout.txt fail, format each partition first\n");
            eraseAll = true;
        }
    } else {
        install_log("ERASE_CHECK: Load factory fail\n");
        eraseAll = true;
    }

    if (eraseAll) {
        for (i = 0; i<nErasePartCnt ; i++) {
            prtkimg->fw[nErasePartIdxList[i]].erased = 1;
            install_debug("%15s  bErased(%d)\n", inv_by_fwtype(FWTYPE(nErasePartIdxList[i])), prtkimg->fw[nErasePartIdxList[i]].erased);
        }
    }

#endif

   //check etc partition
   if (prtkimg->update_etc == 0)
   {
      if (pboottable != NULL)
      {
         if ((etc_index = get_index_by_partname(pboottable, "etc")) < 0)
         {
            install_log("update_etc=n, but cannot find etc partition in boottable\r\n");
            //etc not in boottable force update etc
            prtkimg->update_etc = 1;
         }
         else if (prtkimg->flash_type == MTD_NANDFLASH)
         {
            //etc partition is already in bootable, check if configure size is the same as boottable.
            if (pboottable->part.list[etc_index].loc.size != prtkimg->fw[FW_USR_LOCAL_ETC].flash_allo_size)
            {
               install_fail("update_etc=n, but configure size %lld not the same as it in boottable size %d!\r\n"
                  , prtkimg->fw[FW_USR_LOCAL_ETC].flash_allo_size, pboottable->part.list[etc_index].loc.size);
               return -_eRTK_TAG_UPDATE_ETC_FAIL_CONFIG_SIZE;
            }
            //etc partition is already in bootable, check if offset is the same as boottable.
            if (pboottable->part.list[etc_index].loc.offset != prtkimg->fw[FW_USR_LOCAL_ETC].flash_offset)
            {
               install_fail("update_etc=n, but offset 0x%08llx not the same as it in boottable 0x%08llx!\r\n"
                  , prtkimg->fw[FW_USR_LOCAL_ETC].flash_offset, pboottable->part.list[etc_index].loc.offset);
               return -_eRTK_TAG_UPDATE_ETC_FAIL_BOOTTABLE_OFFSET;
            }
         }
         else
         {
            snprintf(path, sizeof(path), "/mnt/old/usr/local/etc/");
            if (!access(path, F_OK))
            {
               //chroot install
            }
            else
            {
               //rescue linux install
               //etc partition is already in bootable, check if configure size is the same as boottable.
               if (pboottable->part.list[etc_index].loc.size != prtkimg->fw[FW_USR_LOCAL_ETC].flash_allo_size)
               {
                  install_fail("update_etc=n, but configure size %lld not the same as it in boottable size %d!\r\n"
                     , prtkimg->fw[FW_USR_LOCAL_ETC].flash_allo_size, pboottable->part.list[etc_index].loc.size);
                  return -_eRTK_TAG_UPDATE_ETC_FAIL_CONFIG_SIZE;
               }
               if (pboottable->part.list[etc_index].loc.offset&(prtkimg->mtd_erasesize-1) || pboottable->part.list[etc_index].loc.size&(prtkimg->mtd_erasesize-1))
               {
                  install_fail("boottable etc offset or size not erase boundary.");
                  return -1;
               }

               install_log("backup etc partition offset=0x%llx size=0x%x.\r\n", pboottable->part.list[etc_index].loc.offset, pboottable->part.list[etc_index].loc.size);
               snprintf(cmd, sizeof(cmd), "dd if=%s of=%s/jffs.bak.img bs=%u skip=%llu count=%llu"
                                        , prtkimg->mtdblock_path
                                        , PKG_TEMP
                                        , prtkimg->mtd_erasesize
                                        , pboottable->part.list[etc_index].loc.offset/prtkimg->mtd_erasesize
                                        , pboottable->part.list[etc_index].loc.size/prtkimg->mtd_erasesize);
               ret = rtk_command(cmd, __LINE__, __FILE__, 1);
            }
         }
      }
      else
      {
         //etc not in boottable force update etc
         prtkimg->update_etc = 1;
      }
#if 0
      if (prtkimg->safe_upgrade == 0)
      {
         //etc partition is already in bootable, check if offset is the same as boottable.
         if (pboottable->part.list[etc_index].loc.offset != prtkimg->fw[FW_USR_LOCAL_ETC].flash_offset)
         {
            install_fail("update_etc=n, but offset 0x%08x not the same as it in boottable 0x%08x!\r\n"
               , prtkimg->fw[FW_USR_LOCAL_ETC].flash_offset, pboottable->part.list[etc_index].loc.offset);
            return -_RTK_TAG_UPDATE_ETC_FAIL_BOOTTABLE_OFFSET;
         }

      }
#endif
   }

   if (prtkimg->update_cavfile == 0) {

      if (pboottable != NULL)
      {
         switch (prtkimg->install_avfile_count) {
            case 3:
               // backup calogo2
               install_debug("pboottable->fw.list[FWTYPE_CALOGO2].loc.offset=%#llx pboottable->fw.list[FWTYPE_CALOGO2].loc.size=%#x\r\n", pboottable->fw.list[FWTYPE_CALOGO2].loc.offset, pboottable->fw.list[FWTYPE_CALOGO2].loc.size);
               if (pboottable->fw.list[FWTYPE_CALOGO2].loc.size != 0) {
                  if (prtkimg->fw[FW_AUDIO_CLOGO2].flash_allo_size != pboottable->fw.list[FWTYPE_CALOGO2].loc.size) {
                     install_fail("install_avfile_audio_size %dKBytes(%lldKBytes) not the same as %dKBytes", prtkimg->install_avfile_audio_size*1024, prtkimg->fw[FW_AUDIO_CLOGO2].flash_allo_size, pboottable->fw.list[FWTYPE_CALOGO2].loc.size);
                     return -1;
                  }

                  if ((pboottable->fw.list[FWTYPE_CALOGO2].loc.offset == prtkimg->fw[FW_AUDIO_CLOGO2].flash_offset) && (pboottable->fw.list[FWTYPE_CALOGO2].loc.size == prtkimg->fw[FW_AUDIO_CLOGO2].flash_allo_size)) {
                     install_log("caudio logo2 offset=0x%llx size=0x%x same as before, skip backup.\r\n", pboottable->fw.list[FWTYPE_CALOGO2].loc.offset, pboottable->fw.list[FWTYPE_CALOGO2].loc.size);
                     prtkimg->fw[FWTYPE_CALOGO2].install_mode = eSKIP;
                  }
                  else {
                     install_log("backup caudio logo2 offset=0x%llx size=0x%x.\r\n", pboottable->fw.list[FWTYPE_CALOGO2].loc.offset, pboottable->fw.list[FWTYPE_CALOGO2].loc.size);
                     snprintf(cmd, sizeof(cmd), "dd if=%s of=%s/%s bs=%u skip=%llu count=%llu"
                                              , prtkimg->mtdblock_path
                                              , PKG_TEMP
                                              , CUSTOMR_AUDIO_LOGO2_BACKUP_FILENAME
                                              , prtkimg->mtd_erasesize
                                              , pboottable->fw.list[FWTYPE_CALOGO2].loc.offset/prtkimg->mtd_erasesize
                                              , pboottable->fw.list[FWTYPE_CALOGO2].loc.size/prtkimg->mtd_erasesize);
                     ret = rtk_command(cmd, __LINE__, __FILE__, 1);
                     snprintf(prtkimg->fw[FW_AUDIO_CLOGO2].backup_filename, sizeof(prtkimg->fw[FW_AUDIO_CLOGO2].backup_filename), "%s/%s", PKG_TEMP, CUSTOMR_AUDIO_LOGO2_BACKUP_FILENAME);
                     prtkimg->fw[FW_AUDIO_CLOGO2].install_mode = eBACKUP;
                  }
               }

               // backup cvlogo2
               install_debug("pboottable->fw.list[FWTYPE_CVLOGO2].loc.offset=%#llx pboottable->fw.list[FWTYPE_CVLOGO2].loc.size=%#x\r\n", pboottable->fw.list[FWTYPE_CVLOGO2].loc.offset, pboottable->fw.list[FWTYPE_CVLOGO2].loc.size);
               if (pboottable->fw.list[FWTYPE_CVLOGO2].loc.size != 0) {
                  if (prtkimg->fw[FW_VIDEO_CLOGO2].flash_allo_size != pboottable->fw.list[FWTYPE_CVLOGO2].loc.size) {
                     install_fail("install_avfile_video_size %dBytes(%lldBytes) not the same as %dBytes", prtkimg->install_avfile_video_size*1024, prtkimg->fw[FW_VIDEO_CLOGO2].flash_allo_size, pboottable->fw.list[FWTYPE_CVLOGO2].loc.size);
                     return -1;
                  }

                  if ((pboottable->fw.list[FWTYPE_CVLOGO2].loc.offset == prtkimg->fw[FW_VIDEO_CLOGO2].flash_offset) && (pboottable->fw.list[FWTYPE_CVLOGO2].loc.size == prtkimg->fw[FW_VIDEO_CLOGO2].flash_allo_size)) {
                     install_log("cvideo logo2 offset=0x%llx size=0x%x same as before, skip backup.\r\n", pboottable->fw.list[FWTYPE_CVLOGO2].loc.offset, pboottable->fw.list[FWTYPE_CVLOGO2].loc.size);
                     prtkimg->fw[FWTYPE_CVLOGO2].install_mode = eSKIP;
                  }
                  else {
                     install_log("backup cvideo logo2 offset=0x%llx size=0x%x.\r\n", pboottable->fw.list[FWTYPE_CVLOGO2].loc.offset, pboottable->fw.list[FWTYPE_CVLOGO2].loc.size);
                     snprintf(cmd, sizeof(cmd), "dd if=%s of=%s/%s bs=%u skip=%llu count=%llu"
                                              , prtkimg->mtdblock_path
                                              , PKG_TEMP
                                              , CUSTOMR_VIDEO_LOGO2_BACKUP_FILENAME
                                              , prtkimg->mtd_erasesize
                                              , pboottable->fw.list[FWTYPE_CVLOGO2].loc.offset/prtkimg->mtd_erasesize
                                              , pboottable->fw.list[FWTYPE_CVLOGO2].loc.size/prtkimg->mtd_erasesize);
                     ret = rtk_command(cmd, __LINE__, __FILE__, 1);
                     snprintf(prtkimg->fw[FW_VIDEO_CLOGO2].backup_filename, sizeof(prtkimg->fw[FW_VIDEO_CLOGO2].backup_filename), "%s/%s", PKG_TEMP, CUSTOMR_VIDEO_LOGO2_BACKUP_FILENAME);
                     prtkimg->fw[FW_VIDEO_CLOGO2].install_mode = eBACKUP;
                  }
               }
               /* falling througth */
            case 2:
               // backup calogo1
               install_debug("pboottable->fw.list[FWTYPE_CALOGO1].loc.offset=%#llx pboottable->fw.list[FWTYPE_CALOGO1].loc.size=%#x\r\n", pboottable->fw.list[FWTYPE_CALOGO1].loc.offset, pboottable->fw.list[FWTYPE_CALOGO1].loc.size);
               if (pboottable->fw.list[FWTYPE_CALOGO1].loc.size != 0) {
                  if (prtkimg->fw[FW_AUDIO_CLOGO1].flash_allo_size != pboottable->fw.list[FWTYPE_CALOGO1].loc.size) {
                     install_fail("install_avfile_audio_size %dBytes(%lldBytes) not the same as %uBytes", prtkimg->install_avfile_audio_size*1024, prtkimg->fw[FW_AUDIO_CLOGO1].flash_allo_size, pboottable->fw.list[FWTYPE_CALOGO1].loc.size);
                     return -1;
                  }

                  if ((pboottable->fw.list[FWTYPE_CALOGO1].loc.offset == prtkimg->fw[FW_AUDIO_CLOGO1].flash_offset) && (pboottable->fw.list[FWTYPE_CALOGO1].loc.size == prtkimg->fw[FW_AUDIO_CLOGO1].flash_allo_size)) {
                     install_log("caudio logo1 offset=0x%llx size=0x%x same as before, skip backup.\r\n", pboottable->fw.list[FWTYPE_CALOGO1].loc.offset, pboottable->fw.list[FWTYPE_CALOGO1].loc.size);
                     prtkimg->fw[FWTYPE_CALOGO1].install_mode = eSKIP;
                  }
                  else {
                     install_log("backup caudio logo1 offset=0x%llx size=0x%x.\r\n", pboottable->fw.list[FWTYPE_CALOGO1].loc.offset, pboottable->fw.list[FWTYPE_CALOGO1].loc.size);
                     snprintf(cmd, sizeof(cmd), "dd if=%s of=%s/%s bs=%u skip=%llu count=%llu"
                                              , prtkimg->mtdblock_path
                                              , PKG_TEMP
                                              , CUSTOMR_AUDIO_LOGO1_BACKUP_FILENAME
                                              , prtkimg->mtd_erasesize
                                              , pboottable->fw.list[FWTYPE_CALOGO1].loc.offset/prtkimg->mtd_erasesize
                                              , pboottable->fw.list[FWTYPE_CALOGO1].loc.size/prtkimg->mtd_erasesize);
                     ret = rtk_command(cmd, __LINE__, __FILE__, 1);
                     snprintf(prtkimg->fw[FW_AUDIO_CLOGO1].backup_filename, sizeof(prtkimg->fw[FW_AUDIO_CLOGO1].backup_filename), "%s/%s", PKG_TEMP, CUSTOMR_AUDIO_LOGO1_BACKUP_FILENAME);
                     prtkimg->fw[FW_AUDIO_CLOGO1].install_mode = eBACKUP;
                  }
               }

               // backup cvlogo1
               install_debug("pboottable->fw.list[FWTYPE_CVLOGO1].loc.offset=%#llx pboottable->fw.list[FWTYPE_CVLOGO1].loc.size=%#x\r\n", pboottable->fw.list[FWTYPE_CVLOGO1].loc.offset, pboottable->fw.list[FWTYPE_CVLOGO1].loc.size);
               if (pboottable->fw.list[FWTYPE_CVLOGO1].loc.size != 0) {
                  if (prtkimg->fw[FW_VIDEO_CLOGO1].flash_allo_size != pboottable->fw.list[FWTYPE_CVLOGO1].loc.size) {
                     install_fail("install_avfile_video_size %dBytes(%lldBytes) not the same as %dBytes", prtkimg->install_avfile_video_size*1024, prtkimg->fw[FW_VIDEO_CLOGO1].flash_allo_size, pboottable->fw.list[FWTYPE_CVLOGO1].loc.size);
                     return -1;
                  }

                  if ((pboottable->fw.list[FWTYPE_CVLOGO1].loc.offset == prtkimg->fw[FW_VIDEO_CLOGO1].flash_offset) && (pboottable->fw.list[FWTYPE_CVLOGO1].loc.size == prtkimg->fw[FW_VIDEO_CLOGO1].flash_allo_size)) {
                     install_log("cvideo logo1 offset=0x%llx size=0x%x same as before, skip backup.\r\n", pboottable->fw.list[FWTYPE_CVLOGO1].loc.offset, pboottable->fw.list[FWTYPE_CVLOGO1].loc.size);
                     prtkimg->fw[FW_VIDEO_CLOGO1].install_mode = eSKIP;
                  }
                  else {
                     install_log("backup cvideo logo1 offset=0x%llx size=0x%x.\r\n", pboottable->fw.list[FWTYPE_CVLOGO1].loc.offset, pboottable->fw.list[FWTYPE_CVLOGO1].loc.size);
                     snprintf(cmd, sizeof(cmd), "dd if=%s of=%s/%s bs=%u skip=%llu count=%llu"
                                              , prtkimg->mtdblock_path
                                              , PKG_TEMP
                                              , CUSTOMR_VIDEO_LOGO1_BACKUP_FILENAME
                                              , prtkimg->mtd_erasesize
                                              , pboottable->fw.list[FWTYPE_CVLOGO1].loc.offset/prtkimg->mtd_erasesize
                                              , pboottable->fw.list[FWTYPE_CVLOGO1].loc.size/prtkimg->mtd_erasesize);
                     ret = rtk_command(cmd, __LINE__, __FILE__, 1);
                     snprintf(prtkimg->fw[FW_VIDEO_CLOGO1].backup_filename, sizeof(prtkimg->fw[FW_VIDEO_CLOGO1].backup_filename), "%s/%s", PKG_TEMP, CUSTOMR_VIDEO_LOGO1_BACKUP_FILENAME);
                     prtkimg->fw[FW_VIDEO_CLOGO1].install_mode = eBACKUP;
                  }
               }
            case 1:
            case 0:
               break;
            default:
               break;
         }
      }
      else
      {
         //force update?
      }
   }

#ifdef __OFFLINE_GENERATE_BIN__
	if (prtkimg->all_in_one == 0)
		prepare_empty_file(prtkimg->mtd_path, flash_bottom_high_limit, (unsigned char)(prtkimg->erased_content & 0xff));
#endif

   return _eRTK_SUCCESS;
}

// pthead : IN
// tarfile_offset : IN
// prtking : OUT
int fill_rtkimgdesc_file(struct t_tarheader* pthead, unsigned int tarfile_offset, struct t_rtkimgdesc* prtkimg)
{
   // TODO: comatible for the other flash binary utility
   struct t_PARTDESC* rtk_part;
   struct t_FWDESC* rtk_fw;
   struct t_imgdesc* pimgdesc;
   pimgdesc = NULL;

   // firmware file info
   rtk_fw = find_fw_by_filename(pthead->filename);
   if(rtk_fw != NULL)
   {
      install_debug("rtk_fw.filename: %s, filename: %s\r\n", rtk_fw->filename, pthead->filename);
      prtkimg->fw[rtk_fw->efwtype].tarfile_offset = tarfile_offset;
      prtkimg->fw[rtk_fw->efwtype].img_size = octalStringToInt(pthead->filesize, FILESIZELEN);
      sprintf(prtkimg->fw[rtk_fw->efwtype].filename, "%s", pthead->filename);
      return 0;
   }

   // partition file info
   rtk_part = find_part_by_filename((struct t_PARTDESC*)rtk_part_list, pthead->filename);
   if(rtk_part != NULL)
   {
      install_debug("rtk_part.filename: %s, filename: %s\r\n", rtk_part->filename, pthead->filename);
      prtkimg->fw[rtk_part->efwtype].tarfile_offset = tarfile_offset;
      prtkimg->fw[rtk_part->efwtype].img_size = octalStringToInt(pthead->filesize, FILESIZELEN);
      sprintf(prtkimg->fw[rtk_part->efwtype].filename, "%s", pthead->filename);
      return 0;
   }

   // check whether it is utility file or not by filename.
   for(int i=UTIL_FLASHERASE; i<UTIL_MAX; i++ )
   {
      if(strstr(pthead->filename, rtk_util_list[i].pattern) )
      {
         prtkimg->util[i].tarfile_offset = tarfile_offset;
         prtkimg->util[i].img_size = octalStringToInt(pthead->filesize, FILESIZELEN);
         sprintf(prtkimg->util[i].filename, "%s", pthead->filename);
         return 0;
      }
   }

   // check whether it is cipher key or not by filename.
   for(int i=KEY_RSA_PUBLIC; i<KEY_MAX; i++ )
   {
      if(strstr(pthead->filename, rtk_key_list[i].pattern) )
      {
         prtkimg->cipher_key[i].tarfile_offset = tarfile_offset;
         prtkimg->cipher_key[i].img_size = octalStringToInt(pthead->filesize, FILESIZELEN);
         sprintf(prtkimg->cipher_key[i].filename, "%s", pthead->filename);
         return 0;
      }
   }

   if(0 == strcmp(pthead->filename, "config.txt"))
   {
      install_debug("Got config.txt\r\n");
      pimgdesc = &prtkimg->config;
      pimgdesc->tarfile_offset = tarfile_offset;
      pimgdesc->img_size = octalStringToInt(pthead->filesize, FILESIZELEN);
      sprintf(pimgdesc->filename, "%s", pthead->filename);
      return 0;
   }

   if(strstr(pthead->filename, "bootloader.tar"))
   {
      pimgdesc = &prtkimg->bootloader_tar;
      pimgdesc->tarfile_offset = tarfile_offset;
      pimgdesc->img_size = octalStringToInt(pthead->filesize, FILESIZELEN);
      sprintf(pimgdesc->filename, pthead->filename);
      return 0;
   }

   if(strstr(pthead->filename, "factory.tar"))
   {
      pimgdesc = &prtkimg->factory_tar;
      pimgdesc->tarfile_offset = tarfile_offset;
      pimgdesc->img_size = octalStringToInt(pthead->filesize, FILESIZELEN);
      sprintf(pimgdesc->filename, pthead->filename);
      return 0;
   }

   if(strstr(pthead->filename, "postprocess.sh"))
   {
      pimgdesc = &prtkimg->postprocess;
      pimgdesc->tarfile_offset = tarfile_offset;
      pimgdesc->img_size = octalStringToInt(pthead->filesize, FILESIZELEN);
      sprintf(pimgdesc->filename, pthead->filename);
      return 0;
   }

   if(strstr(pthead->filename, "customer"))
   {
      pimgdesc = &prtkimg->customer;
      pimgdesc->tarfile_offset = tarfile_offset;
      pimgdesc->img_size = octalStringToInt(pthead->filesize, FILESIZELEN);
      sprintf(pimgdesc->filename, pthead->filename);
      return 0;
   }

   if(strstr(pthead->filename, "ALSADaemon"))
   {
      pimgdesc = &prtkimg->alsadaemon;
      pimgdesc->tarfile_offset = tarfile_offset;
      pimgdesc->img_size = octalStringToInt(pthead->filesize, FILESIZELEN);
      sprintf(pimgdesc->filename, pthead->filename);
      return 0;
   }

   if(strstr(pthead->filename, "otp_key_verify.aes"))
   {
      pimgdesc = &prtkimg->otp_kevy_verify;
      pimgdesc->tarfile_offset = tarfile_offset;
      pimgdesc->img_size = octalStringToInt(pthead->filesize, FILESIZELEN);
      sprintf(pimgdesc->filename, pthead->filename);
      return 0;
   }

   if(strstr(pthead->filename, "bootloader_nor.tar"))
   {
   	  install_debug("Got norbootloader.tar\r\n");
      pimgdesc = &prtkimg->norbootloader_tar;
      pimgdesc->tarfile_offset = tarfile_offset;
      pimgdesc->img_size = octalStringToInt(pthead->filesize, FILESIZELEN);
      sprintf(pimgdesc->filename, pthead->filename);
      return 0;
   }
   if(strstr(pthead->filename, "tee.bin"))
   {
   	  install_debug("Got tee.bin\r\n");
      pimgdesc = &prtkimg->tee_bin;
      pimgdesc->tarfile_offset = tarfile_offset;
      pimgdesc->img_size = octalStringToInt(pthead->filesize, FILESIZELEN);
      sprintf(pimgdesc->filename, pthead->filename);
      return 0;
   }
   
   if(strstr(pthead->filename, "bl31.bin"))
   {
      install_debug("Got bl31.bin\r\n");
      pimgdesc = &prtkimg->bl31_bin;
      pimgdesc->tarfile_offset = tarfile_offset;
      pimgdesc->img_size = octalStringToInt(pthead->filesize, FILESIZELEN);
      sprintf(pimgdesc->filename, pthead->filename);
      return 0;
   }
   
   if(strstr(pthead->filename, "xen.img"))
   {
      install_debug("Got xen.img\r\n");
      pimgdesc = &prtkimg->xen_bin;
      pimgdesc->tarfile_offset = tarfile_offset;
      pimgdesc->img_size = octalStringToInt(pthead->filesize, FILESIZELEN);
      sprintf(pimgdesc->filename, pthead->filename);
      return 0;
   }

   if(strstr(pthead->filename, "teeUtility.tar"))
   {
      install_debug("Got teeUtility.tar\r\n");
      pimgdesc = &prtkimg->teeFiles_tar;
      pimgdesc->tarfile_offset = tarfile_offset;
      pimgdesc->img_size = octalStringToInt(pthead->filesize, FILESIZELEN);
      sprintf(pimgdesc->filename, pthead->filename);
      return 0;
   }
   
   if (strrchr(pthead->filename, '/') != NULL) {
      if ((unsigned int)(strrchr(pthead->filename, '/') - pthead->filename + 1) == strlen(pthead->filename)) {
         //install_info("%s is a directory\r\n", pthead->filename);
         return 0;
      }
   }

   //install_debug("%s can't be figured\r\n", pthead->filename);
   return 0;
}

