;; NOTE: Assertions have been generated by update_lit_checks.py and should not be edited.
;; RUN: wasm-opt %s -all --generate-stack-ir --optimize-stack-ir --roundtrip -S -o - | filecheck %s
;; RUN: wasm-opt %s -all --generate-stack-ir --optimize-stack-ir --roundtrip --nominal -S -o - | filecheck %s

(module
 (type ${i32} (struct (field i32)))
 ;; CHECK:      (export "export" (func $test))
 (export "export" (func $test))
 ;; CHECK:      (func $test (type $none_=>_none)
 ;; CHECK-NEXT:  (call $help
 ;; CHECK-NEXT:   (rtt.canon $\7bi32\7d)
 ;; CHECK-NEXT:   (block $label$1 (result i32)
 ;; CHECK-NEXT:    (nop)
 ;; CHECK-NEXT:    (i32.const 1)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $test
  (call $help
   (rtt.canon ${i32})
   ;; Stack IR optimizations can remove this block, leaving a nop in an odd
   ;; "stacky" location. On load, we would normally use a local to work around
   ;; that, creating a block to contain the rtt before us and the nop, and then
   ;; returning the local. But we can't use a local for an rtt, so we should not
   ;; optimize this sort of thing in stack IR.
   (block (result i32)
    (nop)
    (i32.const 1)
   )
  )
 )
 ;; CHECK:      (func $help (type $rtt_$\7bi32\7d_i32_=>_none) (param $3 (rtt $\7bi32\7d)) (param $4 i32)
 ;; CHECK-NEXT:  (nop)
 ;; CHECK-NEXT: )
 (func $help (param $3 (rtt ${i32})) (param $4 i32)
  (nop)
 )
)
