(define (ack m n)
  (if (< m 1) (+ n 1)
    (ack (- m 1) (if (> n 0) (ack m (- n 1)) 1))))
(println (ack 3 9))
(exit)
