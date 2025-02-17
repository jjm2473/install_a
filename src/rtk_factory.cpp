#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <rtk_factory.h>
#include <rtk_common.h>
#include <rtk_mtd.h>
#include <rtk_def.h>
#include <rtk_parameter.h>


#define DEBUG(args, ...)    

char factory_dir[32];
static int is_init = 0;
static int gflash_type = MTD_NORFLASH;

// #define SKIP_FACTORY_AREA will NOT write data into factory area.
// comment it if want to write data into factory area.
//#define SKIP_FACTORY_AREA


#ifndef PC_SIMULATE//__OFFLINE_GENERATE_BIN__ 

#define DEFAULT_FACTORY_DIR "/tmp/factory"
#define DEFAULT_FACTORY_DIR1 "/tmp/factory1"
#define DEFAULT_FACTORY_DIR2 "/tmp/factory2"
#define FACTORY_FILE_PATH "/tmp/factory.tar"

#else /* else of ifndef __OFFLINE_GENERATE_BIN__ */

#define DEFAULT_FACTORY_DIR "tmp/factory"
#define DEFAULT_FACTORY_DIR1 "tmp/factory1"
#define DEFAULT_FACTORY_DIR2 "tmp/factory2"
#define FACTORY_FILE_PATH "tmp/factory.tar"

static int factory_init_virt_nand(void);

#endif /* end of ifndef __OFFLINE_GENERATE_BIN__ */


typedef struct posix_header_t
{                   /* byte offset */
    char name[100]; /* 0 */
    char mode[8]; /* 100 */
    char uid[8]; /* 108 */
    char gid[8]; /* 116 */
    char size[12]; /* 124 */
    char mtime[12]; /* 136 */
    char chksum[8]; /* 148 */
    char typeflag; /* 156 */
    char linkname[100]; /* 157 */
    char magic[6]; /* 257 */
    char version[2]; /* 263 */
    char uname[32]; /* 265 */
    char gname[32]; /* 297 */
    char devmajor[8]; /* 329 */
    char devminor[8]; /* 337 */
    char prefix[155]; /* 345 */
    unsigned int rtk_seqnum; /*500 */     
    unsigned int rtk_tarsize; /*504 */       
    char rtk_signature[4]; /*508 */          
} posix_header;

#ifdef PC_SIMULATE//__OFFLINE_GENERATE_BIN__
static char gfile[128];
#endif
static char gdev[128];
static unsigned int gfactory_start;
static unsigned int gfactory_size;
static unsigned int gfactory_block_size;
//static unsigned int gerasesize;
static unsigned int seq_num = 0;
static unsigned int tarsize;
static char current_pp = -1;

int factory_find_latest_update(void) {

   posix_header p0_start, p0_end;
   posix_header p1_start, p1_end;
   int pp_ok = 0;
	
#ifdef SKIP_FACTORY_AREA
   printf("%s, %s, %d, return directly\n", __FILE__, __func__, __LINE__);
   return 0;
#endif

   // get factory header from pp0 and pp1 
   rtk_file_to_ptr(gdev, gfactory_start, &p0_start, sizeof(p0_start));
   rtk_file_to_ptr(gdev, gfactory_start + gfactory_block_size, &p1_start, sizeof(p1_start));
   
   if (!strncmp(p0_start.name, "tmp/factory/", strlen("tmp/factory/")))  {
      rtk_file_to_ptr(gdev, gfactory_start + p0_start.rtk_tarsize, &p0_end, sizeof(p0_end));
      if (!memcmp(&p0_start, &p0_end, sizeof(p0_start))) {
         pp_ok |= 1;
      }     
   } 
   if (!strncmp(p1_start.name, "tmp/factory/", strlen("tmp/factory/")))  {
      rtk_file_to_ptr(gdev, gfactory_start + gfactory_block_size+ p1_start.rtk_tarsize, &p1_end, sizeof(p1_end));
      //    rtk_ptr_to_file("/tmp/p1_end.bin", 0, &p1_end, sizeof(p1_end));
      //    rtk_ptr_to_file("/tmp/p1_start.bin", 0, &p1_start, sizeof(p1_start));         
      if (!memcmp(&p1_start, &p1_end, sizeof(p1_start))) {
         pp_ok |= 2;
      } 
   }

   switch (pp_ok) {

      case 0:
         current_pp = -1;     
      break;
      case 1:
         current_pp = 0;         
         break;
      case 2:
         current_pp = 1;         
         break;
      break;
      case 3:
         if (p0_start.rtk_seqnum < p1_start.rtk_seqnum) {
            if ((p1_start.rtk_seqnum - p0_start.rtk_seqnum) > 0xFFFFFFF0) {
               current_pp = 0;               
            } else {
               current_pp = 1;
            }
         } else {
            if ((p0_start.rtk_seqnum - p1_start.rtk_seqnum) > 0xFFFFFFF0) {
               current_pp = 1;               
            } else {
               current_pp = 0;
            }
         }
      break;

   }


   if (current_pp == 0) {
      seq_num = p0_start.rtk_seqnum;
      tarsize = p0_start.rtk_tarsize;
   } else if (current_pp == 1)  {
      seq_num = p1_start.rtk_seqnum;
      tarsize = p1_start.rtk_tarsize;
   } 

   install_info("pp_ok = %x current_pp = %d seq_num = %d tarsize=%u\n", pp_ok, current_pp, seq_num, tarsize);
	return 0;
}

