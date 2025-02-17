#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <rtk_common.h>
#include <rtk_urltar.h>
#include <rtk_tar.h>
#include <rtk_fwdesc.h>
#include <rtk_imgdesc.h>
#include <rtk_mtd.h>
//#include <rtk_ui.h>
#include <extern_param.h>

#ifdef __OFFLINE_GENERATE_BIN__
#include <rtk_programmer.h>
#endif

#include <rtk_burn.h>
#include <rtk_config.h>
#include <rtk_boottable.h>
#include <rtk_factory.h>
#include <rtk_def.h>

#include <rtk_obfuse.h> //support call secure ta api

#define VERONA_SIGNATURE "VERONA__"
//#define BUF_MAXSIZE 1024*8
//#define FLUSH_SIZE 256*1024
#define BOOTCODE_RESERVED_SIZE   (0x100000+0x20000)

// spare_id, from uboot/examples/flash_writer_u/flashdev_n.h
/* block state */
#define BLOCK_BAD               0x00
#define BLOCK_HWSETTING         0X23
#define BLOCK_KEY_SIG		0x24
#define BLOCK_BOOTCODE          0x79
#define BLOCK_DATA              0x80
#define BLOCK_ENVPARAM_MAGICNO  0X81
#define BLOCK_FACTORY_SETTING   0X82
#define SECURE_FSBL_MAGIC_NUM   0x89        /* Identify secure boot loader in NAND flash spare area*/
#define SECURE_OS_MAGIC_NUM     0x68        /* Identify secure os loader in NAND flash spare area*/
#define BLOCK_OTHER_DATA	0xd0	// unknown data (user program into flash)
#define BLOCK_BBT		0xbb	// bad block table
#define BLOCK_CLEAN		0xff	// block is empty
#define BLOCK_UNDETERMINE	0x55	// block state is undetermined

#ifdef CONFIG_BOOT_FROM_SPI
#define NOR_FACTORY1_START_ADDR		0x10000
#define NOR_FACTORY2_START_ADDR		0x20000
#define NOR_NORMAL_DTS_START_ADDR	0x30000
#define NOR_RESCUE_DTS_START_ADDR	0x40000
#define NOR_KERNEL_START_ADDR		0x50000
#endif

#define _32K_BYTE	32*1024U
#define _64K_BYTE	64*1024U

#define SHA256_SIZE	32

#ifdef __OFFLINE_GENERATE_BIN__
static int rtk_file_to_virt_nand(struct t_rtkimgdesc* prtkimgdesc, const char* filepath, unsigned int soffset, unsigned int imglen, unsigned int doffset, unsigned char block_indicator, unsigned int* pchecksum);
static int rtk_yaffs_to_virt_nand(struct t_rtkimgdesc* prtkimgdesc, const char* filepath, unsigned int soffset, unsigned int imglen, unsigned int doffset, unsigned char block_indicator, unsigned int* pchecksum);
#endif

static int rtk_file_to_flash_pbc(struct t_rtkimgdesc* prtkimgdesc, const char* filepath, unsigned int soffset, unsigned int imglen, unsigned long long doffset, unsigned int alignlen, unsigned int* pchecksum);
static int rtk_extract_cipher_key(struct t_rtkimgdesc* prtkimgdesc);

extern int boot_main(const char *dir);
extern struct t_PARTDESC rtk_part_list[NUM_RTKPART];
extern u32 gDebugPrintfLogLevel;

void del_slash_str(char* str)
{
   int i;
   for(i=strlen(str)-1;i>=0;i--)
      if(str[i]=='/')
      {
         str[i] = 0;
         return ;
      }

}

int rtk_burn_yaffs_img(struct t_rtkimgdesc* prtkimgdesc, struct t_imgdesc* pimgdesc, struct t_PARTDESC* rtk_part)
{
   int ret;
   char command[256] = {0}, path[256] = {0};
   unsigned int yaffs_img_len;
   struct stat st;
   time_t stime, etime;

   stime = time(NULL);

   // show info
   install_info("\r\nBurning %s efwtype(%d) partition with install_offset(%u)  ...\r\n"
   , inv_by_fwtype(pimgdesc->efwtype)
   , pimgdesc->efwtype
   , pimgdesc->install_offset);
   install_info( "  flash_offset:0x%08llx flash_allo_size:0x%08llx bytes (%llu sector)\r\n"
         "tarfile_offset:0x%08x      image_size:0x%08x bytes (%10u Bytes = %6u KB)\r\n"
         , pimgdesc->flash_offset
         , pimgdesc->flash_allo_size
         , pimgdesc->sector
         , pimgdesc->tarfile_offset
         , pimgdesc->img_size
         , pimgdesc->img_size
         , pimgdesc->img_size/1024);

   if(strlen(pimgdesc->filename) == 0)
   {
      if(pimgdesc->flash_allo_size != 0)
      {
         // Extract utility
         if(rtk_extract_utility(prtkimgdesc) < 0)
         {
            install_debug("rtk_extract_utility fail\r\n");
            return 0;
         }

         // flash_erase partition
         snprintf(command, sizeof(command), "%s %s %llu %llu"
                                          , FLASHERASE_BIN
                                          , prtkimgdesc->mtd_path
                                          , pimgdesc->flash_offset
                                          , pimgdesc->sector);
         ret = rtk_command(command, __LINE__, __FILE__);
         if(ret < 0)
         {
            install_debug("Exec command fail\r\n");
            return -1;
         }
      }
      else
      {
         install_debug("impossible\r\n");
      }
   }
   else if(strstr(pimgdesc->filename, ".img"))
   {
      // mkdir parent directory
      // ex: /tmp/packageX
      snprintf(command, sizeof(command), "mkdir -p %s/%s", PKG_TEMP, pimgdesc->filename);
      del_slash_str(command);
      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }

      // Extract file
      snprintf(path, sizeof(path), "%s/%s", PKG_TEMP, pimgdesc->filename);
      if((ret = rtk_extract_file(prtkimgdesc, pimgdesc, path)) < 0)
      {
         install_debug("Extract fail\r\n");
         return -1;
      }

      // get yaffs.img's filelen
      if((ret = rtk_get_size_of_file(path, &yaffs_img_len)) < 0)
      {
         install_debug("file not found\r\n");
         return -1;
      }

      //check size
      if (yaffs_img_len > pimgdesc->flash_allo_size) {
         install_fail("yaffs.img size (%uKB) larger than configure size (%lluKB)!!!\r\n"
                       , yaffs_img_len/1024
                       , pimgdesc->flash_allo_size/1024 );
         return -1;
      }

      // flash_erase
      snprintf(command, sizeof(command), "%s %s %llu %llu"
                                       , FLASHERASE_BIN
                                       , prtkimgdesc->mtd_path
                                       , pimgdesc->flash_offset
                                       , pimgdesc->sector);
      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }

      // nandwrite
      snprintf(command, sizeof(command), "%s -c -y -n -o -s %lld -l %u %s %s"
                                         , NANDWRITE_BIN
                                         , pimgdesc->flash_offset
                                         , yaffs_img_len
                                         , prtkimgdesc->mtd_path
                                         , path);
      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }

#if 0
      // nandwriter use -y option so we cannot verify
      // verify
      if(prtkimgdesc->verify == 1)
      {
         unsigned int error = 0;
         //ret = rtk_file_verify(prtkimgdesc->mtdblock_path, pimgdesc->flash_offset, path, 0, yaffs_img_len, &error);
         ret = rtk_file_verify(prtkimgdesc->mtdblock_path
                              , pimgdesc->flash_offset
                              , prtkimgdesc->tarinfo.tarfile_path
                              , pimgdesc->tarfile_offset+sizeof(t_tarheader)
                              , pimgdesc->img_size
                              , &error);
         if (ret < 0) {
            install_fail("verify error!!!\r\n");
            return -1;
         }
      }
#endif

      //remove temp file
      snprintf(command, sizeof(command), "rm -f %s", path);
      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }
   }
   else if(strstr(pimgdesc->filename, ".tar.bz2"))
   {
      // mkdir parent directory
      // ex: /tmp/packageX
      snprintf(command, sizeof(command), "mkdir -p %s/%s", PKG_TEMP, pimgdesc->filename);
      del_slash_str(command);
      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }

      // Extract file
      // ex: /tmp/packageX/usr.local.etc.tar.bz2
      snprintf(path, sizeof(path), "%s/%s", PKG_TEMP, pimgdesc->filename);
      if((ret = rtk_extract_file(prtkimgdesc, pimgdesc, path)) < 0)
      {
         install_debug("Extract fail\r\n");
         return -1;
      }

      // mkdir and untar etc.tar
      snprintf(path, sizeof(path), "%s/yaffs_img", PKG_TEMP);
      snprintf(command, sizeof(command), "rm -rf %s;mkdir -p %s;tar -xjf %s/%s -C %s"
                                       , path
                                       , path
                                       , PKG_TEMP
                                       , pimgdesc->filename
                                       , path);
      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }

#ifndef __OFFLINE_GENERATE_BIN__
      //remove temp file
      snprintf(command, sizeof(command), "rm -f %s/%s", PKG_TEMP, pimgdesc->filename);
      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }
#endif

      // TODO: SA here
      // for upgrade
      if(pimgdesc->efwtype == FW_USR_LOCAL_ETC)
      {
         // UPGRAD or INSTALL
         if(prtkimgdesc->mode == _UPGRAD)
         {
            if(stat("/usr/local/etc", &st) != 0)
            {
               install_log("Can't find /usr/loca/etc, try mount /usr/local/etc\r\n");
               // try mount
               rtk_command("mkdir -p /usr/local/etc;mount -t jffs2 /dev/mtdblock/2 /usr/local/etc", __LINE__, __FILE__);
            }

            ret = rtk_command("cp -a /usr/local/etc/dvdplayer/* /tmp/etc/dvdplayer", __LINE__, __FILE__);
            if(ret < 0)
            {
               install_debug("Exec command fail\r\n");
#if 0
               return -1;
#endif
            }
         }
      }

#ifndef __OFFLINE_GENERATE_BIN__
      // Extract utility
      if(rtk_extract_utility(prtkimgdesc) < 0)
      {
         install_debug("rtk_extract_utility fail\r\n");
         return -1;
      }
#endif

      // make partition image by mkyaffs2image
      snprintf(command, sizeof(command), "%s %s/yaffs_img/ %s/yaffs.img all-root"
                                       , YAFFS2_BIN
                                       , PKG_TEMP
                                       , PKG_TEMP);
      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }

#ifndef __OFFLINE_GENERATE_BIN__
      //remove temp file
      snprintf(command, sizeof(command), "rm -rf %s/yaffs_img", PKG_TEMP);
      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }
#endif

#ifndef __OFFLINE_GENERATE_BIN__
      // umount
      snprintf(command, sizeof(command), "umount %s", rtk_part->mount_point);
      rtk_command(command, __LINE__, __FILE__);
#endif

      // get yaffs.img's filelen
      snprintf(path, sizeof(path), "%s/yaffs.img", PKG_TEMP);
      if((ret = rtk_get_size_of_file(path, &yaffs_img_len)) < 0)
      {
         install_debug("file not found\r\n");
         return -1;
      }

      //check size
      if (yaffs_img_len > pimgdesc->flash_allo_size) {
         install_fail("yaffs.img size (%uKB) larger than configure size (%lluKB)!!!\r\n"
                       , yaffs_img_len/1024
                       , pimgdesc->flash_allo_size/1024 );
         return -1;
      }

#ifndef __OFFLINE_GENERATE_BIN__

      // flash_erase partition
      snprintf(command, sizeof(command), "%s %s %llu %llu", FLASHERASE_BIN, prtkimgdesc->mtd_path, pimgdesc->flash_offset, pimgdesc->sector);
      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }

      // nandwrite
      snprintf(command, sizeof(command), "%s -c -y -n -o -s %lld -l %u %s %s"
                                         , NANDWRITE_BIN
                                         , pimgdesc->flash_offset
                                         , yaffs_img_len
                                         , prtkimgdesc->mtd_path
                                         , path);
      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }

#else

      ret = rtk_yaffs_to_virt_nand(prtkimgdesc
                                 , path
                                 , 0
                                 , yaffs_img_len
                                 , pimgdesc->flash_offset
                                 , RTK_NAND_BLOCK_STATE_GOOD_BLOCK
                                 , &pimgdesc->checksum);

#endif

#if 0
      // nandwriter use -y option so we cannot verify
      // verify
      if(prtkimgdesc->verify == 1)
      {
         unsigned int error = 0;
         ret = rtk_file_verify(prtkimgdesc->mtdblock_path, pimgdesc->flash_offset, path, 0, yaffs_img_len, &error);
         if (ret < 0) {
            install_fail("verify error!!!\r\n");
            return -1;
         }
      }
#endif
#ifndef __OFFLINE_GENERATE_BIN__
      //remove temp file
      snprintf(command, sizeof(command), "rm -f %s", path);
      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }
#endif
   }

   etime = time(NULL);
   install_info("total time: %.0lf seconds\r\n", difftime(etime,stime));
   install_info("%s partition Complete!!\r\n\r\n", rtk_part->partition_name);
   fflush(stdout);
   return 0;
}

int rtk_burn_jffs_img(struct t_rtkimgdesc* prtkimgdesc, struct t_imgdesc* pimgdesc, struct t_PARTDESC* rtk_part)
{
   int ret;
   char command[256] = {0}, path[128] = {0}, dpath[128] = {0};
   unsigned int etcimg_len;
   time_t stime, etime;

   stime = time(NULL);

   // show info
   install_info("\r\nBurning %s efwtype(%d) partition with install_offset(%u)  ...\r\n"
   , inv_by_fwtype(pimgdesc->efwtype)
   , pimgdesc->efwtype
   , pimgdesc->install_offset);
   install_info( "  flash_offset:0x%08llx flash_allo_size:0x%08llx bytes (%llu sector)\r\n"
         "tarfile_offset:0x%08x      image_size:0x%08x bytes (%10u Bytes = %6u KB)\r\n"
         , pimgdesc->flash_offset
         , pimgdesc->flash_allo_size
         , pimgdesc->sector
         , pimgdesc->tarfile_offset
         , pimgdesc->img_size
         , pimgdesc->img_size
         , pimgdesc->img_size/1024);

   if (prtkimgdesc->update_etc == 0)
   {
      snprintf(path, sizeof(path), "/mnt/old/usr/local/etc/");
      if (!access(path, F_OK))
      {
         // chroot install, the etc will be rediect to "/mnt/old/usr/local/etc/".
         // so we copy the file and move it to the new partition.
         install_log("%s found, start moving files to new partition\r\n", path);

         // mkdir and copy file
         snprintf(dpath, sizeof(dpath), "%s/jffs_bak2_img", PKG_TEMP);
         snprintf(command, sizeof(command), "rm -rf %s;mkdir -p %s;cp -r /mnt/old/usr/local/etc/* %s"
                                          , dpath
                                          , dpath
                                          , dpath);
         if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
         {
            install_debug("Exec command fail\r\n");
            return -1;
         }

         // Extract utility
         snprintf(path, sizeof(path), "%s", JFFS2_BIN);
         if( rtk_extract_file(prtkimgdesc, &prtkimgdesc->util[UTIL_MKJFFS2], path) < 0 )
         {
            install_debug("Extract utility fail\r\n");
            return -1;
         }

         // chmod
         snprintf(command, sizeof(command), "chmod 777 %s", JFFS2_BIN);
         if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
         {
            install_debug("Exec command fail\r\n");
            return -1;
         }

         // mkfs.jffs2
         snprintf(path, sizeof(path), "%s/jffs.bak2.img", PKG_TEMP);
         if (prtkimgdesc->jffs2_nocleanmarker)
            snprintf(command, sizeof(command), "%s --pagesize=%u --eraseblock=%u -n --pad=%llu -d %s -o %s" \
                                             , JFFS2_BIN
                                             , prtkimgdesc->mtd_erasesize
                                             , prtkimgdesc->mtd_erasesize
                                             , pimgdesc->flash_allo_size
                                             , dpath
                                             , path);
         else
            snprintf(command, sizeof(command), "%s --pagesize=%u --eraseblock=%u --pad=%llu -d %s -o %s" \
                                             , JFFS2_BIN
                                             , prtkimgdesc->mtd_erasesize
                                             , prtkimgdesc->mtd_erasesize
                                             , pimgdesc->flash_allo_size
                                             , dpath
                                             , path);

         if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
         {
            install_debug("Exec command fail\r\n");
            return -1;
         }

         //remove temp file
         snprintf(command, sizeof(command), "rm -rf %s", dpath);
         if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
         {
            install_debug("Exec command fail\r\n");
            return -1;
         }
      }
      else
      {
         // rescue install, etc partitioin is not mounted
         // so we move etc partition to the new partition.
         install_log("%s not found, start moving partition to partition\r\n", path);
         snprintf(path, sizeof(path), "%s/jffs.bak.img", PKG_TEMP);
         if (access(path, F_OK))
         {
            install_fail("%s file missing!\r\n", path);
            return -1;
         }
      }

      // installing
      rtk_get_size_of_file(path, &etcimg_len);
      printf("start burning with etcimg (%d=%dKB)\r\n", etcimg_len, etcimg_len/1024);

      ret = rtk_file_to_flash_pbc(prtkimgdesc
                                 , path
                                 , 0
                                 , etcimg_len
                                 , pimgdesc->flash_offset
                                 , pimgdesc->flash_allo_size
                                 , &pimgdesc->checksum);
      if(ret < 0)
      {
         install_debug("Burn not complete, ret=%d\r\n", ret);
         return -1;
      }

      // verify
      if(prtkimgdesc->verify == 1)
      {
         unsigned int error = 0;
         if(prtkimgdesc->flash_type == MTD_NANDFLASH)
            ret = rtk_file_verify(prtkimgdesc->mtdblock_path, pimgdesc->flash_offset, path, 0, etcimg_len, &error);
         else
            ret = rtk_file_verify(prtkimgdesc->mtd_path, pimgdesc->flash_offset, path, 0, etcimg_len, &error);
         if (ret < 0) {
            install_fail("verify error!!!\r\n");
            return -1;
         }
      }

#ifndef __OFFLINE_GENERATE_BIN__
      //remove temp file
      snprintf(command, sizeof(command), "rm -f %s", path);
      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }
#endif
   }
   else if(strstr(pimgdesc->filename, ".tar.bz2"))
   {
      // mkdir parent directory
      // ex: /tmp/packageX
      snprintf(command, sizeof(command), "mkdir -p %s/%s", PKG_TEMP, pimgdesc->filename);
      del_slash_str(command);
      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }

      // Extract file
      // ex: /tmp/packageX/usr.local.etc.tar.bz2
      snprintf(path, sizeof(path), "%s/%s", PKG_TEMP, pimgdesc->filename);
      if((ret = rtk_extract_file(prtkimgdesc, pimgdesc, path)) < 0)
      {
         install_debug("Extract fail\r\n");
         return -1;
      }

      // mkdir and untar etc.tar
      snprintf(dpath, sizeof(dpath), "%s/jffs_img", PKG_TEMP);
      snprintf(command, sizeof(command), "rm -rf %s;mkdir -p %s;tar -xjf %s/%s -C %s"
                                       , dpath
                                       , dpath
                                       , PKG_TEMP
                                       , pimgdesc->filename
                                       , dpath);
      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }

#ifndef __OFFLINE_GENERATE_BIN__
      // Extract utility
      snprintf(path, sizeof(path), "%s", JFFS2_BIN);
      if( rtk_extract_file(prtkimgdesc, &prtkimgdesc->util[UTIL_MKJFFS2], path) < 0 )
      {
         install_debug("Extract utility fail\r\n");
         return -1;
      }
#endif

      // chmod
      snprintf(command, sizeof(command), "chmod 777 %s", JFFS2_BIN);
      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }

      // mkfs.jffs2
      snprintf(path, sizeof(path), "%s/jffs.img", PKG_TEMP);
      if (prtkimgdesc->jffs2_nocleanmarker)
         snprintf(command, sizeof(command), "%s --pagesize=%u --eraseblock=%u -n --pad=%llu -d %s -o %s" \
                                          , JFFS2_BIN
                                          , prtkimgdesc->mtd_erasesize
                                          , prtkimgdesc->mtd_erasesize
                                          , pimgdesc->flash_allo_size
                                          , dpath
                                          , path);
      else
         snprintf(command, sizeof(command), "%s --pagesize=%u --eraseblock=%u --pad=%llu -d %s -o %s" \
                                          , JFFS2_BIN
                                          , prtkimgdesc->mtd_erasesize
                                          , prtkimgdesc->mtd_erasesize
                                          , pimgdesc->flash_allo_size
                                          , dpath
                                          , path);

      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }

      // installing
      rtk_get_size_of_file(path, &etcimg_len);
      printf("start burning with etcimg (%d=%dKB)\r\n", etcimg_len, etcimg_len/1024);

      ret = rtk_file_to_flash_pbc(prtkimgdesc
                                 , path
                                 , 0
                                 , etcimg_len
                                 , pimgdesc->flash_offset
                                 , pimgdesc->flash_allo_size
                                 , &pimgdesc->checksum);
      if(ret < 0)
      {
         install_debug("Burn not complete, ret=%d\r\n", ret);
         return -1;
      }

      // verify
      if(prtkimgdesc->verify == 1)
      {
         unsigned int error = 0;
         if(prtkimgdesc->flash_type == MTD_NANDFLASH)
            ret = rtk_file_verify(prtkimgdesc->mtdblock_path, pimgdesc->flash_offset, path, 0, etcimg_len, &error);
         else
            ret = rtk_file_verify(prtkimgdesc->mtd_path, pimgdesc->flash_offset, path, 0, etcimg_len, &error);
         if (ret < 0) {
            install_fail("verify error!!!\r\n");
            return -1;
         }
      }

