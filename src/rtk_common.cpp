#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <stdarg.h>

#include <rtk_boottable.h>
#include <rtk_common.h>
#include <rtk_mtd.h>
#include <rtk_imgdesc.h>
#include <rtk_fwdesc.h>
#include <rtk_def.h>
#include <rtk_burn.h>
#include <rtk_customer.h>
#include <rtk_shaAPI.h>
#include <ion.h>
#define STAR_COUNT 30.0
#define TIME_OUT 3.0
#define READ_BUFFER 1024

#define UIO_DEV "/dev/uio250"
#define RBUS_MAP_INFO   "/sys/class/uio/uio250/maps/map0"

#ifdef NAS_ENABLE
#include <kcapi/kcapi.h>
#endif

//#define DEBUG_CHECK

u32 gDebugPrintfLogLevel = INSTALL_FAIL_LEVEL | INSTALL_INFO_LEVEL | INSTALL_UI_LEVEL | INSTALL_LOG_LEVEL | INSTALL_DEBUG_LEVEL;

u32 get_checksum(u8 *p, u32 len)
{
    u32 checksum = 0;
    u32 i;

    for(i = 0; i < len; i++)
    {
        checksum += *(p+i);
    }
    return checksum;
}

unsigned long read_value(const char *entry)
{
    char fn[128];
    FILE *fd = NULL;
    unsigned int val = 0;

    snprintf(fn, 128, "%s/%s", RBUS_MAP_INFO, entry);
    fd = fopen(fn, "r");
    if(fd == NULL){
        install_fail("open file:%s\n", fn);
        exit(1);
    }
    if(fscanf(fd, "%x", &val) != 1){
        install_fail("failed to get value\n");
        exit(1);
    }
    
    fclose(fd);
    return val;
}

unsigned int get_chip_rev_id()
{
    int uio_fd;
    char uio_size_buf[16];
    void *mapBuf;
    
    unsigned int  *SB2_CHIP_INFO;
    unsigned long rbus_ofs = 0;
    unsigned long rbus_size = 0;
 
    uio_fd = open( UIO_DEV, O_RDWR | O_SYNC);   
    if(uio_fd < 0) {
        install_fail("%s, open failed, uio_fd(%d)\n", __func__, uio_fd);
        return -1;
    }
    
    rbus_size = read_value("size");
    rbus_ofs  = read_value("offset");
    install_debug("rbus_ofs=%x.\n", rbus_ofs);
    install_debug("rbus_size=%x.\n", rbus_size);
    mapBuf = mmap(0, rbus_size, PROT_READ | PROT_WRITE, MAP_SHARED, uio_fd, rbus_ofs);   
    close(uio_fd);
  
    if( mapBuf == MAP_FAILED ) {
        install_fail("%s, mmap failed\n", __func__);
        return -1;
    }

    SB2_CHIP_INFO = (unsigned int*) (mapBuf + 0x1a204);
    //install_info("SB2_CHIP_INFO: 0x%lx \n", SB2_CHIP_INFO);
    install_info("Start read SB2_CHIP_INFO....\n");

    unsigned int chipVer = REG32(SB2_CHIP_INFO);
    install_debug("Read done(rev.%x).\n", chipVer);
    munmap(mapBuf, rbus_size);
    
    return chipVer;
}

//define KEY_DEBUG
int read_key(const char *filename, unsigned char* key, int size)
{
    unsigned int j;
    FILE*   fd;
    int ret, FileLength;
    unsigned char key_file[size];

    memset(key_file, 0, sizeof(key_file));

    if ((fd = fopen(filename, "rb"))==NULL)
    {
        printf("Open File(%s)error\n", filename);
        return -1;
    }
    else
    {
        fseek(fd, 0, SEEK_SET);
        ret = fread(key_file, 1, sizeof(key_file), fd);
        fclose(fd);
    }

    if (ret != size)
    {
        printf("key_file length invalid\n");
        return -1;
    }

#ifdef KEY_DEBUG
    printf("---------- read key ----------\n");
    for( j=0; j<size; j++)
        printf("0x%02x, ", key_file[j]);
    printf("\n");
    printf("------------------------------\n");
#endif    
    memcpy(key, key_file, size);
#ifdef KEY_DEBUG
    printf("---------- read key ----------\n");
    for( j=0; j<size; j++)
        printf("0x%02x, ", key[j]);
    printf("\n");
    printf("------------------------------\n");
#endif
    return 0;
}

int isready(int fd)
{
    int rc;
    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    rc = select(fd+1, &fds, NULL, NULL, &tv);
    //rc = select(fd+1, &fds, NULL, NULL, NULL);
    if (rc < 0)
        return -1;

    return FD_ISSET(fd, &fds) ? 1 : 0;
}

static inline int memcmp_count(const char *pa, const char *pb, int size)
{
    int i, error_count;

    error_count = 0;
    for (i = 0; i < size; i++, pa++, pb++)
    {
        if (*pa != *pb) error_count++;
    }

    return error_count;
}

static inline void progressbar(unsigned long long len, unsigned long long length, const char* filename)
{
#if 0
    int i, starcount;
    if(len == 0) install_ui("\n");
    install_ui("\r");
    if(filename != NULL) install_ui("%s", filename);
    install_ui("%3d |", (int) (100.0*len/length));
    starcount = (int) (STAR_COUNT*len/length);
    for(i=0; i<starcount; i++) install_ui("*");
    for(i=0; i<STAR_COUNT-starcount; i++) install_ui(" ");
    install_ui("| %llu KB", len>>10);
    if(len == length) install_ui("\n");
#endif
}

void rtk_hexdump( const char * str, unsigned char * pcBuf, unsigned int length )
{
    unsigned int i, j, rows, count;

    printf("======================================================\n");
    printf("%s(base=0x%08x)\n", str, pcBuf);
    count = 0;
    rows = (length+((1<<4)-1)) >> 4;
    for( i = 0; ( i < rows ) && (count < length); i++ )
    {
        printf("%03x :", i<<4 );
        for( j = 0; ( j < 16 ) && (count < length); j++ )
        {
            printf(" %02x",  *pcBuf );
            count++;
            pcBuf++;
        }
        printf("\n");
    }
}

static inline void progressbar_ui(unsigned long long len, unsigned long long length, const char* filename, FILE* customer_fp)
{
    char showStr[128] = {'\0'};

    sprintf(showStr, "%s\\n( %llu KB/ %llu KB )", filename, len>>10, length>>10);
    rtk_customer_write_burn_partname(customer_fp, (const char*)&showStr);
}

unsigned long long fd_to_fd(int sfd, int dfd, unsigned long long length, unsigned int* pchecksum, FILE* ffd, char* filename, FILE* customer_fp)
{
    unsigned long long ret, rret, wret;
    unsigned long long rlen;
    unsigned long long len;
    unsigned long long progress_len, interval;
    char data_buf[2048*4] = {0};
    time_t stime, etime;
    int restart;
    time_t start_time=time(NULL), end_time;
    stime = time(NULL);
    interval = length/10;
    progress_len = 0;
    len = 0;
    if(pchecksum != NULL)
        *pchecksum = 0;

    restart = 1;
    progressbar(0, length, NULL);
    fflush(stdout);
    while(len < length)
    {
        rlen = ((length - len) < sizeof(data_buf)) \
               ? (length - len): sizeof(data_buf);
        ret = isready(sfd);
        // select error
        if(ret < 0)
        {
            install_debug("select error\r\n");
            goto dd_end;
        }
        if(ret == 0)
        {
            if(restart == 1)
            {
                install_debug("first timeout\r\n");
                start_time = time(NULL);
                restart = 0;
            }
            else
            {
                install_debug("second timeout\r\n");
                end_time = time(NULL);
                if(difftime(end_time, start_time) > TIME_OUT)
                {
                    goto dd_end;
                }
            }
            continue;
        }

        restart = 1;

        rret = read(sfd, (void*) data_buf, rlen);
        if(rret < 0)
        {
            install_debug("read source fd fail\r\n");
            goto dd_end;
        }
        else if(rret == 0)
        {
            install_debug("read source fd close\r\n");
            goto dd_end;
        }
        if(ffd == NULL)
            wret = write(dfd, (void*) data_buf, rret);
        else
            wret = fwrite(data_buf, 1, rret, ffd);

        if(wret < 0)
        {
            install_debug("write fail\r\n");
            goto dd_end;
        }
        if(rret!=wret)
        {
            install_debug("rret = %d, wret = %d\r\n", rret, wret);
            goto dd_end;
        }
        if(pchecksum != NULL)
        {
            *pchecksum += get_checksum((u8 *)data_buf, (u32) wret);
        }
        len = len + rret;
        if(len >= progress_len)
        {
            //printf("[*]");
            progressbar(len, length, NULL);
            if (customer_fp && filename)
            {
                progressbar_ui(len, length, filename, customer_fp);
            }
            fflush(stdout);
            progress_len = progress_len + interval;
        }
    }
dd_end:
    etime = time(NULL);
    if(pchecksum != NULL)
    {
        install_info("checksum:0x%08X\r\n\r\n", *pchecksum);
    }

    install_debug("\r\ntotal_time:%.0lf seconds\r\n", difftime(etime,stime));
    return len;
}

