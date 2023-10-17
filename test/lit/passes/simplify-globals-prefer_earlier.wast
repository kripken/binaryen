;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.
;; NOTE: This test was ported using port_passes_tests_to_lit.py and could be cleaned up.

;; RUN: foreach %s %t wasm-opt -all --simplify-globals -S -o - | filecheck %s

;; When a global is copied into another, prefer the earlier one in later gets.
(module
  ;; CHECK:      (type $0 (func))

  ;; CHECK:      (global $global1 i32 (i32.const 42))
  (import "a" "b" (global $global1 i32))
  ;; CHECK:      (global $global2 i32 (i32.const 42))
  (global $global2 i32 (global.get $global1))
  ;; CHECK:      (global $global3 i32 (i32.const 42))
  (global $global3 i32 (global.get $global2))

  ;; CHECK:      (func $simple (type $0)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (i32.const 42)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (i32.const 42)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (i32.const 42)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $simple
    (drop
      (global.get $global1)
    )
    (drop
      (global.get $global2)
    )
    (drop
      (global.get $global3)
    )
  )
)
