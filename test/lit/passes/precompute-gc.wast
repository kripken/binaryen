;; NOTE: Assertions have been generated by update_lit_checks.py and should not be edited.

;; RUN: wasm-opt %s --remove-unused-names --precompute-propagate --fuzz-exec -all -S -o - \
;; RUN:   | filecheck %s

(module
 ;; CHECK:      (type $empty (struct ))
 (type $empty (struct))

 ;; CHECK:      (type $struct (struct (field (mut i32))))
 (type $struct (struct (mut i32)))

 ;; two incompatible struct types
 (type $A (struct (field (mut f32))))

 ;; CHECK:      (type $func-return-i32 (func (result i32)))

 ;; CHECK:      (type $B (struct (field (mut f64))))
 (type $B (struct (field (mut f64))))

 (type $struct_i8 (struct (field i8)))

 (type $func-return-i32 (func (result i32)))

 ;; CHECK:      (import "fuzzing-support" "log-i32" (func $log (type $4) (param i32)))
 (import "fuzzing-support" "log-i32" (func $log (param i32)))

 ;; CHECK:      (func $test-fallthrough (type $func-return-i32) (result i32)
 ;; CHECK-NEXT:  (local $x funcref)
 ;; CHECK-NEXT:  (local.set $x
 ;; CHECK-NEXT:   (block (result nullfuncref)
 ;; CHECK-NEXT:    (drop
 ;; CHECK-NEXT:     (call $test-fallthrough)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:    (ref.null nofunc)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (i32.const 1)
 ;; CHECK-NEXT: )
 (func $test-fallthrough (result i32)
  (local $x funcref)
  (local.set $x
   ;; the fallthrough value should be used. for that to be possible with a block
   ;; we need for it not to have a name, which is why --remove-unused-names is
   ;; run
   (block (result funcref)
    ;; make a call so the block is not trivially removable
    (drop
     (call $test-fallthrough)
    )
    (ref.null func)
   )
  )
  ;; the null in the local should be propagated to here
  (ref.is_null
   (local.get $x)
  )
 )

 ;; CHECK:      (func $load-from-struct (type $3)
 ;; CHECK-NEXT:  (local $x (ref null $struct))
 ;; CHECK-NEXT:  (local.set $x
 ;; CHECK-NEXT:   (struct.new $struct
 ;; CHECK-NEXT:    (i32.const 1)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (call $log
 ;; CHECK-NEXT:   (struct.get $struct 0
 ;; CHECK-NEXT:    (local.get $x)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (local.set $x
 ;; CHECK-NEXT:   (struct.new $struct
 ;; CHECK-NEXT:    (i32.const 2)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (call $log
 ;; CHECK-NEXT:   (struct.get $struct 0
 ;; CHECK-NEXT:    (local.get $x)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (struct.set $struct 0
 ;; CHECK-NEXT:   (local.get $x)
 ;; CHECK-NEXT:   (i32.const 3)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (call $log
 ;; CHECK-NEXT:   (struct.get $struct 0
 ;; CHECK-NEXT:    (local.get $x)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $load-from-struct
  (local $x (ref null $struct))
  (local.set $x
   (struct.new $struct
    (i32.const 1)
   )
  )
  ;; we don't precompute these, as we don't know if the GC data was modified
  ;; elsewhere (we'd need immutability or escape analysis)
  (call $log
   (struct.get $struct 0 (local.get $x))
  )
  ;; Assign a new struct
  (local.set $x
   (struct.new $struct
    (i32.const 2)
   )
  )
  (call $log
   (struct.get $struct 0 (local.get $x))
  )
  ;; Assign a new value
  (struct.set $struct 0
   (local.get $x)
   (i32.const 3)
  )
  (call $log
   (struct.get $struct 0 (local.get $x))
  )
 )
 ;; CHECK:      (func $load-from-struct-bad-merge (type $4) (param $i i32)
 ;; CHECK-NEXT:  (local $x (ref null $struct))
 ;; CHECK-NEXT:  (if
 ;; CHECK-NEXT:   (local.get $i)
 ;; CHECK-NEXT:   (then
 ;; CHECK-NEXT:    (local.set $x
 ;; CHECK-NEXT:     (struct.new $struct
 ;; CHECK-NEXT:      (i32.const 1)
 ;; CHECK-NEXT:     )
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (else
 ;; CHECK-NEXT:    (local.set $x
 ;; CHECK-NEXT:     (struct.new $struct
 ;; CHECK-NEXT:      (i32.const 2)
 ;; CHECK-NEXT:     )
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (call $log
 ;; CHECK-NEXT:   (struct.get $struct 0
 ;; CHECK-NEXT:    (local.get $x)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $load-from-struct-bad-merge (param $i i32)
  (local $x (ref null $struct))
  ;; a merge of two different $x values cannot be precomputed
  (if
   (local.get $i)
   (then
    (local.set $x
     (struct.new $struct
      (i32.const 1)
     )
    )
   )
   (else
    (local.set $x
     (struct.new $struct
      (i32.const 2)
     )
    )
   )
  )
  (call $log
   (struct.get $struct 0 (local.get $x))
  )
 )
 ;; CHECK:      (func $modify-gc-heap (type $5) (param $x (ref null $struct))
 ;; CHECK-NEXT:  (struct.set $struct 0
 ;; CHECK-NEXT:   (local.get $x)
 ;; CHECK-NEXT:   (i32.add
 ;; CHECK-NEXT:    (struct.get $struct 0
 ;; CHECK-NEXT:     (local.get $x)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:    (i32.const 1)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $modify-gc-heap (param $x (ref null $struct))
  (struct.set $struct 0
   (local.get $x)
   (i32.add
    (struct.get $struct 0
     (local.get $x)
    )
    (i32.const 1)
   )
  )
 )
 ;; --fuzz-exec verifies the output of this function, checking that the change
 ;; makde in modify-gc-heap is not ignored
 ;; CHECK:      (func $load-from-struct-bad-escape (type $3)
 ;; CHECK-NEXT:  (local $x (ref null $struct))
 ;; CHECK-NEXT:  (local.set $x
 ;; CHECK-NEXT:   (struct.new $struct
 ;; CHECK-NEXT:    (i32.const 1)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (call $modify-gc-heap
 ;; CHECK-NEXT:   (local.get $x)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (call $log
 ;; CHECK-NEXT:   (struct.get $struct 0
 ;; CHECK-NEXT:    (local.get $x)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $load-from-struct-bad-escape (export "test")
  (local $x (ref null $struct))
  (local.set $x
   (struct.new $struct
    (i32.const 1)
   )
  )
  (call $modify-gc-heap
   (local.get $x)
  )
  (call $log
   (struct.get $struct 0 (local.get $x))
  )
 )
 ;; CHECK:      (func $load-from-struct-bad-arrive (type $5) (param $x (ref null $struct))
 ;; CHECK-NEXT:  (call $log
 ;; CHECK-NEXT:   (struct.get $struct 0
 ;; CHECK-NEXT:    (local.get $x)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $load-from-struct-bad-arrive (param $x (ref null $struct))
  ;; a parameter cannot be precomputed
  (call $log
   (struct.get $struct 0 (local.get $x))
  )
 )
 ;; CHECK:      (func $ref-comparisons (type $11) (param $x (ref null $struct)) (param $y (ref null $struct))
 ;; CHECK-NEXT:  (local $z (ref null $struct))
 ;; CHECK-NEXT:  (local $w (ref null $struct))
 ;; CHECK-NEXT:  (call $log
 ;; CHECK-NEXT:   (ref.eq
 ;; CHECK-NEXT:    (local.get $x)
 ;; CHECK-NEXT:    (local.get $y)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (call $log
 ;; CHECK-NEXT:   (ref.eq
 ;; CHECK-NEXT:    (local.get $x)
 ;; CHECK-NEXT:    (ref.null none)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (call $log
 ;; CHECK-NEXT:   (ref.eq
 ;; CHECK-NEXT:    (local.get $x)
 ;; CHECK-NEXT:    (ref.null none)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (call $log
 ;; CHECK-NEXT:   (i32.const 1)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $ref-comparisons
  (param $x (ref null $struct))
  (param $y (ref null $struct))
  (local $z (ref null $struct))
  (local $w (ref null $struct))
  ;; incoming parameters are unknown
  (call $log
   (ref.eq
    (local.get $x)
    (local.get $y)
   )
  )
  (call $log
   (ref.eq
    (local.get $x)
    ;; locals are ref.null which are known, and will be propagated
    (local.get $z)
   )
  )
  (call $log
   (ref.eq
    (local.get $x)
    (local.get $w)
   )
  )
  ;; null-initialized locals are known and can be compared
  (call $log
   (ref.eq
    (local.get $z)
    (local.get $w)
   )
  )
 )
 ;; CHECK:      (func $new-ref-comparisons (type $func-return-i32) (result i32)
 ;; CHECK-NEXT:  (local $x (ref null $struct))
 ;; CHECK-NEXT:  (local $y (ref null $struct))
 ;; CHECK-NEXT:  (local $tempresult i32)
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (block (result i32)
 ;; CHECK-NEXT:    (drop
 ;; CHECK-NEXT:     (block (result i32)
 ;; CHECK-NEXT:      (drop
 ;; CHECK-NEXT:       (block (result i32)
 ;; CHECK-NEXT:        (local.set $x
 ;; CHECK-NEXT:         (struct.new $struct
 ;; CHECK-NEXT:          (i32.const 1)
 ;; CHECK-NEXT:         )
 ;; CHECK-NEXT:        )
 ;; CHECK-NEXT:        (local.set $y
 ;; CHECK-NEXT:         (local.get $x)
 ;; CHECK-NEXT:        )
 ;; CHECK-NEXT:        (local.set $tempresult
 ;; CHECK-NEXT:         (i32.const 1)
 ;; CHECK-NEXT:        )
 ;; CHECK-NEXT:        (i32.const 1)
 ;; CHECK-NEXT:       )
 ;; CHECK-NEXT:      )
 ;; CHECK-NEXT:      (i32.const 1)
 ;; CHECK-NEXT:     )
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:    (i32.const 1)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (i32.const 1)
 ;; CHECK-NEXT: )
 (func $new-ref-comparisons (result i32)
  (local $x (ref null $struct))
  (local $y (ref null $struct))
  (local $tempresult i32)
  (local.set $x
   (struct.new $struct
    (i32.const 1)
   )
  )
  (local.set $y
   (local.get $x)
  )
  ;; assign the result, so that propagate calculates the ref.eq. both $x and $y
  ;; must refer to the same data, so we can precompute a 1 here.
  (local.set $tempresult
   (ref.eq
    (local.get $x)
    (local.get $y)
   )
  )
  ;; and that 1 is propagated to here.
  (local.get $tempresult)
 )
 ;; CHECK:      (func $propagate-equal (type $func-return-i32) (result i32)
 ;; CHECK-NEXT:  (local $tempresult i32)
 ;; CHECK-NEXT:  (local $tempref (ref null $empty))
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (block (result i32)
 ;; CHECK-NEXT:    (drop
 ;; CHECK-NEXT:     (block (result i32)
 ;; CHECK-NEXT:      (drop
 ;; CHECK-NEXT:       (block (result i32)
 ;; CHECK-NEXT:        (local.set $tempresult
 ;; CHECK-NEXT:         (block (result i32)
 ;; CHECK-NEXT:          (drop
 ;; CHECK-NEXT:           (block (result i32)
 ;; CHECK-NEXT:            (drop
 ;; CHECK-NEXT:             (block (result i32)
 ;; CHECK-NEXT:              (drop
 ;; CHECK-NEXT:               (ref.eq
 ;; CHECK-NEXT:                (local.tee $tempref
 ;; CHECK-NEXT:                 (struct.new_default $empty)
 ;; CHECK-NEXT:                )
 ;; CHECK-NEXT:                (local.get $tempref)
 ;; CHECK-NEXT:               )
 ;; CHECK-NEXT:              )
 ;; CHECK-NEXT:              (i32.const 1)
 ;; CHECK-NEXT:             )
 ;; CHECK-NEXT:            )
 ;; CHECK-NEXT:            (i32.const 1)
 ;; CHECK-NEXT:           )
 ;; CHECK-NEXT:          )
 ;; CHECK-NEXT:          (i32.const 1)
 ;; CHECK-NEXT:         )
 ;; CHECK-NEXT:        )
 ;; CHECK-NEXT:        (i32.const 1)
 ;; CHECK-NEXT:       )
 ;; CHECK-NEXT:      )
 ;; CHECK-NEXT:      (i32.const 1)
 ;; CHECK-NEXT:     )
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:    (i32.const 1)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (i32.const 1)
 ;; CHECK-NEXT: )
 (func $propagate-equal (result i32)
  (local $tempresult i32)
  (local $tempref (ref null $empty))
  ;; assign the result, so that propagate calculates the ref.eq
  (local.set $tempresult
   (ref.eq
    ;; allocate one struct
    (local.tee $tempref
     (struct.new $empty)
    )
    (local.get $tempref)
   )
  )
  ;; we can compute a 1 here as the ref.eq compares a struct to itself. note
  ;; that the ref.eq itself cannot be precomputed away (as it has side effects).
  (local.get $tempresult)
 )
 ;; CHECK:      (func $propagate-unequal (type $func-return-i32) (result i32)
 ;; CHECK-NEXT:  (local $tempresult i32)
 ;; CHECK-NEXT:  (local $tempref (ref null $empty))
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (block (result i32)
 ;; CHECK-NEXT:    (drop
 ;; CHECK-NEXT:     (block (result i32)
 ;; CHECK-NEXT:      (drop
 ;; CHECK-NEXT:       (block (result i32)
 ;; CHECK-NEXT:        (local.set $tempresult
 ;; CHECK-NEXT:         (i32.const 0)
 ;; CHECK-NEXT:        )
 ;; CHECK-NEXT:        (i32.const 0)
 ;; CHECK-NEXT:       )
 ;; CHECK-NEXT:      )
 ;; CHECK-NEXT:      (i32.const 0)
 ;; CHECK-NEXT:     )
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:    (i32.const 0)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (i32.const 0)
 ;; CHECK-NEXT: )
 (func $propagate-unequal (result i32)
  (local $tempresult i32)
  (local $tempref (ref null $empty))
  ;; assign the result, so that propagate calculates the ref.eq.
  ;; the structs are different, so we will precompute a 0 here, and as creating
  ;; heap data does not have side effects, we can in fact replace the ref.eq
  ;; with that value
  (local.set $tempresult
   ;; allocate two different structs
   (ref.eq
    (struct.new $empty)
    (struct.new $empty)
   )
  )
  (local.get $tempresult)
 )

 ;; CHECK:      (func $propagate-uncertain-param (type $6) (param $input (ref $empty)) (result i32)
 ;; CHECK-NEXT:  (local $tempresult i32)
 ;; CHECK-NEXT:  (local $tempref (ref null $empty))
 ;; CHECK-NEXT:  (local.set $tempresult
 ;; CHECK-NEXT:   (ref.eq
 ;; CHECK-NEXT:    (struct.new_default $empty)
 ;; CHECK-NEXT:    (local.get $input)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (local.get $tempresult)
 ;; CHECK-NEXT: )
 (func $propagate-uncertain-param (param $input (ref $empty)) (result i32)
  (local $tempresult i32)
  (local $tempref (ref null $empty))
  (local.set $tempresult
   ;; allocate a struct and compare it to a param, which we know nothing about,
   ;; so we can infer nothing here at all.
   (ref.eq
    (struct.new $empty)
    (local.get $input)
   )
  )
  (local.get $tempresult)
 )

 ;; CHECK:      (func $propagate-different-params (type $12) (param $input1 (ref $empty)) (param $input2 (ref $empty)) (result i32)
 ;; CHECK-NEXT:  (local $tempresult i32)
 ;; CHECK-NEXT:  (local.set $tempresult
 ;; CHECK-NEXT:   (ref.eq
 ;; CHECK-NEXT:    (local.get $input1)
 ;; CHECK-NEXT:    (local.get $input2)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (local.get $tempresult)
 ;; CHECK-NEXT: )
 (func $propagate-different-params (param $input1 (ref $empty)) (param $input2 (ref $empty)) (result i32)
  (local $tempresult i32)
  (local.set $tempresult
   ;; We cannot say anything about parameters - they might alias, or not.
   (ref.eq
    (local.get $input1)
    (local.get $input2)
   )
  )
  (local.get $tempresult)
 )

 ;; CHECK:      (func $propagate-same-param (type $6) (param $input (ref $empty)) (result i32)
 ;; CHECK-NEXT:  (local $tempresult i32)
 ;; CHECK-NEXT:  (local.set $tempresult
 ;; CHECK-NEXT:   (ref.eq
 ;; CHECK-NEXT:    (local.get $input)
 ;; CHECK-NEXT:    (local.get $input)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (local.get $tempresult)
 ;; CHECK-NEXT: )
 (func $propagate-same-param (param $input (ref $empty)) (result i32)
  (local $tempresult i32)
  (local.set $tempresult
   ;; We could optimize this in principle, but atm do not.
   ;; Note that optimize-instructions can handle patterns like this.
   (ref.eq
    (local.get $input)
    (local.get $input)
   )
  )
  (local.get $tempresult)
 )

 ;; CHECK:      (func $propagate-uncertain-local (type $func-return-i32) (result i32)
 ;; CHECK-NEXT:  (local $tempresult i32)
 ;; CHECK-NEXT:  (local $tempref (ref null $empty))
 ;; CHECK-NEXT:  (local $stashedref (ref null $empty))
 ;; CHECK-NEXT:  (local.set $tempref
 ;; CHECK-NEXT:   (struct.new_default $empty)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (local.set $stashedref
 ;; CHECK-NEXT:   (local.get $tempref)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (if
 ;; CHECK-NEXT:   (call $helper
 ;; CHECK-NEXT:    (i32.const 0)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (then
 ;; CHECK-NEXT:    (local.set $tempref
 ;; CHECK-NEXT:     (struct.new_default $empty)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (local.set $tempresult
 ;; CHECK-NEXT:   (ref.eq
 ;; CHECK-NEXT:    (local.get $tempref)
 ;; CHECK-NEXT:    (local.get $stashedref)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (local.get $tempresult)
 ;; CHECK-NEXT: )
 (func $propagate-uncertain-local (result i32)
  (local $tempresult i32)
  (local $tempref (ref null $empty))
  (local $stashedref (ref null $empty))
  (local.set $tempref
   (struct.new $empty)
  )
  (local.set $stashedref
   (local.get $tempref)
  )
  ;; This if makes it impossible to know what value the ref.eq later should
  ;; return.
  (if
   (call $helper
    (i32.const 0)
   )
   (then
    (local.set $tempref
     (struct.new $empty)
    )
   )
  )
  (local.set $tempresult
   (ref.eq
    (local.get $tempref)
    (local.get $stashedref)
   )
  )
  (local.get $tempresult)
 )

 ;; CHECK:      (func $propagate-uncertain-loop (type $3)
 ;; CHECK-NEXT:  (local $tempresult i32)
 ;; CHECK-NEXT:  (local $tempref (ref null $empty))
 ;; CHECK-NEXT:  (local $stashedref (ref null $empty))
 ;; CHECK-NEXT:  (local.set $tempref
 ;; CHECK-NEXT:   (struct.new_default $empty)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (local.set $stashedref
 ;; CHECK-NEXT:   (local.get $tempref)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (loop $loop
 ;; CHECK-NEXT:   (local.set $tempresult
 ;; CHECK-NEXT:    (ref.eq
 ;; CHECK-NEXT:     (local.get $tempref)
 ;; CHECK-NEXT:     (local.get $stashedref)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (local.set $tempref
 ;; CHECK-NEXT:    (struct.new_default $empty)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (br_if $loop
 ;; CHECK-NEXT:    (call $helper
 ;; CHECK-NEXT:     (local.get $tempresult)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $propagate-uncertain-loop
  (local $tempresult i32)
  (local $tempref (ref null $empty))
  (local $stashedref (ref null $empty))
  (local.set $tempref
   (struct.new $empty)
  )
  (local.set $stashedref
   (local.get $tempref)
  )
  (loop $loop
   ;; Each iteration in this loop may see a different struct, so we cannot
   ;; precompute the ref.eq here.
   (local.set $tempresult
    (ref.eq
     (local.get $tempref)
     (local.get $stashedref)
    )
   )
   (local.set $tempref
    (struct.new $empty)
   )
   (br_if $loop
    (call $helper
     (local.get $tempresult)
    )
   )
  )
 )

 ;; CHECK:      (func $propagate-certain-loop (type $3)
 ;; CHECK-NEXT:  (local $tempresult i32)
 ;; CHECK-NEXT:  (local $tempref (ref null $empty))
 ;; CHECK-NEXT:  (local $stashedref (ref null $empty))
 ;; CHECK-NEXT:  (local.set $tempref
 ;; CHECK-NEXT:   (struct.new_default $empty)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (local.set $stashedref
 ;; CHECK-NEXT:   (local.get $tempref)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (loop $loop
 ;; CHECK-NEXT:   (local.set $tempresult
 ;; CHECK-NEXT:    (i32.const 1)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (br_if $loop
 ;; CHECK-NEXT:    (call $helper
 ;; CHECK-NEXT:     (i32.const 1)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $propagate-certain-loop
  (local $tempresult i32)
  (local $tempref (ref null $empty))
  (local $stashedref (ref null $empty))
  ;; As above, but remove the new in the loop, so that each loop iteration does
  ;; in fact have the ref locals identical, and we can precompute a 1.
  (local.set $tempref
   (struct.new $empty)
  )
  (local.set $stashedref
   (local.get $tempref)
  )
  (loop $loop
   (local.set $tempresult
    (ref.eq
     (local.get $tempref)
     (local.get $stashedref)
    )
   )
   (br_if $loop
    (call $helper
     (local.get $tempresult)
    )
   )
  )
 )

 ;; CHECK:      (func $propagate-certain-loop-2 (type $3)
 ;; CHECK-NEXT:  (local $tempresult i32)
 ;; CHECK-NEXT:  (local $tempref (ref null $empty))
 ;; CHECK-NEXT:  (local $stashedref (ref null $empty))
 ;; CHECK-NEXT:  (loop $loop
 ;; CHECK-NEXT:   (local.set $tempref
 ;; CHECK-NEXT:    (struct.new_default $empty)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (local.set $stashedref
 ;; CHECK-NEXT:    (local.get $tempref)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (local.set $tempresult
 ;; CHECK-NEXT:    (i32.const 1)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (br_if $loop
 ;; CHECK-NEXT:    (call $helper
 ;; CHECK-NEXT:     (i32.const 1)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $propagate-certain-loop-2
  (local $tempresult i32)
  (local $tempref (ref null $empty))
  (local $stashedref (ref null $empty))
  (loop $loop
   ;; Another example of a loop where we can optimize. Here the new is inside
   ;; the loop.
   (local.set $tempref
    (struct.new $empty)
   )
   (local.set $stashedref
    (local.get $tempref)
   )
   (local.set $tempresult
    (ref.eq
     (local.get $tempref)
     (local.get $stashedref)
    )
   )
   (br_if $loop
    (call $helper
     (local.get $tempresult)
    )
   )
  )
 )

 ;; CHECK:      (func $propagate-possibly-certain-loop (type $3)
 ;; CHECK-NEXT:  (local $tempresult i32)
 ;; CHECK-NEXT:  (local $tempref (ref null $empty))
 ;; CHECK-NEXT:  (local $stashedref (ref null $empty))
 ;; CHECK-NEXT:  (loop $loop
 ;; CHECK-NEXT:   (if
 ;; CHECK-NEXT:    (call $helper
 ;; CHECK-NEXT:     (i32.const 0)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:    (then
 ;; CHECK-NEXT:     (local.set $tempref
 ;; CHECK-NEXT:      (struct.new_default $empty)
 ;; CHECK-NEXT:     )
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (local.set $stashedref
 ;; CHECK-NEXT:    (local.get $tempref)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (local.set $tempresult
 ;; CHECK-NEXT:    (ref.eq
 ;; CHECK-NEXT:     (local.get $tempref)
 ;; CHECK-NEXT:     (local.get $stashedref)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (br_if $loop
 ;; CHECK-NEXT:    (call $helper
 ;; CHECK-NEXT:     (local.get $tempresult)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $propagate-possibly-certain-loop
  (local $tempresult i32)
  (local $tempref (ref null $empty))
  (local $stashedref (ref null $empty))
  (loop $loop
   ;; As above, but move the set of $stashedref below the if. That means that
   ;; it must be identical to $tempref in each iteration. However, that is
   ;; something we cannot infer atm (while SSA could), so we do not infer
   ;; anything here for now.
   (if
    (call $helper
     (i32.const 0)
    )
    (then
     (local.set $tempref
      (struct.new $empty)
     )
    )
   )
   (local.set $stashedref
    (local.get $tempref)
   )
   (local.set $tempresult
    (ref.eq
     (local.get $tempref)
     (local.get $stashedref)
    )
   )
   (br_if $loop
    (call $helper
     (local.get $tempresult)
    )
   )
  )
 )

 ;; CHECK:      (func $helper (type $13) (param $0 i32) (result i32)
 ;; CHECK-NEXT:  (unreachable)
 ;; CHECK-NEXT: )
 (func $helper (param i32) (result i32)
  (unreachable)
 )

 ;; CHECK:      (func $odd-cast-and-get (type $3)
 ;; CHECK-NEXT:  (local $temp (ref null $B))
 ;; CHECK-NEXT:  (local.set $temp
 ;; CHECK-NEXT:   (ref.null none)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (block ;; (replaces something unreachable we can't emit)
 ;; CHECK-NEXT:    (drop
 ;; CHECK-NEXT:     (ref.null none)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:    (unreachable)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $odd-cast-and-get
  (local $temp (ref null $B))
  ;; Try to cast a null of A to B. While the types are incompatible, ref.cast
  ;; returns a null when given a null (and the null must have the type that the
  ;; ref.cast null instruction has, that is, the value is a null of type $B). So this
  ;; is an odd cast that "works".
  (local.set $temp
   (ref.cast (ref null $B)
    (ref.null $A)
   )
  )
  (drop
   ;; Read from the local, which precompute should set to a null with the proper
   ;; type.
   (struct.get $B 0
    (local.get $temp)
   )
  )
 )

 ;; CHECK:      (func $odd-cast-and-get-tuple (type $3)
 ;; CHECK-NEXT:  (local $temp (tuple (ref null $B) i32))
 ;; CHECK-NEXT:  (local.set $temp
 ;; CHECK-NEXT:   (tuple.make 2
 ;; CHECK-NEXT:    (ref.null none)
 ;; CHECK-NEXT:    (i32.const 10)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (block ;; (replaces something unreachable we can't emit)
 ;; CHECK-NEXT:    (drop
 ;; CHECK-NEXT:     (ref.null none)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:    (unreachable)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $odd-cast-and-get-tuple
  (local $temp (tuple (ref null $B) i32))
  ;; As above, but with a tuple.
  (local.set $temp
   (tuple.make 2
    (ref.cast (ref null $B)
     (ref.null $A)
    )
    (i32.const 10)
   )
  )
  (drop
   (struct.get $B 0
    (tuple.extract 2 0
     (local.get $temp)
    )
   )
  )
 )

 ;; CHECK:      (func $receive-f64 (type $14) (param $0 f64)
 ;; CHECK-NEXT:  (unreachable)
 ;; CHECK-NEXT: )
 (func $receive-f64 (param f64)
  (unreachable)
 )

 ;; CHECK:      (func $odd-cast-and-get-non-null (type $15) (param $temp (ref $func-return-i32))
 ;; CHECK-NEXT:  (local.set $temp
 ;; CHECK-NEXT:   (ref.cast (ref nofunc)
 ;; CHECK-NEXT:    (ref.func $receive-f64)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (call_ref $func-return-i32
 ;; CHECK-NEXT:    (local.get $temp)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $odd-cast-and-get-non-null (param $temp (ref $func-return-i32))
  ;; Try to cast a function to an incompatible type.
  (local.set $temp
   (ref.cast (ref $func-return-i32)
    (ref.func $receive-f64)
   )
  )
  (drop
   ;; Read from the local, checking whether precompute set a value there (it
   ;; should not, as the cast fails).
   (call_ref $func-return-i32
    (local.get $temp)
   )
  )
 )

 ;; CHECK:      (func $new_block_unreachable (type $8) (result anyref)
 ;; CHECK-NEXT:  (block ;; (replaces something unreachable we can't emit)
 ;; CHECK-NEXT:   (drop
 ;; CHECK-NEXT:    (block
 ;; CHECK-NEXT:     (unreachable)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (unreachable)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $new_block_unreachable (result anyref)
  (struct.new $struct
   ;; The value is a block with an unreachable. precompute will get rid of the
   ;; block, after which fuzz-exec should not crash - this is a regression test
   ;; for us being careful in how we execute an unreachable struct.new
   (block $label$1 (result i32)
    (unreachable)
   )
  )
 )

 ;; CHECK:      (func $br_on_cast-on-creation (type $16) (result (ref $empty))
 ;; CHECK-NEXT:  (block $label (result (ref $empty))
 ;; CHECK-NEXT:   (drop
 ;; CHECK-NEXT:    (br_on_cast $label (ref $empty) (ref $empty)
 ;; CHECK-NEXT:     (struct.new_default $empty)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (unreachable)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $br_on_cast-on-creation (result (ref $empty))
  (block $label (result (ref $empty))
   (drop
    (br_on_cast $label anyref (ref $empty)
     (struct.new_default $empty)
    )
   )
   (unreachable)
  )
 )

 ;; CHECK:      (func $ref.is_null (type $4) (param $param i32)
 ;; CHECK-NEXT:  (local $ref (ref null $empty))
 ;; CHECK-NEXT:  (local.set $ref
 ;; CHECK-NEXT:   (struct.new_default $empty)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (call $helper
 ;; CHECK-NEXT:    (i32.const 0)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (local.set $ref
 ;; CHECK-NEXT:   (ref.null none)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (call $helper
 ;; CHECK-NEXT:    (i32.const 1)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (if
 ;; CHECK-NEXT:   (local.get $param)
 ;; CHECK-NEXT:   (then
 ;; CHECK-NEXT:    (local.set $ref
 ;; CHECK-NEXT:     (struct.new_default $empty)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (call $helper
 ;; CHECK-NEXT:    (ref.is_null
 ;; CHECK-NEXT:     (local.get $ref)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $ref.is_null (param $param i32)
  (local $ref (ref null $empty))
  ;; Test ref.null on references, and also test that we can infer multiple
  ;; assignments in the same function, without confusion between them.
  (local.set $ref
   (struct.new $empty)
  )
  (drop
   (call $helper
    ;; The reference here is definitely not null.
    (ref.is_null
     (local.get $ref)
    )
   )
  )
  (local.set $ref
   (ref.null $empty)
  )
  (drop
   (call $helper
    ;; The reference here is definitely null.
    (ref.is_null
     (local.get $ref)
    )
   )
  )
  (if
   (local.get $param)
   (then
    (local.set $ref
     (struct.new $empty)
    )
   )
  )
  (drop
   (call $helper
    ;; The reference here might be null.
    (ref.is_null
     (local.get $ref)
    )
   )
  )
 )

 ;; CHECK:      (func $remove-set (type $17) (result (ref func))
 ;; CHECK-NEXT:  (local $nn funcref)
 ;; CHECK-NEXT:  (local $i i32)
 ;; CHECK-NEXT:  (loop $loop
 ;; CHECK-NEXT:   (local.set $i
 ;; CHECK-NEXT:    (i32.const 0)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (br $loop)
 ;; CHECK-NEXT:   (return
 ;; CHECK-NEXT:    (ref.as_non_null
 ;; CHECK-NEXT:     (local.get $nn)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $remove-set (result (ref func))
  (local $nn (ref func))
  (local $i i32)
  (loop $loop
   ;; Add a local.set here in the loop, just so the entire loop is not optimized
   ;; out.
   (local.set $i
    (i32.const 0)
   )
   ;; This entire block can be precomputed into an unconditional br. That
   ;; removes the local.set, which means the local no longer validates since
   ;; there is a get without a set (the get is never reached, but the validator
   ;; does not take that into account). Fixups will turn the local nullable to
   ;; avoid that problem.
   (block
    (br_if $loop
     (i32.const 1)
    )
    (local.set $nn
     (ref.func $remove-set)
    )
   )
   (return
    (local.get $nn)
   )
  )
 )

 ;; CHECK:      (func $strings (type $18) (param $param (ref string))
 ;; CHECK-NEXT:  (local $s (ref string))
 ;; CHECK-NEXT:  (local.set $s
 ;; CHECK-NEXT:   (string.const "hello, world")
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (call $strings
 ;; CHECK-NEXT:   (string.const "hello, world")
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (call $strings
 ;; CHECK-NEXT:   (string.const "hello, world")
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $strings (param $param (ref string))
  (local $s (ref string))
  (local.set $s
   (string.const "hello, world")
  )
  ;; The constant string should be propagated twice, to both of these calls.
  (call $strings
   (local.get $s)
  )
  (call $strings
   (local.get $s)
  )
 )

 ;; CHECK:      (func $struct.new.packed (type $func-return-i32) (result i32)
 ;; CHECK-NEXT:  (i32.const 120)
 ;; CHECK-NEXT: )
 (func $struct.new.packed (result i32)
  ;; Truncation happens when we write to this packed i8 field, so the result we
  ;; read back is 0x12345678 & 0xff which is 0x78 == 120.
  (struct.get_s $struct_i8 0
   (struct.new $struct_i8
    (i32.const 0x12345678)
   )
  )
 )

 ;; CHECK:      (func $get-nonnullable-in-unreachable (type $8) (result anyref)
 ;; CHECK-NEXT:  (local $x (ref any))
 ;; CHECK-NEXT:  (local.tee $x
 ;; CHECK-NEXT:   (unreachable)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (if
 ;; CHECK-NEXT:   (i32.const 1)
 ;; CHECK-NEXT:   (then
 ;; CHECK-NEXT:    (unreachable)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (local.get $x)
 ;; CHECK-NEXT: )
 (func $get-nonnullable-in-unreachable (result anyref)
  (local $x (ref any))
  ;; We cannot read a non-nullable local without setting it first, but it is ok
  ;; to do so here because we are in unreachable code. We should also not error
  ;; about this get seeming to read the default value from the function entry
  ;; (because it does not, as the entry is not reachable from it). Nothing is
  ;; expected to be optimized here.

  ;; This unreachable set is needed for the later get to validate.
  (local.set $x
   (unreachable)
  )
  ;; This if is needed so we have an interesting enough CFG that a possible
  ;; assertion can be hit about reading the default value from the entry in a
  ;; later block.
  (if
   (i32.const 1)
   (then
    (unreachable)
   )
  )
  (local.get $x)
 )

 ;; CHECK:      (func $get-nonnullable-in-unreachable-entry (type $9) (param $x i32) (param $y (ref any))
 ;; CHECK-NEXT:  (local $0 (ref any))
 ;; CHECK-NEXT:  (unreachable)
 ;; CHECK-NEXT:  (local.set $0
 ;; CHECK-NEXT:   (local.get $y)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (loop $loop
 ;; CHECK-NEXT:   (br_if $loop
 ;; CHECK-NEXT:    (local.get $x)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (drop
 ;; CHECK-NEXT:    (local.get $0)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $get-nonnullable-in-unreachable-entry (param $x i32) (param $y (ref any))
  (local $0 (ref any))
  ;; As above, but now the first basic block is unreachable, and we need to
  ;; detect that specifically, as the block after it *does* have entries even
  ;; though it is unreachable (it is a loop, and has itself as an entry).
  (unreachable)
  (local.set $0
   (local.get $y)
  )
  (loop $loop
   (br_if $loop
    (local.get $x)
   )
   (drop
    (local.get $0)
   )
  )
 )

 ;; CHECK:      (func $get-nonnullable-in-unreachable-later-loop (type $9) (param $x i32) (param $y (ref any))
 ;; CHECK-NEXT:  (local $0 (ref any))
 ;; CHECK-NEXT:  (if
 ;; CHECK-NEXT:   (local.get $x)
 ;; CHECK-NEXT:   (then
 ;; CHECK-NEXT:    (nop)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (unreachable)
 ;; CHECK-NEXT:  (local.set $0
 ;; CHECK-NEXT:   (local.get $y)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (loop $loop
 ;; CHECK-NEXT:   (br_if $loop
 ;; CHECK-NEXT:    (local.get $x)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (drop
 ;; CHECK-NEXT:    (local.get $0)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $get-nonnullable-in-unreachable-later-loop (param $x i32) (param $y (ref any))
  (local $0 (ref any))
  ;; This |if| is added, which means the loop is later in the function.
  ;; Otherwise this is the same as before.
  (if
   (local.get $x)
   (then
    (nop)
   )
  )
  (unreachable)
  (local.set $0
   (local.get $y)
  )
  (loop $loop
   (br_if $loop
    (local.get $x)
   )
   (drop
    (local.get $0)
   )
  )
 )

 ;; CHECK:      (func $get-nonnullable-in-unreachable-tuple (type $19) (result anyref i32)
 ;; CHECK-NEXT:  (local $x (tuple (ref any) i32))
 ;; CHECK-NEXT:  (local.tee $x
 ;; CHECK-NEXT:   (unreachable)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (if
 ;; CHECK-NEXT:   (i32.const 1)
 ;; CHECK-NEXT:   (then
 ;; CHECK-NEXT:    (unreachable)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (local.get $x)
 ;; CHECK-NEXT: )
 (func $get-nonnullable-in-unreachable-tuple (result anyref i32)
  ;; As $get-nonnullable-in-unreachable but the local is a tuple (so we need to
  ;; check isDefaultable, and not just isNullable).
  (local $x (tuple (ref any) i32))
  (local.set $x
   (unreachable)
  )
  (if
   (i32.const 1)
   (then
    (unreachable)
   )
  )
  (local.get $x)
 )
)