unsigned int octalStringToInt(const char* str, unsigned int lenstr)
{
    unsigned int size;
    unsigned int i;
    size = 0;
    for(i=0; i<lenstr; i++)
    {
        if(str[i]<='9'&&str[i]>='0')
            size = size*8 + (str[i]-0x30);
        else
            size = size*8;
    }
    return size;
}

int rtk_get_size_of_file(const char* file_path, unsigned int *file_size)
{
    struct stat stat_buf = {0};
    int ret;
    ret = stat(file_path, &stat_buf);
    if(ret < 0)
    {
        install_debug("Can't find %s\r\n", file_path);
        *file_size = 0;
        return -1;
    }
    *file_size = stat_buf.st_size;
    return 0;
}

int rtk_command(const char* command, int line, const char* file, int show)
{
    int ret;

    if(1 == show)
#if 0
        printf("Command:%s, (line=%d) [file=%s], ret(%d)\r\n", command, line, file, ret);
#else
        install_info("Command:%s\r\n", command);
    else
        install_debug("Command:%s\r\n", command);
#endif
    ret = system(command);

    fflush(stdout);
    fflush(stderr);
#ifndef PC_SIMULATE
    if(ret != 0 || WEXITSTATUS(ret))
    {
        install_debug("Can't exec command:%s, (line=%d) (file=%s) (ret=%d)\r\n", command, line, file, ret);
        return -1;
    }
#endif
    return 0;
}

int rtk_file_to_string(const char* path, char* str)
{

    FILE* filep = NULL;
    char cmd[128] = {0};
    int ret;
    snprintf(cmd, sizeof(cmd), "dd if=%s of=/tmp/str.txt", path);
    ret = rtk_command(cmd, __LINE__, __FILE__, 0);
    if(ret < 0)
    {
        install_debug("cmd fail\r\n");
        return -1;
    }
    filep = fopen("/tmp/str.txt", "r");
    if(filep == NULL)
    {
        install_log("Can't open (%s)\r\n", path);
        return -1;
    }

    if(NULL != fgets(str, 256, filep))
    {
        install_info("Got String(%s)\r\n", str);
    }
    fclose(filep);
    return 0;
}

int rtk_file_to_flash(const char* filepath, unsigned long long soffset, const char* devpath, unsigned long long doffset, unsigned long long len, unsigned int* checksum)
{
    int sfd, dfd;
    int ret;
    sfd = open(filepath, O_RDONLY);
    if(sfd<0)
    {
        install_debug("Can't open %s\r\n", filepath);
        return -1;
    }
    dfd = open(devpath, O_RDWR|O_SYNC);
    if(dfd<0)
    {
        install_debug("Can't open %s\r\n", devpath);
        return -1;
    }
    ret = lseek64(sfd, soffset, SEEK_SET);
    if (ret == (off64_t) -1)
    {
        install_debug("lseek (%s) fail\r\n", filepath);
        close(sfd);
        close(dfd);
        return -1;
    }
    ret = lseek64(dfd, doffset, SEEK_SET);
    if (ret == (off64_t) -1)
    {
        install_debug("lseek (%s) fail\r\n", devpath);
        close(sfd);
        close(dfd);
        return -1;
    }

    ret = fd_to_fd(sfd, dfd, len, checksum, NULL);
    close(sfd);
    close(dfd);

    if((unsigned int)ret != len)
    {
        install_debug("fd_to_fd fail(%d)\r\n", ret);
        return -1;
    }
    return ret;
}

int rtk_file_to_ptr(const char* filepath, unsigned int soffset, void* ptr, unsigned int len)
{
    int sfd;
    int ret;
    sfd = open(filepath, O_RDONLY);
    if(sfd<0)
    {
        install_debug("Can't open %s\r\n", filepath);
        return -1;
    }
    ret = lseek(sfd, soffset, SEEK_SET);
    if(ret < 0)
    {
        install_debug("lseek (%s) fail\r\n", filepath);
        close(sfd);
        return -1;
    }

    ret = read(sfd, ptr, len);
    close(sfd);

    if((unsigned int)ret != len)
    {
        install_debug("read fd fail(%d)\r\n", ret);
        return -1;
    }
    return 0;
}

int rtk_ptr_to_file(const char* filepath, unsigned int soffset, void* ptr, unsigned int len)
{
    int sfd;
    int ret;
    sfd = open(filepath, O_WRONLY );
    if(sfd<0)
    {
        install_debug("Can't open %s\r\n", filepath);
        return -1;
    }
    ret = lseek(sfd, soffset, SEEK_SET);
    if(ret < 0)
    {
        install_debug("lseek (%s) fail\r\n", filepath);
        close(sfd);
        return -1;
    }

    ret = write(sfd, ptr, len);
    install_debug("ret = %d\n", ret);
    close(sfd);

    if((unsigned int)ret != len)
    {
        install_debug("read fd fail(%d)\r\n", ret);
        return -1;
    }
    return 0;
}

int rtk_ptr_to_flash(void* ptr, unsigned int len, const char* devpath, unsigned int doffset)
{
    int dfd, ret;
    dfd = open(devpath, O_RDWR|O_SYNC);
    if(dfd<0)
    {
        install_debug("Can't open %s\r\n", devpath);
    }
    ret = lseek(dfd, doffset, SEEK_SET);
    if(ret < 0)
    {
        install_debug("lseek (%s) fail\r\n", devpath);
        close(dfd);
        return -1;
    }

    ret = write(dfd, ptr, len);
    close(dfd);
    if((unsigned int)ret != len)
    {
        install_debug("ret(%d)!=len(%u)\r\n", ret, len);
        return -1;
    }
    return 0;
}


int rtk_flash_to_ptr(const char* devpath, unsigned int soffset, void* ptr, unsigned int len)
{
    int dfd, ret;
    dfd = open(devpath, O_RDWR|O_SYNC);
    if(dfd<0)
    {
        install_debug("Can't open %s\r\n", devpath);
    }
    ret = lseek(dfd, soffset, SEEK_SET);
    if(ret < 0)
    {
        install_debug("lseek (%s) fail\r\n", devpath);
        close(dfd);
        return -1;
    }

    ret = read(dfd, ptr, len);
    close(dfd);
    if((unsigned int)ret != len)
    {
        install_debug("ret(%d)!=len(%u)\r\n", ret, len);
        return -1;
    }
    return 0;
}