int factory_init(const char* dir, struct t_rtkimgdesc* prtkimg)
{
   char cmd[128];
   char tmp[128];
   int ret;

#ifndef __OFFLINE_GENERATE_BIN__ 
   // sanity-check
   if(dir == NULL)
   {
      install_debug("dir is NULL\r\n");
   }

   if (prtkimg->mtd_erasesize == 0) {
   	install_fail("factory erase size not initialized!!!");
   }

   // gdev
   strcpy(gdev, prtkimg->mtdblock_path);
   
   // gfactory_start
   ret = get_parameter_value("factory_start", &gfactory_start);
   if(ret < 0)
   {
      return -1;
   }
   prtkimg->factory_start = gfactory_start;

   // gfactory_size
   ret = get_parameter_value("factory_size", &gfactory_size);
   if(ret < 0)
   {
      return -1;
   }

   ret = get_parameter_string("boot_flash", tmp, 128);
   if (ret < 0) 
   {
      install_fail("can't get boot type\n");
      return -1;
   }

   if (!strcmp(tmp, "spi")) {
      gflash_type = MTD_NORFLASH;
	  gfactory_size /= 2;
	  prtkimg->mtd_erasesize = prtkimg->norflash_mtd_erasesize;
   } else if (!strcmp(tmp, "nand")) {
      gflash_type = MTD_NANDFLASH;
      gfactory_size /= 2;
#ifdef EMMC_SUPPORT
   } else if (!strcmp(tmp, "emmc")) {
	  gflash_type = MTD_EMMC; 
      gfactory_size /= 2;
#endif
   } else {
      install_fail("can't get boot type\n");
      return -1; 
   }
   if(dir == NULL || strlen(dir) == 0)
      sprintf(factory_dir, "%s", DEFAULT_FACTORY_DIR);
   else
      sprintf(factory_dir, "%s", dir);

   if (prtkimg->flash_type == MTD_EMMC)
      gfactory_block_size = gfactory_size;
   else
	  gfactory_block_size = ((gfactory_size + prtkimg->mtd_erasesize - 1)/prtkimg->mtd_erasesize) * prtkimg->mtd_erasesize;

	prtkimg->factory_size = gfactory_block_size;
   install_debug("gfactory_start = 0x%x\n", gfactory_start);
   install_debug("gfactory_block_size = 0x%x\n", gfactory_block_size);
   install_debug("gfactory_size = 0x%x\n", gfactory_size);
   
#ifndef NAS_ENABLE
   if (!strcmp(prtkimg->mtdblock_path, "/dev/mtdblock4")) {
      gfactory_start = 0;
   }
#endif

   snprintf(cmd, sizeof(cmd), "mkdir -p %s", factory_dir);
   ret = rtk_command(cmd, __LINE__, __FILE__, 0);
   if(ret < 0)
   {
      install_debug("factory_init fail\r\n");
      return -1;
   }

   factory_find_latest_update();
#else
	if(dir == NULL || strlen(dir) == 0)
      sprintf(factory_dir, "%s", DEFAULT_FACTORY_DIR);
   else
      sprintf(factory_dir, "%s", dir);

   snprintf(cmd, sizeof(cmd), "mkdir -p %s", factory_dir);
   ret = rtk_command(cmd, __LINE__, __FILE__, 0);
   if(ret < 0)
   {
      install_debug("factory_init fail\r\n");
      return -1;
   }

	if (prtkimg->flash_type == MTD_NANDFLASH)
   	factory_init_virt_nand();

   current_pp = -1;     
#endif

   is_init = 1;
   return 0;
}

