;; NOTE: Assertions have been generated by update_lit_checks.py and should not be edited.
;; RUN: wasm-opt %s --remove-unused-brs -all -S -o - \
;; RUN:  | filecheck %s

(module
 ;; CHECK:      (type $struct (struct ))
 (type $struct (struct ))

 ;; CHECK:      (func $br_on_non_data-1
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (block $any (result anyref)
 ;; CHECK-NEXT:    (drop
 ;; CHECK-NEXT:     (br $any
 ;; CHECK-NEXT:      (ref.func $br_on_non_data-1)
 ;; CHECK-NEXT:     )
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:    (ref.null any)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $br_on_non_data-1
  (drop
   (block $any (result anyref)
    (drop
     ;; A function is not data, and so we should branch.
     (br_on_non_data $any
      (ref.func $br_on_non_data-1)
     )
    )
    (ref.null any)
   )
  )
 )
 ;; CHECK:      (func $br_on_non_data-2 (param $data dataref)
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (block $any (result anyref)
 ;; CHECK-NEXT:    (drop
 ;; CHECK-NEXT:     (local.get $data)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:    (ref.null any)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $br_on_non_data-2 (param $data (ref data))
  (drop
   (block $any (result anyref)
    (drop
     ;; Data is provided here, and so we will not branch.
     (br_on_non_data $any
      (local.get $data)
     )
    )
    (ref.null any)
   )
  )
 )

 ;; CHECK:      (func $br_on-if (param $0 dataref)
 ;; CHECK-NEXT:  (block $label
 ;; CHECK-NEXT:   (drop
 ;; CHECK-NEXT:    (select (result dataref)
 ;; CHECK-NEXT:     (local.get $0)
 ;; CHECK-NEXT:     (local.get $0)
 ;; CHECK-NEXT:     (i32.const 0)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $br_on-if (param $0 (ref data))
  (block $label
   (drop
    ;; This br is never taken, as the input is non-nullable, so we can remove
    ;; it. When we do so, we replace it with the if. We should not rescan that
    ;; if, which has already been walked, as that would hit an assertion.
    ;;
    (br_on_null $label
     ;; This if can also be turned into a select, separately from the above
     ;; (that is not specifically intended to be tested here).
     (if (result (ref data))
      (i32.const 0)
      (local.get $0)
      (local.get $0)
     )
    )
   )
  )
 )

 ;; CHECK:      (func $nested_br_on (result dataref)
 ;; CHECK-NEXT:  (block $label$1 (result (ref $struct))
 ;; CHECK-NEXT:   (drop
 ;; CHECK-NEXT:    (br $label$1
 ;; CHECK-NEXT:     (struct.new_default $struct)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (unreachable)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $nested_br_on (result dataref)
  (block $label$1 (result dataref)
   (drop
    ;; The inner br_on_data will become a direct br since the type proves it
    ;; is in fact data. That then becomes unreachable, and the parent must
    ;; handle that properly (do nothing without hitting an assertion).
    (br_on_data $label$1
     (br_on_data $label$1
      (struct.new_default $struct)
     )
    )
   )
   (unreachable)
  )
 )

 ;; CHECK:      (func $br_on_cast_static (result (ref $struct))
 ;; CHECK-NEXT:  (local $temp (ref null $struct))
 ;; CHECK-NEXT:  (block $block (result (ref $struct))
 ;; CHECK-NEXT:   (drop
 ;; CHECK-NEXT:    (br $block
 ;; CHECK-NEXT:     (struct.new_default $struct)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (unreachable)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $br_on_cast_static (result (ref $struct))
  (local $temp (ref null $struct))
  (block $block (result (ref $struct))
   (drop
    ;; This static cast can be computed at compile time: it will definitely be
    ;; taken, so we can turn it into a normal br.
    (br_on_cast_static $block $struct
     (struct.new $struct)
    )
   )
   (unreachable)
  )
 )

 ;; CHECK:      (func $br_on_cast_static_no (result (ref $struct))
 ;; CHECK-NEXT:  (local $temp (ref null $struct))
 ;; CHECK-NEXT:  (block $block (result (ref $struct))
 ;; CHECK-NEXT:   (drop
 ;; CHECK-NEXT:    (br_on_cast_static $block $struct
 ;; CHECK-NEXT:     (ref.null $struct)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (unreachable)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $br_on_cast_static_no (result (ref $struct))
  (local $temp (ref null $struct))
  (block $block (result (ref $struct))
   (drop
    (br_on_cast_static $block $struct
     ;; As above, but now the type is nullable, so we cannot infer anything.
     (ref.null $struct)
    )
   )
   (unreachable)
  )
 )

 ;; CHECK:      (func $br_on_cast_fail_static (result (ref $struct))
 ;; CHECK-NEXT:  (local $temp (ref null $struct))
 ;; CHECK-NEXT:  (block $block
 ;; CHECK-NEXT:   (drop
 ;; CHECK-NEXT:    (struct.new_default $struct)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (unreachable)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $br_on_cast_fail_static (result (ref $struct))
  (local $temp (ref null $struct))
  (block $block (result (ref $struct))
   (drop
    ;; As $br_on_cast_static, but this is a failing cast, so we know it will
    ;; *not* be taken.
    (br_on_cast_static_fail $block $struct
     (struct.new $struct)
    )
   )
   (unreachable)
  )
 )

 ;; CHECK:      (func $br_on_cast_dynamic (result (ref $struct))
 ;; CHECK-NEXT:  (local $temp (ref null $struct))
 ;; CHECK-NEXT:  (block $block (result (ref $struct))
 ;; CHECK-NEXT:   (drop
 ;; CHECK-NEXT:    (br_on_cast $block
 ;; CHECK-NEXT:     (struct.new_default_with_rtt $struct
 ;; CHECK-NEXT:      (rtt.canon $struct)
 ;; CHECK-NEXT:     )
 ;; CHECK-NEXT:     (rtt.canon $struct)
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (unreachable)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $br_on_cast_dynamic (result (ref $struct))
  (local $temp (ref null $struct))
  (block $block (result (ref $struct))
   (drop
    ;; This dynamic cast happens to be optimizable since we see both sides use
    ;; rtt.canon, but we do not inspect things that closely, and leave such
    ;; dynamic casts to runtime.
    (br_on_cast $block
     (struct.new_with_rtt $struct
       (rtt.canon $struct)
     )
     (rtt.canon $struct)
    )
   )
   (unreachable)
  )
 )
)