/*------------------------------------------------------
//
//
//
------------------------------------------------------*/
int rtk_file_to_mtd(const char* filepath, unsigned int soffset, unsigned int imglen, const struct t_rtkimgdesc* prtkimgdesc, unsigned int doffset, unsigned int alignlen, unsigned int* checksum)
{
    int sfd, dfd;
    int ret;
    struct erase_info_user erase_u;
    const char *devpath = NULL;

    if((sfd = open(filepath, O_RDONLY)) < 0)
    {
        install_debug("Can't open %s\r\n", filepath);
        return -1;
    }

    //sanity-check
    if (prtkimgdesc->mtd_erasesize == 0)
    {
        install_fail("error! mtd_erasesize not init!\r\n");
        return -1;
    }

    //install_debug("doffset=%u(0x%x) alignlen=%u(0x%x) imglen=%u(0x%x).\r\n", doffset, doffset, alignlen, alignlen, imglen, imglen);

    //dst offset and dst length is align to erase size
    //so use ioctl to erase the area, then open mtd char and write to device
    if (((doffset&(prtkimgdesc->mtd_erasesize-1)) == 0) && ((alignlen&(prtkimgdesc->mtd_erasesize-1)) == 0))
    {
        if((dfd = open(prtkimgdesc->mtd_path, O_RDWR|O_SYNC)) < 0)
        {
            install_debug("Can't open %s\r\n", prtkimgdesc->mtd_path);
            return -1;
        }
        devpath = prtkimgdesc->mtd_path;

#ifndef __OFFLINE_GENERATE_BIN__

        erase_u.start = doffset;
        erase_u.length = alignlen;

        install_debug("erase start=0x%x, length=0x%x\r\nstart erasing", erase_u.start, erase_u.length);
        fflush(stdout);

        if (ioctl(dfd, MEMERASE, &erase_u))
        {
            install_fail("ioctl error!!!\r\n");
            close(dfd);
            return -1;
        }
        install_info("\rerase done                     ");
        fflush(stdout);
    }
    //dst offset and dst length is not align to erase size
    //so let mtd block device to do the work
    else
    {
        if((dfd = open(prtkimgdesc->mtdblock_path, O_RDWR|O_SYNC)) < 0)
        {
            install_fail("Can't open %s\r\n", prtkimgdesc->mtdblock_path);
            return -1;
        }
        devpath = prtkimgdesc->mtdblock_path;
#endif
    }
    install_info("\rstart programming            \r\n");

    if((ret = lseek(sfd, soffset, SEEK_SET)) < 0)
    {
        install_debug("lseek (%s) fail\r\n", filepath);
        close(sfd);
        close(dfd);
        return -1;
    }

    if((ret = lseek(dfd, doffset, SEEK_SET)) < 0)
    {
        install_debug("lseek (%s) fail\r\n", devpath);
        close(sfd);
        close(dfd);
        return -1;
    }

    ret = fd_to_fd(sfd, dfd, imglen, checksum, NULL);
    close(sfd);
    close(dfd);

    if((unsigned int)ret != imglen)
    {
        install_debug("fd_to_fd fail(%d)\r\n", ret);
        return -1;
    }

    return ret;
}

int rtk_unlock_mtd(struct t_rtkimgdesc* prtkimgdesc, unsigned int unlock_start, unsigned int unlock_length)
{
    int dfd;
    struct erase_info_user unlock_u;

    unlock_u.start = unlock_start;
    unlock_u.length = unlock_length;

    if((dfd = open(prtkimgdesc->mtd_path, O_RDWR|O_SYNC)) < 0)
    {
        install_log("Can't open %s\r\n", prtkimgdesc->mtd_path);
        return -1;
    }

    if (ioctl(dfd, MEMUNLOCK, &unlock_u))
    {
        install_log("ioctl error!!!\r\n");
        close(dfd);
        return -1;
    }
    close(dfd);

    return 0;
}

int rtk_erase_mtd(struct t_rtkimgdesc* prtkimgdesc, unsigned int erase_start, unsigned int erase_length)
{
    int dfd;
    int ret;
    char cmd[128] = {0};
    struct erase_info_user erase_u;

    //sanity-check
    if (prtkimgdesc->mtd_erasesize == 0)
    {
        install_fail("error! mtd_erasesize not init!\r\n");
        return -1;
    }

    if (((erase_start&(prtkimgdesc->mtd_erasesize-1)) == 0) && ((erase_length&(prtkimgdesc->mtd_erasesize-1)) == 0))
    {
        if(prtkimgdesc->flash_type == MTD_NANDFLASH)
        {
            // Extract utility
            if(rtk_extract_utility(prtkimgdesc) < 0)
            {
                install_debug("rtk_extract_utility fail\r\n");
                return -1;
            }
            // flash_erase
            snprintf(cmd, sizeof(cmd), "%s %s %u %u", FLASHERASE_BIN, prtkimgdesc->mtd_path, erase_start, erase_length / prtkimgdesc->mtd_erasesize);
            if((ret = rtk_command(cmd, __LINE__, __FILE__)) < 0)
            {
                install_debug("Exec command fail\r\n");
                return -1;
            }
        }
#ifdef EMMC_SUPPORT
        else if(prtkimgdesc->flash_type == MTD_EMMC)
        {
            //TODO
        }
#endif
        else
        {
            if((dfd = open(prtkimgdesc->mtd_path, O_RDWR|O_SYNC)) < 0)
            {
                install_fail("Can't open %s\r\n", prtkimgdesc->mtd_path);
                return -1;
            }

            erase_u.start = erase_start;
            erase_u.length = erase_length;

            install_debug("erase start=0x%x, length=0x%x\r\nstart erasing", erase_u.start, erase_u.length);
            fflush(stdout);


            if (ioctl(dfd, MEMERASE, &erase_u))
            {
                install_fail("ioctl error!!!\r\n");
                close(dfd);
                return -1;
            }
            close(dfd);
            install_info("\rerase done                     ");
            fflush(stdout);
        }
    }
    else
    {
        install_fail("\r\nerase fail!\r\nerase start=0x%x, length=0x%x\r not alligned to 0x%x", erase_start, erase_length, prtkimgdesc->mtd_erasesize);
        return -1;
    }

    return 0;
}

int rtk_file_verify(const char* sfilepath, unsigned long long soffset, const char* dfilepath, unsigned long long doffset, unsigned int imglen, unsigned int *err_count, unsigned int *checksum)
{
    char sbuffer[READ_BUFFER] = {0};
    char dbuffer[READ_BUFFER] = {0};

    int vtempsread, vtempdread, error_count, i;
    unsigned int vslength, read_buffer, schecksum, dchecksum;
    FILE *vsfp, *vdfp;
    unsigned int _1GB = 1*1024*1024*1024;

    vsfp = fopen(sfilepath, "rb");
    vdfp = fopen(dfilepath, "rb");

    error_count = i = 0;
    schecksum = dchecksum = 0;

    if (vsfp == NULL)
    {
        install_fail("cannot open %s!\r\n", sfilepath);
        return -1;
    }

    if (vdfp == NULL)
    {
        install_fail("cannot open %s!\r\n", dfilepath);
        fclose(vsfp);
        return -1;
    }

    if (soffset > _1GB)
    {
        int count = ((soffset+_1GB)-1)/_1GB;
        unsigned int offset = _1GB;
        install_info("[Installer_D]: count=%d\n", count);
        for (i = 0; i < count; i++)
        {
            install_info("[Installer_D]: offset%d = %u\r\n", i, offset);
            if (i == 0)
            {
                if (fseek(vsfp, offset, SEEK_SET) < 0)
                {
                    install_fail("%s seek loop error(offset = %u)!\r\n", sfilepath, offset);
                    fclose(vsfp);
                    fclose(vdfp);
                    return -1;
                }
            }
            else
            {
                if (fseek(vsfp, offset, SEEK_CUR) < 0)
                {
                    install_fail("%s seek loop(SEEK_CUR) error(offset = %u)!\r\n", sfilepath, offset);
                    fclose(vsfp);
                    fclose(vdfp);
                    return -1;
                }
            }

            if ((i+2) >= (count))
            {
                offset = soffset%_1GB;
                if (offset == 0)
                    return 0;
            }
        }
    }
    else
    {
        if (fseek(vsfp, soffset, SEEK_SET) < 0)
        {
            install_fail("%s seek error!\r\n", sfilepath);
            fclose(vsfp);
            fclose(vdfp);
            return -1;
        }
    }
    if (fseek(vdfp, doffset, SEEK_SET) < 0)
    {
        install_fail("%s seek error!\r\n", dfilepath);
        fclose(vsfp);
        fclose(vdfp);
        return -1;
    }

    vslength = imglen;

    install_info("Verifying\r\n");

    do
    {
        read_buffer = (vslength > READ_BUFFER)? READ_BUFFER:vslength;

        vtempsread = fread(sbuffer, 1, read_buffer, vsfp);
        vtempdread = fread(dbuffer, 1, read_buffer, vdfp);

        schecksum += get_checksum((u8 *)sbuffer, (u32) vtempsread);
        dchecksum += get_checksum((u8 *)dbuffer, (u32) vtempdread);

        error_count += memcmp_count(sbuffer, dbuffer, vtempsread);

        vslength -= vtempsread;

        if ((i++/50) == 1 || vslength == 0)
        {
            progressbar(imglen-vslength, imglen, NULL);
            i = 0;
        }
        fflush(stdout);

    }
    while (!feof(vsfp) && !feof(vdfp) && vslength > 0);

    fclose(vsfp);
    fclose(vdfp);

    *err_count = error_count;

    if (error_count > 0)
    {
        install_fail("Fail! total %u bytes checked, %d bytes error\r\n", imglen, error_count);
    }
    else
    {
        install_info("Pass! total %u bytes checked\r\n", imglen);
    }

#ifndef PC_SIMULATE
    if (schecksum != dchecksum)
    {
        install_fail("Checksum: src=0x%08X dst=0x%08X\r\n", schecksum, dchecksum);
        return -1;
    }
#endif

    if( checksum )
    {
        *checksum = dchecksum;
        install_info("checksum(%#x)\n", *checksum);
    }

    return 0;
}

