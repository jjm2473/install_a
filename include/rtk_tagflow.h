#ifndef RTK_TAGFLOW_H
#define RTK_TAGGLOW_H

#include <rtk_boottable.h>
int tagflow3(S_BOOTTABLE* pboottable, struct t_rtkimgdesc* prtkimgdesc, S_BOOTTABLE* pbt);
#ifdef EMMC_SUPPORT
int tagflow3_emmc(S_BOOTTABLE* pboottable, struct t_rtkimgdesc* prtkimgdesc, S_BOOTTABLE* pbt);
#endif
#ifdef NAS_ENABLE
int tagflow3_spi(S_BOOTTABLE* pboottable, struct t_rtkimgdesc* prtkimgdesc, S_BOOTTABLE* pbt);
#endif
int tagflow3_sata(S_BOOTTABLE* pboottable, struct t_rtkimgdesc* prtkimgdesc, S_BOOTTABLE* pbt);
#endif
