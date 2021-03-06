# lips
it's a [lisp][lisp] dialect with a fast virtual machine that
runs [forth][forth]-style [threaded code][threaded]. it works
on AMD64/ARM64 and probably on other 64-bit platforms, but will
most likely crash on 32-bit because of certain technical
assumptions. specifically it relies on these platform
features:
- 8-byte pointer alignment
- no more than 61 bits of code address space (so function
  pointers can go in fixnums)
- sign-extended bit shifting (seems to be the norm but
  not required by the C standard)
- pass at least 6 function arguments in registers
- tail call optimization


## usage
see ([1][make]) and ([2][lips_h]).

## lisp dialect
parentheses, semicolons, and single and double quotes all do
what you would expect. in general functions are call by value
with unspecified order, but this rule is frequently violated in
special cases when the function is known at compile time (eg.
to permit shortcut evaluation). user visible data include
integers, symbols, strings, pairs, functions, hash tables, and
nil, which is self quoting and false. the compiler does a
certain amount of static type and arity checking, so some
obvious programming errors will be caught at compile time.

the examples below are given with their equivalents in scheme.

### special forms
#### `\` lambda
- `(\) = (lambda () '())`
- `(\ a) = (lambda () a)`
- `(\ a a) = (lambda (a) a)`
- `(\ a b (a b)) = (lambda (a b) (a b))`
- `(\ a b . (a b)) = (lambda (a . b) (a b))`

lambdas work on one expression, because there's usually less
parentheses that way. use `,` for sequencing. calling a
function without enough arguments is an error, but too many
is allowed (they're ignored). unlike in scheme the `.` is an
ordinary symbol with special meaning in a lambda list.

#### `,` begin
- `(,) = '()`
- `(, a) = a`
- `(, a b) = (begin a b)`

evaluate expressions left to right. the expression has the
value of its last argument, or nil.

#### `:` define
- `(:) = '()`
- `(: a) = a`
- `(: a b) = (define a b)`
- `(: a b c d) = (define a b) (define c d)`
- `(: a b c d e) = (let* ((a b) (c d)) e)`

definitions are immutable. they are expressions with the value
of their last argument (or nil), so no restrictions apply on
where in a function they may occur. only the even form has an
effect on the current scope.

#### `?` cond
- `(?) = '()`
- `(? x) = x`
- `(? a b) = (cond (a b) (#t '()))`
- `(? a b c) = (cond (a b) (#t c))`
- `(? a b c d) = (cond (a b) (c d) (#t '()))`

etc. nil is the only false value.

#### <code>\`</code> quote
- <code>(\`) = '()</code>
- <code>(\` x) = 'x</code>

the apostrophe still works too.

### functions
this whole section is unstable and  some of these names are
bad, sorry, you know what they say about naming things!
- `ev = eval` `ap = apply` `ccc = call/cc`
- `.` print arguments separated by spaces then newline
- `+` `-` `*` `/` `%` what you think
- `&&` `||` left-to-right, shortcut eval when bound early.
- `<` `<=` `=` `>=` `>` = is recursive with value semantics
  on lists etc. order operations are currently well defined
  only on numbers, but you won't get a type error. all
  comparison functions are variadic and test their arguments
  pairwise left-to-right, with shortcut evaluation when bound
  early.
- there's no built-in `list` but `list = (\ x . x)`
- `*: = car` `:* = cdr` `:: = cons` `*! = set-car!` `!* = set-cdr!`
- `homp` `nump` `twop` `symp` `nilp` type predicates
- hash functions: nullary constructor `tbl`; `tbl-set tbl-get tbl-has tbl-keys tbl-len tbl-del`
- string functions: n-ary constructor `(str 97 97 97) = "aaa"` ; `str-len str-get`

### code examples
#### fizzbuzz
```lisp
(: /p (\ m n (= 0 (% m n)))
   fb (\ m n (? (<= m n) (,
     (. (? (/p m 15) 'fb
           (/p m 5)  'b
           (/p m 3)  'f
           m))
     (fb (+ m 1) n))))
   (fb 1 100))
```

#### a quine
```lisp
(: li (\ x . x) ; list
   quine ((\ q ((ev q) q)) '(\ i (li i (li '` i)))))
; it's ((\ i (li i (li '` i))) '(\ i (li i (li '` i))))
```
## missing features
- macros!!!
- exceptions
- basic string functions
- arrays and many other types
- unicode
- useful i/o
- namespaces / module system

the compiler still lacks several basic features:
- control flow analysis
- dead code elimination
- beta reduction (inlining)
- delta reduction (constant folding)
- a real type system
- native code generation

the memory manager is very simple and might benefit from these
improvements:
- semispaces, which would eliminate most calls to malloc() and
  ensure that recovery from oom is always possible.
- generations, which would make memory use scale better with
  large amounts of persistent data but would add complexity by
  requiring a write barrier.

[lisp]: https://en.wikipedia.org/wiki/Lisp_(programming_language)
[forth]: https://en.wikipedia.org/wiki/Forth_(programming_language)
[threaded]: https://en.wikipedia.org/wiki/Threaded_code 
[make]: makefile
[lips_h]: main.c:#L29

