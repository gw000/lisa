#include "lips.h"
static int copy(vm, num);
static obj cp(vm, obj, num, mem);
static Inline void do_copy(vm, num, mem, num, mem);
static obj sskc(vm, mem, obj);

// a simple copying garbage collector

// gc entry point reqsp : vm x num -> bool
//
// try to return with at least req words of available memory.
// return true on success, false otherwise. this function also
// manages the size of the memory pool. here is the procedure
// in a nutshell:
//
// - copy into a new pool of the same size. if this fails,
//   the request fails (oom).
// - if there's enough space and the garbage collector
//   is running fast enough, return success.
// - otherwise adjust the size and copy again. if this fails,
//   we can still succeed if the first copy left us with
//   enough free space (ie. we tried to grow for performance
//   reasons). but otherwise the request fails (oom).
//
// at a constant rate of allocation, doubling the size of the
// heap halves the amount of time spent in garbage collection.
// the memory manager uses this relation to automatically trade
// space for time to keep the time spent in garbage collection
// within certain limits, given here in units of program time
// per unit garbage collection time:
#define lb 32
#define ub 128
// in theory these bounds limit the overall time spent in
// garbage collection to no more than about 6%, at the cost
// of higher memory use under pressure. anything we do to make
// gc "faster" (other than adjusting the bounds) will have the
// actual effect of improving memory efficiency.
#define grow() (len*=2,vit*=2)
#define shrink() (len/=2,vit/=2)
#define growp (allocd > len || vit < lb)
#define shrinkp (allocd < len/2 && vit >= ub)
_ reqsp(vm v, num req) {
  Z len = v->mem_len, vit = copy(v, len);
  if (vit) { // copy succeeded
    Z allocd = len - (Avail - req);
       if (growp)   do grow();   Wh (growp);
    El if (shrinkp) do shrink(); Wh (shrinkp);
    El R; // no size change needed
    // otherwise grow or shrink
    if (copy(v, len)) R;
    // oh no that didn't work
    // maybe we can still return though
    if (allocd <= Len) R; } // aww darn
  errp(v, "oom"); // this is a bad error
  restart(v); }

// the first step in copying is to allocate
// a new pool of the given length, which must
// be at least enough to support the actual
// amount of reachable memory. if this fails
// then return 0. otherwise swap the pools,
// reset internal symbols, copy the stack,
// global variables, and any user saved
// locations, and free the old pool. then
// return u:
//
//     non-gc running time     t1    t2
// ,.........................,/      |
// -----------------------------------
// |                          `------'
// t0                  gc time (this cycle)
//
//       u = (t2 - t0) / (t2 - t1)
//
// t values come from clock(). if t0 < t1 < t2 then
// u will be >= 1. however, sometimes t1 == t2. in that case
// u = 1.
St int copy(vm v, num len) {
  clock_t t1 = clock(), t2, u;
  M b0 = v->mem_pool, b1 = malloc(w2b(len));
  R !b1 ? 0 :
   (do_copy(v, v->mem_len, b0, len, b1),
    free(b0),
    t2 = clock(),
    u = t1 == t2 ? 1 : (t2 - v->t0) / (t2 - t1),
    v->t0 = t2,
    u); }

St In _ do_copy(vm v, num l0, mem b0, num l1, mem b1) {
  v->mem_len = l1;
  v->mem_pool = Hp = b1;
  M s0 = Sp,
    t0 = b0 + l0,
    t1 = b1 + l1;
  Z ro = t1 - t0;
  Sp += ro, Fp += ro;
  Syms = nil;
  Wh (t0-- > s0) Sp[t0 - s0] = cp(v, *t0, l0, b0);
#define CP(x) x=cp(v,x,l0,b0)
  CP(Ip), CP(Xp), CP(Glob);
  Fo (root r = Safe; r; r = r->next) CP(*(r->one)); }
#undef CP

// the exact method for copying an object into
// the new pool depends on its type. copied
// objects are used to store pointers to their
// new locations, which effectively destroys the
// old data.
Ty O cp_(V, O, Z, M);
St cp_ cphom, cptup, cptwo, cpsym, cpoct, cptbl;
#define cpcc(n) static obj n(V v, O x, Z ln, M lp)

cpcc(cp) {  Sw (kind(x)) {
  Ks Hom: R cphom(v, x, ln, lp);
  Ks Tup: R cptup(v, x, ln, lp);
  Ks Oct: R cpoct(v, x, ln, lp);
  Ks Two: R cptwo(v, x, ln, lp);
  Ks Sym: R cpsym(v, x, ln, lp);
  Ks Tbl: R cptbl(v, x, ln, lp);
  Df:  R x; } }