int rtk_file_checksum(const char* dfilepath, unsigned int doffset, unsigned int imglen, unsigned char *checksum)
{
    int i = 0;
    FILE *vdfp;

#ifndef  PC_SIMULATE
    vdfp = fopen(dfilepath, "rb");

    if (vdfp == NULL)
    {
        install_fail("cannot open %s!\r\n", dfilepath);
        return -1;
    }

    if (fseek(vdfp, doffset, SEEK_SET) < 0)
    {
        install_fail("%s seek error!\r\n", dfilepath);
        fclose(vdfp);
        return -1;
    }

    install_info("Verifying\r\n");

#ifdef NAS_ENABLE
    struct kcapi_handle *handle = NULL;
    int ret = 0;
    void *data = NULL;

    data = malloc(imglen);

    if(kcapi_md_init(&handle, "sha256", 0))
    {
        install_fail("Failed to allocate sha256 kernel crypto\r\n");
        ret = -1;
        goto out;
    }

    if(!data)
    {
        install_fail("Failed to allocate memory for data\r\n");
        ret = -1;
        goto out;
    }

    fread(data, 1, imglen, vdfp);
    /* Compute hash */
    ret = kcapi_md_digest(handle, (uint8_t*)data, imglen, checksum,
                          SHA256_SIZE);

out:
    if(data) free(data);
    if(handle) kcapi_md_destroy(handle);
    fclose(vdfp);
    if(ret >= 0){
    install_info("SHA256 checksum generated by kcapi");
        return ret;
    }
    /* Continue to use MCP for SHA256 hash after kcapi failure */
#endif
    unsigned char*  pBuff_src = NULL;
    unsigned char*  pBuffVirt_src = NULL;
    unsigned long   PhyAddr_src;
    ion_user_handle_t ionhdl_headr_src;

    unsigned char*  pBuff_dst = NULL;
    unsigned char*  pBuffVirt_dst = NULL;
    unsigned long   PhyAddr_dst;
    ion_user_handle_t ionhdl_headr_dst;

    pBuff_src = (unsigned char *)RTKIon_alloc(imglen, &pBuffVirt_src, &PhyAddr_src, &ionhdl_headr_src);
    pBuff_dst = (unsigned char *)RTKIon_alloc(SHA256_SIZE, &pBuffVirt_dst, &PhyAddr_dst, &ionhdl_headr_dst);

    int len = imglen;
    int vtempdread = 0;
    //printf("[Installer_D]: sha256 data len=%d.\n", len);
    vtempdread = fread(pBuffVirt_src, 1, len, vdfp);
    //printf("[Installer_D]: sha256 data vtempdread=%d.\n", vtempdread);
    SHA256_hash(PhyAddr_src, imglen, PhyAddr_dst, NULL);

    fclose(vdfp);
    
    //checksum = (unsigned int*)pBuffVirt_dst;
    
    install_info("SHA256 checksum_ion==>");
    for (i=0; i<32; i++){
        printf("%02x ", *(pBuffVirt_dst+i));        
    }
    printf("\n");
    //*checksum = get_checksum(pBuffVirt_dst, SHA256_SIZE);
    memcpy(checksum, pBuffVirt_dst, SHA256_SIZE);
    //install_info("[Installer_D]: get_checksum(%#x)\n", *checksum);     
    if(pBuff_src || pBuff_dst) {
        printf("Release the memory\n");
        RTKIon_free(pBuff_src, imglen, &ionhdl_headr_src);
        RTKIon_free(pBuff_dst, SHA256_SIZE, &ionhdl_headr_dst);
    }

    install_info("SHA256 checksum_cpy==>");
    for (i=0; i<32; i++){
        printf("%02x ", *(checksum+i));        
    }
    printf("\n");
#endif

    return 0;
}

#define DO_SHA256_UTILITY "../bin/do_sha256"
#define SHA256_SIG "./tmp/sha256_sig"
#define SHA256_PAD "./tmp/sha256_pad"
int rtk_file_checksum_pc(const char* dfilepath, const char* imgpath, unsigned int imglen, unsigned char *checksum)
{
   char dbuffer[SHA256_SIZE+4] = {0};
   char cmd[256] = {0};
   //char doSHA256Path[128] = {0};

   int vtempdread, i;
   unsigned int vslength, read_buffer;
   FILE *vdfp;

   i = 0;

   sprintf(cmd, "tar -xf %s %s -C ./tmp; %s %s %s %s", imgpath, dfilepath, DO_SHA256_UTILITY, dfilepath, SHA256_PAD, SHA256_SIG);
   //install_info("[Installer_D]: cmd = (%s).\n", cmd);
   rtk_command(cmd, __LINE__, __FILE__, 0);

   vdfp = fopen(SHA256_SIG, "rb");

   if (vdfp == NULL) {
      install_fail("cannot open %s!\r\n", dfilepath);
      return -1;
   }

   if (fseek(vdfp, 0, SEEK_SET) < 0) {
      install_fail("%s seek error!\r\n", dfilepath);
      fclose(vdfp);
      return -1;
   }

   vslength = SHA256_SIZE;

   install_info("Verifying\r\n");

   do {
      read_buffer = (vslength > READ_BUFFER)? READ_BUFFER:vslength;

      vtempdread = fread(dbuffer, 1, read_buffer, vdfp);

      //dchecksum += get_checksum((u8 *)dbuffer, (u32) vtempdread);

      vslength -= vtempdread;

      if ((i++/50) == 1 || vslength == 0)
      {
         progressbar(imglen-vslength, imglen, NULL);
         i = 0;
      }
      fflush(stdout);

   } while (!feof(vdfp) && vslength > 0);

   fclose(vdfp);

   memcpy(checksum, dbuffer, SHA256_SIZE);
#ifdef DEBUG_CHECK
   for (i=0; i<8; i++){
        install_info("[Installer_D]: SHA256 checksum%d(%#x)\n", i,*(checksum+i));        
   }
#endif
   return 0;
}

int rtk_ptr_verify(const char* sfilepath, unsigned int soffset, const char* dmemory, unsigned int doffset, unsigned int imglen, unsigned int *err_count)
{
    char sbuffer[READ_BUFFER] = {0};

    int vtempsread, error_count, i;
    unsigned int vslength, read_buffer, schecksum, dchecksum;
    FILE *vsfp;

    vsfp = fopen(sfilepath, "rb");

    error_count = i = 0;
    schecksum = dchecksum = 0;

    if (vsfp == NULL)
    {
        install_fail("cannot open %s!\r\n", sfilepath);
        return -1;
    }

    if (fseek(vsfp, soffset, SEEK_SET) < 0)
    {
        install_fail("%s seek error!\r\n", sfilepath);
        fclose(vsfp);
        return -1;
    }

    dmemory += doffset;

    vslength = imglen;

    install_info("[Verifying]\r\n");

    do
    {
        read_buffer = (vslength > READ_BUFFER)? READ_BUFFER:vslength;

        vtempsread = fread(sbuffer, 1, read_buffer, vsfp);

        schecksum += get_checksum((u8 *)sbuffer, (u32) vtempsread);
        dchecksum += get_checksum((u8 *)dmemory, (u32) vtempsread);

        error_count += memcmp_count(sbuffer, dmemory, vtempsread);

        dmemory += vtempsread;

        vslength-=vtempsread;

        if ((i++/50) == 1 || vslength == 0)
        {
            progressbar(imglen-vslength, imglen, NULL);
            i = 0;
        }
        fflush(stdout);

    }
    while (!feof(vsfp) && vslength > 0);

    fclose(vsfp);

    *err_count = error_count;

    if (error_count > 0)
    {
        install_fail("Fail! total %u bytes checked, %d bytes error\r\n", imglen, error_count);
    }
    else
    {
        install_info("Pass! total %u bytes checked\r\n", imglen);
    }

    if (schecksum != dchecksum)
    {
        install_fail("Checksum: src=0x%08X dst=0x%08X\r\n", schecksum, dchecksum);
        return -1;
    }

    return 0;
}

