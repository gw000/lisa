(::: test (:
 (puts s) (>>= s 0 (slen s) (:
  (loop s i n)
   (? (< i n) (, (putc (sget s i)) (loop s (+ i 1) n)))))

 (term-esc string) (puts (ap str (X 0x1b (chars string))))
 (green) (term-esc "[32m")
 (red)   (term-esc "[31m")
 (reset) (term-esc "[0m")

 (color-text col s) (, (col) (puts s) (reset))
 (green-text s) (color-text green s)
 (red-text s) (color-text red s)
 box (tbl)
 (get) (tget box ())
 (set x) (tset box () x)
 (mut f) (set (f (get)))
 (T x) (L '? x (L green-text ".") (L ', (L mut (cu X x)) (L red-text "X")))
 (\ n ts . (L ',
  (set ())
  (X ', (X (L . (L '` (? (symp n) n (sym n)))) (map T ts)))
  (L .)
  (L each (L get) .)))))

(:
 (unord-eq a b) (&&
  (= (len a) (len b))
  (all (cu memq a) b)))

(: (time f) (: t0 (clock) x (f) (, (. (- (clock) t0) 'ms) x))

     (rev l) (foldl l () (\ a b (X b a)))
     (find l p) (? l (? (p (A l)) (A l) (find (B l) p)))
     (transp x) (ap zip (X L x))
     (sample l) (at l (randn (len l)))
     (intercal i l) (:
      (loop i l m) (? (~ m) l (append l (append i (intercal i m))))
      (? l (loop i (A l) (B l))))
     (abs x) (? (< x 0) (- x) x)
     (even x) (? (= 0 (& x 1)))
     (odd x) (? (= 1 (& x 1)))

)
