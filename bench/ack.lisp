(defun ack (m n)
  (if (= m 0) (+ n 1)
    (ack (- m 1) (if (= n 0) 1 (ack m (- n 1))))))
(print (ack 3 9))

