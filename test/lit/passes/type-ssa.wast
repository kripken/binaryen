;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.
;; RUN: foreach %s %t wasm-opt           --type-ssa -all -S -o - | filecheck %s
;; RUN: foreach %s %t wasm-opt --nominal --type-ssa -all -S -o - | filecheck %s --check-prefix NOMNL

;; Test in both isorecursive and nominal modes to make sure we create the new
;; types properly in both.

;; Every struct.new here should get a new type.
(module
  ;; CHECK:      (type $struct (struct (field i32)))
  ;; NOMNL:      (type $struct (struct (field i32)))
  (type $struct (struct_subtype (field i32) data))

  ;; CHECK:      (type $none_=>_none (func))

  ;; CHECK:      (rec
  ;; CHECK-NEXT:  (type $struct$1 (struct_subtype (field i32) $struct))

  ;; CHECK:       (type $struct$2 (struct_subtype (field i32) $struct))

  ;; CHECK:       (type $struct$3 (struct_subtype (field i32) $struct))

  ;; CHECK:       (type $struct$4 (struct_subtype (field i32) $struct))

  ;; CHECK:       (type $struct$5 (struct_subtype (field i32) $struct))

  ;; CHECK:      (global $g (ref $struct) (struct.new $struct$4
  ;; CHECK-NEXT:  (i32.const 42)
  ;; CHECK-NEXT: ))
  ;; NOMNL:      (type $none_=>_none (func))

  ;; NOMNL:      (type $struct$4 (struct_subtype (field i32) $struct))

  ;; NOMNL:      (type $struct$5 (struct_subtype (field i32) $struct))

  ;; NOMNL:      (type $struct$1 (struct_subtype (field i32) $struct))

  ;; NOMNL:      (type $struct$2 (struct_subtype (field i32) $struct))

  ;; NOMNL:      (type $struct$3 (struct_subtype (field i32) $struct))

  ;; NOMNL:      (global $g (ref $struct) (struct.new $struct$4
  ;; NOMNL-NEXT:  (i32.const 42)
  ;; NOMNL-NEXT: ))
  (global $g (ref $struct) (struct.new $struct
    (i32.const 42)
  ))

  ;; CHECK:      (global $h (ref $struct) (struct.new $struct$5
  ;; CHECK-NEXT:  (i32.const 42)
  ;; CHECK-NEXT: ))
  ;; NOMNL:      (global $h (ref $struct) (struct.new $struct$5
  ;; NOMNL-NEXT:  (i32.const 42)
  ;; NOMNL-NEXT: ))
  (global $h (ref $struct) (struct.new $struct
    (i32.const 42)
  ))

  ;; CHECK:      (func $foo (type $none_=>_none)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.new_default $struct$1)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.new $struct$2
  ;; CHECK-NEXT:    (i32.const 10)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  ;; NOMNL:      (func $foo (type $none_=>_none)
  ;; NOMNL-NEXT:  (drop
  ;; NOMNL-NEXT:   (struct.new_default $struct$1)
  ;; NOMNL-NEXT:  )
  ;; NOMNL-NEXT:  (drop
  ;; NOMNL-NEXT:   (struct.new $struct$2
  ;; NOMNL-NEXT:    (i32.const 10)
  ;; NOMNL-NEXT:   )
  ;; NOMNL-NEXT:  )
  ;; NOMNL-NEXT: )
  (func $foo
    (drop
      (struct.new_default $struct)
    )
    (drop
      (struct.new $struct
        (i32.const 10)
      )
    )
  )

  ;; CHECK:      (func $another-func (type $none_=>_none)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.new $struct$3
  ;; CHECK-NEXT:    (i32.const 100)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  ;; NOMNL:      (func $another-func (type $none_=>_none)
  ;; NOMNL-NEXT:  (drop
  ;; NOMNL-NEXT:   (struct.new $struct$3
  ;; NOMNL-NEXT:    (i32.const 100)
  ;; NOMNL-NEXT:   )
  ;; NOMNL-NEXT:  )
  ;; NOMNL-NEXT: )
  (func $another-func
    (drop
      (struct.new $struct
        (i32.const 100)
      )
    )
  )
)

;; Some of these are uninteresting and should not get a new type.
(module
  ;; CHECK:      (type $anyref_dataref_=>_none (func (param anyref dataref)))

  ;; CHECK:      (type $struct (struct (field anyref)))
  ;; NOMNL:      (type $anyref_dataref_=>_none (func (param anyref dataref)))

  ;; NOMNL:      (type $struct (struct (field anyref)))
  (type $struct (struct_subtype (field (ref null any)) data))

  ;; CHECK:      (rec
  ;; CHECK-NEXT:  (type $struct$1 (struct_subtype (field anyref) $struct))

  ;; CHECK:       (type $struct$2 (struct_subtype (field anyref) $struct))

  ;; CHECK:       (type $struct$3 (struct_subtype (field anyref) $struct))

  ;; CHECK:      (func $foo (type $anyref_dataref_=>_none) (param $any anyref) (param $data dataref)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.new_default $struct$1)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.new $struct$2
  ;; CHECK-NEXT:    (ref.null none)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.new $struct
  ;; CHECK-NEXT:    (local.get $any)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.new $struct$3
  ;; CHECK-NEXT:    (local.get $data)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (block ;; (replaces something unreachable we can't emit)
  ;; CHECK-NEXT:    (drop
  ;; CHECK-NEXT:     (unreachable)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (unreachable)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  ;; NOMNL:      (type $struct$1 (struct_subtype (field anyref) $struct))

  ;; NOMNL:      (type $struct$2 (struct_subtype (field anyref) $struct))

  ;; NOMNL:      (type $struct$3 (struct_subtype (field anyref) $struct))

  ;; NOMNL:      (func $foo (type $anyref_dataref_=>_none) (param $any anyref) (param $data dataref)
  ;; NOMNL-NEXT:  (drop
  ;; NOMNL-NEXT:   (struct.new_default $struct$1)
  ;; NOMNL-NEXT:  )
  ;; NOMNL-NEXT:  (drop
  ;; NOMNL-NEXT:   (struct.new $struct$2
  ;; NOMNL-NEXT:    (ref.null none)
  ;; NOMNL-NEXT:   )
  ;; NOMNL-NEXT:  )
  ;; NOMNL-NEXT:  (drop
  ;; NOMNL-NEXT:   (struct.new $struct
  ;; NOMNL-NEXT:    (local.get $any)
  ;; NOMNL-NEXT:   )
  ;; NOMNL-NEXT:  )
  ;; NOMNL-NEXT:  (drop
  ;; NOMNL-NEXT:   (struct.new $struct$3
  ;; NOMNL-NEXT:    (local.get $data)
  ;; NOMNL-NEXT:   )
  ;; NOMNL-NEXT:  )
  ;; NOMNL-NEXT:  (drop
  ;; NOMNL-NEXT:   (block ;; (replaces something unreachable we can't emit)
  ;; NOMNL-NEXT:    (drop
  ;; NOMNL-NEXT:     (unreachable)
  ;; NOMNL-NEXT:    )
  ;; NOMNL-NEXT:    (unreachable)
  ;; NOMNL-NEXT:   )
  ;; NOMNL-NEXT:  )
  ;; NOMNL-NEXT: )
  (func $foo (param $any (ref null any)) (param $data (ref null data))
    ;; A null is interesting.
    (drop
      (struct.new_default $struct)
    )
    (drop
      (struct.new $struct
        (ref.null none)
      )
    )
    ;; An unknown value of the same type is uninteresting.
    (drop
      (struct.new $struct
        (local.get $any)
      )
    )
    ;; But a more refined type piques our interest.
    (drop
      (struct.new $struct
        (local.get $data)
      )
    )
    ;; An unreachable is boring.
    (drop
      (struct.new $struct
        (unreachable)
      )
    )
  )
)