#ifndef __OFFLINE_GENERATE_BIN__
      //remove temp file
      snprintf(command, sizeof(command), "rm -rf %s/jffs_img;rm -f %s", PKG_TEMP, path);
      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }
#endif
   }
   else if(strstr(pimgdesc->filename, ".img"))
   {
      ret = rtk_file_to_flash_pbc(prtkimgdesc
                                 , prtkimgdesc->tarinfo.tarfile_path
                                 , pimgdesc->tarfile_offset+sizeof(t_tarheader)   // this parameter is strange?  pimgdesc->tarfile_offset?
                                 , pimgdesc->img_size
                                 , pimgdesc->flash_offset
                                 , pimgdesc->flash_allo_size
                                 , &pimgdesc->checksum);
      if(ret < 0)
      {
         install_debug("Burn not complete, ret=%d\r\n", ret);
         return -1;
      }

      // verify
      if(prtkimgdesc->verify == 1)
      {
         unsigned int error = 0;
         if(prtkimgdesc->flash_type == MTD_NANDFLASH) {
            ret = rtk_file_verify(prtkimgdesc->mtdblock_path
                                 , pimgdesc->flash_offset
                                 , prtkimgdesc->tarinfo.tarfile_path
                                 , pimgdesc->tarfile_offset+sizeof(t_tarheader)
                                 , pimgdesc->img_size
                                 , &error);
         }
         else {
            ret = rtk_file_verify(prtkimgdesc->mtd_path
                                 , pimgdesc->flash_offset
                                 , prtkimgdesc->tarinfo.tarfile_path
                                 , pimgdesc->tarfile_offset+sizeof(t_tarheader)
                                 , pimgdesc->img_size
                                 , &error);
         }
         if (ret < 0) {
            install_fail("verify error!!!\r\n");
            return -1;
         }
      }

   }

   etime = time(NULL);
   install_info("total time: %.0lf seconds\r\n", difftime(etime,stime));
   install_info("Complete!!\r\n\r\n");
   fflush(stdout);
   return 0;
}

int rtk_burn_ubifs_img(struct t_rtkimgdesc* prtkimgdesc, struct t_imgdesc* pimgdesc, struct t_PARTDESC* rtk_part)
{
   int ret;
   char command[256];
   time_t stime, etime;

   stime = time(NULL);

   // show info
   install_info("\r\nBurning %s efwtype(%d) partition...\r\n"
   , inv_by_fwtype(pimgdesc->efwtype)
   , pimgdesc->efwtype);
   install_info( "  flash_offset:0x%08llx flash_allo_size:0x%08llx bytes (%llu sector)\r\n"
         "tarfile_offset:0x%08x      image_size:0x%08x bytes (%10u Bytes = %6u KB)\r\n"
         , pimgdesc->flash_offset
         , pimgdesc->flash_allo_size
         , pimgdesc->sector
         , pimgdesc->tarfile_offset
         , pimgdesc->img_size
         , pimgdesc->img_size
         , pimgdesc->img_size/1024);

   // Extract utility
   if(rtk_extract_utility(prtkimgdesc) < 0)
   {
	  install_debug("rtk_extract_utility fail\r\n");
	  return -1;
   }

   if(strlen(pimgdesc->filename) == 0)
   {
      if(pimgdesc->flash_allo_size != 0)
      {
         // flash_erase partition
         snprintf(command, sizeof(command), "%s %s %llu %llu"
                                          , FLASHERASE_BIN
                                          , prtkimgdesc->mtd_path
                                          , pimgdesc->flash_offset
                                          , pimgdesc->sector);
         ret = rtk_command(command, __LINE__, __FILE__);
         if(ret < 0)
         {
            install_debug("Exec command fail\r\n");
            return -1;
         }
      }
      else
      {
         install_debug("impossible\r\n");
      }
   }
   else
   {
        snprintf(command, sizeof(command), "tar -O -xf %s %s | %s %s -y -b %llu -t %llu -S %u -f -"
                            , prtkimgdesc->tarinfo.tarfile_path
                            , pimgdesc->filename
                            , UBIFORMAT_BIN
                            , prtkimgdesc->mtd_path
                            , pimgdesc->flash_offset
                            , pimgdesc->flash_allo_size
                            , pimgdesc->img_size);

      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }
   }

   etime = time(NULL);
   install_info("total time: %.0lf seconds\r\n", difftime(etime,stime));
   install_info("%s partition Complete!!\r\n\r\n", rtk_part->partition_name);
   fflush(stdout);
   return 0;
}

int rtk_burn_preserveimg(struct t_rtkimgdesc* prtkimgdesc, struct t_imgdesc* pimgdesc)
{
   int ret;
   char command[128];

   // Extract utility
   if(rtk_extract_utility(prtkimgdesc) < 0)
   {
      install_debug("rtk_extract_utility fail\r\n");
      return 0;
   }

   // flash_erase preserve partition
   sprintf(command, "/usr/sbin/flash_erase %s %llu %llu", prtkimgdesc->mtd_path, pimgdesc->flash_offset, pimgdesc->sector);
   ret = rtk_command(command, __LINE__, __FILE__);
   if(ret < 0)
   {
      install_debug("Exec command fail\r\n");
      return -1;
   }
/*
   // build preserve image by mkyaffs2image
   ret = rtk_command("mkdir /tmp/preserve;/usr/sbin/mkyaffs2image /tmp/preserve/ /tmp/preserve.img all-root", __LINE__, __FILE__);
   if(ret < 0)
   {
      install_debug("Exec command fail\r\n");
      return -1;
   }

   // get preserveimg's filelen
   ret = rtk_get_size_of_file("/tmp/preserve.img", &etcimg_len);
   if(ret < 0)
   {
      install_debug("file not found\r\n");
      return -1;
   }

   // nandwrite
   sprintf(command, "/usr/sbin/nandwrite -c -e -y -n -o -s %d -l %u %s /tmp/preserve.img", pimgdesc->flash_offset, etcimg_len, prtkimgdesc->mtd_path);
   ret = rtk_command(command, __LINE__, __FILE__);
   if(ret < 0)
   {
      install_debug("Exec command fail\r\n");
      return -1;
   }
   */
   install_info("preserve partition Complete!!\r\n\r\n");
   fflush(stdout);

   return 0;
}

