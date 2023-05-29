#include "i.h"

static NoInline ob push1_gc(li v, ob x) {
  bool ok; return avec(v, x, ok = please(v, 1)),
                  ok ? (*--v->sp = x) : 0; }
ob push1(li v, ob x) {
  return avail(v) ? (*--v->sp = x) : push1_gc(v, x); }

// push things onto the stack
static NoInline bool pushsr(O v, size_t i, va_list xs) {
  bool _; ob x = va_arg(xs, ob);
  return !x ? avail(v) >= i || please(v, i) :
    (avec(v, x, _ = pushsr(v, i + 1, xs)),
     _ && (*--v->sp = x, true)); }

NoInline bool pushs(O v, ...) {
  bool _; va_list xs; return
    va_start(xs, v),
    _ = pushsr(v, 0, xs),
    va_end(xs),
    _; }

static NoInline word listr(state f, va_list xs) {
  word x = va_arg(xs, word);
  if (!x) return nil;
  word y; avec(f, x, y = listr(f, xs));
  return y ? (word) pair(f, x, y) : y; }

NoInline ob list(O f, ...) {
  ob x; va_list xs; return
    va_start(xs, f),
    x = listr(f, xs),
    va_end(xs),
    x; }