#ifdef DEBUG_CHECK
static int is_dir(const char *pfilepath)
{
    struct stat c_fstat;
    int vst, vin;

    vin = open(pfilepath, O_RDONLY);
    vst = fstat(vin, &c_fstat);
    close(vin);

    if (S_ISDIR(c_fstat.st_mode))
        return 1;
    else
        return 0;
}

static int is_link(const char *pfilepath)
{
    struct stat c_fstat;
    int vst, vin;

    vin = open(pfilepath, O_RDONLY);
    vst = fstat(vin, &c_fstat);
    close(vin);

    if (S_ISLNK(c_fstat.st_mode))
        return 1;
    else
        return 0;
}
#endif	//DEBUG_FUNC

int rtk_find_file_in_dir(const char *pdirpath, const char *pkeyword, char *filename, const int filename_maxlen)
{
    struct dirent *ventry = NULL;
    int found;

    DIR *dh = opendir(pdirpath);

    install_debug("[%s]\r\n", __func__);
    found = 0;
    while ((ventry = readdir(dh)) != NULL)
    {
        install_debug("%s\r\n", ventry->d_name);
        if (!strncmp(ventry->d_name, pkeyword, strlen(pkeyword)))
        {
            install_log("find %s in %s\r\n",ventry->d_name, pdirpath);
            snprintf(filename, filename_maxlen, "%s", ventry->d_name);
            found = 1;
        }
    }
    closedir(dh);

    if (found == 0)
    {
        install_log("error! file %s not found in %s\r\n", pkeyword, pdirpath);
        filename[0] = '\0';
        return -1;
    }
    return 0;
}

// given path= "abc/def"
// dirpath will get "abc/"
int rtk_find_dir_path(char *path, char *dirpath, int len)
{
    char *ptr = strrchr(path, '/');

    memset(dirpath, 0, len);
    if( ptr )
    {
        memcpy( dirpath, path, ptr-path+1 );
    }
    else
    {
        strcpy( dirpath, "./");
    }

    return 0;
}

#define DEV_SDA1_PATH "/dev/sda1"
#define DEV_SDB1_PATH "/dev/sdb1"
#define DEV_SDC1_PATH "/dev/sdc1"
#define DEV_SDD1_PATH "/dev/sdd1"

#define DUMP_USB_SDA1_PATH "/tmp/usbmounts/sda1"
#define DUMP_USB_SDB1_PATH "/tmp/usbmounts/sdb1"
#define DUMP_USB_SDC1_PATH "/tmp/usbmounts/sdc1"
#define DUMP_USB_SDD1_PATH "/tmp/usbmounts/sdd1"

#define DUMP_DIR_PREFIX "dump_%03d"

int rtk_dump_flash(struct t_rtkimgdesc* prtkimgdesc)
{
    char cmd[256] = {0}, dev_path[64] = {0}, usb_path[64] = {0}, dump_path[128] = {0};
    int ret, dump_idx, etc_index;
    S_BOOTTABLE boottable, *pboottable = NULL;

    //find usb mount point
    if (!access(DUMP_USB_SDA1_PATH, F_OK))
    {
        snprintf(dev_path, sizeof(dev_path), "%s", DEV_SDA1_PATH);
        snprintf(usb_path, sizeof(usb_path), "%s", DUMP_USB_SDA1_PATH);
    }
    else if (!access(DUMP_USB_SDB1_PATH, F_OK))
    {
        snprintf(dev_path, sizeof(dev_path), "%s", DEV_SDB1_PATH);
        snprintf(usb_path, sizeof(usb_path), "%s", DUMP_USB_SDB1_PATH);
    }
    else if (!access(DUMP_USB_SDC1_PATH, F_OK))
    {
        snprintf(dev_path, sizeof(dev_path), "%s", DEV_SDC1_PATH);
        snprintf(usb_path, sizeof(usb_path), "%s", DUMP_USB_SDC1_PATH);
    }
    else if (!access(DUMP_USB_SDD1_PATH, F_OK))
    {
        snprintf(dev_path, sizeof(dev_path), "%s", DEV_SDD1_PATH);
        snprintf(usb_path, sizeof(usb_path), "%s", DUMP_USB_SDD1_PATH);
    }

    //search for next dump index
    dump_idx = 0;
    do
    {
        snprintf(dump_path, sizeof(dump_path), "%s/"DUMP_DIR_PREFIX, usb_path, dump_idx);
        dump_idx++;
        if (dump_idx == 1000)
        {
            install_log("dump index too large %d\r\n", dump_idx);
            return -1;
        }
    }
    while(!access(dump_path, F_OK));

    install_log("Dump flash start\r\n");

    //start to dump
    etc_index = -1;
    if(prtkimgdesc->flash_type == MTD_NANDFLASH)
    {
        install_log("NAND FLASH does not support dump flash option\r\n");
    }
    else
    {
        memset(&boottable, 0, sizeof(boottable));
        pboottable = read_boottable(&boottable, prtkimgdesc);

        //remount usb as rw
        snprintf(cmd, sizeof(cmd), "mount -o remount,rw %s %s", dev_path, usb_path);
        if ((ret = rtk_command(cmd, __LINE__, __FILE__, 1)) < 0)
        {
            return -1;
        }

        //mkdir for dump
        snprintf(cmd, sizeof(cmd), "mkdir -p %s", dump_path);
        if ((ret = rtk_command(cmd, __LINE__, __FILE__, 1)) < 0)
        {
            return -1;
        }

        if (pboottable)
        {
            //dump etc section
            install_log("\r\n[Dump etc section]\r\n");
            if ((etc_index = get_index_by_partname(pboottable, "etc")) < 0)
            {
                install_log("cannot find etc partition in boottable, skip dump etc\r\n");
            }
            else
            {
                snprintf(cmd, sizeof(cmd), "dd if=%s of=/tmp/jffs2.bin skip=%llu bs=%d count=%llu"
                         , prtkimgdesc->mtdblock_path
                         , pboottable->part.list[etc_index].loc.offset/prtkimgdesc->mtd_erasesize
                         , prtkimgdesc->mtd_erasesize
                         , pboottable->part.list[etc_index].loc.size/prtkimgdesc->mtd_erasesize);
                if ((ret = rtk_command(cmd, __LINE__, __FILE__, 1)) < 0)
                {
                    return -1;
                }
                snprintf(cmd, sizeof(cmd), "cp /tmp/jffs2.bin %s", dump_path);
                if ((ret = rtk_command(cmd, __LINE__, __FILE__, 1)) < 0)
                {
                    return -1;
                }
                if ((ret = rtk_command("sync", __LINE__, __FILE__, 1)) < 0)
                {
                    return -1;
                }
            }
        }
        else
        {
            install_log("cannot find boottable, skip dump etc\r\n");
        }

        //dump factory section
        install_log("\r\n[Dump factory section]\r\n");
        snprintf(cmd, sizeof(cmd), "dd if=%s of=/tmp/factory.bin skip=%d bs=%d count=%d"
                 , prtkimgdesc->mtdblock_path
                 , prtkimgdesc->factory_start/prtkimgdesc->mtd_erasesize
                 , prtkimgdesc->mtd_erasesize
                 , prtkimgdesc->factory_size*2/prtkimgdesc->mtd_erasesize);
        if ((ret = rtk_command(cmd, __LINE__, __FILE__, 1)) < 0)
        {
            return -1;
        }
        snprintf(cmd, sizeof(cmd), "cp /tmp/factory.bin %s", dump_path);
        if ((ret = rtk_command(cmd, __LINE__, __FILE__, 1)) < 0)
        {
            return -1;
        }
        if ((ret = rtk_command("sync", __LINE__, __FILE__, 1)) < 0)
        {
            return -1;
        }

        //dump all image
        install_log("\r\n[Dump all image]\r\n");
        snprintf(cmd, sizeof(cmd), "dd if=%s of=/tmp/data.bin skip=%d bs=%d count=%lld"
                 , prtkimgdesc->mtdblock_path
                 , 0
                 , prtkimgdesc->mtd_erasesize
                 , prtkimgdesc->flash_size/prtkimgdesc->mtd_erasesize);
        if ((ret = rtk_command(cmd, __LINE__, __FILE__, 1)) < 0)
        {
            return -1;
        }
        snprintf(cmd, sizeof(cmd), "cp /tmp/data.bin %s", dump_path);
        if ((ret = rtk_command(cmd, __LINE__, __FILE__, 1)) < 0)
        {
            return -1;
        }
        if ((ret = rtk_command("sync", __LINE__, __FILE__, 1)) < 0)
        {
            return -1;
        }

        //dump layout.txt
        install_log("\r\n[Dump layout.txt]\r\n");
        if (!access("/tmp/factory/layout.txt", F_OK))
        {
            snprintf(cmd, sizeof(cmd), "cp /tmp/factory/layout.txt %s", dump_path);
            if ((ret = rtk_command(cmd, __LINE__, __FILE__, 1)) < 0)
            {
                return -1;
            }
            if ((ret = rtk_command("sync", __LINE__, __FILE__, 1)) < 0)
            {
                return -1;
            }
        }
        else
        {
            install_log("layout.txt not found, skip dump layout\r\n");
        }
    }

    install_log("Dump flash finish\r\n");

    install_log(VT100_LIGHT_GREEN"\r\n\r\nDump path: \"%s\"\r\n\r\n"VT100_NONE, dump_path);

    return 0;
}