int rtk_burn_img(struct t_rtkimgdesc* prtkimgdesc, struct t_imgdesc* pimgdesc, int offset)
{
   int ret;
   unsigned int backup_filesize = 0;
   char cmd[256] = {0};
   time_t stime, etime;

   stime = time(NULL);

#ifndef __OFFLINE_GENERATE_BIN__
   if(prtkimgdesc->eburn == BURN_BY_NANDWRITE)
   {
      // Extract utility
      if(rtk_extract_utility(prtkimgdesc) < 0)
      {
         install_debug("rtk_extract_utility fail\r\n");
         return -1;
      }
   }
#endif

   // show info
   install_info("\r\nBurning %s filename(%s) efwtype(%d)...\r\n"
   , inv_by_fwtype(pimgdesc->efwtype)
   , pimgdesc->filename
   , pimgdesc->efwtype);

#ifndef __OFFLINE_GENERATE_BIN__
   if (pimgdesc->install_mode == eBACKUP) {
      install_debug("Backup mode\r\n");
      if ((ret = rtk_get_size_of_file(pimgdesc->backup_filename, &backup_filesize)) == -1) {
         install_debug("%s not found, fall backup to update mode.", pimgdesc->backup_filename);
         pimgdesc->install_mode = eUPDATE;
      }
      else {
         if (pimgdesc->flash_allo_size < SIZE_ALIGN_BOUNDARY_MORE(backup_filesize, prtkimgdesc->mtd_erasesize)) {
            install_fail("backup file aligned size(%d) > flash alloc size(%lld)", SIZE_ALIGN_BOUNDARY_MORE(backup_filesize, prtkimgdesc->mtd_erasesize), pimgdesc->flash_allo_size);
            return -1;
         }
      }
   }
#else
   pimgdesc->install_mode = eUPDATE;
#endif

   if (pimgdesc->install_mode == eUPDATE) {
      // sanity-check
      if(0 == pimgdesc->img_size)
      {
         install_debug("%s don't exist\r\n", inv_by_fwtype(pimgdesc->efwtype));
         return -1;
      }

      install_info( "  flash_offset:0x%08llx (%llu sector) ~ flash_end:0x%08llx (%llu sector) flash_allo_size:0x%08llx bytes (%llu sector)\r\n"
            "tarfile_offset:0x%08x      image_size:0x%08x bytes (%10u Bytes = %6u KB)\r\n"
            , pimgdesc->flash_offset
            , pimgdesc->flash_offset/prtkimgdesc->mtd_erasesize
            , pimgdesc->flash_offset + pimgdesc->flash_allo_size - 1
            , (pimgdesc->flash_offset + pimgdesc->flash_allo_size)/prtkimgdesc->mtd_erasesize - 1
            , pimgdesc->flash_allo_size
            , pimgdesc->sector
            , pimgdesc->tarfile_offset
            , pimgdesc->img_size
            , pimgdesc->img_size
            , pimgdesc->img_size/1024);
   }
   else if (pimgdesc->install_mode == eBACKUP) {
      install_info( "  flash_offset:0x%08llx (%llu sector) ~ flash_end:0x%08llx (%llu sector) flash_allo_size:0x%08llx bytes (%llu sector)\r\n"
            "  backup mode              image_size:0x%08x bytes (%10u Bytes = %6u KB)\r\n"
            , pimgdesc->flash_offset
            , pimgdesc->flash_offset/prtkimgdesc->mtd_erasesize
            , pimgdesc->flash_offset + pimgdesc->flash_allo_size - 1
            , (pimgdesc->flash_offset + pimgdesc->flash_allo_size)/prtkimgdesc->mtd_erasesize - 1
            , pimgdesc->flash_allo_size
            , pimgdesc->sector
            , backup_filesize
            , backup_filesize
            , backup_filesize/1024);
   }
   else if (pimgdesc->install_mode == eSKIP){
      // skip program
      install_info( "  flash_offset:0x%08llx (%llu sector) ~ flash_end:0x%08llx (%llu sector) flash_allo_size:0x%08llx bytes (%llu sector)\r\n"
         "  skip burn\r\n"
            , pimgdesc->flash_offset
            , pimgdesc->flash_offset/prtkimgdesc->mtd_erasesize
            , pimgdesc->flash_offset + pimgdesc->flash_allo_size - 1
            , (pimgdesc->flash_offset + pimgdesc->flash_allo_size)/prtkimgdesc->mtd_erasesize - 1
            , pimgdesc->flash_allo_size
            , pimgdesc->sector);
      return 0;
   }

   // nand space sanity-check
   if(prtkimgdesc->flash_type == MTD_NANDFLASH)
   {
      if(pimgdesc->flash_offset < (unsigned long long)(prtkimgdesc->reserved_boot_size+prtkimgdesc->reserved_boottable_size))
      {
         install_fail("Out of range flash offset:0x%08llx\r\n", pimgdesc->flash_offset);
         return -1;
      }
   }

   install_debug("start burning\r\n");
   if(prtkimgdesc->eburn == BURN_BY_NANDWRITE)
   {
#ifndef __OFFLINE_GENERATE_BIN__
      // Extract utility
      if(rtk_extract_utility(prtkimgdesc) < 0)
      {
         install_debug("rtk_extract_utility fail\r\n");
         return -1;
      }

      // flash_erase
      snprintf(cmd, sizeof(cmd), "%s %s %llu %llu"
                               , FLASHERASE_BIN
                               , prtkimgdesc->mtd_path
                               , pimgdesc->flash_offset
                               , pimgdesc->sector);
      if((ret = rtk_command(cmd, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }

      if (pimgdesc->install_mode == eBACKUP) {
         snprintf(cmd, sizeof(cmd), "%s -c -x -s %llu -l %u %s %s"
                                  , NANDWRITE_BIN
                                  , pimgdesc->flash_offset
                                  , pimgdesc->img_size
                                  , prtkimgdesc->mtd_path
                                  , pimgdesc->backup_filename);
      }
#ifndef PC_SIMULATE
      else {
         snprintf(cmd, sizeof(cmd), "tar -O -xf %s %s | %s -s %llu -p %s -"
                                  , prtkimgdesc->tarinfo.tarfile_path
                                  , pimgdesc->filename
                                  , NANDWRITE_BIN
                                  , pimgdesc->flash_offset
                                  , prtkimgdesc->mtd_path);
      }

      if((ret = rtk_command(cmd, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec command fail\r\n");
         return -1;
      }
#else
      else {
            ret = rtk_file_checksum_pc(pimgdesc->filename
                                 , prtkimgdesc->tarinfo.tarfile_path
                                 , pimgdesc->img_size
                                 , pimgdesc->sha_hash);
            if (ret < 0) {
               install_fail("rtk_file_checksum error!!!\r\n");
               return -1;
            }
      }
#endif
#else	//__OFFLINE_GENERATE_BIN__
      ret = rtk_file_to_virt_nand(prtkimgdesc
                                 , prtkimgdesc->tarinfo.tarfile_path
                                 , pimgdesc->tarfile_offset+sizeof(t_tarheader)
                                 , pimgdesc->img_size
                                 , pimgdesc->flash_offset
                                 , RTK_NAND_BLOCK_STATE_GOOD_BLOCK
                                 , &pimgdesc->checksum);

#endif
#ifndef PC_SIMULATE
      // verify
      //if(prtkimgdesc->verify == 1) //Add default do verify fw and checksum of each fw.
      {
         unsigned int error = 0;
         if (pimgdesc->install_mode == eBACKUP) {
            ret = rtk_file_verify(prtkimgdesc->mtdblock_path
                                 , pimgdesc->flash_offset
                                 , pimgdesc->backup_filename
                                 , 0
                                 , backup_filesize
                                 , &error);
         }
         else {
            ret = rtk_file_verify(prtkimgdesc->mtdblock_path
                                 , pimgdesc->flash_offset
                                 , prtkimgdesc->tarinfo.tarfile_path
                                 , pimgdesc->tarfile_offset+sizeof(t_tarheader)
                                 , pimgdesc->img_size
                                 , &error
                                 , &(pimgdesc->checksum) );
         }
         if (ret < 0) {
            install_fail("verify error!!!\r\n");
            return -1;
         }
      }
#endif
   }
   else if (prtkimgdesc->eburn == BURN_BY_MTDBLOCK)
   {
      if (pimgdesc->install_mode == eBACKUP) {
         ret = rtk_file_to_flash_pbc(prtkimgdesc
                                    , pimgdesc->backup_filename
                                    , 0
                                    , backup_filesize
                                    , pimgdesc->flash_offset
                                    , pimgdesc->flash_allo_size
                                    , &pimgdesc->checksum);
      }
      else {
         ret = rtk_file_to_flash_pbc(prtkimgdesc
                                    , prtkimgdesc->tarinfo.tarfile_path
                                    , pimgdesc->tarfile_offset+sizeof(t_tarheader)
                                    , pimgdesc->img_size
                                    , pimgdesc->flash_offset
                                    , pimgdesc->flash_allo_size
                                    , &pimgdesc->checksum);
      }

      if(ret < 0)
      {
         install_debug("Burn not complete, ret=%d\r\n", ret);
         return -1;
      }

      if((unsigned int)ret != pimgdesc->img_size)
      {
         install_debug("Burn incomplete, ret=%d\r\n", ret);
         return -1;
      }

      // verify
      if(prtkimgdesc->verify == 1)
      {
         unsigned int error = 0;

         if (pimgdesc->install_mode == eBACKUP) {
            ret = rtk_file_verify(prtkimgdesc->mtd_path
                                    , pimgdesc->flash_offset
                                    , pimgdesc->backup_filename
                                    , 0
                                    , backup_filesize
                                    , &error);
         }
         else {
            ret = rtk_file_verify(prtkimgdesc->mtd_path
                                    , pimgdesc->flash_offset
                                    , prtkimgdesc->tarinfo.tarfile_path
                                    , pimgdesc->tarfile_offset+sizeof(t_tarheader)
                                    , pimgdesc->img_size
                                    , &error);
         }
         if (ret < 0) {
            install_fail("verify error!!!\r\n");
            return -1;
         }
         else {
            pimgdesc->checksum = 0;
#ifndef PC_SIMULATE
            rtk_file_checksum(prtkimgdesc->tarinfo.tarfile_path
                                    , pimgdesc->tarfile_offset+sizeof(t_tarheader)
                                    , pimgdesc->img_size
                                    , pimgdesc->sha_hash);
#else
            rtk_file_checksum_pc(pimgdesc->filename
                                    , prtkimgdesc->tarinfo.tarfile_path
                                    , pimgdesc->img_size
                                    , pimgdesc->sha_hash);            
            goto finish;
#endif
         }
      }
   }
#ifdef EMMC_SUPPORT
	else {
#ifndef PC_SIMULATE
		if (pimgdesc->install_mode == eBACKUP) {
         	ret = rtk_file_to_flash_pbc(prtkimgdesc
                                    , pimgdesc->backup_filename
                                    , 0
                                    , backup_filesize
                                    , pimgdesc->flash_offset
                                    , pimgdesc->flash_allo_size
                                    , &pimgdesc->checksum);
      	}
      	else {
        	ret = rtk_file_to_flash_pbc(prtkimgdesc
                                    , prtkimgdesc->tarinfo.tarfile_path
                                    , pimgdesc->tarfile_offset+sizeof(t_tarheader)
                                    , pimgdesc->img_size
                                    , pimgdesc->flash_offset+offset
                                    , pimgdesc->flash_allo_size-offset
                                    , &pimgdesc->checksum);
      	}

      	if (ret < 0)
      	{
        	install_debug("Burn not complete, ret=%d\r\n", ret);
         	return -1;
      	}

      	if ((unsigned int)ret != pimgdesc->img_size)
      	{
         	install_debug("Burn incomplete, ret=%d\r\n", ret);
         	return -1;
      	}
#endif
        {
            pimgdesc->checksum = 0;
#ifndef PC_SIMULATE
            rtk_file_checksum(prtkimgdesc->tarinfo.tarfile_path
                                 , pimgdesc->tarfile_offset+sizeof(t_tarheader)
                                 , pimgdesc->img_size
                                 , pimgdesc->sha_hash);
#else
            rtk_file_checksum_pc(pimgdesc->filename
                                 , prtkimgdesc->tarinfo.tarfile_path
                                 , pimgdesc->img_size
                                 , pimgdesc->sha_hash);
            install_info("SHA256 checksum ==>\n");
            int i;
            for (i=0; i<32; i++){
                printf("%x ", pimgdesc->sha_hash[i]);        
            }
            printf("\n");
            goto finish;
#endif
        }

		// verify
#ifdef NAS_ENABLE
        if (0)
#else
      	if (1)	//(prtkimgdesc->verify == 1)
#endif
      	{
        	unsigned int error = 0;

         	if (pimgdesc->install_mode == eBACKUP) {
            	ret = rtk_file_verify(prtkimgdesc->mtdblock_path
                                    , pimgdesc->flash_offset
                                    , pimgdesc->backup_filename
                                    , 0
                                    , backup_filesize
                                    , &error);
         	}
         	else {
            	ret = rtk_file_verify(prtkimgdesc->mtdblock_path
                                    , pimgdesc->flash_offset+offset
                                    , prtkimgdesc->tarinfo.tarfile_path
                                    , pimgdesc->tarfile_offset+sizeof(t_tarheader)
                                    , pimgdesc->img_size
                                    , &error);
         	}
         	if (ret < 0) {
            	install_fail("verify error!!!\r\n");
            	return -1;
         	}
      	}
	}
#endif	//EMMC_SUPPORT

finish:
   etime = time(NULL);
   install_info("total time: %.0lf seconds\r\n", difftime(etime,stime));
   install_info("Complete!!\r\n\r\n");
   fflush(stdout);
   return 0;
}

#ifdef EMMC_SUPPORT
#ifdef FLASH_OFFLINE_BIN_WRITER
#ifndef __OFFLINE_GENERATE_BIN__
int rtk_burn_flashbin(char* binFilePath, const char* target_devPath, int sizeCheck, FILE* customer_fp)
{
    	unsigned long long ret_size = 0;
    	int sfd, dev_fd ;
    	unsigned long long src_bin_size =0,emmc_size=0;
	char* pFileName = NULL;

     install_info("binFilePath:%s; target_devPath:%s\n",binFilePath, target_devPath);

	sfd = open(binFilePath,O_RDONLY);
    	if (sfd < 0) {
		install_fail("Can't Open %s (%s)\r\n", binFilePath, strerror(errno));
        	return -1;
    	}

    	dev_fd = open(target_devPath, O_RDWR|O_SYNC);
   	if(dev_fd < 0) {
		install_fail("open mtd block (%s) fail (%s)\r\n", target_devPath, strerror(errno));
      	return -1;
   	}
     src_bin_size = lseek64(sfd, 0, SEEK_END);
     lseek64(sfd, 0, SEEK_SET);
     emmc_size = rtk_get_size_emmc();
     lseek64(dev_fd, 0, SEEK_SET);
	install_info("dev_fd:%llu (%llu), sfd:%llu \n",emmc_size, emmc_size*512, src_bin_size);

    	if (sizeCheck) {
		//unsigned long src_bin_size = fseek(sFp, 0, SEEK_END);
        	if (emmc_size*512 != src_bin_size) {
            	install_fail("source bin size doesn't match emmc size!\n");
            	return -1;
        	}
    	}
	pFileName = strrchr(binFilePath, '/')+1;
   	ret_size = fd_to_fd(sfd, dev_fd, src_bin_size, NULL, NULL, pFileName, customer_fp);
    	if (ret_size < src_bin_size) {
        	install_fail("Failed! (ret_size:%llu)\n", ret_size);
        	return -1;
    	} else {
		install_log("%s burn sucess!\n",binFilePath);
	}

    	close(sfd);
    	close(dev_fd);

    	return 0;
}
#endif
#endif

#ifdef NAS_ENABLE
#define EXT4_MOUNTP "/media"
#else
#define EXT4_MOUNTP "/tmp/ext4TMP"
#endif
int rtk_burn_ext4_img(struct t_rtkimgdesc* prtkimgdesc, struct t_imgdesc* pimgdesc, struct t_PARTDESC* rtk_part, int offset)
{
	int ret;
   	bool bTarBz2 = false, bTar=false;
	char command[256] = {0}, path[256] = {0};
   	time_t stime, etime;
#ifdef __OFFLINE_GENERATE_BIN__
	char tmp_file_name[128];
	unsigned long long tmp_part_size;
	char tmp_mount_point[32];
#endif

   	stime = time(NULL);

   	// show info
   	install_info("\r\nBurning %s efwtype(%d) partition with install_offset(%u)  ...\r\n"
   		, inv_by_fwtype(pimgdesc->efwtype)
   		, pimgdesc->efwtype
		, pimgdesc->install_offset);
	install_info( "  flash_offset:0x%08llx flash_allo_size:0x%08llx bytes (%llu sector)\r\n"
		"tarfile_offset:0x%08x      image_size:0x%08x bytes (%10u Bytes = %6u KB)\r\n"
         	, pimgdesc->flash_offset
         	, pimgdesc->flash_allo_size
         	, pimgdesc->sector
         	, pimgdesc->tarfile_offset
         	, pimgdesc->img_size
         	, pimgdesc->img_size
         	, pimgdesc->img_size/1024);

#ifdef PC_SIMULATE
	goto finished;
#endif

#ifdef NAS_ENABLE
	if(rtk_extract_utility(prtkimgdesc) < 0)
	{
	   install_debug("rtk_extract_utility fail\r\n");
	   return -1;
	}

    install_info("\r\nNAS eburn(%d) hdd_dev_name(%s) filename(%s) mount_dev(%s) fs(%s) filesize(%lld) img_size(%d)...\r\n"
                , prtkimgdesc->eburn
                , prtkimgdesc->hdd_dev_name
                , pimgdesc->filename
                , rtk_part->mount_dev
                , inv_efs_to_str(rtk_part->efs)
                , rtk_part->min_size
                , pimgdesc->img_size);
    if (prtkimgdesc->eburn == BURN_BY_MTDBLOCK){
        if ((strstr(pimgdesc->filename, ".bin") || strstr(pimgdesc->filename, ".img"))
            && !strncmp(rtk_part->mount_dev+5, prtkimgdesc->hdd_dev_name, 3) ) {
            /* Umount before dd */
	    sprintf(command, "umount -lf %s", rtk_part->mount_dev);
            rtk_command(command, __LINE__, __FILE__);

            snprintf(command, sizeof(command), "tar xvOf %s %s | dd of=%s bs=4096 count=%d conv=notrunc,fsync"
                , prtkimgdesc->tarinfo.tarfile_path
                , pimgdesc->filename
                , rtk_part->mount_dev
                , (pimgdesc->img_size+4095)/4096);

           ret = rtk_command(command, __LINE__, __FILE__);
           if (ret < 0)
                return -1;

            /* Resize partition */
            snprintf(command, sizeof(command), "%s %s"
                , RESIZE2FS_BIN
                , rtk_part->mount_dev);
            rtk_command(command, __LINE__, __FILE__);

            goto finished;
        }
        else
            return -1;
    }
#endif

	if (strstr(pimgdesc->filename, ".tar.bz2")) {
		bTarBz2 = true;
	} else if (strstr(pimgdesc->filename, ".tar")) {
		bTar = true;
	} else if (strstr(pimgdesc->filename, ".bin")) {
		snprintf(command, sizeof(command), "tar xvOf %s %s | dd of=%s bs=512 seek=%llu count=%d"
			, prtkimgdesc->tarinfo.tarfile_path
			, pimgdesc->filename
			, prtkimgdesc->mtdblock_path
			, (pimgdesc->flash_offset+offset)/512
			, pimgdesc->img_size/512);

		ret = rtk_command(command, __LINE__, __FILE__);
		if (ret < 0)
			return -1;

		goto finished;
	}
	// if has mapped .tar.bz2 or .tar
	if ( !bTarBz2  && !bTar) {
		install_fail("Target is not .tar.bz2 nor .tar");
		return -1;
	}

#ifndef __OFFLINE_GENERATE_BIN__
	if(rtk_extract_utility(prtkimgdesc) < 0)
	{
	   install_debug("rtk_extract_utility fail\r\n");
	   return -1;
	}
#ifdef ENABLE_ERASE_CHECK
	if (pimgdesc->erased) {
#endif
		// we do format and mount before the install

        	sprintf(command, "%s -t %s %s > /dev/null", MKE2FS_BIN, inv_efs_to_str(rtk_part->efs), rtk_part->mount_dev);
        	ret = rtk_command(command, __LINE__, __FILE__);
        	if (ret < 0)
			return -1;
#ifdef ENABLE_ERASE_CHECK
	} else {
		install_debug("ERASE_CHECK: Not erasing Part:%s\n", pimgdesc->part_name);
    	}
#endif
#else
	// this way avoids too much modification
	strncpy(tmp_mount_point, pimgdesc->mount_point, sizeof(tmp_mount_point));
	sprintf(pimgdesc->mount_point, "%s%s", PKG_TEMP, tmp_mount_point);
#endif

	// in some cases on android, mkdir -p will return error if there is a existed special link.
	// We add rm before mkdir.
	sprintf(command, "rm -rf %s/%s;mkdir -p %s/%s", EXT4_MOUNTP, pimgdesc->mount_point, EXT4_MOUNTP, pimgdesc->mount_point);
	ret = rtk_command(command, __LINE__, __FILE__);
	if (ret < 0)
		return -1;

#ifndef __OFFLINE_GENERATE_BIN__
	sprintf(command, "mount -t %s %s %s/%s", inv_efs_to_str(rtk_part->efs), rtk_part->mount_dev, EXT4_MOUNTP, pimgdesc->mount_point);
    	ret = rtk_command(command, __LINE__, __FILE__);
    	if (ret < 0)
    		return -1;
#endif //__OFFLINE_GENERATE_BIN__

#ifndef PC_SIMULATE
	if (gDebugPrintfLogLevel&INSTALL_MEM_LEVEL)
		system("cat /proc/meminfo");
#endif
	if (pimgdesc->install_mode == eFIRSTTIME) {
			//TODO
     }

	// TODO: delete all at first, and copy new files. Maybe we need partial update.
	snprintf(path, sizeof(path), "%s", pimgdesc->mount_point);

	if(bTarBz2) {
		if (gDebugPrintfLogLevel&INSTALL_TARLOG_LEVEL) {
			snprintf(command, sizeof(command), "tar xvOf %s %s | tar xvfj - -C %s/%s; sync"
				, prtkimgdesc->tarinfo.tarfile_path
				, pimgdesc->filename
				, EXT4_MOUNTP
				, path);
		} else {
			snprintf(command, sizeof(command), "tar xOf %s %s | tar xfj - -C %s/%s; sync"
				, prtkimgdesc->tarinfo.tarfile_path
				, pimgdesc->filename
				, EXT4_MOUNTP
				, path);
		}
	} else if(bTar) {
		if (gDebugPrintfLogLevel&INSTALL_TARLOG_LEVEL) {
			snprintf(command, sizeof(command), "tar xvOf %s %s | tar xvf - -C %s/%s; sync"
				, prtkimgdesc->tarinfo.tarfile_path
			     , pimgdesc->filename
			     , EXT4_MOUNTP
			     , path);
		} else {
			snprintf(command, sizeof(command), "tar xOf %s %s | tar xf - -C %s/%s; sync"
               	, prtkimgdesc->tarinfo.tarfile_path
                    , pimgdesc->filename
                    , EXT4_MOUNTP
                    , path);
		}
	}

	if ( (ret = rtk_command(command, __LINE__, __FILE__)) < 0) {
		install_debug("Exec command fail\r\n");
		return -1;
	}

#ifndef PC_SIMULATE
	if (gDebugPrintfLogLevel&INSTALL_MEM_LEVEL)
		system("cat /proc/meminfo");

      //drop mem cache
      snprintf(command, sizeof(command), "echo 3 > /proc/sys/vm/drop_caches");
      if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
      {
         install_debug("Exec drop command fail\r\n");
      }
	if (gDebugPrintfLogLevel&INSTALL_MEM_LEVEL)
		system("cat /proc/meminfo");
#endif

#ifdef __OFFLINE_GENERATE_BIN__
	sprintf(tmp_file_name, "%s/%s.ext4img", DEFAULT_OUTPUT_DIR, inv_by_fwtype(pimgdesc->efwtype));
	// reserve 0x10 sectors for partition formatting. If no reservation, mbr will be cleared.
	tmp_part_size = pimgdesc->flash_allo_size-MBR_RESERVE_SIZE;
	if (!strncmp(inv_by_fwtype(pimgdesc->efwtype), "system", 6) ||
			!strncmp(inv_by_fwtype(pimgdesc->efwtype), "data", 4))
		sprintf(command, "./make_ext4fs -l %lld -a %s %s %s/", tmp_part_size, inv_by_fwtype(pimgdesc->efwtype), DEFAULT_EXT4_OUTPUT, pimgdesc->mount_point);
	else //if (pimgdesc->flash_allo_size > 512*1024*1024)
		sprintf(command, "./make_ext4fs -l %lld %s %s/", tmp_part_size, DEFAULT_EXT4_OUTPUT, pimgdesc->mount_point);

	ret = rtk_command(command, __LINE__, __FILE__);
	if (ret < 0)
		goto failed;

	//if we don't generate a one big image, we can skip this
	if (prtkimgdesc->all_in_one) {
     	install_info("Burning %s to %s at %llx with %lld bytes\n", tmp_file_name, DEFAULT_TEMP_OUTPUT, pimgdesc->flash_offset, tmp_part_size);
		if (pimgdesc->emmc_partIdx <= 4)
      		ret = rtk_file_to_flash(DEFAULT_EXT4_OUTPUT, 0, DEFAULT_TEMP_OUTPUT, pimgdesc->flash_offset, tmp_part_size, NULL);
		else
			ret = rtk_file_to_flash(DEFAULT_EXT4_OUTPUT, 0, DEFAULT_TEMP_OUTPUT, pimgdesc->flash_offset+MBR_RESERVE_SIZE, tmp_part_size, NULL);

      	if(ret < 0) {
         		install_fail("burn %s into %s fail\r\n", DEFAULT_EXT4_OUTPUT, DEFAULT_TEMP_OUTPUT);
         		goto failed;
      	}

		//remove temp file
      	sprintf(command, "rm -rf %s", DEFAULT_EXT4_OUTPUT);
      	if((ret = rtk_command(command, __LINE__, __FILE__)) < 0) {
	         	install_debug("Exec command fail\r\n");
     	    	return -1;
      	}
	} else {	// not all_in_one mode
		if (pimgdesc->emmc_partIdx <= 4) {
         		sprintf(command, "mv %s %s", DEFAULT_EXT4_OUTPUT, tmp_file_name);
			if((ret = rtk_command(command, __LINE__, __FILE__)) < 0)
            		goto failed;
      		} else {
         			ret = rtk_file_to_flash(DEFAULT_EXT4_OUTPUT, 0, tmp_file_name, MBR_RESERVE_SIZE, tmp_part_size, NULL);
         			if(ret < 0) {
            			install_fail("burn %s into %s fail\r\n", DEFAULT_EXT4_OUTPUT, tmp_file_name);
            			goto failed;
         			}

				//remove temp file
         			sprintf(command, "rm -rf %s", DEFAULT_EXT4_OUTPUT);
         			if((ret = rtk_command(command, __LINE__, __FILE__)) < 0) {
 					install_debug("Exec command fail\r\n");
            			return -1;
         			}
      		}
		}
failed:
		// this way avoids too much modification
	strncpy(pimgdesc->mount_point, tmp_mount_point, sizeof(pimgdesc->mount_point));
#else
#ifdef NAS_ENABLE
	sprintf(command, "umount -lf %s/%s", EXT4_MOUNTP, pimgdesc->mount_point);
    	ret = rtk_command(command, __LINE__, __FILE__);
    	//if (ret < 0)
    	//	return -1;
#endif //NAS_ENABLE
#endif

finished:
   	etime = time(NULL);
   	install_debug("total time: %.0lf seconds\r\n", difftime(etime,stime));
   	install_debug("Complete!!\r\n\r\n");
   	fflush(stdout);
   	return 0;
}
#endif

// burn single partition
int rtk_burn_single_part(struct t_rtkimgdesc* prtkimgdesc, enum FWTYPE efwtype)
{
   int ret=0;
   struct t_PARTDESC* rtk_part = NULL;
   time_t stime, etime;

   stime = time(NULL);
   // find internal partition info data structure

   // take extended partition into consideration
   // if num_of_partition is greater than 4, and the order of the efwtype partition
   // is greater than 3, we've to reserve MBR area.
   int offset=0;
   if( prtkimgdesc->eburn == BURN_BY_MMCBLOCK ) {
   	  int i;
	  int part_order=0;
      for( i=0; i<NUM_RTKPART; i++ ) {
	     if( ! rtk_part_list_sort[i] ) {
		    break;
	     } else if( rtk_part_list_sort[i]->efwtype == efwtype ) {
	     	part_order = i+1;
	     }
      }
	  install_log("num_of_partition(%d), part_order(%d)\n", i, part_order);

	  if( (i>4) && (part_order>3) ) {
	     offset = MBR_RESERVE_SIZE;
	  }
   }

   rtk_part = find_part_by_efwtype((struct t_PARTDESC*)&rtk_part_list, efwtype);
   if(rtk_part == NULL)
   {
      install_log("PASS %s(find_part_by_efwtype return NULL)\r\n", inv_by_fwtype(efwtype));
      return -1;
   }
#if defined(NAS_ENABLE) && defined(CONFIG_BOOT_FROM_SPI)
   if(rtk_part->efs == FS_TYPE_RAWFILE || (rtk_part->efs == FS_TYPE_SQUASH && prtkimgdesc->eburn != BURN_BY_MTDBLOCK))
#else
   if(rtk_part->efs == FS_TYPE_SQUASH || rtk_part->efs == FS_TYPE_RAWFILE)
#endif
   {
      ret = rtk_burn_img(prtkimgdesc, &prtkimgdesc->fw[efwtype], offset);
      prtkimgdesc->fw[efwtype].flash_offset += offset;
   }
   else if(rtk_part->efs == FS_TYPE_UBIFS)
   {
      ret = rtk_burn_ubifs_img(prtkimgdesc, &prtkimgdesc->fw[efwtype], rtk_part);
   }
   else if(rtk_part->efs == FS_TYPE_YAFFS2)
   {
      ret = rtk_burn_yaffs_img(prtkimgdesc, &prtkimgdesc->fw[efwtype], rtk_part);
   }
   else if(rtk_part->efs == FS_TYPE_JFFS2)
   {
      ret = rtk_burn_jffs_img(prtkimgdesc, &prtkimgdesc->fw[efwtype], rtk_part);
   }
#if defined(EMMC_SUPPORT) or defined(NAS_ENABLE)
#if defined(NAS_ENABLE) && defined(CONFIG_BOOT_FROM_SPI)
   else if (rtk_part->efs == FS_TYPE_EXT4 || (rtk_part->efs == FS_TYPE_SQUASH && prtkimgdesc->eburn == BURN_BY_MTDBLOCK))
#else
   else if (rtk_part->efs == FS_TYPE_EXT4)
#endif
   {
      ret = rtk_burn_ext4_img(prtkimgdesc, &prtkimgdesc->fw[efwtype], rtk_part, offset);
      prtkimgdesc->fw[efwtype].flash_offset += offset;
   }
#endif

   etime = time(NULL);
   install_info("total time: %.0lf seconds\r\n", difftime(etime,stime));
   install_info("Complete!!\r\n\r\n");

   return ret;
}

int rtk_burn_img_from_net(struct t_rtkimgdesc* prtkimgdesc, struct t_imgdesc* pimgdesc)
{
   int ret;
   int dev_fd, sock=0;
   unsigned int checksum;

   // show info
   install_info("\r\nBurning %s efwtype(%d) image with install_offset(%u)  ...\r\n"
   , inv_by_fwtype(pimgdesc->efwtype)
   , pimgdesc->efwtype
   , pimgdesc->install_offset);

   // sanity-check
   if(0 == pimgdesc->img_size)
   {
      install_debug("%s don't exist\r\n", inv_by_fwtype(pimgdesc->efwtype));
      return -1;
   }

   install_info( "  flash_offset:0x%08llx flash_allo_size:0x%08llx bytes (%llu KB = %llu sector)\r\n"
         "tarfile_offset:0x%08x      image_size:0x%08x bytes (%u KB)\r\n"
         , pimgdesc->flash_offset
         , pimgdesc->flash_allo_size
         , pimgdesc->flash_allo_size/1024
         , pimgdesc->sector
         , pimgdesc->tarfile_offset
         , pimgdesc->img_size, pimgdesc->img_size/1024);

   // nand space sanity-check
   if(prtkimgdesc->flash_type == MTD_NANDFLASH)
   {
      if(pimgdesc->flash_offset < BOOTCODE_RESERVED_SIZE)
      {
         install_fail("Out of range flash offset:0x%08llx\r\n", pimgdesc->flash_offset);
         return -1;
      }
   }

   // open and lseek block device
   dev_fd = rtk_open_mtd_block_with_offset(pimgdesc->flash_offset);
   if(dev_fd<0)
   {
      install_debug("Can't Open mtdblock(%lld)\r\n", pimgdesc->flash_offset);
      return -1;
   }

   // open tarfile fd streaming
   ret = rtk_open_tarfile_from_urltar(&prtkimgdesc->tarinfo.fd, &prtkimgdesc->url, pimgdesc->tarfile_offset, pimgdesc->install_offset, pimgdesc->img_size);
   //prtkimgdesc->tarinfo.fd = -1;
   //ret = rtk_open_tarfile_with_offset(&prtkimgdesc->tarinfo, pimgdesc->tarfile_offset+sizeof(t_tarheader));
   if(ret < 0)
   {
      install_debug("Can't Open tarfile : %s\r\n", prtkimgdesc->tarinfo.tarfile_path);
      close(dev_fd);
      close(prtkimgdesc->tarinfo.fd);
      return -1;
   }

   // installing
   // read data from tarfile
   // write data into flash
   install_debug("start burning\r\n");
   ret = fd_to_fd(sock, dev_fd, pimgdesc->img_size, &checksum);
   pimgdesc->checksum = checksum;
   close(prtkimgdesc->tarinfo.fd);
   close(dev_fd);

   if((unsigned int)ret != pimgdesc->img_size)
   {
      install_debug("Burn not complete, ret=%d\r\n", ret);
      return -1;
   }
   install_info("Complete!!\r\n");
   fflush(stdout);
   return 0;
}

int rtk_burn_cipher_key(struct t_rtkimgdesc* prtkimgdesc)
{
	int ret = 0;

#ifndef PC_SIMULATE
	install_info("\nBurning rtk_burn_cipher_key() img.secure_boot(%d)\n",prtkimgdesc->secure_boot);

	ret = rtk_extract_cipher_key(prtkimgdesc);
	if(ret < 0) {
		install_fail("Can't extract cipher key\n");
		return -1;
   	}

	unsigned char Krsa[256], Kseed[16], Key[16], Key1[16], Key2[16], Key3[16];
    ret = read_key(KEY_RSA_BIN, Krsa, 256);
    if( prtkimgdesc->secure_boot ) {
        ret += read_key(KEY_KSEED_BIN, Kseed, 16);
        ret += read_key(KEY_K_BIN, Key, 16);
        ret += read_key(KEY_K1_BIN, Key1, 16);
        ret += read_key(KEY_K2_BIN, Key2, 16);
        ret += read_key(KEY_K3_BIN, Key3, 16);
    }
        
    if( ret < 0 ) {
        install_fail("key not correct\n");
        return -1;
    } 

#ifdef KEY_DEBUG
	rtk_hexdump("rsa",   (unsigned char*)&Krsa, 256);
	rtk_hexdump("kseed", (unsigned char*)&Kseed, 32);
	rtk_hexdump("key",   (unsigned char*)&Key, 32);
    rtk_hexdump("key1",  (unsigned char*)&Key1, 32);
	rtk_hexdump("key2",  (unsigned char*)&Key2, 32);
    rtk_hexdump("key3",  (unsigned char*)&Key3, 32);
#endif    
	if (ca_efuse_programmer( Kseed,  Key, Key1, Key2, Key3, Krsa) !=0 ) {
        install_info("ca_efuse_programmer fail!!\r\n\r\n");
		return -1;
    }
#endif
	install_info("Complete!!\r\n\r\n");
	return 0;
}
#ifdef NAS_ENABLE
static int nas_rescue_fwdesc(struct t_rtkimgdesc* prtkimg, struct rtk_fw_header* rtk_fw_nas_rescue)
{
    int i;
    fw_type_code_t fwtype;
    FWTYPE efwtype = FW_UNKNOWN;
    /* Replace kernel, rescue DTB and rescue rootfs with NAS redundant copies */
    for(i=0; i< (int)rtk_fw_nas_rescue->fw_count; i++){
        fwtype = (fw_type_code_t)rtk_fw_nas_rescue->fw_desc[i].v2.type;
        switch(fwtype)
        {
            case FW_TYPE_KERNEL:
                efwtype = FW_NAS_KERNEL;
                break;
            case FW_TYPE_RESCUE_DT:
                efwtype = FW_NAS_RESCUE_DT;
                break;
            case FW_TYPE_RESCUE_ROOTFS:
                efwtype = FW_NAS_RESCUE_ROOTFS;
                break;
            default :
                efwtype = FW_UNKNOWN;
                break;
        }

        if(efwtype != FW_UNKNOWN && prtkimg->fw[efwtype].img_size != 0){
            /* re-calculate checksum, non-secure boot */
            rtk_fw_nas_rescue->fw_tab.checksum -= get_checksum((u8*) &rtk_fw_nas_rescue->fw_desc[i], sizeof(fw_desc_entry_v2_t));

            rtk_fw_nas_rescue->fw_desc[i].v2.target_addr = prtkimg->fw[efwtype].mem_offset;//
            rtk_fw_nas_rescue->fw_desc[i].v2.offset = prtkimg->fw[efwtype].flash_offset;//
            rtk_fw_nas_rescue->fw_desc[i].v2.length = prtkimg->fw[efwtype].img_size;//
            rtk_fw_nas_rescue->fw_desc[i].v2.paddings = prtkimg->fw[efwtype].flash_allo_size;//
            //rtk_fw_nas_rescue->fw_desc[i].v2.checksum = prtkimg->fw[efwtype].checksum;
            memcpy(rtk_fw_nas_rescue->fw_desc[i].v2.sha_hash,  prtkimg->fw[efwtype].sha_hash, SHA256_SIZE);

            /* re-calculate checksum, non-secure boot */
            rtk_fw_nas_rescue->fw_tab.checksum += get_checksum((u8*) &rtk_fw_nas_rescue->fw_desc[i], sizeof(fw_desc_entry_v2_t));
        }
    }
    install_debug("rescue fwdesc checksum:0x%08x\r\n", rtk_fw_nas_rescue->fw_tab.checksum);

    /* Write rescue fwdesc table */
    //else if (prtkimg->flash_type == MTD_EMMC)
    char path[128] = {0};
    int dev_fd;
    snprintf(path, sizeof(path), "%s/%s", PKG_TEMP, RESCUEFWDESCTABLE);
    unlink(path);
    if ((dev_fd = open(path, O_RDWR|O_SYNC|O_CREAT, 0644)) < 0) {
        install_fail("error! open file %s fail\r\n", path);
        return -1;
    }

    // write fw_tab
    write(dev_fd, &rtk_fw_nas_rescue->fw_tab, sizeof(fw_desc_table_t));
#ifdef _DEBUG
    install_test("rescue fw table\r\n");
    dump_fw_desc_table(&rtk_fw_nas_rescue->fw_tab);
#endif

    // write part_desc
    for(i=0;i<(int)rtk_fw_nas_rescue->part_count;i++) {
        write(dev_fd, &rtk_fw_nas_rescue->part_desc[i], sizeof(part_desc_entry_v2_t));
#ifdef _DEBUG
        install_test("rescue part desc[%d]\r\n",i);
        dump_part_desc_entry_v2(&rtk_fw_nas_rescue->part_desc[i]);
#endif
    }

    // write fw_desc w/o secure_boot
    for(i=0;i<(int)rtk_fw_nas_rescue->fw_count;i++) {
        write(dev_fd, &rtk_fw_nas_rescue->fw_desc[i], sizeof(fw_desc_entry_v2_t));
#ifdef _DEBUG
        install_test("rescue fw desc[%d]\r\n",i);
        dump_fw_desc_entry_v22(&rtk_fw_nas_rescue->fw_desc[i]);
#endif
    }
    close(dev_fd);

    /* Write rescue fwdesc table on NAND */
#ifndef __OFFLINE_GENERATE_BIN__
    if(prtkimg->flash_type == MTD_NANDFLASH){
        struct t_imgdesc *nasdesc= &prtkimg->fw[FW_NAS_RESCUE_DT];

        char cmd[128];
        // flash_erase
        snprintf(cmd, sizeof(cmd), "%s %s %llu %d"
                                 , FLASHERASE_BIN
                                 , prtkimg->mtd_path
                                 , nasdesc->flash_offset - prtkimg->mtd_erasesize
                                 , 1);
        if( rtk_command(cmd, __LINE__, __FILE__) < 0)
        {
           install_debug("Exec command fail\r\n");
           return -1;
        }

        snprintf(cmd, sizeof(cmd), "%s -s %llu -p %s %s"
                          , NANDWRITE_BIN
                          , nasdesc->flash_offset - prtkimg->mtd_erasesize
                          , prtkimg->mtd_path, path);

        if( rtk_command(cmd, __LINE__, __FILE__) < 0)
        {
           install_debug("Exec command fail\r\n");
           return -1;
        }
    }
#endif
    return 0;
}
#endif

#if defined(NAS_ENABLE) && defined(CONFIG_BOOT_FROM_SPI)
static int burn_fwdesc_nor(struct t_rtkimgdesc* prtkimg)
{
   int ret=0;
   u8 fw_count;
   struct rtk_fw_header rtk_fw_head;

   memset(&rtk_fw_head, 0, sizeof(rtk_fw_head));

   // sanity-check
   if(prtkimg->fw[FW_KERNEL].img_size == 0 || prtkimg->fw[FW_RESCUE_DT].img_size == 0 \
      || prtkimg->fw[FW_KERNEL_DT].img_size == 0 || prtkimg->fw[FW_AUDIO].img_size == 0 \
      || prtkimg->fw[FW_RESCUE_ROOTFS].img_size == 0
      )
   {
      install_debug("FW_KERNEL(%u), FW_AUDIO(%u), FW_RESCUE_DT(%u), FW_KERNEL_DT(%u), FW_RESCUE_ROOTFS(%u) zero check.\r\n"  \
         , prtkimg->fw[FW_KERNEL].img_size   \
         , prtkimg->fw[FW_AUDIO].img_size \
         , prtkimg->fw[FW_RESCUE_DT].img_size
         , prtkimg->fw[FW_KERNEL_DT].img_size   \
         , prtkimg->fw[FW_RESCUE_ROOTFS].img_size);
      return -1;
   }

   // fw field
   // kernel fw_desc
	fw_count = 1;
   rtk_fw_head.fw_count = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_KERNEL;
   if (!strcmp(prtkimg->fw[FW_KERNEL].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_KERNEL].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_KERNEL].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_KERNEL].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_KERNEL].flash_allo_size;//
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_KERNEL].sha_hash, SHA256_SIZE);

   // Rescue_DT fw_desc
   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_RESCUE_DT;
   if (!strcmp(prtkimg->fw[FW_RESCUE_DT].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_RESCUE_DT].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_RESCUE_DT].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_RESCUE_DT].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_RESCUE_DT].flash_allo_size;//
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_RESCUE_DT].sha_hash, SHA256_SIZE);

   // Kernel_DT fw_desc
   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_KERNEL_DT;
   if (!strcmp(prtkimg->fw[FW_KERNEL_DT].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_KERNEL_DT].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_KERNEL_DT].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_KERNEL_DT].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_KERNEL_DT].flash_allo_size;//
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_KERNEL_DT].sha_hash, SHA256_SIZE);

   // audio fw_desc
   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_AUDIO;
   if (!strcmp(prtkimg->fw[FW_AUDIO].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_AUDIO].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_AUDIO].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_AUDIO].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_AUDIO].flash_allo_size;//
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_AUDIO].sha_hash, SHA256_SIZE);

   // RescueRootfs fw_desc
   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_RESCUE_ROOTFS;
   if (!strcmp(prtkimg->fw[FW_RESCUE_ROOTFS].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_RESCUE_ROOTFS].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_RESCUE_ROOTFS].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_RESCUE_ROOTFS].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_RESCUE_ROOTFS].flash_allo_size;//
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_RESCUE_ROOTFS].sha_hash, SHA256_SIZE);

   // fw_count
   rtk_fw_head.fw_count++;

   // firmware partition
   rtk_fw_head.part_count = 0;

   memcpy(rtk_fw_head.fw_tab.signature, VERONA_SIGNATURE, 8);
   rtk_fw_head.fw_tab.version = FIRMWARE_DESCRIPTION_TABLE_VERSION_02; //non-secure boot

   // save
   ret = save_fwdesc_nor(&rtk_fw_head, prtkimg);
   if(ret < 0)
   {
      install_debug("save_fwdesc() fail\r\n");
      return -1;
   }

   return 0;
}
#else
static int burn_fwdesc_nor(struct t_rtkimgdesc* prtkimg)
{
   int ret;
   u8 fw_count;
   struct rtk_fw_header rtk_fw_head;
   memset(&rtk_fw_head, 0, sizeof(rtk_fw_head));

   // sanity-check
   if(prtkimg->fw[FW_KERNEL].img_size == 0 || prtkimg->fw[FW_AUDIO].img_size == 0 \
      || prtkimg->fw[FW_VIDEO].img_size == 0 || prtkimg->fw[FW_ROOTFS].img_size == 0 \
      || prtkimg->fw[FW_USR_LOCAL_ETC].img_size == 0)
   {
      install_debug("FW_KERNEL(%u), FW_AUDIO(%u), FW_VIDWO(%u), FW_ROOTFS(%u), FW_ETC(%u) zero check.\r\n"  \
         , prtkimg->fw[FW_KERNEL].img_size   \
         , prtkimg->fw[FW_AUDIO].img_size \
         , prtkimg->fw[FW_VIDEO].img_size
         , prtkimg->fw[FW_ROOTFS].img_size   \
         , prtkimg->fw[FW_USR_LOCAL_ETC].img_size);
      return -1;
   }
   fw_count = 6;
   FW_SET(&rtk_fw_head, FW_TYPE_KERNEL);
   FW_SET(&rtk_fw_head, FW_TYPE_AUDIO);
   FW_SET(&rtk_fw_head, FW_TYPE_VIDEO);
   FW_SET(&rtk_fw_head, FW_TYPE_SQUASH);
   FW_SET(&rtk_fw_head, FW_TYPE_JFFS2);
   FW_SET(&rtk_fw_head, FW_TYPE_BOOTCODE);

   // kernel
   rtk_fw_head.fw_desc[FW_TYPE_KERNEL].v2.type = FW_TYPE_KERNEL;
   rtk_fw_head.fw_desc[FW_TYPE_KERNEL].v2.ro = 0;
   rtk_fw_head.fw_desc[FW_TYPE_KERNEL].v2.version = 0;
   rtk_fw_head.fw_desc[FW_TYPE_KERNEL].v2.target_addr = prtkimg->fw[FW_KERNEL].mem_offset;//
   rtk_fw_head.fw_desc[FW_TYPE_KERNEL].v2.offset = prtkimg->fw[FW_KERNEL].flash_offset;//
   rtk_fw_head.fw_desc[FW_TYPE_KERNEL].v2.length = prtkimg->fw[FW_KERNEL].img_size;//
   rtk_fw_head.fw_desc[FW_TYPE_KERNEL].v2.paddings = prtkimg->fw[FW_KERNEL].flash_allo_size;//
   //rtk_fw_head.fw_desc[FW_TYPE_KERNEL].v2.checksum = prtkimg->fw[FW_KERNEL].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_KERNEL].sha_hash, SHA256_SIZE);

   // audio
   rtk_fw_head.fw_desc[FW_TYPE_AUDIO].v2.type = FW_TYPE_AUDIO;
   rtk_fw_head.fw_desc[FW_TYPE_AUDIO].v2.ro = 0;
   rtk_fw_head.fw_desc[FW_TYPE_AUDIO].v2.version = 0;
   rtk_fw_head.fw_desc[FW_TYPE_AUDIO].v2.target_addr = prtkimg->fw[FW_AUDIO].mem_offset;//
   rtk_fw_head.fw_desc[FW_TYPE_AUDIO].v2.offset = prtkimg->fw[FW_AUDIO].flash_offset;//
   rtk_fw_head.fw_desc[FW_TYPE_AUDIO].v2.length = prtkimg->fw[FW_AUDIO].img_size;//
   rtk_fw_head.fw_desc[FW_TYPE_AUDIO].v2.paddings = prtkimg->fw[FW_AUDIO].flash_allo_size;//
   //rtk_fw_head.fw_desc[FW_TYPE_AUDIO].v2.checksum = prtkimg->fw[FW_AUDIO].checksum;//
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_AUDIO].sha_hash, SHA256_SIZE);

   // video
   rtk_fw_head.fw_desc[FW_TYPE_VIDEO].v2.type = FW_TYPE_VIDEO;
   rtk_fw_head.fw_desc[FW_TYPE_VIDEO].v2.ro = 0;
   rtk_fw_head.fw_desc[FW_TYPE_VIDEO].v2.version = 0;
   rtk_fw_head.fw_desc[FW_TYPE_VIDEO].v2.target_addr = prtkimg->fw[FW_VIDEO].mem_offset;//
   rtk_fw_head.fw_desc[FW_TYPE_VIDEO].v2.offset = prtkimg->fw[FW_VIDEO].flash_offset;//
   rtk_fw_head.fw_desc[FW_TYPE_VIDEO].v2.length = prtkimg->fw[FW_VIDEO].img_size;//
   rtk_fw_head.fw_desc[FW_TYPE_VIDEO].v2.paddings = prtkimg->fw[FW_VIDEO].flash_allo_size;//
   //rtk_fw_head.fw_desc[FW_TYPE_VIDEO].v2.checksum = prtkimg->fw[FW_VIDEO].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_VIDEO].sha_hash, SHA256_SIZE);

   // rootfs
   rtk_fw_head.fw_desc[FW_TYPE_SQUASH].v2.type = FW_TYPE_SQUASH;
   rtk_fw_head.fw_desc[FW_TYPE_SQUASH].v2.ro = 0;
   rtk_fw_head.fw_desc[FW_TYPE_SQUASH].v2.version = 0;
   rtk_fw_head.fw_desc[FW_TYPE_SQUASH].v2.target_addr = prtkimg->fw[FW_ROOTFS].mem_offset;//
   rtk_fw_head.fw_desc[FW_TYPE_SQUASH].v2.offset = prtkimg->fw[FW_ROOTFS].flash_offset;//
   rtk_fw_head.fw_desc[FW_TYPE_SQUASH].v2.length = prtkimg->fw[FW_ROOTFS].img_size;//
   rtk_fw_head.fw_desc[FW_TYPE_SQUASH].v2.paddings = prtkimg->fw[FW_ROOTFS].flash_allo_size;//
   //rtk_fw_head.fw_desc[FW_TYPE_SQUASH].v2.checksum = prtkimg->fw[FW_ROOTFS].checksum;//
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_ROOTFS].sha_hash, SHA256_SIZE);

   // etc
   rtk_fw_head.fw_desc[FW_TYPE_JFFS2].v2.type = FW_TYPE_JFFS2;
   rtk_fw_head.fw_desc[FW_TYPE_JFFS2].v2.ro = 0;
   rtk_fw_head.fw_desc[FW_TYPE_JFFS2].v2.version = 0;
   rtk_fw_head.fw_desc[FW_TYPE_JFFS2].v2.target_addr = prtkimg->fw[FW_USR_LOCAL_ETC].mem_offset;//
   rtk_fw_head.fw_desc[FW_TYPE_JFFS2].v2.offset = prtkimg->fw[FW_USR_LOCAL_ETC].flash_offset;//
   rtk_fw_head.fw_desc[FW_TYPE_JFFS2].v2.length = prtkimg->fw[FW_USR_LOCAL_ETC].img_size;//
   rtk_fw_head.fw_desc[FW_TYPE_JFFS2].v2.paddings = prtkimg->fw[FW_USR_LOCAL_ETC].flash_allo_size;//
   //rtk_fw_head.fw_desc[FW_TYPE_JFFS2].v2.checksum = prtkimg->fw[FW_USR_LOCAL_ETC].checksum;//
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_USR_LOCAL_ETC].sha_hash, SHA256_SIZE);

   // bootcode
   rtk_fw_head.fw_desc[FW_TYPE_BOOTCODE].v2.type = FW_TYPE_BOOTCODE;
   rtk_fw_head.fw_desc[FW_TYPE_BOOTCODE].v2.ro = 0;
   rtk_fw_head.fw_desc[FW_TYPE_BOOTCODE].v2.version = 0;
   rtk_fw_head.fw_desc[FW_TYPE_BOOTCODE].v2.target_addr = prtkimg->fw[FW_BOOTCODE].mem_offset;//
   rtk_fw_head.fw_desc[FW_TYPE_BOOTCODE].v2.offset = prtkimg->fw[FW_BOOTCODE].flash_offset;//
   rtk_fw_head.fw_desc[FW_TYPE_BOOTCODE].v2.length = prtkimg->fw[FW_BOOTCODE].img_size;//
   rtk_fw_head.fw_desc[FW_TYPE_BOOTCODE].v2.paddings = prtkimg->fw[FW_BOOTCODE].flash_allo_size;//
   //rtk_fw_head.fw_desc[FW_TYPE_BOOTCODE].v2.checksum = prtkimg->fw[FW_BOOTCODE].checksum;//
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_BOOTCODE].sha_hash, SHA256_SIZE);


   // audio bootfile
   if(prtkimg->fw[FW_AUDIO_BOOTFILE].img_size != 0)
   {
   FW_SET(&rtk_fw_head, FW_TYPE_AUDIO_FILE);
   fw_count++;
   rtk_fw_head.fw_desc[FW_TYPE_AUDIO_FILE].v2.type = FW_TYPE_AUDIO_FILE;
   rtk_fw_head.fw_desc[FW_TYPE_AUDIO_FILE].v2.ro = 0;
   rtk_fw_head.fw_desc[FW_TYPE_AUDIO_FILE].v2.version = 0;
   rtk_fw_head.fw_desc[FW_TYPE_AUDIO_FILE].v2.target_addr = prtkimg->fw[FW_AUDIO_BOOTFILE].mem_offset;//
   rtk_fw_head.fw_desc[FW_TYPE_AUDIO_FILE].v2.offset = prtkimg->fw[FW_AUDIO_BOOTFILE].flash_offset;//
   rtk_fw_head.fw_desc[FW_TYPE_AUDIO_FILE].v2.length = prtkimg->fw[FW_AUDIO_BOOTFILE].img_size;//
   rtk_fw_head.fw_desc[FW_TYPE_AUDIO_FILE].v2.paddings = prtkimg->fw[FW_AUDIO_BOOTFILE].flash_allo_size;//
   //rtk_fw_head.fw_desc[FW_TYPE_AUDIO_FILE].v2.checksum = prtkimg->fw[FW_AUDIO_BOOTFILE].checksum;//
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_AUDIO_BOOTFILE].sha_hash, SHA256_SIZE);
   }

   // video bootfile
   if(prtkimg->fw[FW_VIDEO_BOOTFILE].img_size != 0)
   {
   FW_SET(&rtk_fw_head, FW_TYPE_VIDEO_FILE);
   fw_count++;
   rtk_fw_head.fw_desc[FW_TYPE_VIDEO_FILE].v2.type = FW_TYPE_VIDEO_FILE;
   rtk_fw_head.fw_desc[FW_TYPE_VIDEO_FILE].v2.ro = 0;
   rtk_fw_head.fw_desc[FW_TYPE_VIDEO_FILE].v2.version = 0;
   rtk_fw_head.fw_desc[FW_TYPE_VIDEO_FILE].v2.target_addr = prtkimg->fw[FW_VIDEO_BOOTFILE].mem_offset;//
   rtk_fw_head.fw_desc[FW_TYPE_VIDEO_FILE].v2.offset = prtkimg->fw[FW_VIDEO_BOOTFILE].flash_offset;//
   rtk_fw_head.fw_desc[FW_TYPE_VIDEO_FILE].v2.length = prtkimg->fw[FW_VIDEO_BOOTFILE].img_size;//
   rtk_fw_head.fw_desc[FW_TYPE_VIDEO_FILE].v2.paddings = prtkimg->fw[FW_VIDEO_BOOTFILE].flash_allo_size;//
   //rtk_fw_head.fw_desc[FW_TYPE_VIDEO_FILE].v2.checksum = prtkimg->fw[FW_VIDEO_BOOTFILE].checksum;//
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_VIDEO_BOOTFILE].sha_hash, SHA256_SIZE);
   }
