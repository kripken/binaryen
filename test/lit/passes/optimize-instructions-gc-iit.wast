;; NOTE: Assertions have been generated by update_lit_checks.py and should not be edited.
;; RUN: wasm-opt %s --optimize-instructions --ignore-implicit-traps --enable-reference-types --enable-gc -S -o - \
;; RUN:   | filecheck %s
;; RUN: wasm-opt %s --optimize-instructions --ignore-implicit-traps --enable-reference-types --enable-gc --nominal -S -o - \
;; RUN:   | filecheck %s --check-prefix NOMNL
;; Also test trapsNeverHappen (with nominal; no need for both type system modes).
;; RUN: wasm-opt %s --optimize-instructions --traps-never-happen --enable-reference-types --enable-gc --nominal -S -o - \
;; RUN:   | filecheck %s --check-prefix NOMNL-TNH

(module
  ;; CHECK:      (type $parent (struct (field i32)))
  ;; NOMNL:      (type $parent (struct (field i32)))
  ;; NOMNL-TNH:      (type $parent (struct (field i32)))
  (type $parent (struct (field i32)))
  ;; CHECK:      (type $child (struct (field i32) (field f64)))
  ;; NOMNL:      (type $child (struct (field i32) (field f64)) (extends $parent))
  ;; NOMNL-TNH:      (type $child (struct (field i32) (field f64)) (extends $parent))
  (type $child  (struct (field i32) (field f64)) (extends $parent))
  ;; CHECK:      (type $other (struct (field i64) (field f32)))
  ;; NOMNL:      (type $other (struct (field i64) (field f32)))
  ;; NOMNL-TNH:      (type $other (struct (field i64) (field f32)))
  (type $other  (struct (field i64) (field f32)))

  ;; CHECK:      (func $foo
  ;; CHECK-NEXT:  (nop)
  ;; CHECK-NEXT: )
  ;; NOMNL:      (func $foo
  ;; NOMNL-NEXT:  (nop)
  ;; NOMNL-NEXT: )
  ;; NOMNL-TNH:      (func $foo
  ;; NOMNL-TNH-NEXT:  (nop)
  ;; NOMNL-TNH-NEXT: )
  (func $foo)


  ;; CHECK:      (func $ref-cast-iit (param $parent (ref $parent)) (param $child (ref $child)) (param $other (ref $other)) (param $parent-rtt (rtt $parent)) (param $child-rtt (rtt $child)) (param $other-rtt (rtt $other))
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (block (result (ref $parent))
  ;; CHECK-NEXT:    (drop
  ;; CHECK-NEXT:     (local.get $parent-rtt)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (local.get $parent)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (block (result (ref $child))
  ;; CHECK-NEXT:    (drop
  ;; CHECK-NEXT:     (local.get $parent-rtt)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (local.get $child)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast
  ;; CHECK-NEXT:    (local.get $parent)
  ;; CHECK-NEXT:    (local.get $child-rtt)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (block
  ;; CHECK-NEXT:    (drop
  ;; CHECK-NEXT:     (local.get $child)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (drop
  ;; CHECK-NEXT:     (local.get $other-rtt)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (unreachable)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  ;; NOMNL:      (func $ref-cast-iit (param $parent (ref $parent)) (param $child (ref $child)) (param $other (ref $other)) (param $parent-rtt (rtt $parent)) (param $child-rtt (rtt $child)) (param $other-rtt (rtt $other))
  ;; NOMNL-NEXT:  (drop
  ;; NOMNL-NEXT:   (block (result (ref $parent))
  ;; NOMNL-NEXT:    (drop
  ;; NOMNL-NEXT:     (local.get $parent-rtt)
  ;; NOMNL-NEXT:    )
  ;; NOMNL-NEXT:    (local.get $parent)
  ;; NOMNL-NEXT:   )
  ;; NOMNL-NEXT:  )
  ;; NOMNL-NEXT:  (drop
  ;; NOMNL-NEXT:   (block (result (ref $child))
  ;; NOMNL-NEXT:    (drop
  ;; NOMNL-NEXT:     (local.get $parent-rtt)
  ;; NOMNL-NEXT:    )
  ;; NOMNL-NEXT:    (local.get $child)
  ;; NOMNL-NEXT:   )
  ;; NOMNL-NEXT:  )
  ;; NOMNL-NEXT:  (drop
  ;; NOMNL-NEXT:   (ref.cast
  ;; NOMNL-NEXT:    (local.get $parent)
  ;; NOMNL-NEXT:    (local.get $child-rtt)
  ;; NOMNL-NEXT:   )
  ;; NOMNL-NEXT:  )
  ;; NOMNL-NEXT:  (drop
  ;; NOMNL-NEXT:   (block
  ;; NOMNL-NEXT:    (drop
  ;; NOMNL-NEXT:     (local.get $child)
  ;; NOMNL-NEXT:    )
  ;; NOMNL-NEXT:    (drop
  ;; NOMNL-NEXT:     (local.get $other-rtt)
  ;; NOMNL-NEXT:    )
  ;; NOMNL-NEXT:    (unreachable)
  ;; NOMNL-NEXT:   )
  ;; NOMNL-NEXT:  )
  ;; NOMNL-NEXT: )
  ;; NOMNL-TNH:      (func $ref-cast-iit (param $parent (ref $parent)) (param $child (ref $child)) (param $other (ref $other)) (param $parent-rtt (rtt $parent)) (param $child-rtt (rtt $child)) (param $other-rtt (rtt $other))
  ;; NOMNL-TNH-NEXT:  (drop
  ;; NOMNL-TNH-NEXT:   (block (result (ref $parent))
  ;; NOMNL-TNH-NEXT:    (drop
  ;; NOMNL-TNH-NEXT:     (local.get $parent-rtt)
  ;; NOMNL-TNH-NEXT:    )
  ;; NOMNL-TNH-NEXT:    (local.get $parent)
  ;; NOMNL-TNH-NEXT:   )
  ;; NOMNL-TNH-NEXT:  )
  ;; NOMNL-TNH-NEXT:  (drop
  ;; NOMNL-TNH-NEXT:   (block (result (ref $child))
  ;; NOMNL-TNH-NEXT:    (drop
  ;; NOMNL-TNH-NEXT:     (local.get $parent-rtt)
  ;; NOMNL-TNH-NEXT:    )
  ;; NOMNL-TNH-NEXT:    (local.get $child)
  ;; NOMNL-TNH-NEXT:   )
  ;; NOMNL-TNH-NEXT:  )
  ;; NOMNL-TNH-NEXT:  (drop
  ;; NOMNL-TNH-NEXT:   (ref.cast
  ;; NOMNL-TNH-NEXT:    (local.get $parent)
  ;; NOMNL-TNH-NEXT:    (local.get $child-rtt)
  ;; NOMNL-TNH-NEXT:   )
  ;; NOMNL-TNH-NEXT:  )
  ;; NOMNL-TNH-NEXT:  (drop
  ;; NOMNL-TNH-NEXT:   (block
  ;; NOMNL-TNH-NEXT:    (drop
  ;; NOMNL-TNH-NEXT:     (local.get $child)
  ;; NOMNL-TNH-NEXT:    )
  ;; NOMNL-TNH-NEXT:    (drop
  ;; NOMNL-TNH-NEXT:     (local.get $other-rtt)
  ;; NOMNL-TNH-NEXT:    )
  ;; NOMNL-TNH-NEXT:    (unreachable)
  ;; NOMNL-TNH-NEXT:   )
  ;; NOMNL-TNH-NEXT:  )
  ;; NOMNL-TNH-NEXT: )
  (func $ref-cast-iit
    (param $parent (ref $parent))
    (param $child (ref $child))
    (param $other (ref $other))

    (param $parent-rtt (rtt $parent))
    (param $child-rtt (rtt $child))
    (param $other-rtt (rtt $other))

    ;; a cast of parent to an rtt of parent: static subtyping matches.
    (drop
      (ref.cast
        (local.get $parent)
        (local.get $parent-rtt)
      )
    )
    ;; a cast of child to a supertype: static subtyping matches.
    (drop
      (ref.cast
        (local.get $child)
        (local.get $parent-rtt)
      )
    )
    ;; a cast of parent to a subtype: static subtyping does not match.
    (drop
      (ref.cast
        (local.get $parent)
        (local.get $child-rtt)
      )
    )
    ;; a cast of child to an unrelated type: static subtyping does not match.
    (drop
      (ref.cast
        (local.get $child)
        (local.get $other-rtt)
      )
    )
  )

  ;; CHECK:      (func $ref-cast-iit-bad (param $parent (ref $parent)) (param $parent-rtt (rtt $parent))
  ;; CHECK-NEXT:  (local $2 (ref null $parent))
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (block (result (ref $parent))
  ;; CHECK-NEXT:    (drop
  ;; CHECK-NEXT:     (block (result (rtt $parent))
  ;; CHECK-NEXT:      (local.set $2
  ;; CHECK-NEXT:       (block $block (result (ref $parent))
  ;; CHECK-NEXT:        (call $foo)
  ;; CHECK-NEXT:        (local.get $parent)
  ;; CHECK-NEXT:       )
  ;; CHECK-NEXT:      )
  ;; CHECK-NEXT:      (block $block0 (result (rtt $parent))
  ;; CHECK-NEXT:       (call $foo)
  ;; CHECK-NEXT:       (local.get $parent-rtt)
  ;; CHECK-NEXT:      )
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (ref.as_non_null
  ;; CHECK-NEXT:     (local.get $2)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast
  ;; CHECK-NEXT:    (local.get $parent)
  ;; CHECK-NEXT:    (unreachable)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast
  ;; CHECK-NEXT:    (unreachable)
  ;; CHECK-NEXT:    (local.get $parent-rtt)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  ;; NOMNL:      (func $ref-cast-iit-bad (param $parent (ref $parent)) (param $parent-rtt (rtt $parent))
  ;; NOMNL-NEXT:  (local $2 (ref null $parent))
  ;; NOMNL-NEXT:  (drop
  ;; NOMNL-NEXT:   (block (result (ref $parent))
  ;; NOMNL-NEXT:    (drop
  ;; NOMNL-NEXT:     (block (result (rtt $parent))
  ;; NOMNL-NEXT:      (local.set $2
  ;; NOMNL-NEXT:       (block $block (result (ref $parent))
  ;; NOMNL-NEXT:        (call $foo)
  ;; NOMNL-NEXT:        (local.get $parent)
  ;; NOMNL-NEXT:       )
  ;; NOMNL-NEXT:      )
  ;; NOMNL-NEXT:      (block $block0 (result (rtt $parent))
  ;; NOMNL-NEXT:       (call $foo)
  ;; NOMNL-NEXT:       (local.get $parent-rtt)
  ;; NOMNL-NEXT:      )
  ;; NOMNL-NEXT:     )
  ;; NOMNL-NEXT:    )
  ;; NOMNL-NEXT:    (ref.as_non_null
  ;; NOMNL-NEXT:     (local.get $2)
  ;; NOMNL-NEXT:    )
  ;; NOMNL-NEXT:   )
  ;; NOMNL-NEXT:  )
  ;; NOMNL-NEXT:  (drop
  ;; NOMNL-NEXT:   (ref.cast
  ;; NOMNL-NEXT:    (local.get $parent)
  ;; NOMNL-NEXT:    (unreachable)
  ;; NOMNL-NEXT:   )
  ;; NOMNL-NEXT:  )
  ;; NOMNL-NEXT:  (drop
  ;; NOMNL-NEXT:   (ref.cast
  ;; NOMNL-NEXT:    (unreachable)
  ;; NOMNL-NEXT:    (local.get $parent-rtt)
  ;; NOMNL-NEXT:   )
  ;; NOMNL-NEXT:  )
  ;; NOMNL-NEXT: )
  ;; NOMNL-TNH:      (func $ref-cast-iit-bad (param $parent (ref $parent)) (param $parent-rtt (rtt $parent))
  ;; NOMNL-TNH-NEXT:  (local $2 (ref null $parent))
  ;; NOMNL-TNH-NEXT:  (drop
  ;; NOMNL-TNH-NEXT:   (block (result (ref $parent))
  ;; NOMNL-TNH-NEXT:    (drop
  ;; NOMNL-TNH-NEXT:     (block (result (rtt $parent))
  ;; NOMNL-TNH-NEXT:      (local.set $2
  ;; NOMNL-TNH-NEXT:       (block $block (result (ref $parent))
  ;; NOMNL-TNH-NEXT:        (call $foo)
  ;; NOMNL-TNH-NEXT:        (local.get $parent)
  ;; NOMNL-TNH-NEXT:       )
  ;; NOMNL-TNH-NEXT:      )
  ;; NOMNL-TNH-NEXT:      (block $block0 (result (rtt $parent))
  ;; NOMNL-TNH-NEXT:       (call $foo)
  ;; NOMNL-TNH-NEXT:       (local.get $parent-rtt)
  ;; NOMNL-TNH-NEXT:      )
  ;; NOMNL-TNH-NEXT:     )
  ;; NOMNL-TNH-NEXT:    )
  ;; NOMNL-TNH-NEXT:    (ref.as_non_null
  ;; NOMNL-TNH-NEXT:     (local.get $2)
  ;; NOMNL-TNH-NEXT:    )
  ;; NOMNL-TNH-NEXT:   )
  ;; NOMNL-TNH-NEXT:  )
  ;; NOMNL-TNH-NEXT:  (drop
  ;; NOMNL-TNH-NEXT:   (ref.cast
  ;; NOMNL-TNH-NEXT:    (local.get $parent)
  ;; NOMNL-TNH-NEXT:    (unreachable)
  ;; NOMNL-TNH-NEXT:   )
  ;; NOMNL-TNH-NEXT:  )
  ;; NOMNL-TNH-NEXT:  (drop
  ;; NOMNL-TNH-NEXT:   (ref.cast
  ;; NOMNL-TNH-NEXT:    (unreachable)
  ;; NOMNL-TNH-NEXT:    (local.get $parent-rtt)
  ;; NOMNL-TNH-NEXT:   )
  ;; NOMNL-TNH-NEXT:  )
  ;; NOMNL-TNH-NEXT: )
  (func $ref-cast-iit-bad
    (param $parent (ref $parent))
    (param $parent-rtt (rtt $parent))

    ;; optimizing this cast away requires reordering.
    (drop
      (ref.cast
        (block (result (ref $parent))
          (call $foo)
          (local.get $parent)
        )
        (block (result (rtt $parent))
          (call $foo)
          (local.get $parent-rtt)
        )
      )
    )

    ;; ignore unreachability
    (drop
      (ref.cast
        (local.get $parent)
        (unreachable)
      )
    )
    (drop
      (ref.cast
        (unreachable)
        (local.get $parent-rtt)
      )
    )
  )

  ;; CHECK:      (func $ref-eq-ref-cast (param $x eqref)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (i32.const 1)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  ;; NOMNL:      (func $ref-eq-ref-cast (param $x eqref)
  ;; NOMNL-NEXT:  (drop
  ;; NOMNL-NEXT:   (i32.const 1)
  ;; NOMNL-NEXT:  )
  ;; NOMNL-NEXT: )
  ;; NOMNL-TNH:      (func $ref-eq-ref-cast (param $x eqref)
  ;; NOMNL-TNH-NEXT:  (drop
  ;; NOMNL-TNH-NEXT:   (i32.const 1)
  ;; NOMNL-TNH-NEXT:  )
  ;; NOMNL-TNH-NEXT: )
  (func $ref-eq-ref-cast (param $x eqref)
    ;; we can look through a ref.cast if we ignore traps
    (drop
      (ref.eq
        (local.get $x)
        (ref.cast
          (local.get $x)
          (rtt.canon $parent)
        )
      )
    )
  )
)
