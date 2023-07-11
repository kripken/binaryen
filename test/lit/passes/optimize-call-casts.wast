;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.
;; RUN: foreach %s %t wasm-opt -all --optimize-call-casts -S -o - | filecheck %s

(module
  ;; CHECK:      (type $funcref_ref|func|_funcref_funcref_funcref_=>_none (func (param funcref (ref func) funcref funcref funcref)))

  ;; CHECK:      (type $funcref_ref|func|_=>_none (func (param funcref (ref func))))

  ;; CHECK:      (type $ref|func|_ref|func|_ref|func|_funcref_funcref_=>_none (func (param (ref func) (ref func) (ref func) funcref funcref)))

  ;; CHECK:      (func $called (type $funcref_ref|func|_funcref_funcref_funcref_=>_none) (param $opt funcref) (param $already (ref func)) (param $also funcref) (param $no-cast funcref) (param $late-cast funcref)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.as_func
  ;; CHECK-NEXT:    (local.get $opt)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.as_func
  ;; CHECK-NEXT:    (local.get $already)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.as_func
  ;; CHECK-NEXT:    (local.get $also)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $no-cast)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (if
  ;; CHECK-NEXT:   (i32.const 0)
  ;; CHECK-NEXT:   (return)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.as_func
  ;; CHECK-NEXT:    (local.get $late-cast)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $called
    (param $opt (ref null func))  ;; optimizable
    (param $already (ref func))   ;; already refined
    (param $also (ref null func)) ;; also optimizable
    (param $no-cast (ref null func)) ;; no cast at all
    (param $late-cast (ref null func)) ;; cast is not in entry

    ;; The first and middle parameter will be optimized. This function will
    ;; remain the same, but the call will refer to a new, refined function.

    (drop
      (ref.cast func
        (local.get $opt)
      )
    )
    (drop
      (ref.cast func
        (local.get $already)
      )
    )
    (drop
      (ref.cast func
        (local.get $also)
      )
    )
    (drop
      (local.get $no-cast)
    )
    (if
      (i32.const 0)
      (return)
    )
    (drop
      (ref.cast func
        (local.get $late-cast)
      )
    )
  )

  ;; CHECK:      (func $caller (type $funcref_ref|func|_=>_none) (param $x funcref) (param $y (ref func))
  ;; CHECK-NEXT:  (call $called_2
  ;; CHECK-NEXT:   (ref.as_func
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.get $y)
  ;; CHECK-NEXT:   (ref.as_func
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.get $x)
  ;; CHECK-NEXT:   (local.get $x)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller (param $x (ref null func)) (param $y (ref func))
    ;; This will turn into a call of a new, refined function, and have casts on
    ;; the first and middle parameter.
    (call $called
      (local.get $x)
      (local.get $y)
      (local.get $x)
      (local.get $x)
      (local.get $x)
    )
  )
)

;; =============================================================================

;; CHECK:      (func $called_2 (type $ref|func|_ref|func|_ref|func|_funcref_funcref_=>_none) (param $opt (ref func)) (param $already (ref func)) (param $also (ref func)) (param $no-cast funcref) (param $late-cast funcref)
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (ref.as_func
;; CHECK-NEXT:    (local.get $opt)
;; CHECK-NEXT:   )
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (ref.as_func
;; CHECK-NEXT:    (local.get $already)
;; CHECK-NEXT:   )
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (ref.as_func
;; CHECK-NEXT:    (local.get $also)
;; CHECK-NEXT:   )
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (local.get $no-cast)
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (if
;; CHECK-NEXT:   (i32.const 0)
;; CHECK-NEXT:   (return)
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (ref.as_func
;; CHECK-NEXT:    (local.get $late-cast)
;; CHECK-NEXT:   )
;; CHECK-NEXT:  )
;; CHECK-NEXT: )
(module
  ;; CHECK:      (type $A (struct ))
  (type $A (struct))

  ;; CHECK:      (type $anyref_=>_none (func (param anyref)))

  ;; CHECK:      (type $ref?|$A|_=>_none (func (param (ref null $A))))

  ;; CHECK:      (func $two-casts (type $anyref_=>_none) (param $x anyref)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast null $A
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $A
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $two-casts (param $x (ref null any))
    ;; Two casts appear here. We will simply apply the first of the two (even
    ;; though it is less refined; in an optimized build, other passes would have
    ;; left only the more refined one anyhow, so we don't try hard).
    (drop
      (ref.cast null $A
        (local.get $x)
      )
    )
    (drop
      (ref.cast $A
        (local.get $x)
      )
    )
  )

  ;; CHECK:      (func $caller (type $anyref_=>_none) (param $x anyref)
  ;; CHECK-NEXT:  (call $two-casts_2
  ;; CHECK-NEXT:   (ref.cast null $A
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller (param $x anyref)
    (call $two-casts
      (local.get $x)
    )
  )
)
;; CHECK:      (func $two-casts_2 (type $ref?|$A|_=>_none) (param $x (ref null $A))
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (ref.cast null $A
;; CHECK-NEXT:    (local.get $x)
;; CHECK-NEXT:   )
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (drop
;; CHECK-NEXT:   (ref.cast $A
;; CHECK-NEXT:    (local.get $x)
;; CHECK-NEXT:   )
;; CHECK-NEXT:  )
;; CHECK-NEXT: )