#ifdef TEE_ENABLE
   if(prtkimg->fw[FW_TEE].img_size != 0)
   {
   FW_SET(&rtk_fw_head, FW_TYPE_TEE);
   fw_count++;
   rtk_fw_head.fw_desc[FW_TYPE_TEE].v2.type = FW_TYPE_TEE;
   rtk_fw_head.fw_desc[FW_TYPE_TEE].v2.ro = 0;
   rtk_fw_head.fw_desc[FW_TYPE_TEE].v2.version = 0;
   rtk_fw_head.fw_desc[FW_TYPE_TEE].v2.target_addr = prtkimg->fw[FW_TEE].mem_offset;//
   rtk_fw_head.fw_desc[FW_TYPE_TEE].v2.offset = prtkimg->fw[FW_TEE].flash_offset;//
   rtk_fw_head.fw_desc[FW_TYPE_TEE].v2.length = prtkimg->fw[FW_TEE].img_size;//
   rtk_fw_head.fw_desc[FW_TYPE_TEE].v2.paddings = prtkimg->fw[FW_TEE].flash_allo_size;//
   //rtk_fw_head.fw_desc[FW_TYPE_TEE].v2.checksum = prtkimg->fw[FW_TEE].checksum;//
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_TEE].sha_hash, SHA256_SIZE);
   }
