;; NOTE: Assertions have been generated by update_lit_checks.py and should not be edited.
;; RUN: wasm-opt %s --local-subtyping -all --enable-gc-nn-locals -S -o - \
;; RUN:   | filecheck %s

(module
  ;; CHECK:      (type $struct (struct ))
  (type $struct (struct))

  ;; CHECK:      (import "out" "i32" (func $i32 (result i32)))
  (import "out" "i32" (func $i32 (result i32)))

  ;; CHECK:      (func $non-nullable
  ;; CHECK-NEXT:  (local $x (ref $struct))
  ;; CHECK-NEXT:  (local $y (ref $none_=>_i32))
  ;; CHECK-NEXT:  (local.set $x
  ;; CHECK-NEXT:   (ref.as_non_null
  ;; CHECK-NEXT:    (ref.null $struct)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.set $y
  ;; CHECK-NEXT:   (ref.func $i32)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $x)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $non-nullable
    (local $x (ref null $struct))
    (local $y anyref)
    ;; x is assigned a value that is non-nullable.
    (local.set $x
      (ref.as_non_null (ref.null $struct))
    )
    ;; y is assigned a value that is non-nullable, and also allows a more
    ;; specific heap type.
    (local.set $y
      (ref.func $i32)
    )
    ;; Verify that the presence of a get does not alter things.
    (drop
      (local.get $x)
    )
  )

  ;; CHECK:      (func $uses-default (param $i i32)
  ;; CHECK-NEXT:  (local $x (ref null $struct))
  ;; CHECK-NEXT:  (if
  ;; CHECK-NEXT:   (local.get $i)
  ;; CHECK-NEXT:   (local.set $x
  ;; CHECK-NEXT:    (ref.as_non_null
  ;; CHECK-NEXT:     (ref.null $struct)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $x)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $uses-default (param $i i32)
    (local $x (ref null $struct))
    (if
      (local.get $i)
      ;; The only set to this local uses a non-nullable type.
      (local.set $x
        (ref.as_non_null (ref.null $struct))
      )
    )
    (drop
      ;; This get may use the null value, and so we should not alter the
      ;; type of the local to be non-nullable
      (local.get $x)
    )
  )
)
