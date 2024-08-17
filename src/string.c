#include "i.h"

static word cp_str(state v, word x, word *p0, word *t0) {
  string src = (string) x;
  size_t len = sizeof(struct string) + src->len;
  return (word) (src->ap = memcpy(bump(v, b2w(len)), src, len)); }

static void wk_str(state v, word x, word *p0, word *t0) {
  v->cp += b2w(sizeof(struct string) + ((string) x)->len); }
  
static bool atomp(string s) {
  const char cc[] = " \n\t;#()\"'";
  for (size_t i = 0; i < s->len; i++)
    for (const char *c = cc; *c; c++)
      if (s->text[i] == *c) return false;
  return true; }

static void tx_str(core v, FILE *o, word _) {
  string s = (string) _;
  size_t len = s->len;
  if (atomp(s)) for (size_t l = 0; l < len; l++) putc(s->text[l], o);
  else {
    const char *text = s->text;
    putc('"', o);
    for (char c; len--; putc(c, o))
      if ((c = *text++) == '\\' || c == '"') putc('\\', o);
    putc('"', o); } }

static bool eq_str(state f, word x, word y) {
  if (!hstrp((thread) y)) return false;
  string a = (string) x, b = (string) y;
  if (a->len != b->len) return false;
  return 0 == strncmp(a->text, b->text, a->len); }

struct typ typ_str = {
  .copy = cp_str, .evac = wk_str, .emit = tx_str, .equal = eq_str, };

Vm(slen) {
  word x = sp[0];
  ip = (thread) sp[1];
  sp[1] = strp(x) ? putnum(((string)x)->len) : nil;
  return ip->ap(f, ip, hp, sp + 1); }

#define max(a, b) ((a)>(b)?(a):(b))
#define min(a, b) ((a)<(b)?(a):(b))
Vm(ssub) {
  thread r = (thread) sp[3];
  if (!strp(sp[0])) sp[3] = nil;
  else {
    string s = (string) sp[0];
    size_t i = nump(sp[1]) ? getnum(sp[1]) : 0,
           j = nump(sp[2]) ? getnum(sp[2]) : 0;
    i = max(i, 0), j = min(j, s->len);
    Have(Width(struct string) + b2w(j - i));
    string t = ini_str((string) hp, j - i);
    memcpy(t->text, s->text + i, j);
    sp[3] = (word) t; }
  return r->ap(f, r, hp, sp + 3); }

Vm(sget) {
  thread r = (thread) sp[2];
  if (!strp(sp[0])) sp[2] = nil;
  else {
    string s = (string) sp[0];
    size_t i = min(s->len - 1, getnum(sp[1]));
    i = max(i, 0);
    sp[2] = putnum(s->text[i]); }
  return r->ap(f, r, hp, sp + 2); }

string ini_str(string s, size_t len) {
  s->ap = data, s->typ = &typ_str, s->len = len;
  return s; }

string strof(core f, const char *c) {
  size_t len = strlen(c);
  string o = cells(f, Width(struct string) + b2w(len));
  if (o) memcpy(ini_str(o, len)->text, c, len);
  return o; }
