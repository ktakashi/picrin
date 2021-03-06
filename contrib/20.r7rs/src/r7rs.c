/**
 * See Copyright Notice in picrin.h
 */

#include "picrin.h"

void pic_init_file(pic_state *);
void pic_init_load(pic_state *);
void pic_init_mutable_string(pic_state *);
void pic_init_system(pic_state *);
void pic_init_time(pic_state *);

void
pic_init_r7rs(pic_state *pic)
{
  pic_init_file(pic);
  pic_init_load(pic);
  pic_init_mutable_string(pic);
  pic_init_system(pic);
  pic_init_time(pic);
}