int factory_load(const char *dir, struct t_rtkimgdesc* prtkimg) {

   char cmd[128] = {0};
   int ret;
   
   if(is_init == 0) {
      if (factory_init(NULL, prtkimg) < 0) return -1;   //install_log("re-init\r\n");
   }

#ifdef SKIP_FACTORY_AREA
    printf("%s, %s, %d, return directly\n", __FILE__, __func__, __LINE__);
    return 0;
#endif

   if (current_pp < 0) return -1;

   snprintf(cmd, sizeof(cmd), "rm -rf %s;mkdir -p %s", factory_dir, factory_dir);
   ret = rtk_command(cmd, __LINE__, __FILE__, 0);

#ifndef PC_SIMULATE//__OFFLINE_GENERATE_BIN__ 
   snprintf(cmd, sizeof(cmd), "cd /;dd if=%s bs=%u count=%u skip=%u | tar x", prtkimg->mtdblock_path, prtkimg->mtd_erasesize, prtkimg->factory_size/prtkimg->mtd_erasesize, (prtkimg->factory_start + current_pp * prtkimg->factory_size)/prtkimg->mtd_erasesize);
#else
   if (prtkimg->flash_type == MTD_NORFLASH) {
      snprintf(cmd, sizeof(cmd), "dd if=%s bs=%u count=%u skip=%u | tar xv", gdev, 512, (tarsize+511)/512, 
                                              (gfactory_start + current_pp * gfactory_block_size) /512);
   }
   else if (prtkimg->flash_type == MTD_NANDFLASH){
      snprintf(cmd, sizeof(cmd), "dd if=%s bs=%u count=%u skip=%u | tar xv", gfile, 512, (tarsize+511)/512, 
                                              (current_pp * gfactory_block_size) /512);
   }
	else if (prtkimg->flash_type == MTD_EMMC) {
		snprintf(cmd, sizeof(cmd), "dd if=%s bs=%u count=%u skip=%u | tar xv", prtkimg->mtdblock_path, prtkimg->mtd_erasesize, (tarsize+511)/512, (prtkimg->factory_start + current_pp * prtkimg->factory_size)/prtkimg->mtd_erasesize);
	}      
#endif

   ret = rtk_command(cmd, __LINE__, __FILE__, 0);
   if(ret < 0)
   {
      install_info("factory_load - to extract file from mtdblock's factory section fail\r\n");
      return -1;
   }
   else
   {
      //sprintf(cmd, "dd if=/tmp/factory/layout.txt");
      //ret = rtk_command(cmd, __LINE__, __FILE__);
   }
   return 0;
}

unsigned int factory_tar_checksum(char *header ) {

   unsigned char *data = (unsigned char *) header;
   unsigned int sum;
   int i;
        // check checksum;
        sum=0;
        for (i=0;i<148;i++) sum+=data[i];

        for (i=156;i<512;i++) sum+=data[i];

        for (i=0; i<8; i++) sum += 32;

     DEBUG("check_sum = %06o\n", sum);
     return sum;
}

void factory_tar_fill_checksum(char *cheader ) {

   posix_header* header = (posix_header*) cheader; 
   sprintf(header->chksum, "%06o", factory_tar_checksum(cheader));

   header->chksum[6] = 0;
   header->chksum[7] = 0x20;
   DEBUG("fill checksun  = %s\n", header->chksum);
}

void factory_build_header(const char *path){

   posix_header_t header;
   rtk_file_to_ptr(path, 0, &header, sizeof(header));
   header.rtk_signature[0] = 'R';
   header.rtk_signature[1] = 'T';
   header.rtk_signature[2] = 'K';
   header.rtk_signature[3] = 0;
   rtk_get_size_of_file(path, &(header.rtk_tarsize));
   seq_num ++;
   header.rtk_seqnum = seq_num;
   install_debug("header (%d bytes) %d\n", sizeof(header), header.rtk_tarsize);
   install_debug("%s \n", header.rtk_signature);
   factory_tar_fill_checksum((char *) &header);
   rtk_ptr_to_file(path, 0, &header, sizeof(header));
   rtk_ptr_to_file(path, header.rtk_tarsize, &header, sizeof(header));

}

