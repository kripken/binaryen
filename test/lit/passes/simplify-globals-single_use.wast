;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.
;; NOTE: This test was ported using port_passes_tests_to_lit.py and could be cleaned up.

;; RUN: foreach %s %t wasm-opt -all --simplify-globals -S -o - | filecheck %s

;; $single-use has a single use, and we can fold that code into its use. That
;; global then becomes an unused import.
(module
  ;; CHECK:      (type $A (struct (field anyref)))
  (type $A (struct (field anyref)))

  (global $single-use anyref (struct.new $A
    (i31.new
      (i32.const 42)
    )
  ))

  ;; CHECK:      (import "unused" "unused" (global $single-use anyref))

  ;; CHECK:      (global $other anyref (struct.new $A
  ;; CHECK-NEXT:  (ref.i31
  ;; CHECK-NEXT:   (i32.const 42)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: ))
  (global $other anyref (global.get $single-use))
)

;; As above, but now there is a second use, so we do nothing.
(module
  ;; CHECK:      (type $A (struct (field anyref)))
  (type $A (struct (field anyref)))

  ;; CHECK:      (global $single-use anyref (struct.new $A
  ;; CHECK-NEXT:  (ref.i31
  ;; CHECK-NEXT:   (i32.const 42)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: ))
  (global $single-use anyref (struct.new $A
    (i31.new
      (i32.const 42)
    )
  ))

  ;; CHECK:      (global $other anyref (global.get $single-use))
  (global $other anyref (global.get $single-use))

  ;; CHECK:      (global $other2 anyref (global.get $single-use))
  (global $other2 anyref (global.get $single-use))
)

;; As the first testcase, but now there is a second use in function code, so
;; again we do nothing.
(module
  ;; CHECK:      (type $A (struct (field anyref)))
  (type $A (struct (field anyref)))

  ;; CHECK:      (type $1 (func))

  ;; CHECK:      (global $single-use anyref (struct.new $A
  ;; CHECK-NEXT:  (ref.i31
  ;; CHECK-NEXT:   (i32.const 42)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: ))
  (global $single-use anyref (struct.new $A
    (i31.new
      (i32.const 42)
    )
  ))

  ;; CHECK:      (global $other anyref (global.get $single-use))
  (global $other anyref (global.get $single-use))

  ;; CHECK:      (func $user (type $1)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (global.get $single-use)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $user
    (drop
      (global.get $single-use)
    )
  )
)

;; As the first testcase, but now $single-use is imported, so there is no code
;; to fold.
(module
  (type $A (struct (field anyref)))

  ;; CHECK:      (import "a" "b" (global $single-use anyref))
  (import "a" "b" (global $single-use anyref))

  ;; CHECK:      (global $other anyref (global.get $single-use))
  (global $other anyref (global.get $single-use))
)
