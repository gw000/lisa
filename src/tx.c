#include "la.h"
#include <string.h>

static long txmon(la, FILE*, ob);

// s-expression writer
long la_tx(la v, FILE *o, ob x) {
  if (nump(x)) return fprintf(o, "%ld", getnum(x));
  if (G(x) == disp) return ((mtbl) GF(x))->emit(v, o, x);
  if (primp((mo) x)) return
    fprintf(o, "\\%s", ((struct prim*)x)->nom);
  return txmon(v, o, hnom(v, (mo) x)); }

Vm(tx_f) {
  size_t i = 0, l = fp->argc;
  if (l) {
    while (i < l - 1)
      la_tx(v, stdout, fp->argv[i++]),
      fputc(' ', stdout);
    xp = fp->argv[i];
    la_tx(v, stdout, xp); }
  fputc('\n', stdout);
  return ApC(ret, xp); }

Vm(txc_f) {
  ArityCheck(1);
  return ApC(ret,
    putnum(fputc(getnum(fp->argv[0]), stdout))); }

long fputstr(FILE *o, str s) {
  long i = 0, r = s->len;
  while (i < r) if (fputc(s->text[i++], o) == EOF) return -1;
  return r; }

// FIXME this is really weird
// print a function name
static NoInline long txmon(la v, FILE *o, ob x) {
  if (symp(x)) {
    if (fputc('\\', o) == EOF) return -1;
    long r = la_tx(v, o, x);
    return r < 0 ? r : r + 1; }
  if (!twop(x))
    return fputc('\\', o) == EOF ? -1 : 1;
  long r = 0, a;
  // FIXME this is weird
  if (symp(A(x)) || twop(A(x))) {
    a = txmon(v, o, A(x));
    if (a < 0) return a;
    r += a; }
  if (symp(B(x)) || twop(B(x))) {
    a = txmon(v, o, B(x));
    if (a < 0) return a;
    r += a; }
  return r; }
