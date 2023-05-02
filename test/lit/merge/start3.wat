;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.
;; RUN: wasm-merge %s first %s.second second %s.third third -all -S -o - | filecheck %s

;; Test that we merge start functions. The first module here has none, but the
;; second and third do, so we'll first copy in the second's and then merge in
;; the third's.

(module
)
;; CHECK:      (type $none_=>_none (func))

;; CHECK:      (start $merged.start)

;; CHECK:      (func $start (type $none_=>_none)
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (i32.const 1)
;; CHECK-NEXT:  )
;; CHECK-NEXT: )

;; CHECK:      (func $user (type $none_=>_none)
;; CHECK-NEXT:  (call $start)
;; CHECK-NEXT:  (call $start)
;; CHECK-NEXT: )

;; CHECK:      (func $start_2 (type $none_=>_none)
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (i32.const 2)
;; CHECK-NEXT:  )
;; CHECK-NEXT: )

;; CHECK:      (func $merged.start (type $none_=>_none)
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (i32.const 1)
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (i32.const 2)
;; CHECK-NEXT:  )
;; CHECK-NEXT: )
