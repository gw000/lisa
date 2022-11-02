#include "la.h"

#ifndef DESTDIR
#define DESTDIR
#endif

static void repl(la v) {
  while (!feof(stdin)) {
    ob _ = la_rx(v, stdin);
    if (!_ && !feof(stdin)) errp(v, "# parse error\n");
    if (_ && (_ = la_ev(v, _)))
      la_tx(v, stdout, _),
      fputc('\n', stdout); } }

// takes scripts and if we want a repl, gives a thread
static mo act(la v, const char **nfs) {
  const char *nf = *nfs;
  mo k = nf ? act(v, nfs + 1) : (mo) Tupl((ob) yield);
  return k && nf ? ana_p(v, nf, (ob) k) : k; }

static mo actn(la v, const char *prelu, const char **scripts) {
  mo k = act(v, scripts);
  return k && prelu ? ana_p(v, prelu, (ob) k) : k; }

static NoInline ob la_go(la v) {
  ob xp, *hp, *sp; fr fp; mo ip;
  return Unpack(), ApN(0, xp); }

static NoInline int la_main(bool shell, const char *prelu, const char **scripts) {
  la v = la_ini();
  if (!v) return EXIT_FAILURE;
  v->ip = actn(v, prelu, scripts);
  bool _ = v->ip && la_go(v);
  if (_ && shell) repl(v);
  return la_fin(v),
    _ ? EXIT_SUCCESS : EXIT_FAILURE; }

#include <getopt.h>
int main(int ac, char **av) {
  static const char
    *prelu = DESTDIR "/lib/lisa/lisa.la",
    *usage =
      "usage: %s [options and scripts]\n"
      "with no arguments, interact\n"
      "option:\n"
      "  -h show this message\n"
      "  -i interact\n"
      "  -_ don't bootstrap\n";

  for (bool shell = ac == 1;;) switch (getopt(ac, av, "hi_")) {
    default: return EXIT_FAILURE;
    case 'h': fprintf(stdout, usage, *av); continue;
    case 'i': shell = true; continue;
    case '_': prelu = NULL; continue;
    case -1:
      av += optind;
      prelu = shell || optind != ac ? prelu : NULL;
      return la_main(shell, prelu, (const char**) av); } }