int factory_flush(unsigned int factory_start, unsigned int factory_size, struct t_rtkimgdesc* prtkimg, bool bFlush)
{
   int ret;
   char cmd[128];
   unsigned int size;
   //unsigned int checksum;
   struct stat st = {0};
   char tmp_recovery[32];
   
#ifdef SKIP_FACTORY_AREA
   printf("%s, %s, %d, return directly\n", __FILE__, __func__, __LINE__);
   return 0;
#endif

   if(factory_start == 0 || factory_size == 0) {
      factory_start = gfactory_start;
      factory_size = gfactory_size;
   }

   memset(tmp_recovery, 0, sizeof(tmp_recovery));
   sprintf(tmp_recovery, "%s/recovery", factory_dir);
   printf("[Installer_D]: recovery path=[%s]\n", tmp_recovery);
   if(stat(tmp_recovery, &st) == 0)
   {
      sprintf(cmd, "rm -rf %s", tmp_recovery);
      rtk_command(cmd, __LINE__, __FILE__);
   }

   memset(cmd, 0, sizeof(cmd));   
#ifndef PC_SIMULATE//__OFFLINE_GENERATE_BIN__
   rtk_command("ls -al /tmp/factory", __LINE__, __FILE__);
   if(stat(FACTORY_FILE_PATH, &st) == 0)
   {
      sprintf(cmd, "rm -rf %s", FACTORY_FILE_PATH);
      rtk_command(cmd, __LINE__, __FILE__);
   }

   memset(cmd, 0, sizeof(cmd));   
   sprintf(cmd, "tar cf %s %s; sync", FACTORY_FILE_PATH, factory_dir);
#else
   sprintf(cmd, "find %s | sort | tar cvf %s --no-recursion -T -", factory_dir, FACTORY_FILE_PATH);
#endif
   ret = rtk_command(cmd, __LINE__, __FILE__);
   if(ret < 0)
   {
      install_debug("rtk_command fail\r\n");
      return -1;
   }

   install_debug("*****************\n");
   factory_build_header(FACTORY_FILE_PATH);
   install_debug("*****************\n");
   rtk_get_size_of_file(FACTORY_FILE_PATH, &size);
   if (size > factory_size) {
      install_fail("factory.tar(%d) large then default FACTORY_SIZE(%d)\n", size, factory_size); 
      return -1;
   }

   tarsize = size;
   install_log("%s filesize:%u bytes\r\n", FACTORY_FILE_PATH, size);

   if (current_pp < 0) current_pp = 0;
   else {
      current_pp++;
      current_pp&=0x01;
   }
   install_info("save to current_pp = %d seq_num = %d\n", current_pp, seq_num);

#ifndef __OFFLINE_GENERATE_BIN__
	if (bFlush) {
		sprintf(cmd, "dd if=%s of=%s bs=%u count=%u seek=%u conv=notrunc,fsync  > /dev/null", FACTORY_FILE_PATH, prtkimg->mtdblock_path, prtkimg->mtd_erasesize, factory_size/prtkimg->mtd_erasesize, (factory_start + current_pp * factory_size)/prtkimg->mtd_erasesize);
	} else {
		sprintf(cmd, "dd if=%s of=%s bs=%u count=%u seek=%u conv=notrunc conv=fsync > /dev/null", FACTORY_FILE_PATH, prtkimg->mtdblock_path, prtkimg->mtd_erasesize, factory_size/prtkimg->mtd_erasesize, (factory_start + current_pp * factory_size)/prtkimg->mtd_erasesize);
	}        
#else
	if (prtkimg->flash_type == MTD_NORFLASH) {
         sprintf(cmd, "dd if=%s of=%s bs=%u count=%u seek=%u conv=notrunc,fsync", FACTORY_FILE_PATH, gdev, prtkimg->mtd_erasesize, factory_size/prtkimg->mtd_erasesize, (factory_start + current_pp * gfactory_block_size)/prtkimg->mtd_erasesize);
   }
	else if (prtkimg->flash_type == MTD_NANDFLASH) {
         sprintf(cmd, "dd if=%s of=%s bs=%u count=%u seek=%u conv=notrunc,fsync", FACTORY_FILE_PATH, gfile, prtkimg->mtd_erasesize, factory_size/prtkimg->mtd_erasesize, (current_pp * gfactory_block_size)/prtkimg->mtd_erasesize);
   }
	else if (prtkimg->flash_type == MTD_EMMC) {
	sprintf(cmd, "dd if=%s of=%s bs=%u count=%u seek=%u conv=notrunc,fsync", FACTORY_FILE_PATH, prtkimg->mtdblock_path, prtkimg->mtd_erasesize, factory_size/prtkimg->mtd_erasesize, (factory_start + current_pp * factory_size)/prtkimg->mtd_erasesize);
   }
#endif

	ret = rtk_command(cmd, __LINE__, __FILE__, 1);
   if(ret < 0)
   {
      install_debug("rtk_command fail\r\n");
      return -1;
   }
   return 0;
}

const char *get_factory_tmp_dir(void)
{
   return factory_dir;
}

const char get_factory_current_pp(void)
{
   return current_pp;
}

#ifdef __OFFLINE_GENERATE_BIN__    
static int factory_init_virt_nand(void)
{

   unsigned buffer[128];
   unsigned int factory_file_len = 0;
   int write_bytes;
   FILE *file_fd;

   if (gfactory_size == 0) {
      install_debug("gfactory_size not init!!!\r\n");
      return -1;
   }

   snprintf(gfile, sizeof(gfile), "%s/%s", PKG_TEMP, FACTORYBLOCK);

   if ((file_fd = fopen(gfile, "wb")) == NULL) {
      install_debug("can't open %s\r\n", gfile);
      return -1;
   }

   factory_file_len = gfactory_size*2;

   memset(buffer, 0xff, sizeof(buffer));

   write_bytes = 0;
   do {
      write_bytes += fwrite(buffer, 1, sizeof(buffer), file_fd);
   } while(write_bytes != factory_file_len);

   fclose(file_fd);

   return 0;
}
#endif
