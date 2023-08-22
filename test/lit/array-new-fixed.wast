;; NOTE: Assertions have been generated by update_lit_checks.py and should not be edited.
;; RUN: wasm-opt -all %s -S -o - | filecheck %s

;; Check that we can optionally specify the size of the array.
(module
  ;; CHECK:      (type $array (array i32))
  (type $array (array i32))
  ;; CHECK:      (func $test (type $none_=>_none)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (array.new_fixed $array
  ;; CHECK-NEXT:    (i32.const 0)
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (array.new_fixed $array
  ;; CHECK-NEXT:    (i32.const 0)
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $test
    (drop
      (array.new_fixed $array
        (i32.const 0)
        (i32.const 1)
      )
    )
    (drop
      (array.new_fixed $array 2
        (i32.const 0)
        (i32.const 1)
      )
    )
  )
)
