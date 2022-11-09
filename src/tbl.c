#include "la.h"
#include "alloc.h"
#include "two.h"
#include "mo.h"
#include "tbl.h"
#include "hash.h"
#include "cmp.h"
#include "gc.h"
#include <string.h>

// hash tables
// some of the worst code is here :(

typedef struct la_tbl_ent {
  la_ob key, val;
  struct la_tbl_ent *next;
} *la_tbl_ent;

#define KEY(e) ((la_tbl_ent)(e))->key
#define VAL(e) ((la_tbl_ent)(e))->val
#define NEXT(e) ((ob*)(e))[2]

static Inline tbl ini_tbl(void *_, size_t len, size_t cap, ob *tab) {
  tbl t = _;
  t->head.disp = disp;
  t->head.mtbl = &mtbl_tbl;
  t->len = len;
  t->cap = cap;
  t->tab = tab;
  return t; }

static Inline size_t tbl_idx(size_t cap, size_t co) {
  return co & ((1 << cap) - 1); }

static Inline size_t tbl_load(tbl t) {
  return t->len >> t->cap; }

static Inline ob tbl_bkt_hc(la v, tbl t, size_t hc) {
  return t->tab[tbl_idx(t->cap, hc)]; }

static ob tbl_ent_hc(la v, tbl t, ob k, size_t hc) {
  ob e = tbl_bkt_hc(v, t, hc);
  while (!nump(e) && !eql(v, KEY(e), k)) e = NEXT(e);
  return e; }

static ob tbl_ent(la v, tbl t, ob k) {
  return tbl_ent_hc(v, t, k, hash(v, k)); }

static tbl
  tbl_grow(la, tbl),
  tbl_set_s(la, tbl, ob, ob);
static ob
  tbl_del(la, tbl, ob),
  tbl_keys(la);
static bool
  tblss(la, intptr_t, intptr_t);
static void
  tbl_shrink(la, tbl);

tbl mktbl(la v) {
  tbl t = cells(v, b2w(sizeof(struct tbl)) + 1 + b2w(sizeof(struct tag)));
  if (t) ini_tbl(t, 0, 0, (ob*) ini_mo(t+1, 1)),
         t->tab[0] = putnum(-1);
  return t; }

tbl tblset(la v, tbl t, ob k, ob x) { return
  t = tbl_set_s(v, t, k, x),
  t && tbl_load(t) > 1 ? tbl_grow(v, t) : t; }

ob tblget(la v, tbl t, ob k, ob d) { return
  k = tbl_ent(v, t, k),
  nump(k) ? d : VAL(k); }

#include "vm.h"
Vm(tget_f) {
  ArityCheck(2);
  xp = fp->argv[0];
  Check(tblp(xp));
  xp = tblget(v, (tbl) xp, fp->argv[1], nil);
  return ApC(ret, xp); }

Vm(tdel_f) {
  ArityCheck(2);
  ob x = fp->argv[0];
  Check(tblp(x));
  CallOut(x = tbl_del(v, (tbl) x, fp->argv[1]));
  return ApC(ret, x); }

Vm(tget) { return
  xp = tblget(v, (tbl) xp, *sp++, nil),
  ApN(1, xp); }

Vm(thas) { return
  xp = tblget(v, (tbl) xp, *sp++, 0),
  ApN(1, xp ? T : nil); }

Vm(tlen) { return ApN(1, putnum(((tbl) xp)->len)); }

Vm(thas_f) {
  ArityCheck(2);
  xp = fp->argv[0];
  Check(tblp(xp));
  xp = tblget(v, (tbl) xp, fp->argv[1], 0);
  return ApC(ret, xp ? T : nil); }

Vm(tset_f) {
  if (!fp->argc) return ApC(ret, xp);
  xp = fp->argv[0];
  Check(tblp(xp));
  bool _;
  CallOut(_ = tblss(v, 1, fp->argc));
  return _ ? ApC(ret, fp->argv[fp->argc-1]) : ApC(xoom, nil); }

Vm(tbl_f) {
  ob x = fp->argc;
  CallOut(x = (v->xp = (ob) mktbl(v)) && tblss(v, 0, x));
  return x ? ApC(ret, xp) : ApC(xoom, nil); }

Vm(tkeys_f) {
  ArityCheck(1);
  xp = fp->argv[0];
  Check(tblp(xp));
  ob x;
  CallOut(x = tbl_keys(v));
  return x ? ApC(ret, x) : ApC(xoom, xp); }

Vm(tlen_f) {
  ArityCheck(1);
  xp = fp->argv[0];
  Check(tblp(xp));
  return ApC(ret, putnum(((tbl) xp)->len)); }

Vm(tset) {
  ob x = *sp++;
  CallOut(x = (ob) tblset(v, (tbl) xp, x, *sp));
  return x ? ApN(1, *sp++) : ApC(xoom, xp); }

