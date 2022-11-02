#include "la.h"
#include <string.h>

//symbols
static Inline sym ini_sym(void *_, str nom, size_t code) {
  sym y = _;
  y->disp = disp, y->mtbl = &mtbl_sym;
  y->nom = nom;
  y->code = code;
  y->l = y->r = 0;
  return y; }

// FIXME this is bad
// symbols are interned into a binary search tree. we make no
// attempt to keep it balanced but it gets rebuilt in somewhat
// unpredictable order every gc cycle so hopefully that should
// help keep it from getting too bad. a hash table is probably
// the way to go but rebuilding that is more difficult. the
// existing code is unsuitable because it dynamically resizes
// the table and unpredictable memory allocation isn't safe
// during garbage collection.
//
// FIXME the caller must ensure Avail >= Width(sym)
// (because GC here would void the tree)
static sym sskc(la v, sym *y, str b) {
  if (*y) {
    sym z = *y;
    str a = z->nom;
    size_t n = a->len < b->len ? a->len : b->len;
    int i = strncmp(a->text, b->text, n);
    return i == 0 ? z :
      sskc(v, i < 0 ? &z->r : &z->l, b); }
  return *y = ini_sym(bump(v, Width(sym)), b,
    hash(v, putnum(hash(v, (ob) b)))); }

sym symof(la v, str s) {
  if (Avail < Width(sym)) {
    bool _;
    ob x = (ob) s;
    with(x, _ = please(v, Width(sym)));
    if (!_) return 0;
    s = (str) x; }
  return sskc(v, &v->syms, s); }

Vm(sym_u) {
  Have(Width(sym));
  if (fp->argc && strp(fp->argv[0]))
    return ApC(ret, (ob) sskc(v, &v->syms, (str) fp->argv[0]));
  xp = (ob) ini_sym(hp, 0, v->rand = lcprng(v->rand));
  hp += Width(sym);
  return ApC(ret, xp); }

Vm(ystr_u) {
  ArityCheck(1);
  xp = fp->argv[0];
  Check(symp(xp));
  str s = ((sym) xp)->nom;
  return ApC(ret, s ? (ob) s : nil); }

static Gc(cp_sym) {
  sym src = (sym) x,
      dst = src->nom ?
        sskc(v, &v->syms, (str) cp(v, (ob) src->nom, pool0, top0)) :
        cpyw(bump(v, Width(sym)), src, Width(sym));
  return (ob) (src->disp = (vm*) dst); }

static size_t hash_sym(la v, ob x) { return ((sym) x)->code; }

static int em_sym(la v, FILE *o, ob x) {
  str s = ((sym)x)->nom;
  return !s ? fprintf(o, "#sym") : fputstr(o, s); }

Vm(do_id) { return ApC(ret, (ob) ip); }

const struct mtbl mtbl_sym = {
  .does = do_id,
  .emit = em_sym,
  .copy = cp_sym,
  .hash = hash_sym,
  .equi = eq_no, };
