;; NOTE: Assertions have been generated by update_lit_checks.py and should not be edited.

;; RUN: foreach %s %t wasm-opt --precompute-propagate -all -S -o - | filecheck %s

(module
  ;; CHECK:      (type $struct-imm (struct (field i32)))

  ;; CHECK:      (type $struct-mut (struct (field (mut i32))))
  (type $struct-mut (struct (mut i32)))
  (type $struct-imm (struct i32))

  ;; CHECK:      (func $propagate
  ;; CHECK-NEXT:  (local $ref-imm (ref null $struct-imm))
  ;; CHECK-NEXT:  (local $ref-mut (ref null $struct-mut))
  ;; CHECK-NEXT:  (local.set $ref-imm
  ;; CHECK-NEXT:   (struct.new $struct-imm
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (i32.const 1)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.set $ref-mut
  ;; CHECK-NEXT:   (struct.new $struct-mut
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (struct.get $struct-mut 0
  ;; CHECK-NEXT:    (local.get $ref-mut)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $propagate
    (local $ref-imm (ref null $struct-imm))
    (local $ref-mut (ref null $struct-mut))
    ;; We can propagate from an immutable field of a struct created in this
    ;; function.
    (local.set $ref-imm
      (struct.new $struct-imm
        (i32.const 1)
      )
    )
    (call $helper
      (struct.get $struct-imm 0
        (local.get $ref-imm)
      )
    )
    ;; But the same thing on a mutable field fails.
    (local.set $ref-mut
      (struct.new $struct-mut
        (i32.const 1)
      )
    )
    (call $helper
      (struct.get $struct-mut 0
        (local.get $ref-mut)
      )
    )
  )

  ;; CHECK:      (func $non-constant (param $param i32)
  ;; CHECK-NEXT:  (local $ref (ref null $struct-imm))
  ;; CHECK-NEXT:  (local.set $ref
  ;; CHECK-NEXT:   (struct.new $struct-imm
  ;; CHECK-NEXT:    (local.get $param)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (struct.get $struct-imm 0
  ;; CHECK-NEXT:    (local.get $ref)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $non-constant (param $param i32)
    (local $ref (ref null $struct-imm))
    (local.set $ref
      (struct.new $struct-imm
        ;; This value is not constant, so we have nothing to propagate.
        (local.get $param)
      )
    )
    (call $helper
      (struct.get $struct-imm 0
        (local.get $ref)
      )
    )
  )

  ;; CHECK:      (func $unreachable
  ;; CHECK-NEXT:  (local $ref-imm (ref null $struct-imm))
  ;; CHECK-NEXT:  (local.tee $ref-imm
  ;; CHECK-NEXT:   (block
  ;; CHECK-NEXT:    (unreachable)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (struct.get $struct-imm 0
  ;; CHECK-NEXT:    (local.get $ref-imm)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $unreachable
    (local $ref-imm (ref null $struct-imm))
    ;; Test we do not error on an unreachable value.
    (local.set $ref-imm
      (struct.new $struct-imm
        (unreachable)
      )
    )
    (call $helper
      (struct.get $struct-imm 0
        (local.get $ref-imm)
      )
    )
  )

  ;; CHECK:      (func $param (param $ref-imm (ref null $struct-imm))
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (struct.get $struct-imm 0
  ;; CHECK-NEXT:    (local.get $ref-imm)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $param (param $ref-imm (ref null $struct-imm))
    ;; Test we ignore a param value, whose data we do not know.
    (call $helper
      (struct.get $struct-imm 0
        (local.get $ref-imm)
      )
    )
  )

  ;; CHECK:      (func $local-null
  ;; CHECK-NEXT:  (local $ref-imm (ref null $struct-imm))
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (struct.get $struct-imm 0
  ;; CHECK-NEXT:    (ref.null $struct-imm)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $local-null
    (local $ref-imm (ref null $struct-imm))
    ;; Test we ignore a local value that is null, whose data we do not know.
    (call $helper
      (struct.get $struct-imm 0
        (local.get $ref-imm)
      )
    )
  )

  ;; CHECK:      (func $local-unknown (param $x i32)
  ;; CHECK-NEXT:  (local $ref-imm (ref null $struct-imm))
  ;; CHECK-NEXT:  (if
  ;; CHECK-NEXT:   (local.get $x)
  ;; CHECK-NEXT:   (local.set $ref-imm
  ;; CHECK-NEXT:    (struct.new $struct-imm
  ;; CHECK-NEXT:     (i32.const 1)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.set $ref-imm
  ;; CHECK-NEXT:    (struct.new $struct-imm
  ;; CHECK-NEXT:     (i32.const 2)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (struct.get $struct-imm 0
  ;; CHECK-NEXT:    (local.get $ref-imm)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $local-unknown (param $x i32)
    (local $ref-imm (ref null $struct-imm))
    ;; Do not propagate if a local has more than one possible struct.new with
    ;; different values.
    (if
      (local.get $x)
      (local.set $ref-imm
        (struct.new $struct-imm
          (i32.const 1)
        )
      )
      (local.set $ref-imm
        (struct.new $struct-imm
          (i32.const 2)
        )
      )
    )
    (call $helper
      (struct.get $struct-imm 0
        (local.get $ref-imm)
      )
    )
  )

  ;; CHECK:      (func $local-unknown-ref-same-value (param $x i32)
  ;; CHECK-NEXT:  (local $ref-imm (ref null $struct-imm))
  ;; CHECK-NEXT:  (if
  ;; CHECK-NEXT:   (local.get $x)
  ;; CHECK-NEXT:   (local.set $ref-imm
  ;; CHECK-NEXT:    (struct.new $struct-imm
  ;; CHECK-NEXT:     (i32.const 1)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.set $ref-imm
  ;; CHECK-NEXT:    (struct.new $struct-imm
  ;; CHECK-NEXT:     (i32.const 1)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (struct.get $struct-imm 0
  ;; CHECK-NEXT:    (local.get $ref-imm)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $local-unknown-ref-same-value (param $x i32)
    (local $ref-imm (ref null $struct-imm))
    ;; As above, but the two different refs have the same value, so we can in
    ;; theory optimize. However, atm we do nothing here, as the analysis stops
    ;; when it sees it cannot propagate the local value (the ref, which has two
    ;; possible values).
    (if
      (local.get $x)
      (local.set $ref-imm
        (struct.new $struct-imm
          (i32.const 1)
        )
      )
      (local.set $ref-imm
        (struct.new $struct-imm
          (i32.const 1)
        )
      )
    )
    (call $helper
      (struct.get $struct-imm 0
        (local.get $ref-imm)
      )
    )
  )

  ;; CHECK:      (func $propagate-multi-refs (param $x i32)
  ;; CHECK-NEXT:  (local $ref-imm (ref null $struct-imm))
  ;; CHECK-NEXT:  (if
  ;; CHECK-NEXT:   (local.get $x)
  ;; CHECK-NEXT:   (block $block
  ;; CHECK-NEXT:    (local.set $ref-imm
  ;; CHECK-NEXT:     (struct.new $struct-imm
  ;; CHECK-NEXT:      (i32.const 1)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (call $helper
  ;; CHECK-NEXT:     (i32.const 1)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (block $block0
  ;; CHECK-NEXT:    (local.set $ref-imm
  ;; CHECK-NEXT:     (struct.new $struct-imm
  ;; CHECK-NEXT:      (i32.const 2)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (call $helper
  ;; CHECK-NEXT:     (i32.const 2)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $propagate-multi-refs (param $x i32)
    (local $ref-imm (ref null $struct-imm))
    ;; Propagate more than once in a function, using the same local that is
    ;; reused.
    (if
      (local.get $x)
      (block
        (local.set $ref-imm
          (struct.new $struct-imm
            (i32.const 1)
          )
        )
        (call $helper
          (struct.get $struct-imm 0
            (local.get $ref-imm)
          )
        )
      )
      (block
        (local.set $ref-imm
          (struct.new $struct-imm
            (i32.const 2)
          )
        )
        (call $helper
          (struct.get $struct-imm 0
            (local.get $ref-imm)
          )
        )
      )
    )
  )

  ;; CHECK:      (func $propagate-multi-values (param $x i32)
  ;; CHECK-NEXT:  (local $ref-imm (ref null $struct-imm))
  ;; CHECK-NEXT:  (local.set $ref-imm
  ;; CHECK-NEXT:   (struct.new $struct-imm
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (i32.const 1)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (i32.const 1)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (i32.const 1)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $propagate-multi-values (param $x i32)
    (local $ref-imm (ref null $struct-imm))
    ;; Propagate a ref's value more than once
    (local.set $ref-imm
      (struct.new $struct-imm
        (i32.const 1)
      )
    )
    (call $helper
      (struct.get $struct-imm 0
        (local.get $ref-imm)
      )
    )
    (call $helper
      (struct.get $struct-imm 0
        (local.get $ref-imm)
      )
    )
    (call $helper
      (struct.get $struct-imm 0
        (local.get $ref-imm)
      )
    )
  )

  ;; CHECK:      (func $helper (param $0 i32)
  ;; CHECK-NEXT:  (nop)
  ;; CHECK-NEXT: )
  (func $helper (param i32))
)

(module
  ;; One field is immutable, the other is not, so we can only propagate the
  ;; former.
  ;; CHECK:      (type $struct (struct (field (mut i32)) (field i32)))
  (type $struct (struct (mut i32) i32))

  ;; CHECK:      (func $propagate
  ;; CHECK-NEXT:  (local $ref (ref null $struct))
  ;; CHECK-NEXT:  (local.set $ref
  ;; CHECK-NEXT:   (struct.new $struct
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (struct.get $struct 0
  ;; CHECK-NEXT:    (local.get $ref)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (i32.const 2)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $propagate
    (local $ref (ref null $struct))
    ;; We can propagate from an immutable field of a struct created in this
    ;; function.
    (local.set $ref
      (struct.new $struct
        (i32.const 1)
        (i32.const 2)
      )
    )
    (call $helper
      (struct.get $struct 0
        (local.get $ref)
      )
    )
    (call $helper
      (struct.get $struct 1
        (local.get $ref)
      )
    )
  )

  ;; CHECK:      (func $helper (param $0 i32)
  ;; CHECK-NEXT:  (nop)
  ;; CHECK-NEXT: )
  (func $helper (param i32))
)

(module
  ;; Create an immutable vtable in an immutable field, which lets us read from
  ;; it.

  ;; CHECK:      (type $object (struct (field (ref $vtable))))

  ;; CHECK:      (type $vtable (struct (field funcref)))
  (type $vtable (struct funcref))
  (type $object (struct (ref $vtable)))

  ;; CHECK:      (func $nested-creations
  ;; CHECK-NEXT:  (local $ref (ref null $object))
  ;; CHECK-NEXT:  (local.set $ref
  ;; CHECK-NEXT:   (struct.new $object
  ;; CHECK-NEXT:    (struct.new $vtable
  ;; CHECK-NEXT:     (ref.func $nested-creations)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (ref.func $nested-creations)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $nested-creations
    (local $ref (ref null $object))
    ;; Create an object with a reference to another object, and propagate
    ;; through both of them to a constant value, which saves two struct.gets.
    (local.set $ref
      (struct.new $object
        (struct.new $vtable
          (ref.func $nested-creations)
        )
      )
    )
    (call $helper
      (struct.get $vtable 0
        (struct.get $object 0
          (local.get $ref)
        )
      )
    )
  )

  ;; CHECK:      (func $helper (param $0 funcref)
  ;; CHECK-NEXT:  (nop)
  ;; CHECK-NEXT: )
  (func $helper (param funcref))
)

(module
  ;; As above, but make $vtable not immutable, which prevents optimization.

  ;; CHECK:      (type $object (struct (field (ref $vtable))))

  ;; CHECK:      (type $vtable (struct (field (mut funcref))))
  (type $vtable (struct (mut funcref)))
  (type $object (struct (ref $vtable)))

  ;; CHECK:      (func $nested-creations
  ;; CHECK-NEXT:  (local $ref (ref null $object))
  ;; CHECK-NEXT:  (local.set $ref
  ;; CHECK-NEXT:   (struct.new $object
  ;; CHECK-NEXT:    (struct.new $vtable
  ;; CHECK-NEXT:     (ref.func $nested-creations)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (struct.get $vtable 0
  ;; CHECK-NEXT:    (struct.get $object 0
  ;; CHECK-NEXT:     (local.get $ref)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $nested-creations
    (local $ref (ref null $object))
    (local.set $ref
      (struct.new $object
        (struct.new $vtable
          (ref.func $nested-creations)
        )
      )
    )
    (call $helper
      (struct.get $vtable 0
        ;; Note that we *can* precompute the first struct.get here, but there
        ;; is no constant expression we can emit for it, so we do nothing.
        (struct.get $object 0
          (local.get $ref)
        )
      )
    )
  )

  ;; CHECK:      (func $helper (param $0 funcref)
  ;; CHECK-NEXT:  (nop)
  ;; CHECK-NEXT: )
  (func $helper (param funcref))
)


(module
  ;; As above, but make $object not immutable, which prevents optimization.

  ;; CHECK:      (type $object (struct (field (mut (ref $vtable)))))

  ;; CHECK:      (type $vtable (struct (field funcref)))
  (type $vtable (struct funcref))
  (type $object (struct (mut (ref $vtable))))

  ;; CHECK:      (func $nested-creations
  ;; CHECK-NEXT:  (local $ref (ref null $object))
  ;; CHECK-NEXT:  (local.set $ref
  ;; CHECK-NEXT:   (struct.new $object
  ;; CHECK-NEXT:    (struct.new $vtable
  ;; CHECK-NEXT:     (ref.func $nested-creations)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (struct.get $vtable 0
  ;; CHECK-NEXT:    (struct.get $object 0
  ;; CHECK-NEXT:     (local.get $ref)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $nested-creations
    (local $ref (ref null $object))
    (local.set $ref
      (struct.new $object
        (struct.new $vtable
          (ref.func $nested-creations)
        )
      )
    )
    (call $helper
      (struct.get $vtable 0
        (struct.get $object 0
          (local.get $ref)
        )
      )
    )
  )

  ;; CHECK:      (func $helper (param $0 funcref)
  ;; CHECK-NEXT:  (nop)
  ;; CHECK-NEXT: )
  (func $helper (param funcref))
)

(module
  ;; Create an immutable vtable in an immutable global, which we can optimize
  ;; with.

  ;; CHECK:      (type $vtable (struct (field funcref)))
  (type $vtable (struct funcref))
  ;; CHECK:      (type $object (struct (field (ref $vtable))))
  (type $object (struct (ref $vtable)))

  ;; CHECK:      (global $vtable (ref $vtable) (struct.new $vtable
  ;; CHECK-NEXT:  (ref.func $nested-creations)
  ;; CHECK-NEXT: ))
  (global $vtable (ref $vtable)
    (struct.new $vtable
      (ref.func $nested-creations)
    )
  )

  ;; CHECK:      (func $nested-creations
  ;; CHECK-NEXT:  (local $ref (ref null $object))
  ;; CHECK-NEXT:  (local.set $ref
  ;; CHECK-NEXT:   (struct.new $object
  ;; CHECK-NEXT:    (global.get $vtable)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (ref.func $nested-creations)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $nested-creations
    (local $ref (ref null $object))
    (local.set $ref
      (struct.new $object
        (global.get $vtable)
      )
    )
    (call $helper
      (struct.get $vtable 0
        (struct.get $object 0
          (local.get $ref)
        )
      )
    )
  )

  ;; CHECK:      (func $helper (param $0 funcref)
  ;; CHECK-NEXT:  (nop)
  ;; CHECK-NEXT: )
  (func $helper (param funcref))
)

(module
  ;; Create an immutable vtable in an mutable global, whose mutability prevents
  ;; optimization.

  ;; CHECK:      (type $vtable (struct (field funcref)))
  (type $vtable (struct funcref))
  ;; CHECK:      (type $object (struct (field (ref $vtable))))
  (type $object (struct (ref $vtable)))

  ;; CHECK:      (global $vtable (mut (ref $vtable)) (struct.new $vtable
  ;; CHECK-NEXT:  (ref.func $nested-creations)
  ;; CHECK-NEXT: ))
  (global $vtable (mut (ref $vtable))
    (struct.new $vtable
      (ref.func $nested-creations)
    )
  )

  ;; CHECK:      (func $nested-creations
  ;; CHECK-NEXT:  (local $ref (ref null $object))
  ;; CHECK-NEXT:  (local.set $ref
  ;; CHECK-NEXT:   (struct.new $object
  ;; CHECK-NEXT:    (global.get $vtable)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (struct.get $vtable 0
  ;; CHECK-NEXT:    (struct.get $object 0
  ;; CHECK-NEXT:     (local.get $ref)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $nested-creations
    (local $ref (ref null $object))
    (local.set $ref
      (struct.new $object
        (global.get $vtable)
      )
    )
    (call $helper
      (struct.get $vtable 0
        (struct.get $object 0
          (local.get $ref)
        )
      )
    )
  )

  ;; CHECK:      (func $helper (param $0 funcref)
  ;; CHECK-NEXT:  (nop)
  ;; CHECK-NEXT: )
  (func $helper (param funcref))
)

(module
  ;; Create an immutable vtable in an immutable global, but using an array
  ;; instead of a struct.

  ;; CHECK:      (type $object (struct (field (ref $vtable))))

  ;; CHECK:      (type $vtable (array funcref))
  (type $vtable (array funcref))
  (type $object (struct (ref $vtable)))

  ;; CHECK:      (global $vtable (ref $vtable) (array.init_static $vtable
  ;; CHECK-NEXT:  (ref.func $nested-creations)
  ;; CHECK-NEXT: ))
  (global $vtable (ref $vtable)
    (array.init_static $vtable
      (ref.func $nested-creations)
    )
  )

  ;; CHECK:      (func $nested-creations (param $param i32)
  ;; CHECK-NEXT:  (local $ref (ref null $object))
  ;; CHECK-NEXT:  (local.set $ref
  ;; CHECK-NEXT:   (struct.new $object
  ;; CHECK-NEXT:    (global.get $vtable)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (ref.func $nested-creations)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $helper
  ;; CHECK-NEXT:   (array.get $vtable
  ;; CHECK-NEXT:    (struct.get $object 0
  ;; CHECK-NEXT:     (local.get $ref)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (local.get $param)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $nested-creations (param $param i32)
    (local $ref (ref null $object))
    (local.set $ref
      (struct.new $object
        (global.get $vtable)
      )
    )
    (call $helper
      (array.get $vtable
        (struct.get $object 0
          (local.get $ref)
        )
        (i32.const 0)
      )
    )
    ;; The second operation here uses a param for the array index, which is not
    ;; constant.
    (call $helper
      (array.get $vtable
        (struct.get $object 0
          (local.get $ref)
        )
        (local.get $param)
      )
    )
  )

  ;; CHECK:      (func $helper (param $0 funcref)
  ;; CHECK-NEXT:  (nop)
  ;; CHECK-NEXT: )
  (func $helper (param funcref))
)