#endif

   // save
   ret = save_fwdesc_nor(&rtk_fw_head);
   if(ret < 0)
   {
      install_debug("save_fwdesc() fail\r\n");
      return -1;
   }

   return 0;

}
#endif

#if 1
static int burn_fwdesc_nand(struct t_rtkimgdesc* prtkimg)
{
   int ret;
   u8 fw_count;
   struct rtk_fw_header rtk_fw_head;
   FWTYPE efwtype;

   memset(&rtk_fw_head, 0, sizeof(rtk_fw_head));
#ifdef NAS_ENABLE
   struct rtk_fw_header rtk_fw_nas_rescue;
   memset(&rtk_fw_nas_rescue, 0, sizeof(rtk_fw_nas_rescue));
#endif

   // sanity-check
   if(prtkimg->fw[FW_KERNEL].img_size == 0 || prtkimg->fw[FW_RESCUE_DT].img_size == 0 \
      || prtkimg->fw[FW_KERNEL_DT].img_size == 0
#ifndef NAS_ENABLE
      || prtkimg->fw[FW_AUDIO].img_size == 0
#endif
      )
   {
      install_debug("FW_KERNEL(%u), FW_RESCUE_DT(%u), FW_KERNEL_DT(%u), FW_AUDIO(%u) zero check.\r\n"  \
         , prtkimg->fw[FW_KERNEL].img_size   \
         , prtkimg->fw[FW_RESCUE_DT].img_size \
         , prtkimg->fw[FW_KERNEL_DT].img_size   \
         , prtkimg->fw[FW_AUDIO].img_size);
      return -1;
   }

   // "fw_count" is the number of firmare that reside in partition[0]
   // "rtk_fw_head.fw_count" is the total number of firmware that reside in the install image.

   // fw field
   // kernel fw_desc
   fw_count = 1;
   rtk_fw_head.fw_count = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_KERNEL;
   if (!strcmp(prtkimg->fw[FW_KERNEL].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_KERNEL].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_KERNEL].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_KERNEL].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_KERNEL].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_KERNEL].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_KERNEL].sha_hash, SHA256_SIZE);

   // Rescue_DT fw_desc
   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_RESCUE_DT;
   if (!strcmp(prtkimg->fw[FW_RESCUE_DT].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_RESCUE_DT].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_RESCUE_DT].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_RESCUE_DT].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_RESCUE_DT].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_RESCUE_DT].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_RESCUE_DT].sha_hash, SHA256_SIZE);

   // Kernel_DT fw_desc
   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_KERNEL_DT;
   if (!strcmp(prtkimg->fw[FW_KERNEL_DT].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_KERNEL_DT].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_KERNEL_DT].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_KERNEL_DT].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_KERNEL_DT].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_KERNEL_DT].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_KERNEL_DT].sha_hash, SHA256_SIZE);

   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_RESCUE_ROOTFS;
   if (!strcmp(prtkimg->fw[FW_RESCUE_ROOTFS].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_RESCUE_ROOTFS].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_RESCUE_ROOTFS].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_RESCUE_ROOTFS].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_RESCUE_ROOTFS].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_RESCUE_ROOTFS].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_RESCUE_ROOTFS].sha_hash, SHA256_SIZE);

   if(prtkimg->fw[FW_KERNEL_ROOTFS].img_size != 0)
   {
   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_KERNEL_ROOTFS;
   if (!strcmp(prtkimg->fw[FW_KERNEL_ROOTFS].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_KERNEL_ROOTFS].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_KERNEL_ROOTFS].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_KERNEL_ROOTFS].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_KERNEL_ROOTFS].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_KERNEL_ROOTFS].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_KERNEL_ROOTFS].sha_hash, SHA256_SIZE);
   }

   // audio fw_desc
   if(prtkimg->fw[FW_AUDIO].img_size != 0)
   {
   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_AUDIO;
   if (!strcmp(prtkimg->fw[FW_AUDIO].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_AUDIO].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_AUDIO].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_AUDIO].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_AUDIO].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_AUDIO].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_AUDIO].sha_hash, SHA256_SIZE);
   }
#ifdef TEE_ENABLE
   if(prtkimg->fw[FW_TEE].img_size != 0)
   {
   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_TEE;
   if (!strcmp(prtkimg->fw[FW_TEE].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_TEE].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_TEE].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_TEE].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_TEE].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_TEE].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_TEE].sha_hash, SHA256_SIZE);
   }
   if(prtkimg->fw[FW_BL31].img_size != 0)
   {
   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_BL31;
   if (!strcmp(prtkimg->fw[FW_BL31].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_BL31].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_BL31].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_BL31].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_BL31].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_BL31].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_BL31].sha_hash, SHA256_SIZE);
   }
#endif
#ifdef NAS_ENABLE
#ifdef HYPERVISOR
   if(prtkimg->fw[FW_XEN].img_size != 0)
   {
   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_XEN;
   if (!strcmp(prtkimg->fw[FW_XEN].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_XEN].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_XEN].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_XEN].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_XEN].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_XEN].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_XEN].sha_hash, SHA256_SIZE);
   }
#endif
#endif

   #if 0
   // video fw_desc
   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_VIDEO;
   if (!strcmp(prtkimg->fw[FW_VIDEO].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_VIDEO].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_VIDEO].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_VIDEO].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_VIDEO].flash_allo_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_VIDEO].checksum;
   #endif

   // audio bootfile fw_desc
   if(prtkimg->fw[FW_AUDIO_BOOTFILE].img_size != 0)
   {
      fw_count++;
      rtk_fw_head.fw_count++;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_AUDIO_FILE;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_AUDIO_BOOTFILE].mem_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_AUDIO_BOOTFILE].flash_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_AUDIO_BOOTFILE].img_size;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_AUDIO_BOOTFILE].flash_allo_size;//
      //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_AUDIO_BOOTFILE].checksum;
      memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_AUDIO_BOOTFILE].sha_hash, SHA256_SIZE);
   }

   if(prtkimg->fw[FW_IMAGE_BOOTFILE].img_size != 0)
   {
      fw_count++;
      rtk_fw_head.fw_count++;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_IMAGE_FILE;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_IMAGE_BOOTFILE].mem_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_IMAGE_BOOTFILE].flash_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_IMAGE_BOOTFILE].img_size;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_IMAGE_BOOTFILE].flash_allo_size;//
      //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_IMAGE_BOOTFILE].checksum;
      memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_IMAGE_BOOTFILE].sha_hash, SHA256_SIZE);
   }

   // video bootfile fw_desc
   if(prtkimg->fw[FW_VIDEO_BOOTFILE].img_size != 0)
   {
      fw_count++;
      rtk_fw_head.fw_count++;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_VIDEO_FILE;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_VIDEO_BOOTFILE].mem_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_VIDEO_BOOTFILE].flash_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_VIDEO_BOOTFILE].img_size;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_VIDEO_BOOTFILE].flash_allo_size;//
      //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_VIDEO_BOOTFILE].checksum;
      memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_VIDEO_BOOTFILE].sha_hash, SHA256_SIZE);
   }

   // audio customer logo1 fw_desc
   if(prtkimg->fw[FW_AUDIO_CLOGO1].img_size != 0)
   {
      fw_count++;
      rtk_fw_head.fw_count++;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_AUDIO_FILE1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_AUDIO_CLOGO1].mem_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_AUDIO_CLOGO1].flash_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_AUDIO_CLOGO1].img_size;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_AUDIO_CLOGO1].flash_allo_size;//
      //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum= prtkimg->fw[FW_AUDIO_CLOGO1].checksum;
      memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_AUDIO_CLOGO1].sha_hash, SHA256_SIZE);
   }

   if(prtkimg->fw[FW_IMAGE_CLOGO1].img_size != 0)
   {
      fw_count++;
      rtk_fw_head.fw_count++;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_IMAGE_FILE1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_IMAGE_CLOGO1].mem_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_IMAGE_CLOGO1].flash_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_IMAGE_CLOGO1].img_size;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_IMAGE_CLOGO1].flash_allo_size;//
      //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_IMAGE_CLOGO1].checksum;
      memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_IMAGE_CLOGO1].sha_hash, SHA256_SIZE);
   }

   // video customer logo1 fw_desc
   if(prtkimg->fw[FW_VIDEO_CLOGO1].img_size != 0)
   {
      fw_count++;
      rtk_fw_head.fw_count++;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_VIDEO_FILE1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_VIDEO_CLOGO1].mem_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_VIDEO_CLOGO1].flash_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_VIDEO_CLOGO1].img_size;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_VIDEO_CLOGO1].flash_allo_size;//
      //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_VIDEO_CLOGO1].checksum;
      memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_VIDEO_CLOGO1].sha_hash, SHA256_SIZE);
   }

   // audio customer logo2 fw_desc
   if(prtkimg->fw[FW_AUDIO_CLOGO2].img_size != 0)
   {
      fw_count++;
      rtk_fw_head.fw_count++;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_AUDIO_FILE2;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_AUDIO_CLOGO2].mem_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_AUDIO_CLOGO2].flash_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_AUDIO_CLOGO2].img_size;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_AUDIO_CLOGO2].flash_allo_size;//
      //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_AUDIO_CLOGO2].checksum;
      memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_AUDIO_CLOGO2].sha_hash, SHA256_SIZE);
   }

   if(prtkimg->fw[FW_IMAGE_CLOGO2].img_size != 0)
   {
      fw_count++;
      rtk_fw_head.fw_count++;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_IMAGE_FILE2;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_IMAGE_CLOGO2].mem_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_IMAGE_CLOGO2].flash_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_IMAGE_CLOGO2].img_size;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_IMAGE_CLOGO2].flash_allo_size;//
      //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_IMAGE_CLOGO2].checksum;
      memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_IMAGE_CLOGO2].sha_hash, SHA256_SIZE);
   }

   // video customer logo2 fw_desc
   if(prtkimg->fw[FW_VIDEO_CLOGO2].img_size != 0)
   {
      fw_count++;
      rtk_fw_head.fw_count++;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_VIDEO_FILE2;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_VIDEO_CLOGO2].mem_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_VIDEO_CLOGO2].flash_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_VIDEO_CLOGO2].img_size;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_VIDEO_CLOGO2].flash_allo_size;//
      //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_VIDEO_CLOGO2].checksum;
      memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_VIDEO_CLOGO2].sha_hash, SHA256_SIZE);
   }

   rtk_fw_head.fw_count++;

   // build partition table entry from low address to high address.
   // firmware partition
   rtk_fw_head.part_count = 0;
   rtk_fw_head.part_desc[rtk_fw_head.part_count].type = PART_TYPE_FW;
   rtk_fw_head.part_desc[rtk_fw_head.part_count].ro = 1;
   rtk_fw_head.part_desc[rtk_fw_head.part_count].length = prtkimg->fw[FW_P_FREE_SPACE].flash_offset + prtkimg->fw[FW_P_FREE_SPACE].flash_allo_size;
   rtk_fw_head.part_desc[rtk_fw_head.part_count].fw_count = fw_count;
   rtk_fw_head.part_desc[rtk_fw_head.part_count].fw_type = 0x0;
   //rtk_fw_head.part_desc[0].mount_point

   // subsequent partition
   for(int i=0; i<NUM_RTKPART; i++)
   {
      if( ! rtk_part_list_sort[i] )
         break;

      struct t_PARTDESC* rtk_part = rtk_part_list_sort[i];
      efwtype = rtk_part->efwtype;

      if(rtk_part->efs == FS_TYPE_NONE) continue;

      rtk_fw_head.part_count++;
      install_debug("part_entry add partition_name:%s\r\n", rtk_part->partition_name);

      rtk_fw_head.part_desc[rtk_fw_head.part_count].ro = 0;
      rtk_fw_head.part_desc[rtk_fw_head.part_count].fw_type = rtk_part->efs;
      rtk_fw_head.part_desc[rtk_fw_head.part_count].type = PART_TYPE_FS;
      rtk_fw_head.part_desc[rtk_fw_head.part_count].length = prtkimg->fw[efwtype].flash_allo_size;
      rtk_fw_head.part_desc[rtk_fw_head.part_count].fw_count = 1;
      snprintf((char*)rtk_fw_head.part_desc[rtk_fw_head.part_count].mount_point
                , sizeof(rtk_fw_head.part_desc[rtk_fw_head.part_count].mount_point)
                , "%s"
                , prtkimg->fw[efwtype].mount_point);
   }

   #ifdef RESERVED_AREA
   // reserved_remapping part
   rtk_fw_head.part_count++;
   rtk_fw_head.part_desc[rtk_fw_head.part_count].type = PART_TYPE_RESERVED;
   rtk_fw_head.part_desc[rtk_fw_head.part_count].ro = 1;
   rtk_fw_head.part_desc[rtk_fw_head.part_count].length = prtkimg->reserved_remapping_size;//
   rtk_fw_head.part_desc[rtk_fw_head.part_count].fw_count = 1;
   rtk_fw_head.part_desc[rtk_fw_head.part_count].fw_type = 0x0;
   //rtk_fw_head.part_desc[rtk_fw_head.part_count].mount_point
   #endif

   rtk_fw_head.part_count++;
   install_debug("rtk_fw_head.part_count:%d\r\n", rtk_fw_head.part_count);

   memcpy(rtk_fw_head.fw_tab.signature, VERONA_SIGNATURE, 8);
   //rtk_fw_head.fw_tab.checksum
   if(prtkimg->secure_boot == 0) {
      rtk_fw_head.fw_tab.version = FIRMWARE_DESCRIPTION_TABLE_VERSION_02; //non-secure boot
      rtk_fw_head.secure_boot = 0;
   }
   else {
      rtk_fw_head.fw_tab.version = FIRMWARE_DESCRIPTION_TABLE_VERSION_22; //secure boot
      rtk_fw_head.secure_boot = 1;
   }
   rtk_fw_head.fw_tab.paddings = prtkimg->mtd_erasesize;

   // save
   ret = save_fwdesc(&rtk_fw_head, prtkimg);
   if(ret < 0)
   {
      install_debug("save_fwdesc() fail\r\n");
      return -1;
   }

#ifdef NAS_ENABLE
    if ( 1 == prtkimg->nas_rescue ){
        memcpy(&rtk_fw_nas_rescue, &rtk_fw_head, sizeof(rtk_fw_nas_rescue));
        ret = nas_rescue_fwdesc(prtkimg, &rtk_fw_nas_rescue);
        if(ret < 0){
            install_debug("save NAS rescue fwdesc failed!\r\n");
            return -1;
        }
    }
#endif

#ifdef __OFFLINE_GENERATE_BIN__
   char path[128] = {0};
   unsigned int file_len = 0;

   snprintf(path, sizeof(path), "%s/%s", PKG_TEMP, FWDESCTABLE);

   if((ret = rtk_get_size_of_file(path, &file_len)) < 0)
   {
      install_debug("file not found\r\n");
      return -1;
   }

   rtk_file_to_virt_nand_with_ecc(path
                                 , 0
                                 , file_len
                                 , prtkimg
                                 , prtkimg->reserved_boot_size
                                 , RTK_NAND_BLOCK_STATE_GOOD_BLOCK
                                 , NULL);
#endif

   return 0;
}

#ifdef EMMC_SUPPORT
static int burn_fwdesc_emmc(struct t_rtkimgdesc* prtkimg)
{
   int ret=0;
   u8 fw_count;
   struct rtk_fw_header rtk_fw_head;
   FWTYPE efwtype;
   struct t_PARTDESC* rtk_part;

   memset(&rtk_fw_head, 0, sizeof(rtk_fw_head));

   // sanity-check
   if(prtkimg->fw[FW_KERNEL].img_size == 0 || prtkimg->fw[FW_RESCUE_DT].img_size == 0 \
      || prtkimg->fw[FW_KERNEL_DT].img_size == 0
#ifndef NAS_ENABLE
      || prtkimg->fw[FW_AUDIO].img_size == 0
#endif
      )
   {
      install_debug("FW_KERNEL(%u), FW_AUDIO(%u), FW_RESCUE_DT(%u), FW_KERNEL_DT(%u), FW_ETC(%u) zero check.\r\n"  \
         , prtkimg->fw[FW_KERNEL].img_size   \
         , prtkimg->fw[FW_AUDIO].img_size \
         , prtkimg->fw[FW_RESCUE_DT].img_size
         , prtkimg->fw[FW_KERNEL_DT].img_size   \
         , prtkimg->fw[FW_USR_LOCAL_ETC].img_size);
      return -1;
   }

   // fw field
#ifdef NFLASH_LAOUT
   fw_count = 1;
   rtk_fw_head.fw_count = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_GOLD_KERNEL;
   if (!strcmp(prtkimg->fw[FW_GOLD_KERNEL].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_GOLD_KERNEL].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_GOLD_KERNEL].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_GOLD_KERNEL].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_GOLD_KERNEL].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_GOLD_KERNEL].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_GOLD_KERNEL].sha_hash, SHA256_SIZE);

   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_GOLD_RESCUE_DT;
   if (!strcmp(prtkimg->fw[FW_GOLD_RESCUE_DT].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_GOLD_RESCUE_DT].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_GOLD_RESCUE_DT].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_GOLD_RESCUE_DT].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_GOLD_RESCUE_DT].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_GOLD_RESCUE_DT].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_GOLD_RESCUE_DT].sha_hash, SHA256_SIZE);

   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_GOLD_RESCUE_ROOTFS;
   if (!strcmp(prtkimg->fw[FW_GOLD_RESCUE_ROOTFS].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_GOLD_RESCUE_ROOTFS].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_GOLD_RESCUE_ROOTFS].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_GOLD_RESCUE_ROOTFS].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_GOLD_RESCUE_ROOTFS].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_GOLD_RESCUE_ROOTFS].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_GOLD_RESCUE_ROOTFS].sha_hash, SHA256_SIZE);

   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_GOLD_AUDIO;
   if (!strcmp(prtkimg->fw[FW_GOLD_AUDIO].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_GOLD_AUDIO].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_GOLD_AUDIO].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_GOLD_AUDIO].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_GOLD_AUDIO].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_GOLD_AUDIO].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_GOLD_AUDIO].sha_hash, SHA256_SIZE);

   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_UBOOT;
   if (!strcmp(prtkimg->fw[FW_UBOOT].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_UBOOT].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_UBOOT].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_UBOOT].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_UBOOT].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_UBOOT].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_UBOOT].sha_hash, SHA256_SIZE);

   fw_count++;
   rtk_fw_head.fw_count++;
#else
   // kernel fw_desc
   fw_count = 1;
   rtk_fw_head.fw_count = 0;
