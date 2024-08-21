#include "i.h"

static Inline symbol ini_sym(void *_, string nom, uintptr_t code) {
  symbol y = _; return
    y->ap = data, y->typ = &sym_typ,
    y->nom = nom, y->code = code,
    y->l = y->r = 0, y; }

static Inline symbol ini_anon(void *_, word code) {
  symbol y = _;
  y->ap = data, y->typ = &sym_typ;
  y->nom = 0, y->code = code;
  return y;  }
static word hash_symbol(core v, word _) { return ((symbol) _)->code; }
static word copy_symbol(core f, word x, word *p0, word *t0) {
  symbol src = (symbol) x,
         dst = src->nom ?
           intern(f, (string) cp(f, (word) src->nom, p0, t0)) :
           ini_anon(bump(f, Width(struct symbol) - 2), src->code);
  return (word) (src->ap = (vm*) dst); }
static void walk_symbol(core f, word x, word *p0, word *t0) {
  f->cp += Width(struct symbol) - (((symbol)x)->nom ? 0 : 2); }

  /*
static bool atomp(string s) {
  const char cc[] = " \n\t;#()\"'";
  for (size_t i = 0; i < s->len; i++)
    for (const char *c = cc; *c; c++)
      if (s->text[i] == *c) return false;
  return true; }
  */

static void tx_symbol(core f, FILE *o, word _) {
  symbol s = (symbol) _;
  fputs(s->nom ? s->nom->text : "#anon", o); }

bool literal_equal(core f, word a, word b) {
  return a == b;
}

struct typ sym_typ = {
  .hash = hash_symbol,
  .copy = copy_symbol,
  .evac = walk_symbol,
  .equal = literal_equal,
  .emit = tx_symbol,
};


static symbol intern_r(core v, string b, symbol *y) {
  if (*y) {
    symbol z = *y;
    string a = z->nom;
    int i = strncmp(a->text, b->text,
      a->len < b->len ? a->len : b->len);
    if (i == 0) {
      if (a->len == b->len) return z;
      i = a->len < b->len ? -1 : 1; }
    return intern_r(v, b, i < 0 ? &z->l : &z->r); }
  return *y = ini_sym(bump(v, Width(struct symbol)), b,
    hash(v, putnum(hash(v, (word) b)))); }

symbol intern(core f, string b) {
  return intern_r(f, b, &f->symbols); }
