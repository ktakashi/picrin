/**
 * See Copyright Notice in picrin.h
 */

#include "picrin.h"

void
pic_wind(pic_state *pic, pic_checkpoint *here, pic_checkpoint *there)
{
  if (here == there)
    return;

  if (here->depth < there->depth) {
    pic_wind(pic, here, there->prev);
    pic_apply0(pic, there->in);
  }
  else {
    pic_apply0(pic, there->out);
    pic_wind(pic, here->prev, there);
  }
}

pic_value
pic_dynamic_wind(pic_state *pic, struct pic_proc *in, struct pic_proc *thunk, struct pic_proc *out)
{
  pic_checkpoint *here;
  pic_value val;

  if (in != NULL) {
    pic_apply0(pic, in);        /* enter */
  }

  here = pic->cp;
  pic->cp = (pic_checkpoint *)pic_obj_alloc(pic, sizeof(pic_checkpoint), PIC_TT_CP);
  pic->cp->prev = here;
  pic->cp->depth = here->depth + 1;
  pic->cp->in = in;
  pic->cp->out = out;

  val = pic_apply0(pic, thunk);

  pic->cp = here;

  if (out != NULL) {
    pic_apply0(pic, out);       /* exit */
  }

  return val;
}

void
pic_save_point(pic_state *pic, struct pic_cont *cont)
{
  /* save runtime context */
  cont->cp = pic->cp;
  cont->sp_offset = pic->sp - pic->stbase;
  cont->ci_offset = pic->ci - pic->cibase;
  cont->xp_offset = pic->xp - pic->xpbase;
  cont->arena_idx = pic->arena_idx;
  cont->ip = pic->ip;
  cont->ptable = pic->ptable;
  cont->prev = pic->cc;
  cont->results = pic_undef_value();
  cont->id = pic->ccnt++;

  pic->cc = cont;
}

void
pic_load_point(pic_state *pic, struct pic_cont *cont)
{
  pic_wind(pic, pic->cp, cont->cp);

  /* load runtime context */
  pic->cp = cont->cp;
  pic->sp = pic->stbase + cont->sp_offset;
  pic->ci = pic->cibase + cont->ci_offset;
  pic->xp = pic->xpbase + cont->xp_offset;
  pic->arena_idx = cont->arena_idx;
  pic->ip = cont->ip;
  pic->ptable = cont->ptable;
  pic->cc = cont->prev;
}

static pic_value
cont_call(pic_state *pic)
{
  struct pic_proc *self = pic_get_proc(pic);
  int argc;
  pic_value *argv;
  int id;
  struct pic_cont *cc, *cont;

  pic_get_args(pic, "*", &argc, &argv);

  id = pic_int(pic_proc_env_ref(pic, self, "id"));

  /* check if continuation is alive */
  for (cc = pic->cc; cc != NULL; cc = cc->prev) {
    if (cc->id == id) {
      break;
    }
  }
  if (cc == NULL) {
    pic_errorf(pic, "calling dead escape continuation");
  }

  cont = pic_data_ptr(pic_proc_env_ref(pic, self, "escape"))->data;
  cont->results = pic_list_by_array(pic, argc, argv);

  pic_load_point(pic, cont);

  PIC_LONGJMP(pic, cont->jmp, 1);

  PIC_UNREACHABLE();
}

struct pic_proc *
pic_make_cont(pic_state *pic, struct pic_cont *cont)
{
  static const pic_data_type cont_type = { "cont", NULL, NULL };
  struct pic_proc *c;
  struct pic_data *e;

  c = pic_make_proc(pic, cont_call);

  e = pic_data_alloc(pic, &cont_type, cont);

  /* save the escape continuation in proc */
  pic_proc_env_set(pic, c, "escape", pic_obj_value(e));
  pic_proc_env_set(pic, c, "id", pic_int_value(cont->id));

  return c;
}

pic_value
pic_callcc(pic_state *pic, struct pic_proc *proc)
{
  struct pic_cont cont;

  pic_save_point(pic, &cont);

  if (PIC_SETJMP(pic, cont.jmp)) {
    return pic_values_by_list(pic, cont.results);
  }
  else {
    pic_value val;

    val = pic_apply1(pic, proc, pic_obj_value(pic_make_cont(pic, &cont)));

    pic->cc = pic->cc->prev;

    return val;
  }
}

