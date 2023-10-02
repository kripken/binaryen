;; NOTE: Assertions have been generated by update_lit_checks.py and should not be edited.
;; RUN: wasm-opt %s --remove-unused-brs -all -S -o - \
;; RUN:   | filecheck %s


(module
  ;; Regression test in which we need to calculate a proper LUB.
  ;; CHECK:      (func $selectify-fresh-lub (type $2) (param $x i32) (result anyref)
  ;; CHECK-NEXT:  (select (result i31ref)
  ;; CHECK-NEXT:   (ref.null none)
  ;; CHECK-NEXT:   (ref.i31
  ;; CHECK-NEXT:    (i32.const 0)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.get $x)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $selectify-fresh-lub (param $x i32) (result anyref)
    (if
      (local.get $x)
      (return
        (ref.null none)
      )
      (return
        (ref.i31 (i32.const 0))
      )
    )
  )

  ;; CHECK:      (func $selectify-simple (type $0) (param $0 i32) (result i32)
  ;; CHECK-NEXT:  (if (result i32)
  ;; CHECK-NEXT:   (i32.lt_u
  ;; CHECK-NEXT:    (i32.sub
  ;; CHECK-NEXT:     (local.get $0)
  ;; CHECK-NEXT:     (i32.const 48)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (i32.const 10)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (i32.const 1)
  ;; CHECK-NEXT:   (i32.lt_u
  ;; CHECK-NEXT:    (i32.sub
  ;; CHECK-NEXT:     (i32.or
  ;; CHECK-NEXT:      (local.get $0)
  ;; CHECK-NEXT:      (i32.const 32)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:     (i32.const 97)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (i32.const 6)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $selectify-simple (param $0 i32) (result i32)
    (if (result i32)
      (i32.lt_u
        (i32.sub
          (local.get $0)
          (i32.const 48)
        )
        (i32.const 10)
      )
      (i32.const 1)
      (i32.lt_u
        (i32.sub
          (i32.or
            (local.get $0)
            (i32.const 32)
          )
          (i32.const 97)
        )
        (i32.const 6)
      )
    )
  )

  ;; CHECK:      (func $restructure-br_if (type $0) (param $x i32) (result i32)
  ;; CHECK-NEXT:  (if (result i32)
  ;; CHECK-NEXT:   (local.get $x)
  ;; CHECK-NEXT:   (i32.const 100)
  ;; CHECK-NEXT:   (block $x (result i32)
  ;; CHECK-NEXT:    (nop)
  ;; CHECK-NEXT:    (drop
  ;; CHECK-NEXT:     (i32.const 200)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (i32.const 300)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $restructure-br_if (param $x i32) (result i32)
    ;; this block+br_if can be turned into an if.
    (block $x (result i32)
      (drop
        (br_if $x
          (i32.const 100)
          (local.get $x)
        )
      )
      (drop (i32.const 200))
      (i32.const 300)
    )
  )

  ;; CHECK:      (func $nothing (type $1)
  ;; CHECK-NEXT:  (nop)
  ;; CHECK-NEXT: )
  (func $nothing)


  ;; CHECK:      (func $restructure-br_if-condition-reorderable (type $0) (param $x i32) (result i32)
  ;; CHECK-NEXT:  (if (result i32)
  ;; CHECK-NEXT:   (block (result i32)
  ;; CHECK-NEXT:    (call $nothing)
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (i32.const 100)
  ;; CHECK-NEXT:   (block $x (result i32)
  ;; CHECK-NEXT:    (nop)
  ;; CHECK-NEXT:    (drop
  ;; CHECK-NEXT:     (i32.const 200)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (i32.const 300)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $restructure-br_if-condition-reorderable (param $x i32) (result i32)
    (block $x (result i32)
      (drop
        (br_if $x
          (i32.const 100)
          ;; the condition has side effects, but can be reordered with the value
          (block (result i32)
            (call $nothing)
            (local.get $x)
          )
        )
      )
      (drop (i32.const 200))
      (i32.const 300)
    )
  )

  ;; CHECK:      (func $restructure-br_if-value-effectful (type $0) (param $x i32) (result i32)
  ;; CHECK-NEXT:  (select
  ;; CHECK-NEXT:   (block (result i32)
  ;; CHECK-NEXT:    (call $nothing)
  ;; CHECK-NEXT:    (i32.const 100)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (block $x (result i32)
  ;; CHECK-NEXT:    (nop)
  ;; CHECK-NEXT:    (drop
  ;; CHECK-NEXT:     (i32.const 200)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (i32.const 300)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (block (result i32)
  ;; CHECK-NEXT:    (call $nothing)
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $restructure-br_if-value-effectful (param $x i32) (result i32)
    (block $x (result i32)
      (drop
        (br_if $x
          ;; the value has side effects, but we can use a select instead
          ;; of an if, which keeps the value first
          (block (result i32)
            (call $nothing)
            (i32.const 100)
          )
          ;; the condition has side effects too, but can be be reordered
          ;; to the end of the block
          (block (result i32)
            (call $nothing)
            (local.get $x)
          )
        )
      )
      (drop (i32.const 200))
      (i32.const 300)
    )
  )

  ;; CHECK:      (func $restructure-br_if-value-effectful-corner-case-1 (type $0) (param $x i32) (result i32)
  ;; CHECK-NEXT:  (block $x (result i32)
  ;; CHECK-NEXT:   (drop
  ;; CHECK-NEXT:    (br_if $x
  ;; CHECK-NEXT:     (block (result i32)
  ;; CHECK-NEXT:      (call $nothing)
  ;; CHECK-NEXT:      (i32.const 100)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:     (block (result i32)
  ;; CHECK-NEXT:      (call $nothing)
  ;; CHECK-NEXT:      (local.get $x)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (call $nothing)
  ;; CHECK-NEXT:   (i32.const 300)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $restructure-br_if-value-effectful-corner-case-1 (param $x i32) (result i32)
    (block $x (result i32)
      (drop
        (br_if $x
          (block (result i32)
            (call $nothing)
            (i32.const 100)
          )
          (block (result i32)
            (call $nothing)
            (local.get $x)
          )
        )
      )
      ;; the condition cannot be reordered with this
      (call $nothing)
      (i32.const 300)
    )
  )

  ;; CHECK:      (func $get-i32 (type $3) (result i32)
  ;; CHECK-NEXT:  (i32.const 400)
  ;; CHECK-NEXT: )
  (func $get-i32 (result i32)
    (i32.const 400)
  )

  ;; CHECK:      (func $restructure-br_if-value-effectful-corner-case-2 (type $0) (param $x i32) (result i32)
  ;; CHECK-NEXT:  (block $x (result i32)
  ;; CHECK-NEXT:   (drop
  ;; CHECK-NEXT:    (br_if $x
  ;; CHECK-NEXT:     (block (result i32)
  ;; CHECK-NEXT:      (call $nothing)
  ;; CHECK-NEXT:      (i32.const 100)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:     (block (result i32)
  ;; CHECK-NEXT:      (call $nothing)
  ;; CHECK-NEXT:      (local.get $x)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (drop
  ;; CHECK-NEXT:    (i32.const 300)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (call $get-i32)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $restructure-br_if-value-effectful-corner-case-2 (param $x i32) (result i32)
    (block $x (result i32)
      (drop
        (br_if $x
          (block (result i32)
            (call $nothing)
            (i32.const 100)
          )
          (block (result i32)
            (call $nothing)
            (local.get $x)
          )
        )
      )
      (drop (i32.const 300))
      ;; the condition cannot be reordered with this
      (call $get-i32)
    )
  )
  ;; CHECK:      (func $restructure-br_if-value-effectful-corner-case-3 (type $0) (param $x i32) (result i32)
  ;; CHECK-NEXT:  (block $x (result i32)
  ;; CHECK-NEXT:   (drop
  ;; CHECK-NEXT:    (br_if $x
  ;; CHECK-NEXT:     (block (result i32)
  ;; CHECK-NEXT:      (call $nothing)
  ;; CHECK-NEXT:      (i32.const 100)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:     (local.get $x)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (call $nothing)
  ;; CHECK-NEXT:   (i32.const 100)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $restructure-br_if-value-effectful-corner-case-3 (param $x i32) (result i32)
    (block $x (result i32)
      (drop
        (br_if $x
          ;; we can't do an if because of effects here
          (block (result i32)
            (call $nothing)
            (i32.const 100)
          )
          (local.get $x)
        )
      )
      ;; and we can't do a select because of effects here
      (call $nothing)
      (i32.const 100)
    )
  )

  ;; CHECK:      (func $restructure-br_if-value-effectful-corner-case-4 (type $0) (param $x i32) (result i32)
  ;; CHECK-NEXT:  (block $x (result i32)
  ;; CHECK-NEXT:   (drop
  ;; CHECK-NEXT:    (br_if $x
  ;; CHECK-NEXT:     (block (result i32)
  ;; CHECK-NEXT:      (call $nothing)
  ;; CHECK-NEXT:      (i32.const 100)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:     (local.get $x)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (drop
  ;; CHECK-NEXT:    (i32.const 300)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (call $get-i32)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $restructure-br_if-value-effectful-corner-case-4 (param $x i32) (result i32)
    (block $x (result i32)
      (drop
        (br_if $x
          ;; we can't do an if because of effects here
          (block (result i32)
            (call $nothing)
            (i32.const 100)
          )
          (local.get $x)
        )
      )
      (drop (i32.const 300))
      ;; and we can't do a select because of effects here
      (call $get-i32)
    )
  )

  ;; CHECK:      (func $restructure-select-no-multivalue (type $1)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (block $block (result i32 i32)
  ;; CHECK-NEXT:    (drop
  ;; CHECK-NEXT:     (br_if $block
  ;; CHECK-NEXT:      (tuple.make
  ;; CHECK-NEXT:       (i32.const 1)
  ;; CHECK-NEXT:       (call $restructure-br_if
  ;; CHECK-NEXT:        (i32.const 2)
  ;; CHECK-NEXT:       )
  ;; CHECK-NEXT:      )
  ;; CHECK-NEXT:      (i32.const 3)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (tuple.make
  ;; CHECK-NEXT:     (i32.const 4)
  ;; CHECK-NEXT:     (i32.const 5)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $restructure-select-no-multivalue
    (drop
      (block $block (result i32 i32)
        (drop
          (br_if $block
            (tuple.make
              (i32.const 1)
              ;; Add a side effect to prevent us turning $block into a
              ;; restructured if - instead, we will try a restructured select.
              ;; But, selects cannot return multiple values in the spec, so we
              ;; can do nothing here.
              (call $restructure-br_if
                (i32.const 2)
              )
            )
            (i32.const 3)
          )
        )
        (tuple.make
          (i32.const 4)
          (i32.const 5)
        )
      )
    )
  )

  ;; CHECK:      (func $if-of-if (type $1)
  ;; CHECK-NEXT:  (local $x i32)
  ;; CHECK-NEXT:  (if
  ;; CHECK-NEXT:   (select
  ;; CHECK-NEXT:    (local.tee $x
  ;; CHECK-NEXT:     (i32.const 1)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (i32.const 0)
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (call $if-of-if)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $if-of-if
    (local $x i32)
    ;; The outer if has side effects in the condition while the inner one does
    ;; not, which means we can fold them.
    (if
      (local.tee $x
        (i32.const 1)
      )
      (if
        (local.get $x)
        (call $if-of-if)
      )
    )
  )

  ;; CHECK:      (func $if-of-if-but-side-effects (type $1)
  ;; CHECK-NEXT:  (local $x i32)
  ;; CHECK-NEXT:  (if
  ;; CHECK-NEXT:   (local.tee $x
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (if
  ;; CHECK-NEXT:    (local.tee $x
  ;; CHECK-NEXT:     (i32.const 2)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (call $if-of-if-but-side-effects)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $if-of-if-but-side-effects
    (local $x i32)
    ;; The inner if has side effects in the condition, which prevents this
    ;; optimization.
    (if
      (local.tee $x
        (i32.const 1)
      )
      (if
        (local.tee $x
          (i32.const 2)
        )
        (call $if-of-if-but-side-effects)
      )
    )
  )

  ;; CHECK:      (func $if-of-if-but-too-costly (type $1)
  ;; CHECK-NEXT:  (local $x i32)
  ;; CHECK-NEXT:  (if
  ;; CHECK-NEXT:   (local.tee $x
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (if
  ;; CHECK-NEXT:    (i32.eqz
  ;; CHECK-NEXT:     (i32.eqz
  ;; CHECK-NEXT:      (i32.eqz
  ;; CHECK-NEXT:       (i32.eqz
  ;; CHECK-NEXT:        (i32.eqz
  ;; CHECK-NEXT:         (i32.eqz
  ;; CHECK-NEXT:          (i32.eqz
  ;; CHECK-NEXT:           (i32.eqz
  ;; CHECK-NEXT:            (i32.eqz
  ;; CHECK-NEXT:             (local.get $x)
  ;; CHECK-NEXT:            )
  ;; CHECK-NEXT:           )
  ;; CHECK-NEXT:          )
  ;; CHECK-NEXT:         )
  ;; CHECK-NEXT:        )
  ;; CHECK-NEXT:       )
  ;; CHECK-NEXT:      )
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (call $if-of-if-but-too-costly)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $if-of-if-but-too-costly
    (local $x i32)
    ;; The inner if's condition has no effects, but it is very costly, so do not
    ;; run it unconditionally - leave this unoptimized.
    (if
      (local.tee $x
        (i32.const 1)
      )
      (if
        (i32.eqz (i32.eqz (i32.eqz (i32.eqz (i32.eqz (i32.eqz (i32.eqz (i32.eqz (i32.eqz
          (local.get $x)
        )))))))))
        (call $if-of-if-but-too-costly)
      )
    )
  )

  ;; CHECK:      (func $if-of-if-but-inner-else (type $1)
  ;; CHECK-NEXT:  (local $x i32)
  ;; CHECK-NEXT:  (if
  ;; CHECK-NEXT:   (local.tee $x
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (if
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:    (call $if-of-if-but-inner-else)
  ;; CHECK-NEXT:    (call $if-of-if)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $if-of-if-but-inner-else
    (local $x i32)
    ;; The inner if has an else. For now, leave this unoptimized.
    (if
      (local.tee $x
        (i32.const 1)
      )
      (if
        (local.get $x)
        (call $if-of-if-but-inner-else)
        (call $if-of-if)
      )
    )
  )

  ;; CHECK:      (func $if-of-if-but-outer-else (type $1)
  ;; CHECK-NEXT:  (local $x i32)
  ;; CHECK-NEXT:  (if
  ;; CHECK-NEXT:   (local.tee $x
  ;; CHECK-NEXT:    (i32.const 1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (if
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:    (call $if-of-if-but-outer-else)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (call $if-of-if)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $if-of-if-but-outer-else
    (local $x i32)
    ;; The outer if has an else. For now, leave this unoptimized.
    (if
      (local.tee $x
        (i32.const 1)
      )
      (if
        (local.get $x)
        (call $if-of-if-but-outer-else)
      )
      (call $if-of-if)
    )
  )
)