void rtk_install_debug_printf(u32 debugLevel, const char* filename, const char* funcname, u32 fileline, const char* fmt, ...)
{
    va_list arglist;

    if (debugLevel& gDebugPrintfLogLevel)
    {
        switch(debugLevel)
        {
        case INSTALL_DEBUG_LEVEL:
            printf("[DEBUG][%s:%s():%d]", filename, funcname, fileline);
            break;
        case INSTALL_INFO_LEVEL:
            printf("[INFO][%s:%s():%d]", filename, funcname, fileline);
            break;
        case INSTALL_FAIL_LEVEL:
            printf("[FAIL][%s:%s():%d]", filename, funcname, fileline);
            break;
        case INSTALL_LOG_LEVEL:
            printf("[LOG][%s:%s():%d]", filename, funcname, fileline);
            break;
        case INSTALL_WARNING_LEVEL:
            printf("[WARN][%s:%s():%d]", filename, funcname, fileline);
            break;
        case INSTALL_UI_LEVEL:
            va_start( arglist, fmt );
            vprintf( fmt, arglist);
            va_end( arglist );
            return;
        default:
            printf("[%s:%s():%d]", filename, funcname, fileline);
            break;
        }
        va_start( arglist, fmt );
        vprintf( fmt, arglist);
        printf("\n");
        va_end( arglist );
    }
}


#ifdef __OFFLINE_GENERATE_BIN__

unsigned char R7_6b[78], R6_6b[78], R5_6b[78], R4_6b[78], R3_6b[78], R2_6b[78], R1_6b[78], R0_6b[78];
unsigned char R_6b[78] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
unsigned short synd[6] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
const unsigned char gen_poly_6b[79] = {1,0,1,1,1,1,1,0,1,0,0,0,1,1,0,1,1,0,0,1,1,1,0,1,0,0,1,1,1,0,1,1,0,0,0,0,1,1,1,1,0,0,1,0,0,1,1,1,0,0,0,0,1,1,0,0,1,0,0,1,0,0,1,1,0,0,1,1,1,1,0,0,1,1,1,1,1,1,1};
//unsigned char dummy[78];

unsigned char inline ToBit(unsigned short value, int bth)
{
    value = (value >> bth) & 0x1;
    return (unsigned char)value;
}

void encode_ecc_byte(unsigned char input)
{
    int i,j;

    for (i=0; i<78; i++)
    {
        if (i == 0)
            R7_6b[0] = 0x0 ^ (gen_poly_6b[0] & (ToBit(input, 7) ^ R_6b[77]));
        else
            R7_6b[i] = R_6b[i-1] ^ (gen_poly_6b[i] & (ToBit(input, 7) ^ R_6b[77]));
    }

    for (i=0; i<78; i++)
    {
        if (i == 0)
            R6_6b[0] = 0x0 ^ (gen_poly_6b[0] & (ToBit(input, 6) ^ R7_6b[77]));
        else
            R6_6b[i] = R7_6b[i-1] ^ (gen_poly_6b[i] & (ToBit(input, 6) ^ R7_6b[77]));
    }

    for (i=0; i<78; i++)
    {
        if (i == 0)
            R5_6b[0] = 0x0 ^ (gen_poly_6b[0] & (ToBit(input, 5) ^ R6_6b[77]));
        else
            R5_6b[i] = R6_6b[i-1] ^ (gen_poly_6b[i] & (ToBit(input, 5) ^ R6_6b[77]));
    }

    for (i=0; i<78; i++)
    {
        if (i == 0)
            R4_6b[0] = 0x0 ^ (gen_poly_6b[0] & (ToBit(input, 4) ^ R5_6b[77]));
        else
            R4_6b[i] = R5_6b[i-1] ^ (gen_poly_6b[i] & (ToBit(input, 4) ^ R5_6b[77]));
    }

    for (i=0; i<78; i++)
    {
        if (i == 0)
            R3_6b[0] = 0x0 ^ (gen_poly_6b[0] & (ToBit(input, 3) ^ R4_6b[77]));
        else
            R3_6b[i] = R4_6b[i-1] ^ (gen_poly_6b[i] & (ToBit(input, 3) ^ R4_6b[77]));
    }

    for (i=0; i<78; i++)
    {
        if (i == 0)
            R2_6b[0] = 0x0 ^ (gen_poly_6b[0] & (ToBit(input, 2) ^ R3_6b[77]));
        else
            R2_6b[i] = R3_6b[i-1] ^ (gen_poly_6b[i] & (ToBit(input, 2) ^ R3_6b[77]));
    }

    for (i=0; i<78; i++)
    {
        if (i == 0)
            R1_6b[0] = 0x0 ^ (gen_poly_6b[0] & (ToBit(input, 1) ^ R2_6b[77]));
        else
            R1_6b[i] = R2_6b[i-1] ^ (gen_poly_6b[i] & (ToBit(input, 1) ^ R2_6b[77]));
    }

    for (i=0; i<78; i++)
    {
        if (i == 0)
            R0_6b[0] = 0x0 ^ (gen_poly_6b[0] & (ToBit(input, 0) ^ R1_6b[77]));
        else
            R0_6b[i] = R1_6b[i-1] ^ (gen_poly_6b[i] & (ToBit(input, 0) ^ R1_6b[77]));
    }

    //dumpR();

    memset(synd, 0, sizeof(synd));

    for (i=0; i<6; i++)
        for (j=12; j>=0; j--)
            synd[i] |= (R0_6b[j+(i*13)] << j);

    for (i=0; i<6; i++)
        for (j=0; j<13; j++)
            R_6b[(i*13+j)] = ToBit(synd[i], j);

    //dumpSynd();
}

int gen_10bytes_ecc(unsigned char *data_buf, unsigned char *ecc)
{
    //unsigned char ecc[10] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,};
    int i, j;

    memset(R0_6b, 0, sizeof(R0_6b));
    memset(R1_6b, 0, sizeof(R1_6b));
    memset(R2_6b, 0, sizeof(R2_6b));
    memset(R3_6b, 0, sizeof(R3_6b));
    memset(R4_6b, 0, sizeof(R4_6b));
    memset(R5_6b, 0, sizeof(R5_6b));
    memset(R6_6b, 0, sizeof(R6_6b));
    memset(R7_6b, 0, sizeof(R7_6b));

    memset(R_6b, 0, sizeof(R_6b));
    memset(synd, 0, sizeof(synd));

    for ( i=0; i< 518; i++)
        encode_ecc_byte(data_buf[i]);

    for (i=0; i<9; i++)
        for (j=7; j>=0; j--)
            ecc[i] |= (R_6b[j+((8-i)*8+6)] << j);

    for (j=5; j>=0; j--)
        ecc[9] |= (R_6b[j] << (j+2));

    //printf("ecc0=%x, ecc1=%x, ecc2=%x, ecc3=%x, ecc4=%x, ecc5=%x, ecc6=%x, ecc7=%x, ecc8=%x, ecc9=%x\n"
    //        , ecc[0], ecc[1], ecc[2], ecc[3], ecc[4], ecc[5], ecc[6], ecc[7], ecc[8], ecc[9]);
    //Answer: ecc0=5e, ecc1=96, ecc2=cb, ecc3=cb, ecc4=8, ecc5=8, ecc6=3, ecc7=24, ecc8=70, ecc9=c8

    return 0;
}