#endif   
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_KERNEL;
   if (!strcmp(prtkimg->fw[FW_KERNEL].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_KERNEL].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_KERNEL].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_KERNEL].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_KERNEL].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_KERNEL].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_KERNEL].sha_hash, SHA256_SIZE);

   // Rescue_DT fw_desc
   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_RESCUE_DT;
   if (!strcmp(prtkimg->fw[FW_RESCUE_DT].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_RESCUE_DT].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_RESCUE_DT].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_RESCUE_DT].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_RESCUE_DT].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_RESCUE_DT].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_RESCUE_DT].sha_hash, SHA256_SIZE);

   // Kernel_DT fw_desc
   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_KERNEL_DT;
   if (!strcmp(prtkimg->fw[FW_KERNEL_DT].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_KERNEL_DT].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_KERNEL_DT].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_KERNEL_DT].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_KERNEL_DT].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_KERNEL_DT].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_KERNEL_DT].sha_hash, SHA256_SIZE);

   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_RESCUE_ROOTFS;
   if (!strcmp(prtkimg->fw[FW_RESCUE_ROOTFS].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_RESCUE_ROOTFS].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_RESCUE_ROOTFS].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_RESCUE_ROOTFS].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_RESCUE_ROOTFS].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_RESCUE_ROOTFS].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_RESCUE_ROOTFS].sha_hash, SHA256_SIZE);

   if(prtkimg->fw[FW_KERNEL_ROOTFS].img_size != 0)
   {
   fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_KERNEL_ROOTFS;
   if (!strcmp(prtkimg->fw[FW_KERNEL_ROOTFS].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_KERNEL_ROOTFS].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_KERNEL_ROOTFS].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_KERNEL_ROOTFS].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_KERNEL_ROOTFS].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_KERNEL_ROOTFS].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_KERNEL_ROOTFS].sha_hash, SHA256_SIZE);
   }

   // audio fw_desc
   if(prtkimg->fw[FW_AUDIO].img_size != 0)
   {
	fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_AUDIO;
   if (!strcmp(prtkimg->fw[FW_AUDIO].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_AUDIO].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_AUDIO].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_AUDIO].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_AUDIO].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_AUDIO].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_AUDIO].sha_hash, SHA256_SIZE);
   }
#ifdef TEE_ENABLE
   if(prtkimg->fw[FW_TEE].img_size != 0)
   {
	fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_TEE;
   if (!strcmp(prtkimg->fw[FW_TEE].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_TEE].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_TEE].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_TEE].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_TEE].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_TEE].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_TEE].sha_hash, SHA256_SIZE);
   }
   if(prtkimg->fw[FW_BL31].img_size != 0)
   {
	fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_BL31;
   if (!strcmp(prtkimg->fw[FW_BL31].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_BL31].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_BL31].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_BL31].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_BL31].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_BL31].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_BL31].sha_hash, SHA256_SIZE);
   }
#endif
#ifdef NAS_ENABLE
#ifdef HYPERVISOR
   if(prtkimg->fw[FW_XEN].img_size != 0)
   {
	fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_XEN;
   if (!strcmp(prtkimg->fw[FW_XEN].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_XEN].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_XEN].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_XEN].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_XEN].flash_allo_size;//
   //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_XEN].checksum;
   memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_XEN].sha_hash, SHA256_SIZE);
   }
#endif
#endif

#if 0
   // video fw_desc
	fw_count++;
   rtk_fw_head.fw_count++;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_VIDEO;
   if (!strcmp(prtkimg->fw[FW_VIDEO].compress_type, "lzma"))
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_VIDEO].mem_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_VIDEO].flash_offset;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_VIDEO].img_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_VIDEO].flash_allo_size;//
   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_VIDEO].checksum;

	// video2 fw_desc
	if(prtkimg->fw[FW_VIDEO2].img_size != 0)
	{
		fw_count++;
   		rtk_fw_head.fw_count++;
   		rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_VIDEO2;
		if (!strcmp(prtkimg->fw[FW_VIDEO2].compress_type, "lzma"))
      	   rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
   		rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
   		rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
   		rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_VIDEO2].mem_offset;//
   		rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_VIDEO2].flash_offset;//
   		rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_VIDEO2].img_size;//
   		rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_VIDEO2].flash_allo_size;//
   		rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_VIDEO2].checksum;
	}

    // ecpu fw_desc
    if(prtkimg->fw[FW_ECPU].img_size != 0)
    {
        fw_count++;
        rtk_fw_head.fw_count++;
        rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_ECPU;
		if (!strcmp(prtkimg->fw[FW_ECPU].compress_type, "lzma"))
      		rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.lzma = 1;
        rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
        rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
        rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_ECPU].mem_offset;//
        rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_ECPU].flash_offset;//
        rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_ECPU].img_size;//
        rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_ECPU].flash_allo_size;//
        rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_ECPU].checksum;
    }
#endif
   // audio bootfile fw_desc
   if(prtkimg->fw[FW_AUDIO_BOOTFILE].img_size != 0)
   {
      fw_count++;
      rtk_fw_head.fw_count++;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_AUDIO_FILE;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_AUDIO_BOOTFILE].mem_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_AUDIO_BOOTFILE].flash_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_AUDIO_BOOTFILE].img_size;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_AUDIO_BOOTFILE].flash_allo_size;//
      //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_AUDIO_BOOTFILE].checksum;
      memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_AUDIO_BOOTFILE].sha_hash, SHA256_SIZE);
   }

   if(prtkimg->fw[FW_IMAGE_BOOTFILE].img_size != 0)
   {
      fw_count++;
      rtk_fw_head.fw_count++;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_IMAGE_FILE;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_IMAGE_BOOTFILE].mem_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_IMAGE_BOOTFILE].flash_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_IMAGE_BOOTFILE].img_size;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_IMAGE_BOOTFILE].flash_allo_size;//
      //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_IMAGE_BOOTFILE].checksum;
      memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_IMAGE_BOOTFILE].sha_hash, SHA256_SIZE);
   }

   // video bootfile fw_desc
   if(prtkimg->fw[FW_VIDEO_BOOTFILE].img_size != 0)
   {
      fw_count++;
      rtk_fw_head.fw_count++;
		rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_VIDEO_FILE;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_VIDEO_BOOTFILE].mem_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_VIDEO_BOOTFILE].flash_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_VIDEO_BOOTFILE].img_size;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_VIDEO_BOOTFILE].flash_allo_size;//
      //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_VIDEO_BOOTFILE].checksum;
      memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_VIDEO_BOOTFILE].sha_hash, SHA256_SIZE);
   }

   // audio customer logo1 fw_desc
   if(prtkimg->fw[FW_AUDIO_CLOGO1].img_size != 0)
   {
      fw_count++;
      rtk_fw_head.fw_count++;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_AUDIO_FILE1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_AUDIO_CLOGO1].mem_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_AUDIO_CLOGO1].flash_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_AUDIO_CLOGO1].img_size;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_AUDIO_CLOGO1].flash_allo_size;//
      //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_AUDIO_CLOGO1].checksum;
      memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_AUDIO_CLOGO1].sha_hash, SHA256_SIZE);
   }

   if(prtkimg->fw[FW_IMAGE_CLOGO1].img_size != 0)
   {
      fw_count++;
      rtk_fw_head.fw_count++;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_IMAGE_FILE1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_IMAGE_CLOGO1].mem_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_IMAGE_CLOGO1].flash_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_IMAGE_CLOGO1].img_size;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_IMAGE_CLOGO1].flash_allo_size;//
      //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_IMAGE_CLOGO1].checksum;
      memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_IMAGE_CLOGO1].sha_hash, SHA256_SIZE);
   }

   // video customer logo1 fw_desc
   if(prtkimg->fw[FW_VIDEO_CLOGO1].img_size != 0)
   {
      fw_count++;
      rtk_fw_head.fw_count++;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_VIDEO_FILE1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_VIDEO_CLOGO1].flash_offset;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_VIDEO_CLOGO1].img_size;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_VIDEO_CLOGO1].flash_allo_size;
      //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_VIDEO_CLOGO1].checksum;
      memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_VIDEO_CLOGO1].sha_hash, SHA256_SIZE);
   }

   // audio customer logo2 fw_desc
   if(prtkimg->fw[FW_AUDIO_CLOGO2].img_size != 0)
   {
      fw_count++;
      rtk_fw_head.fw_count++;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_AUDIO_FILE2;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_AUDIO_CLOGO2].mem_offset;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_AUDIO_CLOGO2].flash_offset;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_AUDIO_CLOGO2].img_size;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_AUDIO_CLOGO2].flash_allo_size;
      //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_AUDIO_CLOGO2].checksum;
      memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_AUDIO_CLOGO2].sha_hash, SHA256_SIZE);
   }

   if(prtkimg->fw[FW_IMAGE_CLOGO2].img_size != 0)
   {
      fw_count++;
      rtk_fw_head.fw_count++;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_IMAGE_FILE2;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_IMAGE_CLOGO2].mem_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_IMAGE_CLOGO2].flash_offset;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_IMAGE_CLOGO2].img_size;//
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_IMAGE_CLOGO2].flash_allo_size;//
      //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_IMAGE_CLOGO2].checksum;
      memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_IMAGE_CLOGO2].sha_hash, SHA256_SIZE);
   }

   // video customer logo2 fw_desc
   if(prtkimg->fw[FW_VIDEO_CLOGO2].img_size != 0)
   {
      fw_count++;
      rtk_fw_head.fw_count++;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.type = FW_TYPE_VIDEO_FILE2;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.ro = 1;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.version = 0;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.target_addr = prtkimg->fw[FW_VIDEO_CLOGO2].mem_offset;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.offset = prtkimg->fw[FW_VIDEO_CLOGO2].flash_offset;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.length = prtkimg->fw[FW_VIDEO_CLOGO2].img_size;
      rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.paddings = prtkimg->fw[FW_VIDEO_CLOGO2].flash_allo_size;
      //rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.checksum = prtkimg->fw[FW_VIDEO_CLOGO2].checksum;
      memcpy(rtk_fw_head.fw_desc[rtk_fw_head.fw_count].v2.sha_hash,  prtkimg->fw[FW_VIDEO_CLOGO2].sha_hash, SHA256_SIZE);
   }

   // fw_count
   rtk_fw_head.fw_count++;

   // build partition table entry from low address to high address.
   // firmware partition
   rtk_fw_head.part_count = 0;
   rtk_fw_head.part_desc[rtk_fw_head.part_count].type = PART_TYPE_FW;
   rtk_fw_head.part_desc[rtk_fw_head.part_count].ro = 1;
   rtk_fw_head.part_desc[rtk_fw_head.part_count].length = prtkimg->fw[FW_P_FREE_SPACE].flash_offset + prtkimg->fw[FW_P_FREE_SPACE].flash_allo_size;
   rtk_fw_head.part_desc[rtk_fw_head.part_count].fw_count = fw_count;
   rtk_fw_head.part_desc[rtk_fw_head.part_count].fw_type = 0x0;

   // subsequent partition
   for(int i=0; i<NUM_RTKPART; i++)
   {
      if( ! rtk_part_list_sort[i] )
         break;

      rtk_part = rtk_part_list_sort[i];
      efwtype = rtk_part->efwtype;

      if(rtk_part->efs == FS_TYPE_NONE) continue;

      rtk_fw_head.part_count++;
      install_debug("part_entry add partition_name:%s (@p%d)\r\n", rtk_part->partition_name, prtkimg->fw[efwtype].emmc_partIdx);

      rtk_fw_head.part_desc[rtk_fw_head.part_count].ro = 0;
      rtk_fw_head.part_desc[rtk_fw_head.part_count].fw_type = rtk_part->efs;
      rtk_fw_head.part_desc[rtk_fw_head.part_count].type = PART_TYPE_FS;
      rtk_fw_head.part_desc[rtk_fw_head.part_count].length = prtkimg->fw[efwtype].flash_allo_size;
      rtk_fw_head.part_desc[rtk_fw_head.part_count].fw_count = 1;
      rtk_fw_head.part_desc[rtk_fw_head.part_count].emmc_partIdx = prtkimg->fw[efwtype].emmc_partIdx;
      snprintf((char*)rtk_fw_head.part_desc[rtk_fw_head.part_count].mount_point
              , sizeof(rtk_fw_head.part_desc[rtk_fw_head.part_count].mount_point)
              , "%s"
              , prtkimg->fw[efwtype].mount_point);
   }
   rtk_fw_head.part_count++;
   install_debug("rtk_fw_head.part_count:%d\r\n", rtk_fw_head.part_count);

   memcpy(rtk_fw_head.fw_tab.signature, VERONA_SIGNATURE, 8);
   if(prtkimg->secure_boot == 0) {
      rtk_fw_head.fw_tab.version = FIRMWARE_DESCRIPTION_TABLE_VERSION_02; //non-secure boot
      rtk_fw_head.secure_boot = 0;
   }
   else {
      rtk_fw_head.fw_tab.version = FIRMWARE_DESCRIPTION_TABLE_VERSION_22; //secure boot
      rtk_fw_head.secure_boot = 1;
   }

   // save
   ret = save_fwdesc_emmc(&rtk_fw_head, prtkimg);
   if(ret < 0)
   {
      install_debug("save_fwdesc() fail\r\n");
      return -1;
   }

   return 0;

}
#endif

int rtk_burn_fwdesc(struct t_rtkimgdesc* prtkimgdesc,S_BOOTTABLE* pboottable)
{
   int ret = 0;
   char* block_path = NULL;
   char command[128] = {0};

   if(prtkimgdesc->flash_type == MTD_NANDFLASH)
   {
      ret = burn_fwdesc_nand(prtkimgdesc);
      if( (ret==0) && pboottable ) {
        update_fw(pboottable, FWTYPE_FWTBL, &prtkimgdesc->fw[FW_FW_TBL]);
      }
   }
#ifdef EMMC_SUPPORT
   else if ((prtkimgdesc->flash_type == MTD_EMMC) || (prtkimgdesc->flash_type == MTD_SATA))
   {
      ret = burn_fwdesc_emmc(prtkimgdesc);
      if( (ret==0) && pboottable ) {
        update_fw(pboottable, FWTYPE_FWTBL, &prtkimgdesc->fw[FW_FW_TBL]);
      }
   }
#endif
#ifdef CONFIG_BOOT_FROM_SPI
   else if(prtkimgdesc->flash_type == MTD_DATAFLASH)
   {
      ret = burn_fwdesc_nor(prtkimgdesc);
      if( (ret==0) && pboottable ) {
        update_fw(pboottable, FWTYPE_FWTBL, &prtkimgdesc->fw[FW_FW_TBL]);
      }
   }
#endif
   else if(prtkimgdesc->flash_type == MTD_NORFLASH || prtkimgdesc->flash_type == MTD_DATAFLASH)
   {
      ret = burn_fwdesc_nor(prtkimgdesc);
      ret = get_mtd_block_name(&block_path);
      if(ret < 0)
      {
         install_debug("get_mtd_block_name fail\r\n");
         return -1;
      }
      // modify the signature (first 8 bytes of boot table)
      sprintf(command, "echo -n SCIT____ | dd of=%s bs=1 seek=%llu", block_path, ((prtkimgdesc->flash_size) - 0x100000 - 0x100));
      ret = rtk_command(command, __LINE__, __FILE__);
      if(ret < 0)
      {
         install_debug("modify SCIT____ fail\r\n");
         return -1;
      }
   }
   else
   {
      install_debug("Unknown MTD TYPE\r\n");
      ret = -1;
   }
   return ret;
}
#endif

int rtk_install_factory(struct t_rtkimgdesc* prtkimgdesc, bool bFlush)
{
   int ret = 0;	// i, file_missing;
   char cmd[128] = {0}, path[128] = {0};
   char workpath[128] = {0};

   install_info("\r\n[Install factory]\r\n");

	// if we only update bootcode, no need to untar factory.tar
	if (prtkimgdesc->only_bootcode != 1) {
   	   //extract factory from install package
       snprintf(path, sizeof(path), "%s.tar", FACTORY_INSTALL_TEMP);
       if (rtk_extract_file(prtkimgdesc, &prtkimgdesc->factory_tar, path) < 0) {
          install_debug("Can't extract %s\r\n", prtkimgdesc->factory_tar.filename);
       }
       else {
          snprintf(cmd, sizeof(cmd), "rm -rf %s;mkdir -p %s;tar -xf %s -C %s/"
             , FACTORY_INSTALL_TEMP
             , FACTORY_INSTALL_TEMP
             , path
             , FACTORY_INSTALL_TEMP);

          if((ret = rtk_command(cmd, __LINE__, __FILE__, 0)) < 0)
          {
             install_debug("untar %s.tar fail\r\n", FACTORY_INSTALL_TEMP);
             return -1;
          }
       }
	}

   //load factory from flash
   if ((ret = factory_load(NULL, prtkimgdesc)) < 0) {
      install_debug("factory_load fail\r\n");
#ifndef __OFFLINE_GENERATE_BIN__
      //return -1;
#endif
   }

   if (!get_factory_tmp_dir()){
      install_debug("can't get factory path\r\n");
      return -1;
   }

   getcwd(workpath, 128);
   chdir(get_factory_tmp_dir());

	// if we only update bootcode, no need to untar factory.tar
   if (prtkimgdesc->only_bootcode != 1) {
      if(prtkimgdesc->kill_000 == 1) {
         install_log("\r\n[kill %s]\r\n", BOOTPARAM_FILENAME);
         snprintf(cmd, sizeof(cmd), "rm -f %s", BOOTPARAM_FILENAME);
         if((ret = rtk_command(cmd, __LINE__, __FILE__, 1)) < 0)
         {
            install_debug("kill %s fail!\r\n", BOOTPARAM_FILENAME);
         }
      }

      if ((prtkimgdesc->install_factory == 1) || (prtkimgdesc->only_factory == 1)) {
		 //copy file from "package factory dir" to "flash load factory dir"
         install_log("\r\n[ifcmd0]\r\n");
         if((ret = rtk_command(prtkimgdesc->ifcmd0, __LINE__, __FILE__, 1)) < 0) {
            install_debug("install factory command0 fail!\r\n");
         }
         install_log("\r\n[ifcmd1]\r\n");
         if((ret = rtk_command(prtkimgdesc->ifcmd1, __LINE__, __FILE__, 1)) < 0) {
            install_debug("install factory command1 fail!\r\n");
         }
         install_log("\r\n");
      }
   }

   //sanity-check
#if (0)	// Chuck TODO
   file_missing = 0;
   for (i = 0; i < (int)(sizeof(essential_files_in_factory)/sizeof(essential_files_in_factory[0])); i++) {
      if (!access(essential_files_in_factory[i], F_OK)) {
         install_debug("file:\"%s\" checked!\r\n", essential_files_in_factory[i]);
         continue;
      } else {
         install_fail("file:\"%s\" missing in factory!\r\n", essential_files_in_factory[i]);
         file_missing = 1;
      }
   }
   if (file_missing) {
      install_fail("factory not saved!!!\r\n");
      return -1;
   }
#endif

   chdir(workpath);

   //copy the layout.txt_bak back to factory
   if (prtkimgdesc->safe_upgrade == 1)
   {
      snprintf(cmd, sizeof(cmd), "cp %s/%s_bak %s/"
                              , PKG_TEMP
                              , LAYOUT_FILENAME
                              , get_factory_tmp_dir());
      ret = rtk_command(cmd, __LINE__, __FILE__, 0);
   }

   //flush factory
   if((ret = factory_flush(prtkimgdesc->factory_start, prtkimgdesc->factory_size, prtkimgdesc, bFlush)) < 0)
   {
      install_debug("factory_flush fail\r\n");
      return -1;
   }

   return ret;
}

int rtk_flush_pingpong_factory_mac(struct t_rtkimgdesc* prtkimgdesc)
{
   int ret;
   char v_current_pp = -1;

   // load factory from flash
   if ((ret = factory_load(NULL, prtkimgdesc)) < 0) {
      install_debug("factory_load fail\r\n");
#ifndef __OFFLINE_GENERATE_BIN__
      return -1;
#endif
   }

   // save factory to flash twice
   // for updating both ping pong to the lastest version
   if((ret = factory_flush(prtkimgdesc->factory_start, prtkimgdesc->factory_size, prtkimgdesc)) < 0)
   {
      install_debug("factory_flush fail\r\n");
      return -1;
   }
   if((ret = factory_flush(prtkimgdesc->factory_start, prtkimgdesc->factory_size, prtkimgdesc)) < 0)
   {
      install_debug("factory_flush fail\r\n");
      return -1;
   }

   // if current_pp == 1, save one more time let current_pp == 0
   v_current_pp = get_factory_current_pp();
   install_debug("current_pp = %d\r\n", v_current_pp);
   if (v_current_pp == 1)
   {
        install_debug("claire set fsync false this time");
      if((ret = factory_flush(prtkimgdesc->factory_start, prtkimgdesc->factory_size, prtkimgdesc)) < 0)
      {
         install_debug("factory_flush fail\r\n");
         return -1;
      }
   }
   return ret;
}

