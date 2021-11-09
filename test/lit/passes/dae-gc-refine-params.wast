;; NOTE: Assertions have been generated by update_lit_checks.py and should not be edited.
;; RUN: wasm-opt %s -all --dae -S -o - | filecheck %s
;; RUN: wasm-opt %s -all --dae --nominal -S -o - | filecheck %s --check-prefix NOMNL

(module
 ;; CHECK:      (type ${i32} (struct (field i32)))
 ;; NOMNL:      (type ${i32} (struct_subtype (field i32) ${}))
 (type ${i32} (struct_subtype (field i32) ${}))

 ;; CHECK:      (type ${} (struct ))
 ;; NOMNL:      (type ${} (struct_subtype  data))
 (type ${} (struct))

 ;; CHECK:      (type ${i32_i64} (struct (field i32) (field i64)))
 ;; NOMNL:      (type ${i32_i64} (struct_subtype (field i32) (field i64) ${i32}))
 (type ${i32_i64} (struct_subtype (field i32) (field i64) ${i32}))

 ;; CHECK:      (type ${f64} (struct (field f64)))
 ;; NOMNL:      (type ${f64} (struct_subtype (field f64) ${}))
 (type ${f64} (struct_subtype (field f64) ${}))

 ;; CHECK:      (type ${i32_f32} (struct (field i32) (field f32)))
 ;; NOMNL:      (type ${i32_f32} (struct_subtype (field i32) (field f32) ${i32}))
 (type ${i32_f32} (struct_subtype (field i32) (field f32) ${i32}))

 ;; CHECK:      (func $call-various-params-no
 ;; CHECK-NEXT:  (call $various-params-no
 ;; CHECK-NEXT:   (ref.null ${})
 ;; CHECK-NEXT:   (ref.null ${i32})
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (call $various-params-no
 ;; CHECK-NEXT:   (ref.null ${i32})
 ;; CHECK-NEXT:   (ref.null ${f64})
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 ;; NOMNL:      (func $call-various-params-no (type $none_=>_none)
 ;; NOMNL-NEXT:  (call $various-params-no
 ;; NOMNL-NEXT:   (ref.null ${})
 ;; NOMNL-NEXT:   (ref.null ${i32})
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT:  (call $various-params-no
 ;; NOMNL-NEXT:   (ref.null ${i32})
 ;; NOMNL-NEXT:   (ref.null ${f64})
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT: )
 (func $call-various-params-no
  ;; The first argument gets {} and {i32}; the second {i32} and {f64}; none of
  ;; those pairs can be optimized.
  (call $various-params-no
   (ref.null ${})
   (ref.null ${i32})
  )
  (call $various-params-no
   (ref.null ${i32})
   (ref.null ${f64})
  )
 )
 ;; This function is called in ways that do not allow us to alter the types of
 ;; its parameters (see last function).
 ;; CHECK:      (func $various-params-no (param $x (ref null ${})) (param $y (ref null ${}))
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (local.get $x)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (local.get $y)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 ;; NOMNL:      (func $various-params-no (type $ref?|${}|_ref?|${}|_=>_none) (param $x (ref null ${})) (param $y (ref null ${}))
 ;; NOMNL-NEXT:  (drop
 ;; NOMNL-NEXT:   (local.get $x)
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT:  (drop
 ;; NOMNL-NEXT:   (local.get $y)
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT: )
 (func $various-params-no (param $x (ref null ${})) (param $y (ref null ${}))
  ;; "Use" the locals to avoid other optimizations kicking in.
  (drop (local.get $x))
  (drop (local.get $y))
 )

 ;; CHECK:      (func $call-various-params-yes
 ;; CHECK-NEXT:  (call $various-params-yes
 ;; CHECK-NEXT:   (ref.null ${i32})
 ;; CHECK-NEXT:   (i32.const 0)
 ;; CHECK-NEXT:   (ref.null ${i32})
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (call $various-params-yes
 ;; CHECK-NEXT:   (ref.null ${i32})
 ;; CHECK-NEXT:   (i32.const 1)
 ;; CHECK-NEXT:   (ref.null ${i32_i64})
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 ;; NOMNL:      (func $call-various-params-yes (type $none_=>_none)
 ;; NOMNL-NEXT:  (call $various-params-yes
 ;; NOMNL-NEXT:   (ref.null ${i32})
 ;; NOMNL-NEXT:   (i32.const 0)
 ;; NOMNL-NEXT:   (ref.null ${i32})
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT:  (call $various-params-yes
 ;; NOMNL-NEXT:   (ref.null ${i32})
 ;; NOMNL-NEXT:   (i32.const 1)
 ;; NOMNL-NEXT:   (ref.null ${i32_i64})
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT: )
 (func $call-various-params-yes
  ;; The first argument gets {i32} and {i32}; the second {i32} and {i32_i64};
  ;; both of those pairs can be optimized to {i32}.
  ;; There is also an i32 in the middle, which should not confuse us.
  (call $various-params-yes
   (ref.null ${i32})
   (i32.const 0)
   (ref.null ${i32})
  )
  (call $various-params-yes
   (ref.null ${i32})
   (i32.const 1)
   (ref.null ${i32_i64})
  )
 )
 ;; This function is called in ways that *do* allow us to alter the types of
 ;; its parameters (see last function).
 ;; CHECK:      (func $various-params-yes (param $x (ref null ${i32})) (param $i i32) (param $y (ref null ${i32}))
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (local.get $x)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (local.get $i)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (local.get $y)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 ;; NOMNL:      (func $various-params-yes (type $ref?|${i32}|_i32_ref?|${i32}|_=>_none) (param $x (ref null ${i32})) (param $i i32) (param $y (ref null ${i32}))
 ;; NOMNL-NEXT:  (drop
 ;; NOMNL-NEXT:   (local.get $x)
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT:  (drop
 ;; NOMNL-NEXT:   (local.get $i)
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT:  (drop
 ;; NOMNL-NEXT:   (local.get $y)
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT: )
 (func $various-params-yes (param $x (ref null ${})) (param $i i32) (param $y (ref null ${}))
  ;; "Use" the locals to avoid other optimizations kicking in.
  (drop (local.get $x))
  (drop (local.get $i))
  (drop (local.get $y))
 )

 ;; CHECK:      (func $call-various-params-set
 ;; CHECK-NEXT:  (call $various-params-set
 ;; CHECK-NEXT:   (ref.null ${i32})
 ;; CHECK-NEXT:   (ref.null ${i32})
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (call $various-params-set
 ;; CHECK-NEXT:   (ref.null ${i32})
 ;; CHECK-NEXT:   (ref.null ${i32_i64})
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 ;; NOMNL:      (func $call-various-params-set (type $none_=>_none)
 ;; NOMNL-NEXT:  (call $various-params-set
 ;; NOMNL-NEXT:   (ref.null ${i32})
 ;; NOMNL-NEXT:   (ref.null ${i32})
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT:  (call $various-params-set
 ;; NOMNL-NEXT:   (ref.null ${i32})
 ;; NOMNL-NEXT:   (ref.null ${i32_i64})
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT: )
 (func $call-various-params-set
  ;; The first argument gets {i32} and {i32}; the second {i32} and {i32_i64;
  ;; both of those pairs can be optimized to {i32}
  (call $various-params-set
   (ref.null ${i32})
   (ref.null ${i32})
  )
  (call $various-params-set
   (ref.null ${i32})
   (ref.null ${i32_i64})
  )
 )
 ;; This function is called in ways that *do* allow us to alter the types of
 ;; its parameters (see last function), however, we reuse the parameters by
 ;; writing to them, which causes problems in one case.
 ;; CHECK:      (func $various-params-set (param $x (ref null ${i32})) (param $y (ref null ${i32}))
 ;; CHECK-NEXT:  (local $2 (ref null ${}))
 ;; CHECK-NEXT:  (local.set $2
 ;; CHECK-NEXT:   (local.get $x)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (block
 ;; CHECK-NEXT:   (drop
 ;; CHECK-NEXT:    (local.get $2)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (drop
 ;; CHECK-NEXT:    (local.get $y)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (local.set $2
 ;; CHECK-NEXT:    (ref.null ${})
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (drop
 ;; CHECK-NEXT:    (local.get $2)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (local.set $y
 ;; CHECK-NEXT:    (ref.null ${i32_i64})
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (drop
 ;; CHECK-NEXT:    (local.get $y)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 ;; NOMNL:      (func $various-params-set (type $ref?|${i32}|_ref?|${i32}|_=>_none) (param $x (ref null ${i32})) (param $y (ref null ${i32}))
 ;; NOMNL-NEXT:  (local $2 (ref null ${}))
 ;; NOMNL-NEXT:  (local.set $2
 ;; NOMNL-NEXT:   (local.get $x)
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT:  (block
 ;; NOMNL-NEXT:   (drop
 ;; NOMNL-NEXT:    (local.get $2)
 ;; NOMNL-NEXT:   )
 ;; NOMNL-NEXT:   (drop
 ;; NOMNL-NEXT:    (local.get $y)
 ;; NOMNL-NEXT:   )
 ;; NOMNL-NEXT:   (local.set $2
 ;; NOMNL-NEXT:    (ref.null ${})
 ;; NOMNL-NEXT:   )
 ;; NOMNL-NEXT:   (drop
 ;; NOMNL-NEXT:    (local.get $2)
 ;; NOMNL-NEXT:   )
 ;; NOMNL-NEXT:   (local.set $y
 ;; NOMNL-NEXT:    (ref.null ${i32_i64})
 ;; NOMNL-NEXT:   )
 ;; NOMNL-NEXT:   (drop
 ;; NOMNL-NEXT:    (local.get $y)
 ;; NOMNL-NEXT:   )
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT: )
 (func $various-params-set (param $x (ref null ${})) (param $y (ref null ${}))
  ;; "Use" the locals to avoid other optimizations kicking in.
  (drop (local.get $x))
  (drop (local.get $y))
  ;; Write to $x a value that will not fit in the refined type, which will
  ;; force us to do a fixup: the param will get the new type, and a new local
  ;; will stay at the old type, and we will use that local throughout the
  ;; function.
  (local.set $x (ref.null ${}))
  (drop
   (local.get $x)
  )
  ;; Write to $y in a way that does not cause any issue, and we should not do
  ;; any fixup while we refine the type.
  (local.set $y (ref.null ${i32_i64}))
  (drop
   (local.get $y)
  )
 )

 ;; CHECK:      (func $call-various-params-tee
 ;; CHECK-NEXT:  (call $various-params-tee
 ;; CHECK-NEXT:   (ref.null ${i32})
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 ;; NOMNL:      (func $call-various-params-tee (type $none_=>_none)
 ;; NOMNL-NEXT:  (call $various-params-tee
 ;; NOMNL-NEXT:   (ref.null ${i32})
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT: )
 (func $call-various-params-tee
  ;; The argument gets {i32}, which allows us to refine.
  (call $various-params-tee
   (ref.null ${i32})
  )
 )
 ;; CHECK:      (func $various-params-tee (param $x (ref null ${i32}))
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (local.get $x)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (block $block (result (ref null ${i32}))
 ;; CHECK-NEXT:    (local.tee $x
 ;; CHECK-NEXT:     (ref.null ${i32_i64})
 ;; CHECK-NEXT:    )
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 ;; NOMNL:      (func $various-params-tee (type $ref?|${i32}|_=>_none) (param $x (ref null ${i32}))
 ;; NOMNL-NEXT:  (drop
 ;; NOMNL-NEXT:   (local.get $x)
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT:  (drop
 ;; NOMNL-NEXT:   (block $block (result (ref null ${i32}))
 ;; NOMNL-NEXT:    (local.tee $x
 ;; NOMNL-NEXT:     (ref.null ${i32_i64})
 ;; NOMNL-NEXT:    )
 ;; NOMNL-NEXT:   )
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT: )
 (func $various-params-tee (param $x (ref null ${}))
  ;; "Use" the locals to avoid other optimizations kicking in.
  (drop (local.get $x))
  ;; Write to $x in a way that allows us to make the type more specific. We
  ;; must also update the type of the tee (if we do not, a validation error
  ;; would occur), and that will also cause the block's type to update as well.
  (drop
   (block (result (ref null ${}))
    (local.tee $x (ref.null ${i32_i64}))
   )
  )
 )

 ;; CHECK:      (func $call-various-params-null
 ;; CHECK-NEXT:  (call $various-params-null
 ;; CHECK-NEXT:   (ref.as_non_null
 ;; CHECK-NEXT:    (ref.null ${i32})
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (ref.null ${i32})
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (call $various-params-null
 ;; CHECK-NEXT:   (ref.as_non_null
 ;; CHECK-NEXT:    (ref.null ${i32})
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:   (ref.as_non_null
 ;; CHECK-NEXT:    (ref.null ${i32})
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 ;; NOMNL:      (func $call-various-params-null (type $none_=>_none)
 ;; NOMNL-NEXT:  (call $various-params-null
 ;; NOMNL-NEXT:   (ref.as_non_null
 ;; NOMNL-NEXT:    (ref.null ${i32})
 ;; NOMNL-NEXT:   )
 ;; NOMNL-NEXT:   (ref.null ${i32})
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT:  (call $various-params-null
 ;; NOMNL-NEXT:   (ref.as_non_null
 ;; NOMNL-NEXT:    (ref.null ${i32})
 ;; NOMNL-NEXT:   )
 ;; NOMNL-NEXT:   (ref.as_non_null
 ;; NOMNL-NEXT:    (ref.null ${i32})
 ;; NOMNL-NEXT:   )
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT: )
 (func $call-various-params-null
  ;; The first argument gets non-null values, allowing us to refine it. The
  ;; second gets only one.
  (call $various-params-null
   (ref.as_non_null (ref.null ${i32}))
   (ref.null ${i32})
  )
  (call $various-params-null
   (ref.as_non_null (ref.null ${i32}))
   (ref.as_non_null (ref.null ${i32}))
  )
 )
 ;; This function is called in ways that allow us to make the first parameter
 ;; non-nullable.
 ;; CHECK:      (func $various-params-null (param $x (ref ${i32})) (param $y (ref null ${i32}))
 ;; CHECK-NEXT:  (local $temp i32)
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (local.get $x)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (local.get $y)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (local.set $temp
 ;; CHECK-NEXT:   (local.get $temp)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 ;; NOMNL:      (func $various-params-null (type $ref|${i32}|_ref?|${i32}|_=>_none) (param $x (ref ${i32})) (param $y (ref null ${i32}))
 ;; NOMNL-NEXT:  (local $temp i32)
 ;; NOMNL-NEXT:  (drop
 ;; NOMNL-NEXT:   (local.get $x)
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT:  (drop
 ;; NOMNL-NEXT:   (local.get $y)
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT:  (local.set $temp
 ;; NOMNL-NEXT:   (local.get $temp)
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT: )
 (func $various-params-null (param $x (ref null ${})) (param $y (ref null ${}))
  (local $temp i32)
  ;; "Use" the locals to avoid other optimizations kicking in.
  (drop (local.get $x))
  (drop (local.get $y))
  ;; Use a local in this function as well, which should be ignored by this pass
  ;; (when we scan and update all local.gets and sets, we should only do so on
  ;; parameters, and not vars - and we can crash if we scan/update things we
  ;; should not).
  (local.set $temp (local.get $temp))
 )

 ;; CHECK:      (func $call-various-params-middle
 ;; CHECK-NEXT:  (call $various-params-middle
 ;; CHECK-NEXT:   (ref.null ${i32_i64})
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (call $various-params-middle
 ;; CHECK-NEXT:   (ref.null ${i32_f32})
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 ;; NOMNL:      (func $call-various-params-middle (type $none_=>_none)
 ;; NOMNL-NEXT:  (call $various-params-middle
 ;; NOMNL-NEXT:   (ref.null ${i32_i64})
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT:  (call $various-params-middle
 ;; NOMNL-NEXT:   (ref.null ${i32_f32})
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT: )
 (func $call-various-params-middle
  ;; The argument gets {i32_i64} and {i32_f32}. This allows us to refine from
  ;; {} to {i32}, a type "in the middle".
  (call $various-params-middle
   (ref.null ${i32_i64})
  )
  (call $various-params-middle
   (ref.null ${i32_f32})
  )
 )
 ;; CHECK:      (func $various-params-middle (param $x (ref null ${i32}))
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (local.get $x)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 ;; NOMNL:      (func $various-params-middle (type $ref?|${i32}|_=>_none) (param $x (ref null ${i32}))
 ;; NOMNL-NEXT:  (drop
 ;; NOMNL-NEXT:   (local.get $x)
 ;; NOMNL-NEXT:  )
 ;; NOMNL-NEXT: )
 (func $various-params-middle (param $x (ref null ${}))
  ;; "Use" the local to avoid other optimizations kicking in.
  (drop (local.get $x))
 )
)
