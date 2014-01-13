#include <stdlib.h>

#include "picrin.h"
#include "picrin/gc.h"
#include "picrin/proc.h"
#include "picrin/macro.h"
#include "xhash/xhash.h"

void pic_init_core(pic_state *);

pic_state *
pic_open(int argc, char *argv[], char **envp)
{
  pic_value t;

  pic_state *pic;
  int ai;

  pic = (pic_state *)malloc(sizeof(pic_state));

  /* command line */
  pic->argc = argc;
  pic->argv = argv;
  pic->envp = envp;

  /* root block */
  pic->blk = (struct pic_block *)malloc(sizeof(struct pic_block));
  pic->blk->prev = NULL;
  pic->blk->depth = 0;
  pic->blk->in = pic->blk->out = NULL;
  pic->blk->refcnt = 1;

  /* prepare VM stack */
  pic->stbase = pic->sp = (pic_value *)calloc(PIC_STACK_SIZE, sizeof(pic_value));
  pic->stend = pic->stbase + PIC_STACK_SIZE;

  /* callinfo */
  pic->cibase = pic->ci = (pic_callinfo *)calloc(PIC_STACK_SIZE, sizeof(pic_callinfo));
  pic->ciend = pic->cibase + PIC_STACK_SIZE;

  /* exception handlers */
  pic->rescue = (struct pic_proc **)calloc(PIC_RESCUE_SIZE, sizeof(struct pic_proc *));
  pic->ridx = 0;
  pic->rlen = PIC_RESCUE_SIZE;

  /* memory heap */
  pic->heap = (struct pic_heap *)calloc(1, sizeof(struct pic_heap));
  init_heap(pic->heap);

  /* symbol table */
  pic->sym_tbl = xh_new();
  pic->sym_pool = (const char **)calloc(PIC_SYM_POOL_SIZE, sizeof(const char *));
  pic->slen = 0;
  pic->scapa = pic->slen + PIC_SYM_POOL_SIZE;
  pic->uniq_sym_count = 0;

  /* irep */
  pic->irep = (struct pic_irep **)calloc(PIC_IREP_SIZE, sizeof(struct pic_irep *));
  pic->ilen = 0;
  pic->icapa = PIC_IREP_SIZE;

  /* globals */
  pic->global_tbl = xh_new();
  pic->globals = (pic_value *)calloc(PIC_GLOBALS_SIZE, sizeof(pic_value));
  pic->glen = 0;
  pic->gcapa = PIC_GLOBALS_SIZE;

  /* libraries */
  pic->lib_tbl = pic_nil_value();
  pic->lib = NULL;

  /* pool */
  pic->pool = (pic_value *)calloc(PIC_POOL_SIZE, sizeof(pic_value));
  pic->plen = 0;
  pic->pcapa = PIC_POOL_SIZE;

  /* error handling */
  pic->jmp = NULL;
  pic->errmsg = NULL;

  /* GC arena */
  pic->arena_idx = 0;

  /* native stack marker */
  pic->native_stack_start = &t;

#define register_core_symbol(pic,slot,name) do {	\
    pic->slot = pic_intern_cstr(pic, name);		\
  } while (0)

  ai = pic_gc_arena_preserve(pic);
  register_core_symbol(pic, sDEFINE, "define");
  register_core_symbol(pic, sLAMBDA, "lambda");
  register_core_symbol(pic, sIF, "if");
  register_core_symbol(pic, sBEGIN, "begin");
  register_core_symbol(pic, sSETBANG, "set!");
  register_core_symbol(pic, sQUOTE, "quote");
  register_core_symbol(pic, sQUASIQUOTE, "quasiquote");
  register_core_symbol(pic, sUNQUOTE, "unquote");
  register_core_symbol(pic, sUNQUOTE_SPLICING, "unquote-splicing");
  register_core_symbol(pic, sDEFINE_SYNTAX, "define-syntax");
  register_core_symbol(pic, sDEFINE_MACRO, "define-macro");
  register_core_symbol(pic, sDEFINE_LIBRARY, "define-library");
  register_core_symbol(pic, sIMPORT, "import");
  register_core_symbol(pic, sEXPORT, "export");
  pic_gc_arena_restore(pic, ai);

  pic_init_core(pic);

  /* set library */
  pic_make_library(pic, pic_parse(pic, "user"));
  pic_in_library(pic, pic_parse(pic, "user"));

  return pic;
}

void
pic_close(pic_state *pic)
{
  free(pic);
}