int rtk_create_remove_file_factory_mac(struct t_rtkimgdesc* prtkimgdesc, const char *filename, int mode)
{
   int ret;
   char cmd[128] = {0}, path[128] = {0};

   // load factory from flash
   if ((ret = factory_load(NULL, prtkimgdesc)) < 0) {
      install_debug("factory_load fail\r\n");
#ifndef __OFFLINE_GENERATE_BIN__
      return -1;
#endif
   }

   if (!get_factory_tmp_dir()){
      install_debug("can't get factory path\r\n");
      return -1;
   }

   switch(mode) {
      case CREATE_FILE_FACTORY_MODE:
         snprintf(path, sizeof(path), "%s/%s", get_factory_tmp_dir(), filename);
         snprintf(cmd, sizeof(cmd), "touch %s", path);
         if((ret = rtk_command(cmd, __LINE__, __FILE__), 0) < 0)
         {
            install_debug("error! create %s fail!\r\n", path);
         }
         install_log("create %s in factory\r\n", filename);
         break;
      case REMOVE_FILE_FACTORY_MODE:
         snprintf(path, sizeof(path), "%s/%s", get_factory_tmp_dir(), filename);
         snprintf(cmd, sizeof(cmd), "rm -f %s", path);
         if((ret = rtk_command(cmd, __LINE__, __FILE__), 0) < 0)
         {
            install_debug("error! remove %s fail!\r\n", path);
         }
         install_log("remove %s in factory\r\n", filename);
         break;
      default:
         install_fail("error! undefine mode = %d\r\n", mode);
         break;
   }

   // save factory to flash
   if((ret = factory_flush(prtkimgdesc->factory_start, prtkimgdesc->factory_size, prtkimgdesc)) < 0)
   {
      install_debug("factory_flush fail\r\n");
      return -1;
   }

   return ret;
}

int rtk_check_update_bootcode(struct t_rtkimgdesc* prtkimgdesc)
{
   int ret;
   char path[128] = {0};

   // load factory from flash
   if ((ret = factory_load(NULL, prtkimgdesc)) < 0) {
      install_debug("factory_load fail\r\n");
#ifndef __OFFLINE_GENERATE_BIN__
      return -1;
#endif
   }

   if (!get_factory_tmp_dir()){
      install_debug("can't get factory path\r\n");
      return -1;
   }

   snprintf(path, sizeof(path), "%s/%s", get_factory_tmp_dir(), UPDATE_BOOTCODE_FILENAME);
   if (access(path, F_OK)) {
      install_debug("update_bootcode not found in factory\r\n");
      return 0;
   }
   install_debug("update_bootcode found in factory\r\n");

   ret = rtk_create_remove_file_factory_mac(prtkimgdesc, UPDATE_BOOTCODE_FILENAME, REMOVE_FILE_FACTORY_MODE);
   if (ret < 0)
      return -1;
   else
      return 1;
}

static int rtk_burn_bootcode_part(struct t_rtkimgdesc* prtkimgdesc, char *path, unsigned int offset, unsigned char spare_id)
{
	unsigned len, need_block_size, num_of_block;
	char cmd[128];

	if( rtk_get_size_of_file(path, &len) < 0)
	{
		install_fail("\"%s\" file not found\r\n", path);
		return -1;
	}
	need_block_size = SIZE_ALIGN_BOUNDARY_MORE(len, prtkimgdesc->mtd_erasesize);
	num_of_block = need_block_size/prtkimgdesc->mtd_erasesize;

	for (int i = 0; i < NAND_BOOT_BACKUP_COUNT; i++)
	{
		install_debug("offset:0x%08x, end:0x%08x, size:0x%08x(%u)\r\n", offset, offset+len-1, len, len);

		snprintf(cmd, sizeof(cmd), "%s %s %u %u"
								 , FLASHERASE_BIN
								 , prtkimgdesc->mtd_path
								 , offset
								 , num_of_block);
		if( rtk_command(cmd, __LINE__, __FILE__) < 0)
		{
			install_debug("Exec command fail\r\n");
			return -1;
		}

		unsigned blk_id = 0;
		snprintf(cmd, sizeof(cmd), "%s -s %u -p %s --rtk_oob=%d --rtk_blk=%u %s"
								, NANDWRITE_BIN
								, offset
								, prtkimgdesc->mtd_path
								, spare_id
								, blk_id
								, path);
		if( rtk_command(cmd, __LINE__, __FILE__) < 0)
		{
			install_debug("Exec command fail\r\n");
			return -1;
		}

		// verify
		if(prtkimgdesc->verify == 1)
		{
			unsigned error, checksum;
			int ret = rtk_file_verify(prtkimgdesc->mtdblock_path
								   , offset
								   , path
								   , 0
								   , len
								   , &error
								   , &checksum );
			if (ret < 0) {
				install_fail("verify error!!!\r\n");
				return -1;
			}
		}

		offset = offset + need_block_size;
	}

	return offset;
}

int rtk_burn_bootcode_nand(struct t_rtkimgdesc* prtkimgdesc)
{
	int offset;
	char buf[128];

	snprintf(buf, sizeof(buf), "%s.tar", BOOTCODE_TEMP);
	if(rtk_extract_file(prtkimgdesc, &prtkimgdesc->bootloader_tar, buf) < 0)
	{
		install_fail("Can't extract bootcode\r\n");
		return -1;
	}

	snprintf(buf, sizeof(buf), "rm -rf %s;mkdir -p %s;tar -xf %s.tar -C %s/", BOOTCODE_TEMP, BOOTCODE_TEMP, BOOTCODE_TEMP, BOOTCODE_TEMP);

	if( rtk_command(buf, __LINE__, __FILE__, 0) < 0)
	{
		install_fail("untar %s.tar fail\r\n", BOOTCODE_TEMP);
		return -1;
	}

	if(prtkimgdesc->eburn == BURN_BY_NANDWRITE)
	{
		// Extract utility
		if(rtk_extract_utility(prtkimgdesc) < 0)
		{
			install_debug("rtk_extract_utility fail\r\n");
			return -1;
		}
	}

	offset = prtkimgdesc->mtd_erasesize*RTK_NAND_HWSETTING_START_BLOCK;

	install_ui("\n[Burn hwsetting]\n");
	snprintf(buf, sizeof(buf), "%s/%s", BOOTCODE_TEMP, HWSETTING_FILENAME);
	if( (offset=rtk_burn_bootcode_part(prtkimgdesc, buf, offset, BLOCK_HWSETTING)) < 0 )
	{
		install_fail("burn hwsetting fail\n");
		return -1;
	}

	install_ui("\n[Burn uboot]\n");
	snprintf(buf, sizeof(buf), "%s/%s", BOOTCODE_TEMP, UBOOT_FILENAME);
	if( (offset=rtk_burn_bootcode_part(prtkimgdesc, buf, offset, BLOCK_BOOTCODE)) < 0 )
	{
		install_fail("burn hwsetting fail\n");
		return -1;
	}

	return 0;
}

int rtk_burn_bootcode_mac_nand(struct t_rtkimgdesc* prtkimgdesc)
{
   int ret = 0;
   char cmd[128] = {0}, path[128] = {0};

   snprintf(path, sizeof(path), "%s.tar", BOOTCODE_TEMP);
   if(rtk_extract_file(prtkimgdesc, &prtkimgdesc->bootloader_tar, path) < 0)
   {
      install_fail("Can't extract bootcode\r\n");
      return -1;
   }

   snprintf(cmd, sizeof(cmd), "rm -rf %s;mkdir -p %s;tar -xf %s.tar -C %s/", BOOTCODE_TEMP, BOOTCODE_TEMP, BOOTCODE_TEMP, BOOTCODE_TEMP);

   if((ret = rtk_command(cmd, __LINE__, __FILE__, 0)) < 0)
   {
      install_fail("untar %s.tar fail\r\n", BOOTCODE_TEMP);
   }
#ifndef __OFFLINE_GENERATE_BIN__
   //ret = boot_main(BOOTCODE_TEMP);
   sync();
#else

   //nand offline generate start
   int i, need_block_size, burn_count;
   char dpath[128] = {0};
   unsigned int len, start, rescue_img_size, hashtarget_size;
   unsigned int reserved_hwsetting_end_block_size, reserved_hashtarget_block_size, reserved_rescue_block_size;
   t_extern_param_nand extern_param_nand = {0};

   //nand hwsetting starts from 3rd block
   start = prtkimgdesc->mtd_erasesize*RTK_NAND_HWSETTING_START_BLOCK;

   //init programmer
   if ((ret = write_programmer_init(prtkimgdesc
      , &reserved_hwsetting_end_block_size
      , &reserved_hashtarget_block_size
      , &reserved_rescue_block_size)) < 0)
   {
      return -1;
   }

   //write start of programmer define file
   if ((ret = write_programmer_def_file_wrapper(prtkimgdesc, eWRITE_PROGRAMMER_HEAD)) < 0)
   {
      return -1;
   }

   //---------[hwsetting section]---------//
   install_ui("\r\n[Burn hwsetting]\r\n");
   snprintf(path, sizeof(path), "%s/%s", BOOTCODE_TEMP, HWSETTING_FILENAME);
   if((ret = rtk_get_size_of_file(path, &len)) < 0)
   {
      install_fail("\"%s\" file not found\r\n", path);
      return -1;
   }

   need_block_size = SIZE_ALIGN_BOUNDARY_MORE(len, prtkimgdesc->mtd_erasesize);
   install_debug("need_block_size=%d, len=%d, mtd_erasesize=%d\r\n", need_block_size, len, prtkimgdesc->mtd_erasesize);


   for (i = 0; i < NAND_BOOT_BACKUP_COUNT; i++) {
      install_debug("start:0x%08x, end:0x%08x, size:0x%08x(%u)\r\n", start, start+len-1, len, len);

      //write hwsetting part to programmer define file
      if ((ret = write_programmer_def_file_wrapper(prtkimgdesc
         , eWRITE_PROGRAMMER_HWSETTING
         , start/prtkimgdesc->mtd_erasesize
         , need_block_size/prtkimgdesc->mtd_erasesize
         , 0)) < 0)
      {
         return -1;
      }
      //write data and ecc to file
      if((ret = rtk_file_to_virt_nand_with_ecc(path, 0, len, prtkimgdesc, start, RTK_NAND_BLOCK_STATE_HW_SETTING, NULL, 1)) < 0)
      {
         install_fail("burn hwsetting fail\r\n");
         return -1;
      }
      start = start + need_block_size;
   }

   start += reserved_hwsetting_end_block_size*prtkimgdesc->mtd_erasesize;

   //---------[boot-rom section]---------//
   install_info("\r\n[Burn bootrom]\r\n");
   snprintf(path, sizeof(path), "%s/%s", BOOTCODE_TEMP, HASHTARGET_FILENAME);
   if((ret = rtk_get_size_of_file(path, &len)) < 0)
   {
      install_fail("\"%s\" file not found!\r\n", path);
      return -1;
   }
   install_debug("len of hash_target.bin [0x%08x]\r\n", len);

   snprintf(dpath, sizeof(dpath), "%s/%s_temp", BOOTCODE_TEMP, HASHTARGET_FILENAME);

   snprintf(cmd, sizeof(cmd), "touch %s", dpath);
   if((ret = rtk_command(cmd, __LINE__, __FILE__, 0)) < 0)
   {
      install_fail("touch %s error!\r\n", dpath);
   }

   ret = rtk_ptr_to_flash(&len, 4, dpath, 0);
   ret = rtk_file_to_flash(path, 0, dpath, 4, len, NULL);

   hashtarget_size = len+4;
   need_block_size = SIZE_ALIGN_BOUNDARY_MORE(hashtarget_size, prtkimgdesc->mtd_erasesize);
   install_debug("need_block_size=%d, len=%d, mtd_erasesize=%d\r\n", need_block_size, len, prtkimgdesc->mtd_erasesize);

   for (i = 0; i < NAND_BOOT_BACKUP_COUNT; i++) {
      install_debug("start:0x%08x, end:0x%08x, size:0x%08x(%u)\r\n", start, start+hashtarget_size-1, hashtarget_size, hashtarget_size);
      //write bootcode part to programmer define file
      if ((ret = write_programmer_def_file_wrapper(prtkimgdesc
         , eWRITE_PROGRAMMER_HASHTARGET
         , start/prtkimgdesc->mtd_erasesize
         , need_block_size/prtkimgdesc->mtd_erasesize
         , reserved_hashtarget_block_size)) < 0)
      {
         return -1;
      }
      //write data and ecc to file
      if((ret = rtk_file_to_virt_nand_with_ecc(dpath, 0, hashtarget_size, prtkimgdesc, start, RTK_NAND_BLOCK_STATE_BOOTCODE, NULL, 2)) < 0)
      {
         install_fail("burn bootrom fail\r\n");
         return -1;
      }
      start = start + need_block_size + reserved_hashtarget_block_size*prtkimgdesc->mtd_erasesize;
   }

   //---------[rescue section]---------//
   install_info("\r\n[Burn rescue]\r\n");
   snprintf(path, sizeof(path), "%s/%s", BOOTCODE_TEMP, NAND_RESCUE_FILENAME);
   if((ret = rtk_get_size_of_file(path, &len)) < 0)
   {
      install_fail("\"%s\" file not found\r\n", path);
      return -1;
   }
   snprintf(dpath, sizeof(dpath), "%s/%s_temp", BOOTCODE_TEMP, NAND_RESCUE_FILENAME);

   snprintf(cmd, sizeof(cmd), "touch %s", dpath);
   if((ret = rtk_command(cmd, __LINE__, __FILE__, 0)) < 0)
   {
      install_fail("touch %s error!\r\n", dpath);
   }

   //---------[extern-param section]---------//
   extern_param_nand.flash_type = FLASH_MAGICNO_NAND;
   extern_param_nand.region = 0;
   extern_param_nand.mac_hi = DEFAULT_MAC_ADDRESS_HI;
   extern_param_nand.mac_lo = DEFAULT_MAC_ADDRESS_LO;

   extern_param_nand.array_img_saddr = (unsigned char *)BOOTCODE_DDR_BASE;
   extern_param_nand.array_img_size = hashtarget_size;

   extern_param_nand.rescue_img_size = extern_param_nand.rescue_img_part0_len = len;
   extern_param_nand.rescue_img_part0_saddr = (unsigned char *)0xa0a003a0;
   extern_param_nand.env_param_saddr = (unsigned char *)0xa0010000;

   ret = rtk_ptr_to_flash(&extern_param_nand, sizeof(extern_param_nand), dpath, 0);
   ret = rtk_file_to_flash(path, 0, dpath, sizeof(extern_param_nand), len, NULL);

   need_block_size = SIZE_ALIGN_BOUNDARY_MORE(len+sizeof(extern_param_nand), prtkimgdesc->mtd_erasesize);
   burn_count = (prtkimgdesc->bootcode_size - start) / need_block_size;
   burn_count = burn_count > NAND_BOOT_BACKUP_COUNT ? NAND_BOOT_BACKUP_COUNT : burn_count;
   install_debug("need_block_size=%d, len=%d, mtd_erasesize=%d, burn_count=%d\r\n", need_block_size, len, prtkimgdesc->mtd_erasesize, burn_count);

   for (i = 0; i < burn_count; i++) {
      install_debug("start:0x%08x, end:0x%08x, size:0x%08x(%u)\r\n", start, start+len+sizeof(extern_param_nand)-1, len+sizeof(extern_param_nand), len+sizeof(extern_param_nand));
      //write rescue part to programmer define file
      if ((ret = write_programmer_def_file_wrapper(prtkimgdesc
         , eWRITE_PROGRAMMER_RESCUE_N_LOGO
         , start/prtkimgdesc->mtd_erasesize
         , need_block_size/prtkimgdesc->mtd_erasesize
         , reserved_rescue_block_size)) < 0)
      {
         return -1;
      }
      //write data and ecc to file
      if((ret = rtk_file_to_virt_nand_with_ecc(dpath, 0, len+sizeof(extern_param_nand), prtkimgdesc, start, RTK_NAND_BLOCK_STATE_RESCUE_N_LOGO, NULL, 2)) < 0)
      {
         install_fail("burn rescue fail\r\n");
         return -1;
      }
      start = start + need_block_size + reserved_rescue_block_size*prtkimgdesc->mtd_erasesize;
   }

   install_ui("\r\n\r\n");

   //write bootcode packed to programmer define file
   if ((ret = write_programmer_def_file_wrapper(prtkimgdesc
      , eWRITE_PROGRAMMER_BOOTCODE_PACKED
      , RTK_NAND_HWSETTING_START_BLOCK
      , start/prtkimgdesc->mtd_erasesize-RTK_NAND_HWSETTING_START_BLOCK
      , prtkimgdesc->bootcode_size/prtkimgdesc->mtd_erasesize-start/prtkimgdesc->mtd_erasesize)) < 0)
   {
      return -1;
   }

   //write AP part to programmer define file
   if ((ret = write_programmer_def_file_wrapper(prtkimgdesc
      , eWRITE_PROGRAMMER_AP
      , prtkimgdesc->bootcode_size/prtkimgdesc->mtd_erasesize
      , (prtkimgdesc->flash_size-prtkimgdesc->bootcode_size)/prtkimgdesc->mtd_erasesize-prtkimgdesc->flash_reserved_area_block_count
      , prtkimgdesc->flash_reserved_area_block_count)) < 0)
   {
      return -1;
   }

   //write end of programmer define file
   if ((ret = write_programmer_def_file_wrapper(prtkimgdesc, eWRITE_PROGRAMMER_TAIL)) < 0) {
      return -1;
   }

   install_ui("\r\n\r\n");

   //sanity-check
   if (start >  prtkimgdesc->bootcode_size) {
      install_fail("bootcode end 0x%x > bootcode size 0x%x", start, prtkimgdesc->bootcode_size);
      return -1;
   }

#endif
   return ret;

}
#ifdef EMMC_SUPPORT
int rtk_burn_bootcode_emmc(struct t_rtkimgdesc* prtkimgdesc)
{
   int ret = 0;
   int i =0;
   unsigned int len, start, error;
   char cmd[128] = {0}, path[128] = {0};
   int ChipVer = 0;
   
   static char BOOTCODE_AREA_FILE_PATH[][50]={
   "/tmp/bootloader/hw_setting.bin",
   "/tmp/bootloader/uboot.bin",
   "/tmp/bootloader/fsbl.bin",
   "/tmp/bootloader/tee.bin",
   "/tmp/bootloader/bl31.bin",
   "/tmp/bootloader/rsa_bin_fw.bin",
   "/tmp/bootloader/rsa_bin_tee.bin"
   };

#ifdef PC_SIMULATE
   return 0;
#endif
   
   if (get_chip_rev_id() >= RTD129x_CHIP_REVISION_B00) {
      ChipVer = 1;
      install_debug("Bootcode(LK).\n");
   }

   int verify_flag = 1;

   snprintf(path, sizeof(path), "%s.tar", BOOTCODE_TEMP);
   if(rtk_extract_file(prtkimgdesc, &prtkimgdesc->bootloader_tar, path) < 0)
   {
      install_fail("Can't extract bootcode\r\n");
      return -1;
   }

   snprintf(cmd, sizeof(cmd), "rm -rf %s;mkdir -p %s;tar -xf %s.tar -C %s/", BOOTCODE_TEMP, BOOTCODE_TEMP, BOOTCODE_TEMP, BOOTCODE_TEMP);

   ret = rtk_command(cmd, __LINE__, __FILE__, 0);
   if(ret < 0)
   {
      install_fail("untar %s.tar fail\r\n", BOOTCODE_TEMP);
      return -1;
   }

   start = prtkimgdesc->bootcode_start;
   install_info("\r\n[Start Burn Bootcode area fw]\r\n");
   for (i=0; i<(int)(sizeof(BOOTCODE_AREA_FILE_PATH)/sizeof(BOOTCODE_AREA_FILE_PATH[0])); i++){
      if ((!prtkimgdesc->secure_boot) && (i >= 5))
        return 0;

      if (i > 0)
         start += SIZE_ALIGN_BOUNDARY_MORE(len, prtkimgdesc->mtd_erasesize);

      memset(path, 0, sizeof(path));
      if (ChipVer) {
         if (i == 1)
            strcpy(path, "/tmp/bootloader/lk.bin");
         else if(i == 3)
            strcpy(path, "/tmp/bootloader/tee_enc.bin");
         else if(i == 4)
            strcpy(path, "/tmp/bootloader/bl31_enc.bin");
         else
            strcpy(path, BOOTCODE_AREA_FILE_PATH[i]);
      }
      else
         strcpy(path, BOOTCODE_AREA_FILE_PATH[i]);
         
      ret = rtk_get_size_of_file(path, &len);
      if(ret < 0)
      {         
         install_fail("\"%s\" file not found\r\n", path);
         return -1;
      }

      install_info("\r\n[Burn fw(%d): %s]\r\n", i, path);
      install_debug("start:0x%08x, size:0x%08x(%u), end:0x%08x\r\n", start, len, len, start+len-1);
      ret = rtk_file_to_flash(path, 0, prtkimgdesc->mtdblock_path, start, len, NULL);
      if(ret < 0)
      {
         install_fail("burn bootcode area fw(%s) fail\r\n", path);
         return -1;
      }

      // verify
      if(verify_flag) {
         ret = rtk_file_verify(prtkimgdesc->mtdblock_path
            , start
            , path
            , 0
            , len
            , &error);

         if (ret < 0) {
            install_fail("verify error!!!\r\n");
            return -1;
         }
      }
   }

   return 0;
}
#endif