int fd_to_fd_virt_nand(int sfd, int dfd, unsigned int length, unsigned int* pchecksum, unsigned char block_indicator, const unsigned int rtk_unit_page_count_per_nand_page, unsigned int mode)
{
    int i, j, ret, rret, wret;
    unsigned int rlen;
    unsigned int len;
    unsigned int progress_len, interval;
    unsigned char data_buf[RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE];
    unsigned char tag_lsb_no, tag_msb_no;
    unsigned int rtk_unit_page_count;
    unsigned char *rpdata = NULL;

    time_t stime, etime;
    int restart;
    time_t start_time, end_time;

    stime = time(NULL);
    interval = length/10;
    progress_len = 0;
    len = 0;
    if(pchecksum != NULL)
        *pchecksum = 0;

    restart = 1;
    progressbar(0, length, NULL);
    fflush(stdout);

    rtk_unit_page_count = 0;
    tag_lsb_no = tag_msb_no = 0;

    if (rtk_unit_page_count_per_nand_page == 0)
    {
        install_fail("error! rtk_unit_page_count_per_nand_page = %d\r\n", rtk_unit_page_count_per_nand_page);
    }

    rpdata = (unsigned char *)malloc(RTK_UNIT_PAGE_SIZE*rtk_unit_page_count_per_nand_page);
    if (rpdata == NULL)
    {
        install_debug("error! read buffer is NULL\r\n");
        goto dd_virt_nand_end;
    }

    while(len < length)
    {
        rlen = ((length - len) < RTK_UNIT_PAGE_SIZE*rtk_unit_page_count_per_nand_page) \
               ? (length - len): RTK_UNIT_PAGE_SIZE*rtk_unit_page_count_per_nand_page;
        ret = isready(sfd);
        // select error
        if(ret < 0)
        {
            install_debug("select error\r\n");
            goto dd_virt_nand_end;
        }
        if(ret == 0)
        {
            if(restart == 1)
            {
                install_debug("first timeout\r\n");
                start_time = time(NULL);
                restart = 0;
            }
            else
            {
                install_debug("second timeout\r\n");
                end_time = time(NULL);
                if(difftime(end_time, start_time) > TIME_OUT)
                {
                    goto dd_virt_nand_end;
                }
            }
            continue;
        }

        restart = 1;

        memset(rpdata, 0xff, RTK_UNIT_PAGE_SIZE*rtk_unit_page_count_per_nand_page);

        if((rret = read(sfd, (void*) rpdata, rlen)) < 0)
        {
            install_debug("read source fd fail\r\n");
            goto dd_virt_nand_end;
        }
        else if(rret == 0)
        {
            install_debug("read source fd close\r\n");
            goto dd_virt_nand_end;
        }

        for (i = 0; i < rtk_unit_page_count_per_nand_page; i++)
        {

            memcpy(&data_buf[0], &rpdata[RTK_UNIT_PAGE_SIZE*i], RTK_UNIT_PAGE_SIZE);
            memset(&data_buf[512], 0x0, RTK_UNIT_OOB_SIZE);

            switch (mode)
            {
            case 3:
                data_buf[512] = block_indicator;
                data_buf[513] = 0xff;
                data_buf[514] = 0xff;
                data_buf[515] = 0xff;
                data_buf[516] = 0xff;
                data_buf[517] = 0xff;
                break;
            case 2:
                tag_lsb_no = rtk_unit_page_count/rtk_unit_page_count_per_nand_page%256;
                tag_msb_no = rtk_unit_page_count/rtk_unit_page_count_per_nand_page/256;
                data_buf[512] = block_indicator;
                data_buf[513] = 0x0;
                data_buf[514] = tag_lsb_no;
                data_buf[515] = tag_msb_no;
                data_buf[516] = 0xff;
                data_buf[517] = 0xff;
                break;
            case 1:
                data_buf[512] = block_indicator;
                data_buf[513] = 0x0;
                data_buf[514] = 0x0;
                data_buf[515] = 0x0;
                data_buf[516] = 0xff;
                data_buf[517] = 0xff;
                break;
            case 0:
                if (rtk_unit_page_count%rtk_unit_page_count_per_nand_page == 0 )
                {
                    data_buf[512] = block_indicator;
                    data_buf[513] = 0xff;
                    data_buf[514] = 0xff;
                    data_buf[515] = 0xff;
                    data_buf[516] = 0xff;
                    data_buf[517] = 0xff;
                }
                break;
            default:
                install_fail("error! undefined mode=%d\r\n", mode);
                return -1;
                break;
            }

            gen_10bytes_ecc(&data_buf[0], &data_buf[518]);

            if((wret = write(dfd, (void*) data_buf, RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE)) < 0)
            {
                install_debug("write fail\r\n");
                goto dd_virt_nand_end;
            }
            else if(wret != RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE)
            {
                printf("error, write size %d != %d\r\n", wret, RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE);
                goto dd_virt_nand_end;
            }

            rtk_unit_page_count++;
        }

        len = len + rret;
        if(len >= progress_len)
        {
            progressbar(len, length, NULL);
            fflush(stdout);
            progress_len = progress_len + interval;
        }
    }

dd_virt_nand_end:
    free(rpdata);
    etime = time(NULL);
    if(pchecksum != NULL)
    {
        if (*pchecksum != 0)
            install_debug("checksum:0x%08X\r\n\r\n", *pchecksum);
    }

    //install_debug("\r\ntotal_time:%.0lf seconds\r\n", difftime(etime,stime));
    return len;
}

int rtk_file_to_virt_nand_with_ecc(const char* filepath, unsigned int soffset, unsigned int imglen, const struct t_rtkimgdesc* prtkimgdesc, unsigned int doffset, unsigned char block_indicator, unsigned int* checksum, unsigned int mode)
{
    int sfd, dfd;
    int ret;
    int v_nand_doffset;

    if((sfd = open(filepath, O_RDONLY)) < 0)
    {
        install_debug("Can't open %s\r\n", filepath);
        return -1;
    }

    //sanity-check
    if (prtkimgdesc->mtd_erasesize == 0 || prtkimgdesc->page_size == 0 || prtkimgdesc->oob_size == 0)
    {
        install_fail("error! mtd_erasesize or page_size or oob_size not init!\r\n");
        return -1;
    }
    if (doffset&(RTK_UNIT_PAGE_SIZE-1))
    {
        install_fail("error! doffset not in alligned!\r\n");
        return -1;
    }

    if((dfd = open(prtkimgdesc->mtdblock_path, O_RDWR|O_SYNC)) < 0)
    {
        install_fail("Can't open %s\r\n", prtkimgdesc->mtdblock_path);
        return -1;
    }

    if((ret = lseek(sfd, soffset, SEEK_SET)) < 0)
    {
        install_debug("lseek (%s) fail\r\n", filepath);
        close(sfd);
        close(dfd);
        return -1;
    }

    v_nand_doffset = doffset/RTK_UNIT_PAGE_SIZE*(RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE);

    if((ret = lseek(dfd, v_nand_doffset, SEEK_SET)) < 0)
    {
        install_debug("lseek (%s) fail\r\n", prtkimgdesc->mtdblock_path);
        close(sfd);
        close(dfd);
        return -1;
    }

    ret = fd_to_fd_virt_nand(sfd, dfd, imglen, checksum, block_indicator, prtkimgdesc->page_size/RTK_UNIT_PAGE_SIZE, mode);

    close(sfd);
    close(dfd);

    if(ret != imglen)
    {
        install_debug("fd_to_fd fail(%d)\r\n", ret);
        return -1;
    }

    return 0;
}