static pic_value
pic_va_values(pic_state *pic, int n, ...)
{
  pic_vec *args = pic_make_vec(pic, n);
  va_list ap;
  int i = 0;

  va_start(ap, n);

  while (i < n) {
    args->data[i++] = va_arg(ap, pic_value);
  }

  va_end(ap);

  return pic_values(pic, n, args->data);
}

pic_value
pic_values0(pic_state *pic)
{
  return pic_va_values(pic, 0);
}

pic_value
pic_values1(pic_state *pic, pic_value arg1)
{
  return pic_va_values(pic, 1, arg1);
}

pic_value
pic_values2(pic_state *pic, pic_value arg1, pic_value arg2)
{
  return pic_va_values(pic, 2, arg1, arg2);
}

pic_value
pic_values3(pic_state *pic, pic_value arg1, pic_value arg2, pic_value arg3)
{
  return pic_va_values(pic, 3, arg1, arg2, arg3);
}

pic_value
pic_values4(pic_state *pic, pic_value arg1, pic_value arg2, pic_value arg3, pic_value arg4)
{
  return pic_va_values(pic, 4, arg1, arg2, arg3, arg4);
}

pic_value
pic_values5(pic_state *pic, pic_value arg1, pic_value arg2, pic_value arg3, pic_value arg4, pic_value arg5)
{
  return pic_va_values(pic, 5, arg1, arg2, arg3, arg4, arg5);
}

pic_value
pic_values(pic_state *pic, int argc, pic_value *argv)
{
  int i;

  for (i = 0; i < argc; ++i) {
    pic->sp[i] = argv[i];
  }
  pic->ci->retc = (int)argc;

  return argc == 0 ? pic_undef_value() : pic->sp[0];
}

pic_value
pic_values_by_list(pic_state *pic, pic_value list)
{
  pic_value v, it;
  int i;

  i = 0;
  pic_for_each (v, list, it) {
    pic->sp[i++] = v;
  }
  pic->ci->retc = i;

  return pic_nil_p(list) ? pic_undef_value() : pic->sp[0];
}

int
pic_receive(pic_state *pic, int n, pic_value *argv)
{
  pic_callinfo *ci;
  int i, retc;

  /* take info from discarded frame */
  ci = pic->ci + 1;
  retc = ci->retc;

  for (i = 0; i < retc && i < n; ++i) {
    argv[i] = ci->fp[i];
  }

  return retc;
}

static pic_value
pic_cont_callcc(pic_state *pic)
{
  struct pic_proc *cb;

  pic_get_args(pic, "l", &cb);

  return pic_callcc(pic, cb);
}

static pic_value
pic_cont_dynamic_wind(pic_state *pic)
{
  struct pic_proc *in, *thunk, *out;

  pic_get_args(pic, "lll", &in, &thunk, &out);

  return pic_dynamic_wind(pic, in, thunk, out);
}

static pic_value
pic_cont_values(pic_state *pic)
{
  int argc;
  pic_value *argv;

  pic_get_args(pic, "*", &argc, &argv);

  return pic_values(pic, argc, argv);
}

static pic_value
pic_cont_call_with_values(pic_state *pic)
{
  struct pic_proc *producer, *consumer;
  int argc;
  pic_vec *args;

  pic_get_args(pic, "ll", &producer, &consumer);

  pic_apply0(pic, producer);

  argc = pic_receive(pic, 0, NULL);
  args = pic_make_vec(pic, argc);

  pic_receive(pic, argc, args->data);

  return pic_apply_trampoline(pic, consumer, argc, args->data);
}

void
pic_init_cont(pic_state *pic)
{
  pic_defun(pic, "call-with-current-continuation", pic_cont_callcc);
  pic_defun(pic, "call/cc", pic_cont_callcc);
  pic_defun(pic, "escape", pic_cont_callcc);
  pic_defun(pic, "dynamic-wind", pic_cont_dynamic_wind);

  pic_defun(pic, "values", pic_cont_values);
  pic_defun(pic, "call-with-values", pic_cont_call_with_values);
}
