;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.
;; NOTE: This test was ported using port_test.py and could be cleaned up.

;; RUN: foreach %s %t wasm-opt --optimize-instructions --all-features -S -o - | filecheck %s

(module
 ;; CHECK:      (type $i32_i32_=>_none (func (param i32 i32)))
 (type $i32_i32_=>_none (func (param i32 i32)))

 ;; CHECK:      (type $none_=>_i32 (func (result i32)))
 (type $none_=>_i32 (func (result i32)))

 ;; CHECK:      (type $i32_=>_none (func (param i32)))

 ;; CHECK:      (type $none_=>_none (func))

 ;; CHECK:      (elem declare func $fallthrough-no-params $foo)

 ;; CHECK:      (func $foo (param $0 i32) (param $1 i32)
 ;; CHECK-NEXT:  (unreachable)
 ;; CHECK-NEXT: )
 (func $foo (param i32) (param i32)
  (unreachable)
 )
 ;; CHECK:      (func $bar (param $x i32) (param $y i32)
 ;; CHECK-NEXT:  (call $foo
 ;; CHECK-NEXT:   (local.get $x)
 ;; CHECK-NEXT:   (local.get $y)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $bar (param $x i32) (param $y i32)
  ;; This call_ref should become a direct call.
  (call_ref
   (local.get $x)
   (local.get $y)
   (ref.func $foo)
  )
 )

 ;; CHECK:      (func $fallthrough (param $x i32)
 ;; CHECK-NEXT:  (call_ref
 ;; CHECK-NEXT:   (local.tee $x
 ;; CHECK-NEXT:    (i32.const 1)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (local.get $x)
 ;; CHECK-NEXT:   (block $block (result (ref $i32_i32_=>_none))
 ;; CHECK-NEXT:    (local.set $x
 ;; CHECK-NEXT:     (i32.const 2)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:    (ref.func $foo)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $fallthrough (param $x i32)
  ;; This call_ref should become a direct call, even though it doesn't have a
  ;; simple ref.func as the target - we need to look into the fallthrough, and
  ;; handle things with locals.
  (call_ref
   ;; Write to $x before the block, and write to it in the block; we should not
   ;; reorder these things as the side effects could alter what value appears
   ;; in the get of $x.
   (local.tee $x
    (i32.const 1)
   )
   (local.get $x)
   (block (result (ref $i32_i32_=>_none))
    (local.set $x
     (i32.const 2)
    )
    (ref.func $foo)
   )
  )
 )

 ;; CHECK:      (func $fallthrough-no-params (result i32)
 ;; CHECK-NEXT:  (call_ref
 ;; CHECK-NEXT:   (block $block (result (ref $none_=>_i32))
 ;; CHECK-NEXT:    (nop)
 ;; CHECK-NEXT:    (ref.func $fallthrough-no-params)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $fallthrough-no-params (result i32)
  ;; A fallthrough appears here, but there are no operands so this is easier to
  ;; optimize.
  (call_ref
   (block (result (ref $none_=>_i32))
    (nop)
    (ref.func $fallthrough-no-params)
   )
  )
 )

   ;; Add a non-nullable!

 ;; CHECK:      (func $fallthrough-unreachable
 ;; CHECK-NEXT:  (call_ref
 ;; CHECK-NEXT:   (unreachable)
 ;; CHECK-NEXT:   (unreachable)
 ;; CHECK-NEXT:   (block $block (result (ref $i32_i32_=>_none))
 ;; CHECK-NEXT:    (nop)
 ;; CHECK-NEXT:    (ref.func $foo)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $fallthrough-unreachable
  ;; If the call is not reached, do not optimize it.
  (call_ref
   (unreachable)
   (unreachable)
   (block (result (ref $i32_i32_=>_none))
    (nop)
    (ref.func $foo)
   )
  )
 )
)