#define inb(o,l,u) (o>=l&&o<u)
#define fresh(o) inb((mem)(o),Pool,Pool+Len)
cpcc(cptwo) {
  Tw dst, src = gettwo(x);
  R fresh(src->x) ? src->x :
   (dst = bump(v, Size(two)),
    dst->x = src->x, src->x = (obj) puttwo(dst),
    dst->y = cp(v, src->y, ln, lp),
    dst->x = cp(v, dst->x, ln, lp),
    puttwo(dst)); }

cpcc(cptup) {
  Ve dst, src = gettup(x);
  if (fresh(*src->xs)) R *src->xs;
  dst = bump(v, Size(tup) + src->len);
  num l = dst->len = src->len;
  dst->xs[0] = src->xs[0];
  src->xs[0] = puttup(dst);
  dst->xs[0] = cp(v, dst->xs[0], ln, lp);
  for (num i = 1; i < l; ++i)
    dst->xs[i] = cp(v, src->xs[i], ln, lp);
  R puttup(dst); }

cpcc(cpoct) {
  By dst, src = getoct(x);
  R src->len == 0 ? *(mem)src->text :
    (dst = bump(v, Size(oct) + b2w(src->len)),
     memcpy(dst->text, src->text, dst->len = src->len),
     src->len = 0,
     *(mem)src->text = putoct(dst)); }

cpcc(cpsym) {
  sym src = getsym(x), dst;
  if (fresh(src->nom)) R src->nom;
  if (nilp(src->nom)) // anonymous symbol
    dst = bump(v, Size(sym)),
    memcpy(dst, src, w2b(Size(sym)));
  El dst = getsym(sskc(v, &Syms, cp(v, src->nom, ln, lp)));
  R src->nom = putsym(dst); }

#define stale(o) inb((mem)(o),lp,lp+ln)
cpcc(cphom) {
  hom dst, src = Gh(x), end = src, start;
  if (fresh(G(src))) R (obj) G(src);
  Wh (*end++);
  start = (hom) G(end);
  num i, len = (end+1) - start;
  dst = bump(v, len);
  G(dst+len-2) = NULL;
  G(dst+len-1) = (terp*) dst;
  for (i = 0; i < len - 2; i++)
    G(dst+i) = G(start+i),
    G(start+i) = (terp*) Ph(dst+i);
  for (obj u; i--;)
    u = (obj) G(dst+i),
    G(dst+i) = (terp*) (stale(u) ? cp(v, u, ln, lp) : u);
  R Ph(dst += src - start); }

St tble cptble(vm v, tble src, num ln, mem lp) {
  if (!src) R NULL;
  tble dst = (tble) bump(v, 3);
  dst->next = cptble(v, src->next, ln, lp);
  dst->key = cp(v, src->key, ln, lp);
  dst->val = cp(v, src->val, ln, lp);
  R dst; }

cpcc(cptbl) {
  tbl src = gettbl(x);
  if (fresh(src->tab)) R (obj) src->tab;
  Z src_cap = src->cap;
  tbl dst = bump(v, 3 + src_cap);
  dst->len = src->len;
  dst->cap = src_cap;
  dst->tab = (tble*) (dst + 1);
  tble *src_tab = src->tab;
  src->tab = (tble*) puttbl(dst);
  Wh (src_cap--)
    dst->tab[src_cap] = cptble(v, src_tab[src_cap], ln, lp);
  R puttbl(dst); }


// XXX data constructors

St N hc(vm, obj);

////
/// data constructors and utility functions
//

// pairs
O pair(V v, O a, O b) { Tw t;
 R Avail >= 2 ?
  (t = bump(v, 2),
   t->x = a, t->y = b,
   puttwo(t)) :
  (with(a, with(b, reqsp(v, 2))),
   pair(v, a, b)); }

// strings
obj string(vm v, Ko Ch* c) {
  Z bs = 1 + strlen(c);
  oct o = cells(v, Size(oct) + b2w(bs));
  memcpy(o->text, c, o->len = bs);
  return putoct(o); }

//symbols

// symbols are interned into a binary search tree. we make no
// attempt to keep it balanced but it gets rebuilt in somewhat
// unpredictable order every gc cycle so hopefully that should
// help keep it from getting too bad. a hash table is probably
// the way to go but rebuilding that is more difficult. the
// existing code is unsuitable because it dynamically resizes
// the table and unpredictable memory allocation isn't safe
// during garbage collection. 
St O ssk(V v, O y, O x) {
 int i = strcmp(symnom(y), chars(x));
 R i == 0 ? y :
  sskc(v, i < 0 ? &(getsym(y)->r) : &(getsym(y)->l), x); }

