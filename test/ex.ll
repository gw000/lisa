
; some miscellaneous functions
(test execution

 (: (ack m n)
   (? (< m 1) (+ n 1)
    (ack (- m 1) (? (> n 0) (ack m (- n 1)) 1)))
  (= 1021 (ack 3 7)))
 
 (: (fib n)
   (? (< n 3) 1
    (+ (fib (- n 1)) (fib (- n 2))))
  (= 144 (fib 12)))
 
 (: quine ((\ q (L q (Q q))) '(\ q (L q (Q q))))
  (= quine (ev quine)))
 (: (tarai x y z) (?
     (<= x y) y
     (tarai (tarai (- x 1) y z)
            (tarai (- y 1) x z)
            (tarai (- z 1) x y)))
  (= 13 (tarai 12 13 14)))


 ; church numerals
 (: (((mul f) g) x) (f (g x))
    (((add f) g) x) ((mul (f x)) (g x))
    ((tet f) g) ((g (\ g (g f))) I)
    pow I one I zero (\ I) succ (add I)
    (C n) (? (= n 0) zero ((add one) (C (- n 1)))) ; ℕ->⛪
    (N c) ((c (\ x (+ x 1))) 0)                    ; ⛪->ℕ

    two (succ one) four (two two) five (succ four)
    ten ((mul two) five)
    one-hundred ((pow two) ten)
  (&&
    (= 100
     (N (C 100))
     (N one-hundred))
    (= 65536 (N ((tet two) four)))))

 ; hyperoperations
 (: (hy n) (? (= n 0) + (\ x y
     ((: (hz hx x y i) (? (< i y) (hx x (hz hx x y (+ i 1))) x))
      (hy (- n 1)) x y 1)))
    pow (hy 2)
    tet (hy 3)
  (= 65536 (<< 1 16) (pow 2 16) (tet 2 4))))