int rtk_burn_bootcode_mac_spi(struct t_rtkimgdesc* prtkimgdesc)
{
   int ret;
   unsigned long long start;
   unsigned int len, rescue_img_size, erase_start, erase_length, error;
   char cmd[128] = {0}, path[128] = {0}, rescue_file_name[128] = {0};
   S_BOOTTABLE boottable, *pbt = NULL;
   t_extern_param extern_param = {0};

   int verify_flag = 1;

   snprintf(path, sizeof(path), "%s.tar", BOOTCODE_TEMP);
   if(rtk_extract_file(prtkimgdesc, &prtkimgdesc->bootloader_tar, path) < 0)
   {
      install_fail("Can't extract bootcode\r\n");
      return -1;
   }

   snprintf(cmd, sizeof(cmd), "rm -rf %s;mkdir -p %s;tar -xf %s.tar -C %s/", BOOTCODE_TEMP, BOOTCODE_TEMP, BOOTCODE_TEMP, BOOTCODE_TEMP);

   ret = rtk_command(cmd, __LINE__, __FILE__, 0);
   if(ret < 0)
   {
      install_fail("untar %s.tar fail\r\n", BOOTCODE_TEMP);
      return -1;
   }

   if ((ret = rtk_find_file_in_dir(BOOTCODE_TEMP, FIND_RESCUE_FILENAME, rescue_file_name, sizeof(rescue_file_name))) == 0) {
      snprintf(path, sizeof(path), "%s/%s", BOOTCODE_TEMP, rescue_file_name);
      //check rescue size from image file
      rtk_get_size_of_file(path, &rescue_img_size);
      install_debug("rescue found size (%u Bytes = %u KB)\r\n", rescue_img_size, rescue_img_size/1024);
   }
   else {
      rescue_img_size = 0;
   }

   // pre-erase for fast programming
   if (prtkimgdesc->norflash_size_32MB == 1)
   {
      //for 32MB nor flash, start erase from 0x3000
      erase_start = NOR_HWSETTING_START_ADDR;
      erase_length = prtkimgdesc->fw[FW_BOOTCODE].flash_allo_size-NOR_HWSETTING_START_ADDR;
   }
   else
   {
      erase_start = prtkimgdesc->fw[FW_BOOTCODE].flash_offset;
      erase_length = prtkimgdesc->fw[FW_BOOTCODE].flash_allo_size;
   }

#ifndef __OFFLINE_GENERATE_BIN__
   if ((ret = rtk_erase_mtd(prtkimgdesc, erase_start, erase_length)) < 0)
   {
      install_fail("erase error!\r\n");
      return -1;
   }
#endif

   if (prtkimgdesc->norflash_size_32MB == 1)
   {
      printf("skip resetrom & extern_param!\r\n");
   }
   else
   {
      //---------[reset-rom section]---------//
      printf("\r\n[Burn resetrom]\r\n");
      start = 0;

      snprintf(path, sizeof(path), "%s/%s", BOOTCODE_TEMP, RESETROM_FILENAME);
      ret = rtk_get_size_of_file(path, &len);
      if(ret < 0)
      {
         install_fail("\"%s\" file not found\r\n", path);
         return -1;
      }

      printf("start:0x%016llx, size:0x%08x(%u), end:0x%08llx\r\n", start, len, len, start+len-1);
      ret = rtk_file_to_flash(path, 0, prtkimgdesc->mtd_path, start, len, NULL);
      if(ret < 0)
      {
         install_fail("burn resetrom fail\r\n");
         return -1;
      }

      // verify
      if(verify_flag) {
         ret = rtk_file_verify(prtkimgdesc->mtd_path
            , start
            , path
            , 0
            , len
            , &error);

         if (ret < 0) {
            install_fail("verify error!!!\r\n");
            return -1;
         }
      }

      //---------[read extern parameter from flash]---------//
      //printf("\r\n[Read extern_param]\r\n");
      start = start + len;
      //rtk_flash_to_ptr(prtkimgdesc->mtdblock_path, start, &extern_param, sizeof(extern_param));
      //printf("start:0x%08x, size:0x%08x(%u), end:0x%08x\r\n", start, sizeof(extern_param), sizeof(extern_param), start+sizeof(extern_param)-1);

      //---------[parsing rest-rom to get factory size]---------//
      //ret = rtk_file_to_ptr(path, 0x1c, (void*) &extern_param.factory_size, 4);
      //extern_param.factory_size = SIZE_ALIGN_BOUNDARY_MORE(extern_param.factory_size, prtkimgdesc->mtd_erasesize);

      //---------[extern-param section]---------//
      printf("\r\n[Burn extern_param]\r\n");
      len = sizeof(extern_param);
      extern_param.flash_type = prtkimgdesc->flash_type;
      extern_param.region = 0;
      extern_param.mac_hi = DEFAULT_MAC_ADDRESS_HI;
      extern_param.mac_lo = DEFAULT_MAC_ADDRESS_LO;

      //extern_param.factory_saddr = (unsigned char*) (prtkimgdesc->flash_size - extern_param.factory_size);
      //extern_param.rescue_img_saddr = (unsigned char*) prtkimgdesc->rescue_start;

      extern_param.bootcode_img_saddr = 0x0;
      extern_param.bootcode_img_size = prtkimgdesc->rescue_start;
      extern_param.rescue_img_part0_saddr = prtkimgdesc->rescue_start;
      extern_param.rescue_img_part1_saddr = 0;
      extern_param.rescue_img_part1_len = 0;
      extern_param.env_param_saddr = 0;
      extern_param.rescue_img_size = extern_param.rescue_img_part0_len = rescue_img_size;

      //--ignore_native_rescue = y, means rescue will be overwritten so set the size to 0.--//
      if(prtkimgdesc->ignore_native_rescue == 1)
      {
         extern_param.rescue_img_size = extern_param.rescue_img_part0_len  = 0;
         install_debug("ignore_native_rescue set, resecue_img_size = 0, rescue_img_part0_len = 0\r\n");
      }
      else
      {
         if (rescue_img_size == 0)
         {
            install_debug("Can't resecue find file, resecue_img_size = 0, rescue_img_part0_len = 0\r\n");
         }
      }

      printf("start:0x%08llx, size:0x%08x(%u), end:0x%08llx\r\n", start, len, len, start+len-1);
      ret = rtk_ptr_to_flash(&extern_param, len, prtkimgdesc->mtd_path, start);
      if(ret < 0)
      {
         install_fail("burn extern_param fail\r\n");
         return -1;
      }

      // verify
      if(verify_flag) {
         ret = rtk_ptr_verify(prtkimgdesc->mtd_path
            , start
            , (char *)&extern_param
            , 0
            , len
            , &error);

         if (ret < 0) {
            install_fail("verify error!!!\r\n");
            return -1;
         }
      }
   }


   //---------[hwsetting section]---------//
   printf("\r\n[Burn hwsetting]\r\n");
   start = NOR_HWSETTING_START_ADDR;

   snprintf(path, sizeof(path), "%s/%s", BOOTCODE_TEMP, HWSETTING_FILENAME);
   ret = rtk_get_size_of_file(path, &len);
   if(ret < 0)
   {
      install_fail("\"%s\" file not found\r\n", path);
      return -1;
   }
   printf("start:0x%08llx, size:0x%08x(%u), end:0x%08llx\r\n", start, len, len, start+len-1);
   ret = rtk_file_to_flash(path, 0, prtkimgdesc->mtd_path, start, len, NULL);
   if(ret < 0)
   {
      install_fail("burn hwsetting fail\r\n");
      return -1;
   }

   // verify
   if(verify_flag) {
      ret = rtk_file_verify(prtkimgdesc->mtd_path
         , start
         , path
         , 0
         , len
         , &error);

      if (ret < 0) {
         install_fail("verify error!!!\r\n");
         return -1;
      }
   }

   //---------[boot-rom section]---------//
   printf("\r\n[Burn bootrom]\r\n");

   start = NOR_HWSETTING_START_ADDR + len;
   snprintf(path, sizeof(path), "%s/%s", BOOTCODE_TEMP, HASHTARGET_FILENAME);
   ret = rtk_get_size_of_file(path, &len);
   if(ret < 0)
   {
      install_fail("\"%s\" file not found!\r\n", path);
      return -1;
   }
   printf("len of hash_target.bin big-endian:[0x%08x]\r\n", len);
   ret = rtk_ptr_to_flash(&len, 4, prtkimgdesc->mtd_path, start);
   if(ret < 0)
   {
      install_fail("burn bootrom size fail\r\n");
      return -1;
   }

   printf("start:0x%08llx, size:0x%08x(%u), end:0x%08llx\r\n", start+4, len, len, start+4+len-1);

   //write boot-rom
   ret = rtk_file_to_flash(path, 0, prtkimgdesc->mtd_path, start+4, len, NULL);
   if(ret < 0)
   {
      install_fail("burn bootrom fail\r\n");
      return -1;
   }

   // verify
   if(verify_flag)
   {
      unsigned int hash_target_len;

      ret = rtk_flash_to_ptr(prtkimgdesc->mtd_path
         , start
         , &hash_target_len
         , 4);

      printf("bootrom length=0x%x, read=0x%x\r\n", len, hash_target_len);

      if (len != hash_target_len)
      {
         install_fail("verify error!!!\r\n");
         return -1;
      }

      ret = rtk_file_verify(prtkimgdesc->mtd_path
         , start+4
         , path
         , 0
         , len
         , &error);

      if (ret < 0) {
         install_fail("verify error!!!\r\n");
         return -1;
      }
   }

   //---------[rescue section]---------//
   printf("\r\n[Burn rescue]\r\n");

   start = prtkimgdesc->rescue_start;

   if(prtkimgdesc->ignore_native_rescue == 1)
   {
      len = 0;
      install_log("ignore_native_rescue set, skip burn rescue process\r\n");
   }
   else
   {
      len = rescue_img_size;
      if (len == 0)
      {
         install_log("rescue not found!, skip process.\r\n");
      }
      else
      {
         printf("start:0x%08llx, size:0x%08x(%u), end:0x%08llx\r\n", start, len, len, start+len-1);
         // write rescue
         snprintf(path, sizeof(path), "%s/%s", BOOTCODE_TEMP, rescue_file_name);
         ret = rtk_file_to_flash(path, 0, prtkimgdesc->mtd_path, start, len, NULL);
         if(ret < 0)
         {
            install_fail("burn hwsetting fail\r\n");
            return -1;
         }

         // verify
         if(verify_flag)
         {
            ret = rtk_file_verify(prtkimgdesc->mtd_path
               , start
               , path
               , 0
               , len
               , &error);

            if (ret < 0) {
               install_fail("verify error!!!\r\n");
               return -1;
            }
         }
      }
   }

   //---------[update boottable]---------//
   memset(&boottable, 0, sizeof(boottable));
   pbt = read_boottable(&boottable, prtkimgdesc);
   boottable.fw.list[FWTYPE_NRESCUE].loc.target = 0x80100000;
   boottable.fw.list[FWTYPE_NRESCUE].loc.offset = start;
   boottable.fw.list[FWTYPE_NRESCUE].loc.size = len;
   if(pbt == NULL)
   {
      install_log("\r\nboottable is NULL, we set native rescue\r\n");
      //Can't get boottable
      boottable.boottype = BOOTTYPE_NATIVE_RESCUE;
      //boottable.tag = TAG_UNKNOWN;
      snprintf(boottable.imgcksum, sizeof(boottable.imgcksum), "%s", hash_file(prtkimgdesc->tarinfo.tarfile_path));
   }
   write_boottable(&boottable, prtkimgdesc->factory_start, prtkimgdesc->factory_size, prtkimgdesc);

   return 0;

}

int rtk_erase(struct t_rtkimgdesc* prtkimgdesc, unsigned int erase_start, unsigned int erase_length)
{
   int ret = 0;
   if ((ret = rtk_erase_mtd(prtkimgdesc, erase_start, erase_length)) < 0) {
      install_fail("erase error!\r\n");
      return -1;
   }
   return 0;
}

#ifdef BURN_BOOTCODE
extern int boot_main(const char *dir);
#define BOOT_CODE_DIR "/tmp/tar_file_tmp"
int rtk_burn_bootcode(struct t_rtkimgdesc* prtkimgdesc)
{
   int ret;
   char cmd[128] = {0};
   sprintf(cmd, "mkdir -p %s;tar -xf %s -C %s", BOOT_CODE_DIR, "/tmp/bootfw", BOOT_CODE_DIR);
   if(check_target_flash_is_spi() == 1)
   {
      if(rtk_extract_file(prtkimgdesc, &prtkimgdesc->fw[FW_BOOTCODE], "/tmp/bootfw") < 0)
      {
         install_debug("Can't extract bootcode\r\n");
      }
      else
      {
         // in NOR FLASH
         ret = rtk_command(cmd, __LINE__, __FILE__);
         ret = boot_main(BOOT_CODE_DIR);
         install_log("boot_main:ret:%d\r\n", ret);
      }
      return 1;
   }
   if(checkinstallbootcode(prtkimgdesc) != 1)
   {
      install_log("Bootcode not installed\r\n");
      return -1;
   }
   if(rtk_extract_file(prtkimgdesc, &prtkimgdesc->fw[FW_BOOTCODE], "/tmp/bootfw") < 0)
   {
      install_debug("Can't extract bootcode\r\n");
      return -1;
   }

   ret = rtk_command(cmd, __LINE__, __FILE__);
   ret = boot_main(BOOT_CODE_DIR);
   install_log("boot_main:ret:%d\r\n", ret);
   return 0;
}
#endif

int rtk_extract_file(struct t_rtkimgdesc* prtkimgdesc, struct t_imgdesc* pimgdesc, const char* file_path)
{
   int ret;
   int dfd;
#ifdef __OFFLINE_GENERATE_BIN__
   char cmd[128] = {0};
#endif

   // sanity-check fun-call dep
   if(0 == prtkimgdesc->tarfileparsed)
   {
      install_debug("Install_a doesn't parse the tarfile, so we can't extract any file\r\n");
      return -1;
   }
   // sanity-check
   if((NULL == pimgdesc) || (0 == pimgdesc->img_size))
   {
      install_debug("%s don't exist\r\n", file_path);
      return -1;
   }
   if(NULL == file_path)
   {
      install_debug("no idea where file stored\r\n");
      return -1;
   }
   // show info
   install_info("\r\nExtract %s file into %s...\r\n", pimgdesc->filename, file_path);
   install_info( "tarfile_offset:0x%08x (%u Bytes = %u KB)\r\n" \
         , pimgdesc->tarfile_offset, pimgdesc->img_size, pimgdesc->img_size/1024);


   // open dfd
   dfd = open(file_path, O_CREAT|O_WRONLY|O_TRUNC, 0644);
   if(dfd < 0)
   {
      install_debug("Can't Open %s\r\n", file_path);
      return -1;
   }

   // open tarfile fd streaming
   prtkimgdesc->tarinfo.fd = -1;
   ret = rtk_open_tarfile_with_offset(&prtkimgdesc->tarinfo, pimgdesc->tarfile_offset+sizeof(t_tarheader));
   if(0 != ret)
   {
      install_debug("Can't Open tarfile : %s\r\n", prtkimgdesc->tarinfo.tarfile_path);
      close(dfd);
      close(prtkimgdesc->tarinfo.fd);
      return -1;
   }


   // extracting
   // read data from tarfile
   // write data into file
   install_info("extract %s from %s", pimgdesc->filename, prtkimgdesc->tarinfo.tarfile_path);
   ret = fd_to_fd(prtkimgdesc->tarinfo.fd, dfd, pimgdesc->img_size);
   close(prtkimgdesc->tarinfo.fd);
   close(dfd);


   if((unsigned int)ret != pimgdesc->img_size)
   {
      install_debug("Extract not complete, ret=%d\r\n", ret);
      return -1;
   }

#ifdef __OFFLINE_GENERATE_BIN__
   snprintf(cmd, sizeof(cmd), "chmod 777 %s", file_path);
   if((ret = rtk_command(cmd, __LINE__, __FILE__, 0)) < 0)
   {
      return -1;
   }
#endif

   install_info("extract ok\r\n");
   return 0;
}

int rtk_extract_utility(struct t_rtkimgdesc* prtkimgdesc)
{
   extern struct t_UTILDESC rtk_util_list[];

   static int utility_extracted = 0;
   int ret;
   char cmd[256] = {0};

   if(utility_extracted == 1)
   {
      return 0;
   }

   // Extract utility
   strcpy(cmd, "chmod 777");
   ret = 0;
   for( UTILTYPE i=UTIL_FLASHERASE; i<UTIL_MAX; i=(UTILTYPE)(i+1) )
   {
      if( prtkimgdesc->util[i].img_size )
      {
         if( rtk_extract_file(prtkimgdesc, &(prtkimgdesc->util[i]), rtk_util_list[i].bin_path) < 0 )
         {
            install_debug("Extract utility (%s) fail\n", prtkimgdesc->util[i].filename);
            return -1;
         }
         sprintf(cmd+strlen(cmd), " %s", rtk_util_list[i].bin_path);
         ret++;
      }
   }

   if( ret == 0 )
   {
      install_debug("No utiltiy to extract...\n");
   }
   else if( (ret = rtk_command(cmd, __LINE__, __FILE__)) < 0)
   {
      install_debug("Exec command fail\r\n");
      return -1;
   }
   utility_extracted = 1;
   return 0;
}

int rtk_extract_cipher_key(struct t_rtkimgdesc* prtkimgdesc)
{
   extern struct t_KEYDESC rtk_key_list[];

   for( KEYTYPE i=KEY_RSA_PUBLIC; i<KEY_MAX; i=(KEYTYPE)(i+1) )
   {
	  if( prtkimgdesc->cipher_key[i].img_size )
	  {
		 if( rtk_extract_file(prtkimgdesc, &(prtkimgdesc->cipher_key[i]), rtk_key_list[i].bin_path) < 0 )
		 {
			install_debug("Extract key(%s) fail\n", prtkimgdesc->cipher_key[i].filename);
			return -1;
		 }
	  }
   }
   return 0;
}

#ifdef __OFFLINE_GENERATE_BIN__
int rtk_factory_to_virt_nand(struct t_rtkimgdesc* prtkimgdesc)
{
   char path[128] = {0};
   unsigned int file_len = 0;
   int ret;

   snprintf(path, sizeof(path), "%s/%s", PKG_TEMP, FACTORYBLOCK);

   if((ret = rtk_get_size_of_file(path, &file_len)) < 0)
   {
      install_debug("file not found\r\n");
      return -1;
   }

   ret = rtk_file_to_virt_nand_with_ecc(path
                                       , 0
                                       , file_len
                                       , prtkimgdesc
                                       , prtkimgdesc->factory_start
                                       , RTK_NAND_BLOCK_STATE_GOOD_BLOCK
                                       , NULL
                                       , 3);
   return ret;
}
#endif

// protect bootcode
static int rtk_file_to_flash_pbc(struct t_rtkimgdesc* prtkimgdesc, const char* filepath, unsigned int soffset, unsigned int imglen, unsigned long long doffset, unsigned int alignlen, unsigned int* pchecksum)
{
   if(prtkimgdesc->ignore_native_rescue == 0)
   {
      // sanity-check
      // protect bootcode
      if(doffset < prtkimgdesc->bootcode_size)
      {
         install_debug("protect bootcode\r\n");
         return -1;
      }
   }
   //printf("[Installer_D]: prtkimg->mtdblock_path = [%s].\n", prtkimgdesc->mtdblock_path );
	if ((prtkimgdesc->flash_type == MTD_EMMC) || (prtkimgdesc->flash_type == MTD_SATA))
   		return rtk_file_to_flash(filepath, soffset, prtkimgdesc->mtdblock_path, doffset, imglen, pchecksum);
	else
   		return rtk_file_to_mtd(filepath, soffset, imglen, prtkimgdesc, (unsigned int)doffset, alignlen, pchecksum);
}

#ifdef __OFFLINE_GENERATE_BIN__
static int rtk_file_to_virt_nand(struct t_rtkimgdesc* prtkimgdesc, const char* filepath, unsigned int soffset, unsigned int imglen, unsigned int doffset, unsigned char block_indicator, unsigned int* pchecksum)
{
   // sanity-check
   // protect bootcode
   if(doffset < prtkimgdesc->bootcode_size)
   {
      install_debug("protect bootcode\r\n");
      return -1;
   }
   return rtk_file_to_virt_nand_with_ecc(filepath, soffset, imglen, prtkimgdesc, doffset, block_indicator, pchecksum);
}

static int rtk_yaffs_to_virt_nand(struct t_rtkimgdesc* prtkimgdesc, const char* filepath, unsigned int soffset, unsigned int imglen, unsigned int doffset, unsigned char block_indicator, unsigned int* pchecksum)
{
   // sanity-check
   // protect bootcode
   if(doffset < prtkimgdesc->bootcode_size)
   {
      install_debug("protect bootcode\r\n");
      return -1;
   }
   return rtk_yaffs_to_virt_nand_with_ecc(filepath, soffset, imglen, prtkimgdesc, doffset, block_indicator, pchecksum);
}
#endif