St O sskc(V v, M y, O x) {
  if (!nilp(*y)) return ssk(v, *y, x);
  Sy u = bump(v, Size(sym));
  u->nom = x, u->code = hc(v, x);
  u->l = nil, u->r = nil;
  return *y = putsym(u); }

O intern(V v, O x) {
  if (Avail < Size(sym))
    with(x, reqsp(v, Size(sym)));
  return sskc(v, &Syms, x); }

St In N hash_bytes(Z len, char *us) {
  Z h = 1;
  for (; len--; h *= mix, h ^= *us++);
  return mix * h; }

St N hc(vm v, obj x) {
  N r;
  Sw (kind(x)) {
   Ks Sym: r = getsym(x)->code; Bk;
   Ks Oct: r = hash_bytes(getoct(x)->len, getoct(x)->text); Bk;
   Ks Two: r = hc(v, X(x)) ^ hc(v, Y(x)); Bk;
   Ks Hom: r = hc(v, homnom(v, x)) ^ (mix * (uintptr_t) G(x)); Bk;
   Ks Tbl: r = mix * mix; // umm this'll do for now ...
   Df:     r = mix * x; } 
  R (r<<48)|(r>>16); }

St In Z hbi(Z cap, N co) {
  return co % cap; }

St In tble hb(O t, N code) {
  return gettbl(t)->tab[hbi(gettbl(t)->cap, code)]; }

// tblrsz(vm, tbl, new_size): destructively resize a hash table.
// new_size words of memory are allocated for the new bucket array.
// the old table entries are reused to populate the modified table.
St _ tblrsz(V v, O t, Z ns) {
  tble e, ch, *b, *d;
  Mm(t, b = memset(cells(v, ns), 0, w2b(ns)));
  Ht o = gettbl(t);
  Z u, n = o->cap;
  d = o->tab; o->tab = b; o->cap = ns;
  Wh (n--) Fo (ch = d[n]; ch;
    e = ch,
    ch = ch->next,
    u = hbi(ns, hc(v, e->key)),
    e->next = b[u],
    b[u] = e); }

St _ tblade(V v, O t, O k, O x, Z b) {
  tble e;
  with(t, with(k, with(x, e = cells(v, Size(tble)))));
  tbl y = gettbl(t);
  e->key = k, e->val = x;
  e->next = y->tab[b], y->tab[b] = e;
  ++y->len; }

O tblset(V v, O t, O k, O val) {
  Z b = hbi(gettbl(t)->cap, hc(v, k));
  tble e = gettbl(t)->tab[b];
  for (;e; e = e->next)
    if (e->key == k) return e->val = val;
  mm(&t); mm(&val);
  tblade(v, t, k, val, b);
  if (gettbl(t)->len / gettbl(t)->cap > 2)
    tblrsz(v, t, gettbl(t)->cap * 2);
  return um, um, val; }

St O tblkeys_j(V v, tble e, O l) {
  if (!e) return l;
  obj x = e->key;
  with(x, l = tblkeys_j(v, e->next, l));
  return pair(v, x, l); }

St O tblkeys_i(V v, O t, Z i) {
  if (i == gettbl(t)->cap) return nil;
  obj k;
  with(t, k = tblkeys_i(v, t, i+1));
  return tblkeys_j(v, gettbl(t)->tab[i], k); }

obj tblkeys(vm v, obj t) {
  return tblkeys_i(v, t, 0); }

O tbldel(vm v, obj t, obj k) {
  Ht y = gettbl(t);
  O r = nil;
  Z b = hbi(y->cap, hc(v, k));
  tble e = y->tab[b];
  struct tble _v = {0,0,e};
  for (tble l = &_v; l && l->next; l = l->next)
    if (l->next->key == k) {
      r = l->next->val;
      l->next = l->next->next;
      y->len--;
      break; }
  y->tab[b] = _v.next;
  if (y->len && y->cap / y->len > 2)
    with(r, with(t, tblrsz(v, t, y->cap / 2)));
  return r; }

O tblget(vm v, obj t, obj k) {
  tble e = hb(t, hc(v, k));
  Fo (;e; e = e->next) if (eql(e->key, k)) R e->val;
  R 0; }

O table(vm v) {
  tbl t = cells(v, sizeof(struct tbl)/w2b(1) + 1);
  tble *b = (tble*)(t+1);
  *b = NULL;
  t->tab = b;
  t->len = 0;
  t->cap = 1;
  return puttbl(t); }
