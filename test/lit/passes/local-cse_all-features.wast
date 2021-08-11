;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.
;; NOTE: This test was ported using port_test.py and could be cleaned up.

;; RUN: foreach %s %t wasm-opt --local-cse --all-features -S -o - | filecheck %s

(module
  ;; CHECK:      (type $i32_=>_i32 (func (param i32) (result i32)))

  ;; CHECK:      (type $none_=>_none (func))

  ;; CHECK:      (type $none_=>_i64 (func (result i64)))

  ;; CHECK:      (elem declare func $calls $ref.func)

  ;; CHECK:      (func $calls (param $x i32) (result i32)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (call_ref
  ;; CHECK-NEXT:    (i32.const 10)
  ;; CHECK-NEXT:    (ref.func $calls)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (call_ref
  ;; CHECK-NEXT:    (i32.const 10)
  ;; CHECK-NEXT:    (ref.func $calls)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (i32.const 20)
  ;; CHECK-NEXT: )
  (func $calls (param $x i32) (result i32)
    (drop
      (call_ref (i32.const 10) (ref.func $calls))
    )
    (drop
      (call_ref (i32.const 10) (ref.func $calls))
    )
    (i32.const 20)
  )

  ;; CHECK:      (func $ref.func
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.func $ref.func)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.func $ref.func)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $ref.func
    ;; RefFunc and other constants should be ignored - don't undo the effects
    ;; of constant propagation.
    (drop
      (ref.func $ref.func)
    )
    (drop
      (ref.func $ref.func)
    )
  )

  ;; CHECK:      (func $many-sets (result i64)
  ;; CHECK-NEXT:  (local $temp i64)
  ;; CHECK-NEXT:  (local $1 i64)
  ;; CHECK-NEXT:  (local.set $temp
  ;; CHECK-NEXT:   (local.tee $1
  ;; CHECK-NEXT:    (i64.add
  ;; CHECK-NEXT:     (i64.const 1)
  ;; CHECK-NEXT:     (i64.const 2)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.set $temp
  ;; CHECK-NEXT:   (i64.const 9999)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.set $temp
  ;; CHECK-NEXT:   (local.get $1)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.get $temp)
  ;; CHECK-NEXT: )
  (func $many-sets (result i64)
    (local $temp i64)
    ;; Assign to $temp three times here. We can optimize the add regardless of
    ;; that, and should not be confused by the sets themselves having effects
    ;; that are in conflict (the value is what matters).
    (local.set $temp
      (i64.add
        (i64.const 1)
        (i64.const 2)
      )
    )
    (local.set $temp
      (i64.const 9999)
    )
    (local.set $temp
      (i64.add
        (i64.const 1)
        (i64.const 2)
      )
    )
    (local.get $temp)
  )

  ;; CHECK:      (func $switch-children (param $x i32) (result i32)
  ;; CHECK-NEXT:  (local $1 i32)
  ;; CHECK-NEXT:  (block $label$1 (result i32)
  ;; CHECK-NEXT:   (br_table $label$1 $label$1
  ;; CHECK-NEXT:    (local.get $1)
  ;; CHECK-NEXT:    (local.tee $1
  ;; CHECK-NEXT:     (i32.and
  ;; CHECK-NEXT:      (local.get $x)
  ;; CHECK-NEXT:      (i32.const 3)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $switch-children (param $x i32) (result i32)
    (block $label$1 (result i32)
      ;; We can optimize the two children of this switch. This test verifies
      ;; that we do so properly and do not hit an assertion involving the
      ;; ordering of the switch's children, which was incorrect in the past.
      (br_table $label$1 $label$1
        (i32.and
          (local.get $x)
          (i32.const 3)
        )
        (i32.and
          (local.get $x)
          (i32.const 3)
        )
      )
    )
  )
)

(module
  ;; CHECK:      (type $none_=>_none (func))

  ;; CHECK:      (global $glob (mut i32) (i32.const 1))
  (global $glob (mut i32) (i32.const 1))

  ;; CHECK:      (global $other-glob (mut i32) (i32.const 1))
  (global $other-glob (mut i32) (i32.const 1))

  ;; CHECK:      (func $global
  ;; CHECK-NEXT:  (local $0 i32)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.tee $0
  ;; CHECK-NEXT:    (global.get $glob)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $0)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (global.set $other-glob
  ;; CHECK-NEXT:   (i32.const 100)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $0)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (global.set $glob
  ;; CHECK-NEXT:   (i32.const 200)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (global.get $glob)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $global
    ;; We should optimize redundantglobal.get operations.
    (drop (global.get $glob))
    (drop (global.get $glob))
    ;; We can do it past a write to another global
    (global.set $other-glob (i32.const 100))
    (drop (global.get $glob))
    ;; But we can't do it past a write to our global.
    (global.set $glob (i32.const 200))
    (drop (global.get $glob))
  )
)

(module
  ;; CHECK:      (type $A (struct (field i32)))
  (type $A (struct (field i32)))

  ;; CHECK:      (type $B (array (mut i32)))
  (type $B (array (mut i32)))


  ;; CHECK:      (type $ref|$A|_=>_none (func (param (ref $A))))

  ;; CHECK:      (type $ref?|$A|_=>_none (func (param (ref null $A))))

  ;; CHECK:      (type $none_=>_none (func))

  ;; CHECK:      (type $ref?|$B|_ref|$A|_=>_none (func (param (ref null $B) (ref $A))))

  ;; CHECK:      (func $struct-gets-nullable (param $ref (ref null $A))
  ;; CHECK-NEXT:  (local $1 i32)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.tee $1
  ;; CHECK-NEXT:    (struct.get $A 0
  ;; CHECK-NEXT:     (local.get $ref)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $1)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $1)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $struct-gets-nullable (param $ref (ref null $A))
    ;; Repeated loads from a struct can be optimized, even with a nullable
    ;; reference: if we trap, it does not matter that we replaced the later
    ;; expressions).
    (drop
      (struct.get $A 0
        (local.get $ref)
      )
    )
    (drop
      (struct.get $A 0
        (local.get $ref)
      )
    )
    (drop
      (struct.get $A 0
        (local.get $ref)
      )
    )
  )

  ;; CHECK:      (func $struct-gets (param $ref (ref $A))
  ;; CHECK-NEXT:  (local $1 i32)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.tee $1
  ;; CHECK-NEXT:    (struct.get $A 0
  ;; CHECK-NEXT:     (local.get $ref)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $1)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $1)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $struct-gets (param $ref (ref $A))
    ;; Repeated loads from a struct can be optimized.
    ;;
    ;; Note that these struct.gets cannot trap as the reference is non-nullable,
    ;; so this is "easier" than the previous testcase.
    (drop
      (struct.get $A 0
        (local.get $ref)
      )
    )
    (drop
      (struct.get $A 0
        (local.get $ref)
      )
    )
    (drop
      (struct.get $A 0
        (local.get $ref)
      )
    )
  )

  ;; CHECK:      (func $non-nullable-value (param $ref (ref $A))
  ;; CHECK-NEXT:  (local $1 (ref null $A))
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.as_non_null
  ;; CHECK-NEXT:    (local.tee $1
  ;; CHECK-NEXT:     (select (result (ref $A))
  ;; CHECK-NEXT:      (local.get $ref)
  ;; CHECK-NEXT:      (local.get $ref)
  ;; CHECK-NEXT:      (i32.const 1)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.as_non_null
  ;; CHECK-NEXT:    (local.get $1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $non-nullable-value (param $ref (ref $A))
    ;; The value that is repeated is non-nullable, which we must do some fixups
    ;; for after creating a local of that type.
    (drop
      (select (result (ref $A))
        (local.get $ref)
        (local.get $ref)
        (i32.const 1)
      )
    )
    (drop
      (select (result (ref $A))
        (local.get $ref)
        (local.get $ref)
        (i32.const 1)
      )
    )
  )

  ;; CHECK:      (func $creations
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.new_with_rtt $A
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (rtt.canon $A)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.new_with_rtt $A
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (rtt.canon $A)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (array.new_with_rtt $B
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (rtt.canon $B)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (array.new_with_rtt $B
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (rtt.canon $B)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $creations
    ;; Allocating GC data has no side effects, but each allocation is unique
    ;; and so we cannot replace separate allocations with a single one.
    (drop
      (struct.new_with_rtt $A
        (i32.const 1)
        (rtt.canon $A)
      )
    )
    (drop
      (struct.new_with_rtt $A
        (i32.const 1)
        (rtt.canon $A)
      )
    )
    (drop
      (array.new_with_rtt $B
        (i32.const 1)
        (i32.const 1)
        (rtt.canon $B)
      )
    )
    (drop
      (array.new_with_rtt $B
        (i32.const 1)
        (i32.const 1)
        (rtt.canon $B)
      )
    )
  )

  ;; CHECK:      (func $structs-and-arrays-do-not-alias (param $array (ref null $B)) (param $struct (ref $A))
  ;; CHECK-NEXT:  (local $2 i32)
  ;; CHECK-NEXT:  (array.set $B
  ;; CHECK-NEXT:   (local.get $array)
  ;; CHECK-NEXT:   (i32.const 0)
  ;; CHECK-NEXT:   (local.tee $2
  ;; CHECK-NEXT:    (struct.get $A 0
  ;; CHECK-NEXT:     (local.get $struct)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (array.set $B
  ;; CHECK-NEXT:   (local.get $array)
  ;; CHECK-NEXT:   (i32.const 1)
  ;; CHECK-NEXT:   (local.get $2)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (array.set $B
  ;; CHECK-NEXT:   (local.get $array)
  ;; CHECK-NEXT:   (i32.const 2)
  ;; CHECK-NEXT:   (local.get $2)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $structs-and-arrays-do-not-alias (param $array (ref null $B)) (param $struct (ref $A))
    ;; ArraySets to consecutive elements, using some fixed StructGet value. This
    ;; common pattern in j2cl can be optimized, as structs and arrays do not
    ;; alias.
    (array.set $B
      (local.get $array)
      (i32.const 0)
      (struct.get $A 0
        (local.get $struct)
      )
    )
    (array.set $B
      (local.get $array)
      (i32.const 1)
      (struct.get $A 0
        (local.get $struct)
      )
    )
    (array.set $B
      (local.get $array)
      (i32.const 2)
      (struct.get $A 0
        (local.get $struct)
      )
    )
  )
)

(module
  ;; Real-world testcase from AssemblyScript, containing multiple nested things
  ;; that can be optimized. The inputs to the add (the xors) are identical, and
  ;; we can avoid repeating them.
  ;; CHECK:      (type $i32_i32_=>_i32 (func (param i32 i32) (result i32)))

  ;; CHECK:      (func $div16_internal (param $0 i32) (param $1 i32) (result i32)
  ;; CHECK-NEXT:  (local $2 i32)
  ;; CHECK-NEXT:  (i32.add
  ;; CHECK-NEXT:   (local.tee $2
  ;; CHECK-NEXT:    (i32.xor
  ;; CHECK-NEXT:     (i32.shr_s
  ;; CHECK-NEXT:      (i32.shl
  ;; CHECK-NEXT:       (local.get $0)
  ;; CHECK-NEXT:       (i32.const 16)
  ;; CHECK-NEXT:      )
  ;; CHECK-NEXT:      (i32.const 16)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:     (i32.shr_s
  ;; CHECK-NEXT:      (i32.shl
  ;; CHECK-NEXT:       (local.get $1)
  ;; CHECK-NEXT:       (i32.const 16)
  ;; CHECK-NEXT:      )
  ;; CHECK-NEXT:      (i32.const 16)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.get $2)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $div16_internal (param $0 i32) (param $1 i32) (result i32)
    (i32.add
      (i32.xor
        (i32.shr_s
          (i32.shl
            (local.get $0)
            (i32.const 16)
          )
          (i32.const 16)
        )
        (i32.shr_s
          (i32.shl
            (local.get $1)
            (i32.const 16)
          )
          (i32.const 16)
        )
      )
      (i32.xor
        (i32.shr_s
          (i32.shl
            (local.get $0)
            (i32.const 16)
          )
          (i32.const 16)
        )
        (i32.shr_s
          (i32.shl
            (local.get $1)
            (i32.const 16)
          )
          (i32.const 16)
        )
      )
    )
  )
)
