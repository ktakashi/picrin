/**
 * See Copyright Notice in picrin.h
 */

#ifndef PICRIN_VECTOR_H
#define PICRIN_VECTOR_H

#if defined(__cplusplus)
extern "C" {
#endif

struct pic_vector {
  PIC_OBJECT_HEADER
  pic_value *data;
  int len;
};

#define pic_vec_p(v) (pic_type(v) == PIC_TT_VECTOR)
#define pic_vec_ptr(o) ((struct pic_vector *)pic_ptr(o))

pic_vec *pic_make_vec(pic_state *, int);

#if defined(__cplusplus)
}
#endif

#endif
