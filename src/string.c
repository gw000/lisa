#include "i.h"

static word copy_string(state v, word x, word *p0, word *t0) {
  string src = (string) x;
  size_t len = sizeof(struct string) + src->len;
  return (word) (src->ap = memcpy(bump(v, b2w(len)), src, len)); }

static void walk_string(state v, word x, word *p0, word *t0) {
  v->cp += Width(struct string) + b2w(((string) x)->len); }

static void print_string(core v, output o, word _) {
  string s = (string) _;
  size_t len = s->len;
  const char *text = s->text;
  o->putc(v, o, '"');
  for (char c; len--; o->putc(v, o, c))
    if ((c = *text++) == '\\' || c == '"') o->putc(v, o, '\\');
  o->putc(v, o, '"'); }

static word hash_string(core v, word _) {
  string s = (string) _;
  uintptr_t h = 1;
  size_t words = s->len / sizeof(word),
         bytes = s->len % sizeof(word);
  const char *bs = s->text + s->len - bytes;
  while (bytes--) h = mix * (h ^ (mix * bs[bytes]));
  const intptr_t *ws = (intptr_t*) s->text;
  while (words--) h = mix * (h ^ (mix * ws[words]));
  return h; }

static bool string_equal(state f, word x, word y) {
  string a = (string) x, b = (string) y;
  if (a->len != b->len) return false;
  return 0 == strncmp(a->text, b->text, a->len); }

struct typ string_type = {
  .hash = hash_string,
  .copy = copy_string,
  .evac = walk_string,
  .emit = print_string,
  .equal = string_equal, };

Vm(slen) {
  word x = sp[0];
  ip = (thread) sp[1];
  sp[1] = strp(x) ? putnum(((string)x)->len) : nil;
  return ip->ap(f, ip, hp, sp + 1); }

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
  s->ap = data, s->typ = &string_type, s->len = len;
  return s; }

