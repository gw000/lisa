#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

// thanks !!

typedef intptr_t ob;
typedef struct la *la; // what lists act on
typedef struct fr { // call frame
  ob clos, retp, subd, argc, argv[]; } *fr;
typedef struct mo *mo; // procedures
#define Vm(n, ...)\
  ob n(la v, ob xp, mo ip, ob *hp, ob *sp, fr fp)
typedef Vm(vm);
#define Gc(n) ob n(la v, ob x, size_t len0, ob *pool0)
Gc(cp);

// TODO include type data
typedef struct mtbl {
  vm *does;
  void (*emit)(la, FILE*, ob);
  ob (*copy)(la, ob, size_t, ob*);
  size_t (*hash)(la, ob);
  // tbl dyn; // TODO everything else; user methods
} *mtbl;

struct mo { vm *ll; };

// pairs
typedef struct two {
  vm *disp; mtbl mtbl;
  ob a, b; } *two;

// strings
// TODO maybe pre-hash strings for faster lookup & comparison
typedef struct str { vm *disp; mtbl mtbl; size_t len; char text[]; } *str;

// symbols
// FIXME this is a silly way to store internal symbols
// - it's slower than a hash table
// - anonymous symbols waste 2 words
typedef struct sym {
  vm *disp; mtbl mtbl;
  ob nom, code, l, r; } *sym;

// hash tables
typedef struct tbl { vm *disp; mtbl mtbl; ob *tab; size_t len, cap; } *tbl;

// grammar symbols
enum lex { Def, Cond, Lamb, Quote, Seq, Splat, Eval, LexN };

// linked list for gc protection
typedef struct keep { ob *it; struct keep *et; } *keep;

struct la {
  // vm state -- kept in CPU registers most of the time
  mo ip; // current thread
  fr fp; // top of control stack
  ob xp, // free register
     *hp, // top of heap
     *sp; // top of stack
          // sp - hp = free memory

  // memory state
  keep keep; // list of C stack addresses to copy on gc
  size_t t0, len; // memory pool size
  ob *pool, // memory pool
     topl, // global namespace
     syms, // internal symbols
     rand, // random seed
     lex[LexN]; }; // grammar symbols

// pairs
ob pair(la, ob, ob);
size_t llen(ob);

// hash tables
size_t hash(la, ob), hashb(const char*, size_t);
ob table(la),
   tbl_set(la, ob, ob, ob),
   tbl_get(la, ob, ob);

// strings & symbols
ob string(la, const char*),
   intern(la, ob),
   interns(la, const char*),
   sskc(la, ob*, ob); // FIXME a symbol-interning function that should be private

// functions
mo mkmo(la, size_t), // allocator
   ana(la, ob, ob), // compiler interface
   button(mo); // get tag at end
               //
#define Push(...) pushs(v, __VA_ARGS__, (ob) 0)
bool
  primp(ob), // is it a primitive function?
  pushs(la, ...), // push onto stack
  please(la, size_t), // gc interface
  eql(ob, ob); // logical equality

ob hnom(la, ob); // try to get function name FIXME don't expose
ob rx(la, FILE*); // read sexp
void tx(la, FILE*, ob); // write sexp

// internal libc substitutes
intptr_t lcprng(intptr_t);
void setw(void*, uintptr_t, size_t),
     cpyw(void*, const void*, size_t),
     rcpyw(void*, const void*, size_t),
     cpy8(void*, const void*, size_t);
char cmin(char);
size_t slen(const char*);
int scmp(const char*, const char*);

#define nil putnum(0)
#define F(_) ((mo)(_)+1)
#define G(_) ((mo)(_))->ll
#define FF(x) F(F(x))
#define GF(x) G(F(x))
#define A(o) ((two)(o))->a
#define B(o) ((two)(o))->b
#define AA(o) A(A(o))
#define AB(o) A(B(o))
#define BA(o) B(A(o))
#define BB(o) B(B(o))
#define Avail (v->sp - v->hp)
#define mm(r) ((v->keep = &((struct keep){(r), v->keep})))
#define um (v->keep = v->keep->et)
#define with(y,...) (mm(&(y)), (__VA_ARGS__), um)
#define Width(t) b2w(sizeof(struct t))

#define T putnum(-1)

#define Inline inline __attribute__((always_inline))
#define NoInline __attribute__((noinline))
ob nope(la, const char*, ...) NoInline; // runtime error

#define getnum(_) ((ob)(_)>>1)
#define putnum(_) (((ob)(_)<<1)|1)

#define nilp(_) ((ob)(_)==nil)
#define nump(_) ((ob)(_)&1)
#define homp(_) (!nump(_))
// these should hopefully almost always be inlined but we
// might need pointers to them.
bool strp(ob), twop(ob), tblp(ob), symp(ob);

static Inline size_t b2w(size_t b) {
  size_t quot = b / sizeof(ob), rem = b % sizeof(ob);
  return rem ? quot + 1 : quot; }

// this can give a false positive if x is a fixnum
static Inline bool livep(la v, ob x) {
  return (ob*) x >= v->pool && (ob*) x < v->pool + v->len; }

// unchecked allocator -- make sure there's enough memory!
static Inline void *bump(la v, size_t n) {
  void *x = v->hp;
  v->hp += n;
  return x; }

static Inline void *cells(la v, size_t n) {
  return Avail >= n || please(v, n) ? bump(v, n) : 0; }

_Static_assert(-1 == -1 >> 1, "signed >>");
_Static_assert(sizeof(void*) == sizeof(size_t), "size_t matches pointer size");

#define Builtins(_) _(two) _(str) _(sym) _(tbl)
#define GcProto(n) ob cp_##n(la, ob, size_t, ob*);
#define HashProto(n) size_t hash_##n(la, ob);
#define EmProto(n) void em_##n(la, FILE*, ob);
#define DoProto(n) Vm(do_##n);
Builtins(GcProto) Builtins(HashProto) Builtins(EmProto) Builtins(DoProto)
#undef GcProto
#undef HashProto
#undef EmProto
#undef DoProto

struct prim { vm *go; const char *nom; };
extern struct prim primitives[];

extern struct mtbl
  s_mtbl_two,
  s_mtbl_str,
  s_mtbl_tbl,
  s_mtbl_sym;
#define mtbl_str (&s_mtbl_str)
#define mtbl_two (&s_mtbl_two)
#define mtbl_sym (&s_mtbl_sym)
#define mtbl_tbl (&s_mtbl_tbl)

static Inline size_t ror(size_t x, size_t n) {
  return (x<<((8*sizeof(size_t))-n))|(x>>n); }