int fd_to_fd_yaffs_to_virt_nand(int sfd, int dfd, unsigned int length, unsigned int* pchecksum, unsigned char block_indicator, const unsigned int rtk_unit_page_count_per_nand_page)
{
    int ret, rret, wret, i;
    unsigned int rlen;
    unsigned int len;
    unsigned int progress_len, interval;
    unsigned char data_buf[RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE];
    unsigned char tag_lsb_no, tag_msb_no;
    unsigned int rtk_unit_page_count;

    unsigned char *prdata_buf = NULL;

    time_t stime, etime;
    int restart;
    time_t start_time, end_time;

    stime = time(NULL);
    interval = length/10;
    progress_len = 0;
    len = 0;
    if(pchecksum != NULL)
        *pchecksum = 0;

    restart = 1;
    progressbar(0, length, NULL);
    fflush(stdout);

    rtk_unit_page_count = 0;

    if (rtk_unit_page_count_per_nand_page == 0)
    {
        install_fail("error! rtk_unit_page_count_per_nand_page = %d\r\n", rtk_unit_page_count_per_nand_page);
    }

    prdata_buf = (unsigned char *)malloc((RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE)*rtk_unit_page_count_per_nand_page);
    if (prdata_buf == NULL)
    {
        install_fail("error! prdata_buf = NULL\r\n");
    }

    while(len < length)
    {
        rlen = ((length - len) < (RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE)*rtk_unit_page_count_per_nand_page) \
               ? (length - len): (RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE)*rtk_unit_page_count_per_nand_page;
        ret = isready(sfd);
        // select error
        if(ret < 0)
        {
            install_debug("select error\r\n");
            goto dd_virt_nand_end;
        }
        if(ret == 0)
        {
            if(restart == 1)
            {
                install_debug("first timeout\r\n");
                start_time = time(NULL);
                restart = 0;
            }
            else
            {
                install_debug("second timeout\r\n");
                end_time = time(NULL);
                if(difftime(end_time, start_time) > TIME_OUT)
                {
                    goto dd_virt_nand_end;
                }
            }
            continue;
        }

        restart = 1;

        memset(prdata_buf, 0x0, (RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE)*rtk_unit_page_count_per_nand_page);

        if((rret = read(sfd, (void*) prdata_buf, rlen)) < 0)
        {
            install_debug("read source fd fail\r\n");
            goto dd_virt_nand_end;
        }
        else if(rret == 0)
        {
            install_debug("read source fd close\r\n");
            goto dd_virt_nand_end;
        }
        else if(rret != (RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE)*rtk_unit_page_count_per_nand_page)
        {
            install_fail("error, read size %d != %d\r\n", rret, (RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE)*rtk_unit_page_count_per_nand_page);
            goto dd_virt_nand_end;
        }


        for (i = 0; i < rtk_unit_page_count_per_nand_page; i++)
        {

            memset(data_buf, 0x0, RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE);

            //copy data
            memcpy(&data_buf[0], &prdata_buf[RTK_UNIT_PAGE_SIZE*i], RTK_UNIT_PAGE_SIZE);

            //memcpy(&data_buf[RTK_UNIT_PAGE_SIZE], &prdata_buf[RTK_UNIT_PAGE_SIZE*rtk_unit_page_count_per_nand_page+RTK_UNIT_OOB_SIZE*i], RTK_UNIT_OOB_SIZE-RTK_NAND_ECC_SIZE_PER_UNIT_PAGE);

            if (i == 0)
            {
                //copy oob (tag)
                data_buf[RTK_UNIT_PAGE_SIZE] = RTK_NAND_BLOCK_STATE_GOOD_BLOCK;
                data_buf[RTK_UNIT_PAGE_SIZE+1] = prdata_buf[RTK_UNIT_PAGE_SIZE*rtk_unit_page_count_per_nand_page];
                data_buf[RTK_UNIT_PAGE_SIZE+2] = prdata_buf[RTK_UNIT_PAGE_SIZE*rtk_unit_page_count_per_nand_page+1];
                data_buf[RTK_UNIT_PAGE_SIZE+3] = prdata_buf[RTK_UNIT_PAGE_SIZE*rtk_unit_page_count_per_nand_page+2];
                data_buf[RTK_UNIT_PAGE_SIZE+4] = prdata_buf[RTK_UNIT_PAGE_SIZE*rtk_unit_page_count_per_nand_page+3];

            }
            else
            {
                //copy oob (tag)
                data_buf[RTK_UNIT_PAGE_SIZE] = prdata_buf[RTK_UNIT_PAGE_SIZE*rtk_unit_page_count_per_nand_page+4*i];
                data_buf[RTK_UNIT_PAGE_SIZE+1] = prdata_buf[RTK_UNIT_PAGE_SIZE*rtk_unit_page_count_per_nand_page+1+4*i];
                data_buf[RTK_UNIT_PAGE_SIZE+2] = prdata_buf[RTK_UNIT_PAGE_SIZE*rtk_unit_page_count_per_nand_page+2+4*i];
                data_buf[RTK_UNIT_PAGE_SIZE+3] = prdata_buf[RTK_UNIT_PAGE_SIZE*rtk_unit_page_count_per_nand_page+3+4*i];
            }

            gen_10bytes_ecc(&data_buf[0], &data_buf[RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE-RTK_NAND_ECC_SIZE_PER_UNIT_PAGE]);

            if((wret = write(dfd, (void*) data_buf, RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE)) < 0)
            {
                install_debug("write fail\r\n");
                goto dd_virt_nand_end;
            }
            else if(wret != RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE)
            {
                install_fail("error, write size %d != %d\r\n", wret, RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE);
                goto dd_virt_nand_end;
            }
        }

        len = len + rret;
        if(len >= progress_len)
        {
            progressbar(len, length, NULL);
            fflush(stdout);
            progress_len = progress_len + interval;
        }

        rtk_unit_page_count++;

    }
dd_virt_nand_end:

    free(prdata_buf);

    etime = time(NULL);
    if(pchecksum != NULL)
    {
        if (*pchecksum != 0)
            install_debug("checksum:0x%08X\r\n\r\n", *pchecksum);
    }

    //install_debug("\r\ntotal_time:%.0lf seconds\r\n", difftime(etime,stime));
    return len;
}

int rtk_yaffs_to_virt_nand_with_ecc(const char* filepath, unsigned int soffset, unsigned int imglen, const struct t_rtkimgdesc* prtkimgdesc, unsigned int doffset, unsigned char block_indicator, unsigned int* checksum)
{
    int sfd, dfd;
    int ret;
    int v_nand_doffset;

    if((sfd = open(filepath, O_RDONLY)) < 0)
    {
        install_debug("Can't open %s\r\n", filepath);
        return -1;
    }

    //sanity-check
    if (prtkimgdesc->mtd_erasesize == 0 || prtkimgdesc->page_size == 0 || prtkimgdesc->oob_size == 0)
    {
        install_fail("error! mtd_erasesize or page_size or oob_size not init!\r\n");
        return -1;
    }
    if (doffset&(RTK_UNIT_PAGE_SIZE-1))
    {
        install_fail("error! doffset not in alligned!\r\n");
        return -1;
    }

    //install_debug("doffset=%u(0x%x) alignlen=%u(0x%x) imglen=%u(0x%x).\r\n", doffset, doffset, alignlen, alignlen, imglen, imglen);

    if((dfd = open(prtkimgdesc->mtdblock_path, O_RDWR|O_SYNC)) < 0)
    {
        install_fail("Can't open %s\r\n", prtkimgdesc->mtdblock_path);
        return -1;
    }

    if((ret = lseek(sfd, soffset, SEEK_SET)) < 0)
    {
        install_debug("lseek (%s) fail\r\n", filepath);
        close(sfd);
        close(dfd);
        return -1;
    }

    v_nand_doffset = doffset/RTK_UNIT_PAGE_SIZE*(RTK_UNIT_PAGE_SIZE+RTK_UNIT_OOB_SIZE);

    if((ret = lseek(dfd, v_nand_doffset, SEEK_SET)) < 0)
    {
        install_debug("lseek (%s) fail\r\n", prtkimgdesc->mtdblock_path);
        close(sfd);
        close(dfd);
        return -1;
    }

    ret = fd_to_fd_yaffs_to_virt_nand(sfd, dfd, imglen, checksum, block_indicator, prtkimgdesc->page_size/RTK_UNIT_PAGE_SIZE);

    close(sfd);
    close(dfd);

    if(ret != imglen)
    {
        install_debug("fd_to_fd fail(%d)\r\n", ret);
        return -1;
    }

    return 0;
}


#endif
