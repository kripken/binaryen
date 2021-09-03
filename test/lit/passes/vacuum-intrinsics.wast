;; NOTE: Assertions have been generated by update_lit_checks.py and should not be edited.
;; RUN: wasm-opt %s --vacuum -all -S -o - | filecheck %s

(module
  ;; CHECK:      (import "binaryen-intrinsics" "call.if.used" (func $call.if.used (param funcref) (result i32)))
  (import "binaryen-intrinsics" "call.if.used" (func $call.if.used (param funcref) (result i32)))

  ;; CHECK:      (import "binaryen-intrinsics" "call.if.used" (func $call.if.used-fj (param f32 funcref) (result i64)))
  (import "binaryen-intrinsics" "call.if.used" (func $call.if.used-fj (param f32) (param funcref) (result i64)))

  ;; CHECK:      (func $used
  ;; CHECK-NEXT:  (local $i32 i32)
  ;; CHECK-NEXT:  (local.set $i32
  ;; CHECK-NEXT:   (call $call.if.used
  ;; CHECK-NEXT:    (ref.func $i)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $used
    (local $i32 i32)
    ;; The result is used (by the local.set), so we cannot do anything here.
    (local.set $i32
      (call $call.if.used (ref.func $i))
    )
  )

  ;; CHECK:      (func $unused
  ;; CHECK-NEXT:  (nop)
  ;; CHECK-NEXT: )
  (func $unused
    ;; The result is unused, so we can remove the call.
    (drop
      (call $call.if.used (ref.func $i))
    )
  )

  ;; CHECK:      (func $unused-fj
  ;; CHECK-NEXT:  (nop)
  ;; CHECK-NEXT: )
  (func $unused-fj
    ;; As above, but with an extra float param and a different result type, and
    (drop
      (call $call.if.used-fj (f32.const 2.71828) (ref.func $fj))
    )
  )

  ;; CHECK:      (func $unused-fj-side-effects
  ;; CHECK-NEXT:  (local $f32 f32)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.tee $f32
  ;; CHECK-NEXT:    (f32.const 2.718280076980591)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.func $fj)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $unused-fj-side-effects
    (local $f32 f32)
    ;; As above, but side effects in the param. We must keep that around, and
    ;; drop it.
    (drop
      (call $call.if.used-fj
        (local.tee $f32
          (f32.const 2.71828)
        )
        (ref.func $fj)
      )
    )
  )

  ;; CHECK:      (func $i (result i32)
  ;; CHECK-NEXT:  (unreachable)
  ;; CHECK-NEXT: )
  (func $i (result i32)
    ;; Helper function for the above.
    (unreachable)
  )

  ;; CHECK:      (func $fj (param $0 f32) (result i64)
  ;; CHECK-NEXT:  (unreachable)
  ;; CHECK-NEXT: )
  (func $fj (param f32) (result i64)
    ;; Helper function for the above.
    (unreachable)
  )
)
