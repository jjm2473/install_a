#ifndef RTK_TAR_H
#define RTK_TAR_H
#include <stdlib.h>
#define TARFILENAMELEN 256
struct tarinfo {
  char tarfile_path[TARFILENAMELEN];
  int fd;
  unsigned int tarfile_offset;
  unsigned int tarfile_size;
};
int rtk_open_tarfile_with_offset(struct tarinfo* ptarinfo, unsigned int tarfile_offset);
int parse_tar(struct t_rtkimgdesc* prtkimg);
int parse_urltar(struct t_rtkimgdesc*);
int parse_flashtar(unsigned int backup_start_offset, unsigned int backup_end_offset, struct t_rtkimgdesc* prtkimg);

#endif
