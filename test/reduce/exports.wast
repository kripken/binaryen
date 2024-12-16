;; We can remove the non-function exports, which do not show up in the
;; output that we keep fixed while reducing
(module
  (memory $m 10 20)

  (global $g i32 (i32.const 42))

  (export "m" (memory $m))
  (export "g" (global $g))

  (func $f (export "f") (result i32)
    (drop (i32.const 1234)) ;; this can also be reduced away
    (i32.const 5678)
  )
)

