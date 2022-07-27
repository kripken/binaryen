;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.

;; Tests for the "1a" form of non-nullable locals. This will likely be the final
;; form in the spec. We test:
;;
;;  * Just printing the module as read from text here. There is no limit on
;;    locals that way: everything is allowed.
;;  * Round-tripping through the binary. In the binary we enforce "1a", so some
;;    locals become nullable.
;;  * Optimizing also enforces "1a".

;; RUN: wasm-opt %s -all             -S -o - | filecheck %s --check-prefix PRINT
;; RUN: wasm-opt %s -all --roundtrip -S -o - | filecheck %s --check-prefix ROUNDTRIP
;; RUN: wasm-opt %s -all -O1         -S -o - | filecheck %s --check-prefix OPTIMIZE

(module
  (func $no-uses (export "no-uses")
    ;; A local with no uses validates.
    (local $x (ref func))
  )

  ;; PRINT:      (func $func-scope
  ;; PRINT-NEXT:  (local $x (ref func))
  ;; PRINT-NEXT:  (local.set $x
  ;; PRINT-NEXT:   (ref.func $helper)
  ;; PRINT-NEXT:  )
  ;; PRINT-NEXT:  (drop
  ;; PRINT-NEXT:   (local.get $x)
  ;; PRINT-NEXT:  )
  ;; PRINT-NEXT: )
  ;; ROUNDTRIP:      (func $func-scope
  ;; ROUNDTRIP-NEXT:  (local $x (ref func))
  ;; ROUNDTRIP-NEXT:  (local.set $x
  ;; ROUNDTRIP-NEXT:   (ref.func $helper)
  ;; ROUNDTRIP-NEXT:  )
  ;; ROUNDTRIP-NEXT:  (drop
  ;; ROUNDTRIP-NEXT:   (local.get $x)
  ;; ROUNDTRIP-NEXT:  )
  ;; ROUNDTRIP-NEXT: )
  (func $func-scope (export "func-scope")
    ;; a set in the func scope helps a get validate there.
    (local $x (ref func))
    (local.set $x
      (ref.func $helper)
    )
    (drop
      (local.get $x)
    )
  )

  ;; PRINT:      (func $inner-scope
  ;; PRINT-NEXT:  (local $x (ref func))
  ;; PRINT-NEXT:  (block $b
  ;; PRINT-NEXT:   (local.set $x
  ;; PRINT-NEXT:    (ref.func $helper)
  ;; PRINT-NEXT:   )
  ;; PRINT-NEXT:   (drop
  ;; PRINT-NEXT:    (local.get $x)
  ;; PRINT-NEXT:   )
  ;; PRINT-NEXT:  )
  ;; PRINT-NEXT: )
  ;; ROUNDTRIP:      (func $inner-scope
  ;; ROUNDTRIP-NEXT:  (local $x (ref func))
  ;; ROUNDTRIP-NEXT:  (local.set $x
  ;; ROUNDTRIP-NEXT:   (ref.func $helper)
  ;; ROUNDTRIP-NEXT:  )
  ;; ROUNDTRIP-NEXT:  (drop
  ;; ROUNDTRIP-NEXT:   (local.get $x)
  ;; ROUNDTRIP-NEXT:  )
  ;; ROUNDTRIP-NEXT: )
  (func $inner-scope (export "inner-scope")
    ;; a set in an inner scope helps a get validate there.
    (local $x (ref func))
    (block $b
      (local.set $x
        (ref.func $helper)
      )
      (drop
        (local.get $x)
      )
    )
  )

  ;; PRINT:      (func $func-to-inner
  ;; PRINT-NEXT:  (local $x (ref func))
  ;; PRINT-NEXT:  (local.set $x
  ;; PRINT-NEXT:   (ref.func $helper)
  ;; PRINT-NEXT:  )
  ;; PRINT-NEXT:  (block $b
  ;; PRINT-NEXT:   (drop
  ;; PRINT-NEXT:    (local.get $x)
  ;; PRINT-NEXT:   )
  ;; PRINT-NEXT:  )
  ;; PRINT-NEXT: )
  ;; ROUNDTRIP:      (func $func-to-inner
  ;; ROUNDTRIP-NEXT:  (local $x (ref func))
  ;; ROUNDTRIP-NEXT:  (local.set $x
  ;; ROUNDTRIP-NEXT:   (ref.func $helper)
  ;; ROUNDTRIP-NEXT:  )
  ;; ROUNDTRIP-NEXT:  (block $label$1
  ;; ROUNDTRIP-NEXT:   (drop
  ;; ROUNDTRIP-NEXT:    (local.get $x)
  ;; ROUNDTRIP-NEXT:   )
  ;; ROUNDTRIP-NEXT:  )
  ;; ROUNDTRIP-NEXT: )
  (func $func-to-inner (export "func-to-inner")
    ;; a set in an outer scope helps a get validate.
    (local $x (ref func))
    (local.set $x
      (ref.func $helper)
    )
    (block $b
      (drop
        (local.get $x)
      )
    )
  )

  ;; PRINT:      (func $inner-to-func
  ;; PRINT-NEXT:  (local $x funcref)
  ;; PRINT-NEXT:  (block $b
  ;; PRINT-NEXT:   (local.set $x
  ;; PRINT-NEXT:    (ref.func $helper)
  ;; PRINT-NEXT:   )
  ;; PRINT-NEXT:  )
  ;; PRINT-NEXT:  (drop
  ;; PRINT-NEXT:   (local.get $x)
  ;; PRINT-NEXT:  )
  ;; PRINT-NEXT: )
  ;; ROUNDTRIP:      (func $inner-to-func
  ;; ROUNDTRIP-NEXT:  (local $x funcref)
  ;; ROUNDTRIP-NEXT:  (block $label$1
  ;; ROUNDTRIP-NEXT:   (local.set $x
  ;; ROUNDTRIP-NEXT:    (ref.func $helper)
  ;; ROUNDTRIP-NEXT:   )
  ;; ROUNDTRIP-NEXT:  )
  ;; ROUNDTRIP-NEXT:  (drop
  ;; ROUNDTRIP-NEXT:   (local.get $x)
  ;; ROUNDTRIP-NEXT:  )
  ;; ROUNDTRIP-NEXT: )
  (func $inner-to-func (export "inner-to-func")
    ;; a set in an inner scope does *not* help a get validate, but the type is
    ;; nullable so that's ok.
    (local $x (ref null func))
    (block $b
      (local.set $x
        (ref.func $helper)
      )
    )
    (drop
      (local.get $x)
    )
  )

  ;; PRINT:      (func $if-condition
  ;; PRINT-NEXT:  (local $x (ref func))
  ;; PRINT-NEXT:  (if
  ;; PRINT-NEXT:   (call $helper2
  ;; PRINT-NEXT:    (local.tee $x
  ;; PRINT-NEXT:     (ref.func $helper)
  ;; PRINT-NEXT:    )
  ;; PRINT-NEXT:   )
  ;; PRINT-NEXT:   (drop
  ;; PRINT-NEXT:    (local.get $x)
  ;; PRINT-NEXT:   )
  ;; PRINT-NEXT:   (drop
  ;; PRINT-NEXT:    (local.get $x)
  ;; PRINT-NEXT:   )
  ;; PRINT-NEXT:  )
  ;; PRINT-NEXT: )
  ;; ROUNDTRIP:      (func $if-condition
  ;; ROUNDTRIP-NEXT:  (local $x (ref func))
  ;; ROUNDTRIP-NEXT:  (if
  ;; ROUNDTRIP-NEXT:   (call $helper2
  ;; ROUNDTRIP-NEXT:    (local.tee $x
  ;; ROUNDTRIP-NEXT:     (ref.func $helper)
  ;; ROUNDTRIP-NEXT:    )
  ;; ROUNDTRIP-NEXT:   )
  ;; ROUNDTRIP-NEXT:   (drop
  ;; ROUNDTRIP-NEXT:    (local.get $x)
  ;; ROUNDTRIP-NEXT:   )
  ;; ROUNDTRIP-NEXT:   (drop
  ;; ROUNDTRIP-NEXT:    (local.get $x)
  ;; ROUNDTRIP-NEXT:   )
  ;; ROUNDTRIP-NEXT:  )
  ;; ROUNDTRIP-NEXT: )
  ;; OPTIMIZE:      (func $if-condition
  ;; OPTIMIZE-NEXT:  (drop
  ;; OPTIMIZE-NEXT:   (call $helper2
  ;; OPTIMIZE-NEXT:    (ref.func $no-uses)
  ;; OPTIMIZE-NEXT:   )
  ;; OPTIMIZE-NEXT:  )
  ;; OPTIMIZE-NEXT: )
  (func $if-condition (export "if-condition")
    (local $x (ref func))
    (if
      (call $helper2
        ;; Tee in the condition is good enough for the arms.
        (local.tee $x
          (ref.func $helper)
        )
      )
      (drop
        (local.get $x)
      )
      (drop
        (local.get $x)
      )
    )
  )

  ;; PRINT:      (func $get-without-set-but-param (param $x (ref func))
  ;; PRINT-NEXT:  (drop
  ;; PRINT-NEXT:   (local.get $x)
  ;; PRINT-NEXT:  )
  ;; PRINT-NEXT: )
  ;; ROUNDTRIP:      (func $get-without-set-but-param (param $x (ref func))
  ;; ROUNDTRIP-NEXT:  (drop
  ;; ROUNDTRIP-NEXT:   (local.get $x)
  ;; ROUNDTRIP-NEXT:  )
  ;; ROUNDTRIP-NEXT: )
  ;; OPTIMIZE:      (func $get-without-set-but-param (param $0 (ref func))
  ;; OPTIMIZE-NEXT:  (nop)
  ;; OPTIMIZE-NEXT: )
  (func $get-without-set-but-param (export "get-without-set-but-param")
    ;; As a parameter, this is ok to get without a set.
    (param $x (ref func))
    (drop
      (local.get $x)
    )
  )

  ;; PRINT:      (func $helper
  ;; PRINT-NEXT:  (nop)
  ;; PRINT-NEXT: )
  ;; ROUNDTRIP:      (func $helper
  ;; ROUNDTRIP-NEXT:  (nop)
  ;; ROUNDTRIP-NEXT: )
  (func $helper)

  ;; PRINT:      (func $helper2 (param $0 anyref) (result i32)
  ;; PRINT-NEXT:  (unreachable)
  ;; PRINT-NEXT: )
  ;; ROUNDTRIP:      (func $helper2 (param $0 anyref) (result i32)
  ;; ROUNDTRIP-NEXT:  (unreachable)
  ;; ROUNDTRIP-NEXT: )
  ;; OPTIMIZE:      (func $helper2 (param $0 anyref) (result i32)
  ;; OPTIMIZE-NEXT:  (unreachable)
  ;; OPTIMIZE-NEXT: )
  (func $helper2 (param anyref) (result i32)
    (unreachable)
  )
)