// FIXME so bad :(
static ob tbl_del(la v, tbl y, ob key) {
  size_t b = tbl_idx(y->cap, hash(v, key));
  ob val = nil,
     e = y->tab[b],
     prev[] = {0,0,e};
  for (ob l = (ob) &prev; !nump(l) && !nump(NEXT(l)); l = NEXT(l))
    if (eql(v, KEY(NEXT(l)), key)) {
      val = VAL(NEXT(l));
      NEXT(l) = NEXT(NEXT(l));
      y->len--;
      break; }
  y->tab[b] = NEXT(prev);
  tbl_shrink(v, y);
  return val; }

// tbl_grow(vm, tbl, new_size): destructively resize a hash table.
// new_size words of memory are allocated for the new bucket array.
// the old table entries are reused to populate the modified table.
static tbl tbl_grow(la v, tbl t) {
  ob *tab0, *tab1;
  size_t cap0 = t->cap,
         cap1 = cap0 + 1,
         len = 1ul << cap1;

  with(t, tab1 = (ob*) mkmo(v, len));
  if (!tab1) return 0;
  memset(tab1, -1, len * sizeof(ob));
  tab0 = t->tab;

  for (size_t i, cap = 1 << cap0; cap--;)
    for (ob e, es = tab0[cap]; !nump(es);
      e = es,
      es = NEXT(es),
      i = tbl_idx(cap1, hash(v, KEY(e))),
      NEXT(e) = tab1[i],
      tab1[i] = e);

  t->cap = cap1;
  t->tab = tab1;
  return t; }

static tbl tbl_set_s(la v, tbl t, ob k, ob x) {
  size_t hc = hash(v, k);
  ob e = tbl_ent_hc(v, t, k, hc);
  if (!nump(e)) return VAL(e) = x, t;
  size_t i = tbl_idx(t->cap, hc);
  with(t, e = tupl(v, k, x, t->tab[i], NULL));
  if (!e) return 0;
  t->tab[i] = e;
  t->len++;
  return t; }

// get table keys
// XXX calling convention: table in v->xp
static ob tbl_keys(la v) {
  size_t len = ((tbl) v->xp)->len;
  two ks;
  ks = cells(v, b2w(sizeof(struct two)) * len);
  if (!ks) return 0;
  ob r = nil, *tab = ((tbl) v->xp)->tab;
  while (len) for (ob e = *tab++; !nump(e);
    ini_two(ks, KEY(e), r),
    r = (ob) ks++,
    e = NEXT(e),
    len--);
  return r; }

// do a bunch of table assignments.
// XXX calling convention: table in v->xp
// FIXME gross!
static bool tblss(la v, intptr_t i, intptr_t l) {
  bool _ = true;
  while (_ && i <= l - 2)
    _ = tblset(v,
      (tbl) v->xp,
      v->fp->argv[i],
      v->fp->argv[i+1]),
    i += 2;
  return _; }

// shrinking a table never allocates memory, so it's safe
// to do at any time.
static void tbl_shrink(la v, tbl t) {
  // do nothing if we're already overloaded
  if (tbl_load(t)) return;

  ob e = nil, f, g;
  size_t i = 1ul << t->cap;

  // collect all entries
  while (i--) for (f = t->tab[i], t->tab[i] = putnum(-1); !nump(f);
    g = NEXT(f), NEXT(f) = e, e = f, f = g);

  // shrink bucket array
  while (t->cap && tbl_load(t) < 1) t->cap--;
  i = 1ul << t->cap;
  ini_mo(t->tab, i);

  // reinsert
  while (!nilp(e))
    i = tbl_idx(t->cap, hash(v, KEY(e))),
    f = NEXT(e),
    NEXT(e) = t->tab[i],
    t->tab[i] = e,
    e = f; }

static Vm(aptbl) {
  ob a = fp->argc;
  switch (a) {
    case 0: return ApC(ret, putnum(((tbl) ip)->len));
    case 1: return
      xp = tblget(v, (tbl) ip, fp->argv[0], nil),
      ApC(ret, xp);
    default:
      xp = (ob) ip;
      bool _;
      CallOut(_ = tblss(v, 1, a));
      return _ ? ApC(ret, fp->argv[a-1]) : ApC(xoom, nil); } }

static long txtbl(la v, FILE *o, ob _) {
  tbl t = (tbl) _;
  return fprintf(o, "#tbl:%ld/%ld", t->len, 1ul << t->cap); }

static intptr_t hxtbl(la v, ob _) {
  return ror(mix * 9, 3 * sizeof(intptr_t) / 4); }

#include "gc.h"
static Gc(cptbl) {
  tbl src = (tbl) x,
      dst = bump(v, b2w(sizeof(struct tbl)));
  src->head.disp = (vm*) dst;
  return (ob) ini_tbl(dst, src->len, src->cap,
    (ob*) cp(v, (ob) src->tab, pool0, top0)); }

const struct mtbl mtbl_tbl = {
  .does = aptbl,
  .emit = txtbl,
  .evac = cptbl,
  .hash = hxtbl,
  .equi = neql, };
