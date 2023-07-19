;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.
;; RUN: foreach %s %t wasm-opt -all --gufa -tnh -S -o - | filecheck %s

(module
  ;; CHECK:      (type $funcref_funcref_funcref_funcref_=>_none (func (param funcref funcref funcref funcref)))

  ;; CHECK:      (type $none_=>_none (func))

  ;; CHECK:      (import "a" "b" (global $unknown-i32 i32))
  (import "a" "b" (global $unknown-i32 i32))

  ;; CHECK:      (import "a" "b" (global $unknown-funcref1 funcref))
  (import "a" "b" (global $unknown-funcref1 funcref))

  ;; CHECK:      (import "a" "b" (global $unknown-funcref2 funcref))
  (import "a" "b" (global $unknown-funcref2 funcref))

  ;; CHECK:      (import "a" "b" (global $unknown-nn-func1 (ref func)))
  (import "a" "b" (global $unknown-nn-func1 (ref func)))

  ;; CHECK:      (import "a" "b" (global $unknown-nn-func2 (ref func)))
  (import "a" "b" (global $unknown-nn-func2 (ref func)))

  ;; CHECK:      (func $called (type $funcref_funcref_funcref_funcref_=>_none) (param $x funcref) (param $no-cast funcref) (param $y funcref) (param $z funcref)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.as_func
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.as_func
  ;; CHECK-NEXT:    (local.get $y)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.as_func
  ;; CHECK-NEXT:    (local.get $z)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $called (param $x funcref) (param $no-cast funcref) (param $y funcref) (param $z funcref)
    ;; All but the second parameter are cast here, which allows some
    ;; optimization in the caller. Nothing significant changes here.
    (drop
      (ref.cast func
        (local.get $x)
      )
    )
    (drop
      (ref.cast func
        (local.get $y)
      )
    )
    (drop
      (ref.cast func
        (local.get $z)
      )
    )
  )

  ;; CHECK:      (func $caller (type $none_=>_none)
  ;; CHECK-NEXT:  (local $f funcref)
  ;; CHECK-NEXT:  (local.set $f
  ;; CHECK-NEXT:   (select (result funcref)
  ;; CHECK-NEXT:    (global.get $unknown-funcref1)
  ;; CHECK-NEXT:    (global.get $unknown-funcref2)
  ;; CHECK-NEXT:    (global.get $unknown-i32)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (ref.as_func
  ;; CHECK-NEXT:    (local.get $f)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (ref.cast null func
  ;; CHECK-NEXT:    (local.get $f)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.get $f)
  ;; CHECK-NEXT:   (ref.as_func
  ;; CHECK-NEXT:    (local.get $f)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (ref.as_func
  ;; CHECK-NEXT:    (local.get $f)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (local.get $f)
  ;; CHECK-NEXT:   (ref.as_func
  ;; CHECK-NEXT:    (local.get $f)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (ref.as_func
  ;; CHECK-NEXT:    (local.get $f)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller
    (local $f funcref)

    ;; Fill the local with an unknown value (so that trivial inference doesn't
    ;; optimize away the thing we care about below).
    (local.set $f
      (select
        (global.get $unknown-funcref1)
        (global.get $unknown-funcref2)
        (global.get $unknown-i32)
      )
    )

    ;; All but the third parameter are cast here. The cast has no effect by
    ;; itself as the type is funcref, but GUFA will refine casts when it can
    ;; (but not add a new cast, which might not be worth it).
    ;;
    ;; Specifically here, the first and last cast can be refined, since those
    ;; are cast both here and in the called function. Those casts will lose the
    ;; "null" and become non-nullable.
    (call $called
      (ref.cast null func
        (local.get $f)
      )
      (ref.cast null func
        (local.get $f)
      )
      (local.get $f)
      (ref.cast null func
        (local.get $f)
      )
    )

    ;; Another call, but with different casts.
    (call $called
      (ref.cast func ;; this is now non-nullable, and will not change
        (local.get $f)
      )
      (local.get $f) ;; this is not cast, and will not change.
      (ref.cast null func
        (local.get $f) ;; this is now cast, and will be optimized.
      )
      (ref.cast null func ;; this is the same as before, and will be optimized.
        (local.get $f)
      )
    )
  )
)

(module
  ;; CHECK:      (type $A (struct (field (mut i32))))
  (type $A (struct (field (mut i32))))

  ;; CHECK:      (type $B (sub $A (struct (field (mut i32)))))
  (type $B (sub $A (struct (field (mut i32)))))

  ;; CHECK:      (type $none_=>_none (func))

  ;; CHECK:      (type $ref?|$A|_=>_none (func (param (ref null $A))))

  ;; CHECK:      (type $anyref_=>_none (func (param anyref)))

  ;; CHECK:      (export "out" (func $caller))

  ;; CHECK:      (func $maker (type $none_=>_none)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.new $A
  ;; CHECK-NEXT:    (i32.const 10)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.new $B
  ;; CHECK-NEXT:    (i32.const 20)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $maker
    ;; A always contains 10, and B always contains 20.
    (drop
      (struct.new $A
        (i32.const 10)
      )
    )
    (drop
      (struct.new $B
        (i32.const 20)
      )
    )
  )

  ;; CHECK:      (func $called (type $ref?|$A|_=>_none) (param $x (ref null $A))
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $called (param $x (ref null $A))
    ;; Cast the input to a $B, which will help the caller.
    (drop
      (ref.cast $B
        (local.get $x)
      )
    )
  )

  ;; CHECK:      (func $caller (type $anyref_=>_none) (param $any anyref)
  ;; CHECK-NEXT:  (local $x (ref null $A))
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (local.tee $x
  ;; CHECK-NEXT:    (ref.cast $B
  ;; CHECK-NEXT:     (local.get $any)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.get $A 0
  ;; CHECK-NEXT:    (ref.cast $A
  ;; CHECK-NEXT:     (local.get $any)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.test $B
  ;; CHECK-NEXT:    (local.get $any)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (i32.const 20)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (i32.const 1)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller (export "out") (param $any anyref)
    (local $x (ref null $A))
    ;; The called function casts to $B. This lets us infer the value of the
    ;; fallthrough ref.cast, which will turn into $B. Furthermore, that then
    ;; tells us what is written into the local $x, and the forward flow
    ;; analysis will use that fact in the local.get $x below.
    (call $called
      (local.tee $x
        (ref.cast $A
          (local.get $any)
        )
      )
    )
    ;; We can't infer anything here at the moment, but a more sophisticated
    ;; analysis could. (Other passes can help here, however, by using $x where
    ;; $any appears.)
    (drop
      (struct.get $A 0
        (ref.cast $A
          (local.get $any)
        )
      )
    )
    (drop
      (ref.test $B
        (local.get $any)
      )
    )
    ;; We know that $x must contain $B, so this can be inferred to be 20, and
    ;; the ref.is to 1.
    (drop
      (struct.get $A 0
        (local.get $x)
      )
    )
    (drop
      (ref.test $B
        (local.get $x)
      )
    )
  )
)

;; A local.tee by itself, without a cast.
(module
  ;; CHECK:      (type $A (struct (field (mut i32))))
  (type $A (struct (field (mut i32))))

  ;; CHECK:      (type $B (sub $A (struct (field (mut i32)))))
  (type $B (sub $A (struct (field (mut i32)))))

  ;; CHECK:      (type $ref?|$A|_=>_none (func (param (ref null $A))))

  ;; CHECK:      (type $none_=>_none (func))

  ;; CHECK:      (export "out" (func $caller))

  ;; CHECK:      (func $maker (type $none_=>_none)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.new $A
  ;; CHECK-NEXT:    (i32.const 10)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.new $B
  ;; CHECK-NEXT:    (i32.const 20)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $maker
    ;; A always contains 10, and B always contains 20.
    (drop
      (struct.new $A
        (i32.const 10)
      )
    )
    (drop
      (struct.new $B
        (i32.const 20)
      )
    )
  )

  ;; CHECK:      (func $called (type $ref?|$A|_=>_none) (param $x (ref null $A))
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $called (param $x (ref null $A))
    ;; Cast the input to a $B, which will help the caller.
    (drop
      (ref.cast $B
        (local.get $x)
      )
    )
  )

  ;; CHECK:      (func $caller (type $ref?|$A|_=>_none) (param $a (ref null $A))
  ;; CHECK-NEXT:  (local $x (ref null $A))
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (local.tee $x
  ;; CHECK-NEXT:    (local.get $a)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (i32.const 20)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller (export "out") (param $a (ref null $A))
    (local $x (ref null $A))
    ;; The change compared to before is that we only have a local.tee here, and
    ;; no ref.cast. We can still infer the type of the tee's value, and
    ;; therefore the type of $x when it is read below, and optimize there.
    (call $called
      (local.tee $x
        (local.get $a)
      )
    )
    ;; This can be inferred to be 20.
    (drop
      (struct.get $A 0
        (local.get $x)
      )
    )
  )
)

;; As above, but add a local.tee in the called function.
(module
  ;; CHECK:      (type $A (struct (field (mut i32))))
  (type $A (struct (field (mut i32))))

  ;; CHECK:      (type $B (sub $A (struct (field (mut i32)))))
  (type $B (sub $A (struct (field (mut i32)))))

  ;; CHECK:      (type $ref?|$A|_=>_none (func (param (ref null $A))))

  ;; CHECK:      (type $anyref_=>_none (func (param anyref)))

  ;; CHECK:      (export "out" (func $caller))

  ;; CHECK:      (func $called (type $ref?|$A|_=>_none) (param $x (ref null $A))
  ;; CHECK-NEXT:  (local $local (ref null $A))
  ;; CHECK-NEXT:  (nop)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (i32.const 42)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.tee $local
  ;; CHECK-NEXT:     (local.get $x)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $called (param $x (ref null $A))
    (local $local (ref null $A))
    ;; Some nops and such do not bother us.
    (nop)
    (drop
      (i32.const 42)
    )
    (drop
      (ref.cast $B
        ;; This local.tee should not stop us from optimizing.
        (local.tee $local
          (local.get $x)
        )
      )
    )
  )

  ;; CHECK:      (func $caller (type $anyref_=>_none) (param $any anyref)
  ;; CHECK-NEXT:  (local $x (ref null $A))
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (local.tee $x
  ;; CHECK-NEXT:    (ref.cast $B
  ;; CHECK-NEXT:     (local.get $any)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller (export "out") (param $any anyref)
    (local $x (ref null $A))
    (call $called
      (local.tee $x
        (ref.cast $A ;; this cast will be refined
          (local.get $any)
        )
      )
    )
  )
)

;; As above, but now add some control flow before the cast in the function.
(module
  ;; CHECK:      (type $A (struct (field (mut i32))))
  (type $A (struct (field (mut i32))))

  ;; CHECK:      (type $ref?|$A|_=>_none (func (param (ref null $A))))

  ;; CHECK:      (type $B (sub $A (struct (field (mut i32)))))
  (type $B (sub $A (struct (field (mut i32)))))

  ;; CHECK:      (type $anyref_=>_none (func (param anyref)))

  ;; CHECK:      (export "out" (func $caller))

  ;; CHECK:      (func $called (type $ref?|$A|_=>_none) (param $x (ref null $A))
  ;; CHECK-NEXT:  (local $local (ref null $A))
  ;; CHECK-NEXT:  (if
  ;; CHECK-NEXT:   (i32.const 0)
  ;; CHECK-NEXT:   (return)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $called (param $x (ref null $A))
    (local $local (ref null $A))
    ;; Control flow before the cast *does* stop us from optimizing.
    (if
      (i32.const 0)
      (return)
    )
    (drop
      (ref.cast $B
        (local.get $x)
      )
    )
  )

  ;; CHECK:      (func $caller (type $anyref_=>_none) (param $any anyref)
  ;; CHECK-NEXT:  (local $x (ref null $A))
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (local.tee $x
  ;; CHECK-NEXT:    (ref.cast $A
  ;; CHECK-NEXT:     (local.get $any)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller (export "out") (param $any anyref)
    (local $x (ref null $A))
    (call $called
      (local.tee $x
        (ref.cast $A ;; this cast will *not* be refined
          (local.get $any)
        )
      )
    )
  )
)

;; As above, but make the cast uninteresting so we do not optimize.
(module
  ;; CHECK:      (type $A (struct (field (mut i32))))
  (type $A (struct (field (mut i32))))

  (type $B (sub $A (struct (field (mut i32)))))

  ;; CHECK:      (type $ref?|$A|_=>_none (func (param (ref null $A))))

  ;; CHECK:      (type $anyref_=>_none (func (param anyref)))

  ;; CHECK:      (export "out" (func $caller))

  ;; CHECK:      (func $called (type $ref?|$A|_=>_none) (param $x (ref null $A))
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $A
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $called (param $x (ref null $A))
    (drop
      (ref.cast $A ;; This cast adds no information ($A to $A).
        (local.get $x)
      )
    )
  )

  ;; CHECK:      (func $caller (type $anyref_=>_none) (param $any anyref)
  ;; CHECK-NEXT:  (local $x (ref null $A))
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (local.tee $x
  ;; CHECK-NEXT:    (ref.cast $A
  ;; CHECK-NEXT:     (local.get $any)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller (export "out") (param $any anyref)
    (local $x (ref null $A))
    (call $called
      (local.tee $x
        (ref.cast $A ;; this cast will *not* be refined
          (local.get $any)
        )
      )
    )
  )
)

;; As above, but two casts in the called function.
(module
  ;; CHECK:      (type $A (struct (field (mut i32))))
  (type $A (struct (field (mut i32))))

  ;; CHECK:      (type $B (sub $A (struct (field (mut i32)))))
  (type $B (sub $A (struct (field (mut i32)))))

  ;; CHECK:      (type $ref?|$A|_=>_none (func (param (ref null $A))))

  ;; CHECK:      (type $C (sub $B (struct (field (mut i32)))))
  (type $C (sub $B (struct (field (mut i32)))))

  ;; CHECK:      (type $anyref_=>_none (func (param anyref)))

  ;; CHECK:      (export "out" (func $caller))

  ;; CHECK:      (func $called (type $ref?|$A|_=>_none) (param $x (ref null $A))
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $C
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $called (param $x (ref null $A))
    ;; Two casts. We keep the first, which is simple to do, and good enough in
    ;; the general case as other optimizations will leave the most-refined one.
    ;; (But in this test, it is less optimal actually.)
    (drop
      (ref.cast $B
        (local.get $x)
      )
    )
    (drop
      (ref.cast $C
        (local.get $x)
      )
    )
  )

  ;; CHECK:      (func $caller (type $anyref_=>_none) (param $any anyref)
  ;; CHECK-NEXT:  (local $x (ref null $A))
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (local.tee $x
  ;; CHECK-NEXT:    (ref.cast $B
  ;; CHECK-NEXT:     (local.get $any)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller (export "out") (param $any anyref)
    (local $x (ref null $A))
    (call $called
      (local.tee $x
        (ref.cast $A ;; this cast will be refined to $B.
          (local.get $any)
        )
      )
    )
  )
)

;; Multiple parameters with control flow between them.
(module
  ;; CHECK:      (type $A (struct (field (mut i32))))
  (type $A (struct (field (mut i32))))

  ;; CHECK:      (type $B (sub $A (struct (field (mut i32)))))
  (type $B (sub $A (struct (field (mut i32)))))

  ;; CHECK:      (type $ref?|$A|_ref?|$A|_ref?|$A|_=>_none (func (param (ref null $A) (ref null $A) (ref null $A))))

  ;; CHECK:      (type $anyref_=>_none (func (param anyref)))

  ;; CHECK:      (export "out" (func $caller))

  ;; CHECK:      (func $called (type $ref?|$A|_ref?|$A|_ref?|$A|_=>_none) (param $x (ref null $A)) (param $y (ref null $A)) (param $z (ref null $A))
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.get $y)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.get $z)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $called (param $x (ref null $A)) (param $y (ref null $A)) (param $z (ref null $A))
    ;; All parameters are cast.
    (drop
      (ref.cast $B
        (local.get $x)
      )
    )
    (drop
      (ref.cast $B
        (local.get $y)
      )
    )
    (drop
      (ref.cast $B
        (local.get $z)
      )
    )
  )

  ;; CHECK:      (func $caller (type $anyref_=>_none) (param $any anyref)
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (block (result (ref $A))
  ;; CHECK-NEXT:    (ref.cast $A
  ;; CHECK-NEXT:     (local.get $any)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (block (result (ref $A))
  ;; CHECK-NEXT:    (if
  ;; CHECK-NEXT:     (i32.const 0)
  ;; CHECK-NEXT:     (return)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (ref.cast $B
  ;; CHECK-NEXT:     (local.get $any)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (block (result (ref $A))
  ;; CHECK-NEXT:    (ref.cast $B
  ;; CHECK-NEXT:     (local.get $any)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller (export "out") (param $any anyref)
    ;; All three params get similar blocks+casts, but the middle one might
    ;; transfer control flow, so we cannot optimize the first parameter (we
    ;; might not reach the call, so we can't assume the call's cast suceeds).
    ;; As a result, only the last two casts are refined to $B, but not the
    ;; first.
    (call $called
      (block (result (ref $A))
        (ref.cast $A
          (local.get $any)
        )
      )
      (block (result (ref $A))
        (if
          (i32.const 0)
          (return)
        )
        (ref.cast $A
          (local.get $any)
        )
      )
      (block (result (ref $A))
        (ref.cast $A
          (local.get $any)
        )
      )
    )
  )
)

;; As above, but without the cast in the middle of the called function.
(module
  ;; CHECK:      (type $A (struct (field (mut i32))))
  (type $A (struct (field (mut i32))))

  ;; CHECK:      (type $B (sub $A (struct (field (mut i32)))))
  (type $B (sub $A (struct (field (mut i32)))))

  ;; CHECK:      (type $ref?|$A|_ref?|$A|_ref?|$A|_=>_none (func (param (ref null $A) (ref null $A) (ref null $A))))

  ;; CHECK:      (type $anyref_=>_none (func (param anyref)))

  ;; CHECK:      (export "out" (func $caller))

  ;; CHECK:      (func $called (type $ref?|$A|_ref?|$A|_ref?|$A|_=>_none) (param $x (ref null $A)) (param $y (ref null $A)) (param $z (ref null $A))
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (local.get $y)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.get $z)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $called (param $x (ref null $A)) (param $y (ref null $A)) (param $z (ref null $A))
    (drop
      (ref.cast $B
        (local.get $x)
      )
    )
    (drop
      ;; This changed to not have a cast.
      (local.get $y)
    )
    (drop
      (ref.cast $B
        (local.get $z)
      )
    )
  )

  ;; CHECK:      (func $caller (type $anyref_=>_none) (param $any anyref)
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (block (result (ref $A))
  ;; CHECK-NEXT:    (ref.cast $A
  ;; CHECK-NEXT:     (local.get $any)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (block (result (ref $A))
  ;; CHECK-NEXT:    (if
  ;; CHECK-NEXT:     (i32.const 0)
  ;; CHECK-NEXT:     (return)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (ref.cast $A
  ;; CHECK-NEXT:     (local.get $any)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (block (result (ref $A))
  ;; CHECK-NEXT:    (ref.cast $B
  ;; CHECK-NEXT:     (local.get $any)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller (export "out") (param $any anyref)
    ;; We can't refine the first cast because of control flow, like before, and
    ;; we can't refine the middle because the called function has no cast, but
    ;; we can still refine the last one.
    (call $called
      (block (result (ref $A))
        (ref.cast $A
          (local.get $any)
        )
      )
      (block (result (ref $A))
        (if
          (i32.const 0)
          (return)
        )
        (ref.cast $A
          (local.get $any)
        )
      )
      (block (result (ref $A))
        (ref.cast $A
          (local.get $any)
        )
      )
    )
  )
)

;; As above, but with a different control flow transfer in the caller, a call.
(module
  ;; CHECK:      (type $A (struct (field (mut i32))))
  (type $A (struct (field (mut i32))))

  ;; CHECK:      (type $B (sub $A (struct (field (mut i32)))))
  (type $B (sub $A (struct (field (mut i32)))))

  ;; CHECK:      (type $none_=>_anyref (func (result anyref)))

  ;; CHECK:      (type $ref?|$A|_ref?|$A|_ref?|$A|_=>_none (func (param (ref null $A) (ref null $A) (ref null $A))))

  ;; CHECK:      (type $anyref_=>_none (func (param anyref)))

  ;; CHECK:      (import "a" "b" (func $get-any (type $none_=>_anyref) (result anyref)))
  (import "a" "b" (func $get-any (result anyref)))

  ;; CHECK:      (export "out" (func $caller))

  ;; CHECK:      (func $called (type $ref?|$A|_ref?|$A|_ref?|$A|_=>_none) (param $x (ref null $A)) (param $y (ref null $A)) (param $z (ref null $A))
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.get $y)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.get $z)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $called (param $x (ref null $A)) (param $y (ref null $A)) (param $z (ref null $A))
    ;; All parameters are cast.
    (drop
      (ref.cast $B
        (local.get $x)
      )
    )
    (drop
      (ref.cast $B
        (local.get $y)
      )
    )
    (drop
      (ref.cast $B
        (local.get $z)
      )
    )
  )

  ;; CHECK:      (func $caller (type $anyref_=>_none) (param $any anyref)
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (ref.cast $A
  ;; CHECK-NEXT:    (local.get $any)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (call $get-any)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.get $any)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller (export "out") (param $any anyref)
    (call $called
      (ref.cast $A
        (local.get $any)
      )
      (ref.cast $A
        ;; This call might transfer control flow (if it throws), so we
        ;; can't optimize before it, but the last two casts will become $B.
        (call $get-any)
      )
      (ref.cast $A
        (local.get $any)
      )
    )
  )
)

;; As above, but with yet another control flow transfer, using an if.
(module
  ;; CHECK:      (type $A (struct (field (mut i32))))
  (type $A (struct (field (mut i32))))

  ;; CHECK:      (type $B (sub $A (struct (field (mut i32)))))
  (type $B (sub $A (struct (field (mut i32)))))

  ;; CHECK:      (type $none_=>_anyref (func (result anyref)))

  ;; CHECK:      (type $ref?|$A|_ref?|$A|_ref?|$A|_=>_none (func (param (ref null $A) (ref null $A) (ref null $A))))

  ;; CHECK:      (type $anyref_=>_none (func (param anyref)))

  ;; CHECK:      (import "a" "b" (func $get-any (type $none_=>_anyref) (result anyref)))
  (import "a" "b" (func $get-any (result anyref)))

  ;; CHECK:      (export "out" (func $caller))

  ;; CHECK:      (func $called (type $ref?|$A|_ref?|$A|_ref?|$A|_=>_none) (param $x (ref null $A)) (param $y (ref null $A)) (param $z (ref null $A))
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.get $y)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.get $z)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $called (param $x (ref null $A)) (param $y (ref null $A)) (param $z (ref null $A))
    ;; All parameters are cast.
    (drop
      (ref.cast $B
        (local.get $x)
      )
    )
    (drop
      (ref.cast $B
        (local.get $y)
      )
    )
    (drop
      (ref.cast $B
        (local.get $z)
      )
    )
  )

  ;; CHECK:      (func $caller (type $anyref_=>_none) (param $any anyref)
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (ref.cast $A
  ;; CHECK-NEXT:    (local.get $any)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (if (result (ref $A))
  ;; CHECK-NEXT:    (i32.const 0)
  ;; CHECK-NEXT:    (return)
  ;; CHECK-NEXT:    (ref.cast $A
  ;; CHECK-NEXT:     (local.get $any)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.get $any)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller (export "out") (param $any anyref)
    (call $called
      (ref.cast $A
        (local.get $any)
      )
      ;; This if arm transfers control flow, so while we have a fallthrough
      ;; value we cannot optimize it (in fact, it might never be reached, at
      ;; least if the constant were not 0). As a result we'll optimize only the
      ;; very last cast.
      (if (result (ref $A))
        (i32.const 0)
        (return)
        (ref.cast $A
          (local.get $any)
        )
      )
      (ref.cast $A
        (local.get $any)
      )
    )
  )
)

;; A cast that will fail.
(module
  ;; CHECK:      (type $A (struct (field (mut i32))))
  (type $A (struct (field (mut i32))))

  (type $B (sub $A (struct (field (mut i32)))))

  ;; CHECK:      (type $ref?|$A|_=>_none (func (param (ref null $A))))

  ;; CHECK:      (type $none_=>_none (func))

  ;; CHECK:      (export "out" (func $caller))

  ;; CHECK:      (func $called (type $ref?|$A|_=>_none) (param $x (ref null $A))
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (unreachable)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $called (param $x (ref null $A))
    (drop
      (ref.cast $B
        (local.get $x)
      )
    )
  )

  ;; CHECK:      (func $caller (type $none_=>_none)
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (unreachable)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller (export "out")
    (call $called
      ;; The called function will cast to $B, but this is an $A, so the cast
      ;; will fail. We can infer that the code here is unreachable. Note that no
      ;; ref.cast appears here - we infer this even without seeing a cast.
      (struct.new $A
        (i32.const 10)
      )
    )
  )
)

;; Test that we refine using ref.as_non_null and not just ref.cast.
(module
  ;; CHECK:      (type $A (struct (field (mut i32))))
  (type $A (struct (field (mut i32))))

  ;; CHECK:      (type $ref?|$A|_=>_none (func (param (ref null $A))))

  ;; CHECK:      (type $anyref_=>_none (func (param anyref)))

  ;; CHECK:      (export "out" (func $caller))

  ;; CHECK:      (func $called (type $ref?|$A|_=>_none) (param $x (ref null $A))
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.as_non_null
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $called (param $x (ref null $A))
    (drop
      (ref.as_non_null
        (local.get $x)
      )
    )
  )

  ;; CHECK:      (func $caller (type $anyref_=>_none) (param $any anyref)
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (ref.cast $A
  ;; CHECK-NEXT:    (local.get $any)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller (export "out") (param $any anyref)
    (call $called
      ;; This cast can become non-nullable.
      (ref.cast null $A
        (local.get $any)
      )
    )
  )
)

;; Verify we do not propagate *less*-refined information.
(module
  ;; CHECK:      (type $A (struct (field (mut i32))))
  (type $A (struct (field (mut i32))))

  ;; CHECK:      (type $B (sub $A (struct (field (mut i32)))))
  (type $B (sub $A (struct (field (mut i32)))))

  ;; CHECK:      (type $C (sub $B (struct (field (mut i32)))))
  (type $C (sub $B (struct (field (mut i32)))))

  ;; CHECK:      (type $anyref_=>_none (func (param anyref)))

  ;; CHECK:      (type $ref?|$A|_=>_none (func (param (ref null $A))))

  ;; CHECK:      (type $none_=>_none (func))

  ;; CHECK:      (export "caller-C" (func $caller-C))

  ;; CHECK:      (export "caller-B" (func $caller-B))

  ;; CHECK:      (export "caller-A" (func $caller-A))

  ;; CHECK:      (func $called (type $ref?|$A|_=>_none) (param $x (ref null $A))
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $B
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $called (param $x (ref null $A))
    ;; This function casts the A to a B.
    (drop
      (ref.cast $B
        (local.get $x)
      )
    )
  )

  ;; CHECK:      (func $maker (type $none_=>_none)
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.new $A
  ;; CHECK-NEXT:    (i32.const 10)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.new $B
  ;; CHECK-NEXT:    (i32.const 20)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.new $C
  ;; CHECK-NEXT:    (i32.const 30)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $maker
    ;; A always contains 10, and B 20, and C 30.
    (drop
      (struct.new $A
        (i32.const 10)
      )
    )
    (drop
      (struct.new $B
        (i32.const 20)
      )
    )
    (drop
      (struct.new $C
        (i32.const 30)
      )
    )
  )

  ;; CHECK:      (func $caller-C (type $anyref_=>_none) (param $any anyref)
  ;; CHECK-NEXT:  (local $temp-C (ref $C))
  ;; CHECK-NEXT:  (local $temp-any anyref)
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (local.tee $temp-C
  ;; CHECK-NEXT:    (ref.cast $C
  ;; CHECK-NEXT:     (local.tee $temp-any
  ;; CHECK-NEXT:      (local.get $any)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (i32.const 30)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.get $B 0
  ;; CHECK-NEXT:    (ref.cast $B
  ;; CHECK-NEXT:     (local.get $temp-any)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.get $A 0
  ;; CHECK-NEXT:    (ref.cast $A
  ;; CHECK-NEXT:     (local.get $any)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller-C (export "caller-C") (param $any anyref)
    (local $temp-C (ref $C))
    (local $temp-any anyref)
    (call $called
      (local.tee $temp-C
        (ref.cast $C ;; This cast is already more refined than even the called
                     ;; function casts to. It should stay as it is.
          (local.tee $temp-any
            (local.get $any)
          )
        )
      )
    )
    (drop
      (struct.get $A 0 ;; the reference contains a C, so this value is 30.
        (ref.cast $A
          (local.get $temp-C)
        )
      )
    )
    (drop
      (struct.get $A 0 ;; We infer that $temp-any is $B from the cast in the
                       ;; call, so the cast here can be improved, but not the
                       ;; value (which can be 20 or 30).
                       ;; TODO: We can infer from the ref.cast $C in this
                       ;;       function backwards into the tee and its value.
        (ref.cast $A
          (local.get $temp-any)
        )
      )
    )
    (drop
      (struct.get $A 0 ;; We have not inferred anything about the param, so this
                       ;; is not optimized yet, but it could be. TODO
        (ref.cast $A
          (local.get $any)
        )
      )
    )
  )

  ;; CHECK:      (func $caller-B (type $anyref_=>_none) (param $any anyref)
  ;; CHECK-NEXT:  (local $temp (ref $A))
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (local.tee $temp
  ;; CHECK-NEXT:    (ref.cast $B
  ;; CHECK-NEXT:     (local.get $any)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.get $A 0
  ;; CHECK-NEXT:    (local.get $temp)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller-B (export "caller-B") (param $any anyref)
    (local $temp (ref $A))
    (call $called
      (local.tee $temp
        (ref.cast $B ;; This cast is equal to the called cast. It should remain.
          (local.get $any)
        )
      )
    )
    (drop
      (struct.get $A 0 ;; Nothing can be inferred here.
        (local.get $temp)
      )
    )
  )

  ;; CHECK:      (func $caller-A (type $anyref_=>_none) (param $any anyref)
  ;; CHECK-NEXT:  (local $temp (ref $A))
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (local.tee $temp
  ;; CHECK-NEXT:    (ref.cast $B
  ;; CHECK-NEXT:     (local.get $any)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (struct.get $A 0
  ;; CHECK-NEXT:    (local.get $temp)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller-A (export "caller-A") (param $any anyref)
    (local $temp (ref $A))
    (call $called
      (local.tee $temp
        (ref.cast $A ;; This cast is less refined, and can be improved to B.
          (local.get $any)
        )
      )
    )
    (drop
      (struct.get $A 0 ;; Nothing can be inferred here.
        (local.get $temp)
      )
    )
  )
)

;; Refine a type to unreachable. B1 and B2 are sibling subtypes of A, and the
;; caller passes in a B1 that is cast in the function to B2.
(module
  ;; CHECK:      (type $A (struct (field (mut i32))))
  (type $A (struct (field (mut i32))))

  (rec
    ;; CHECK:      (type $ref?|$A|_=>_none (func (param (ref null $A))))

    ;; CHECK:      (rec
    ;; CHECK-NEXT:  (type $B1 (sub $A (struct (field (mut i32)))))
    (type $B1 (sub $A (struct (field (mut i32)))))

    ;; CHECK:       (type $B2 (sub $A (struct (field (mut i32)))))
    (type $B2 (sub $A (struct (field (mut i32)))))
  )

  ;; CHECK:      (type $anyref_=>_none (func (param anyref)))

  ;; CHECK:      (export "caller" (func $caller))

  ;; CHECK:      (func $called (type $ref?|$A|_=>_none) (param $x (ref null $A))
  ;; CHECK-NEXT:  (drop
  ;; CHECK-NEXT:   (ref.cast $B1
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $called (param $x (ref null $A))
    (drop
      (ref.cast $B1
        (local.get $x)
      )
    )
  )

  ;; CHECK:      (func $caller (type $anyref_=>_none) (param $any anyref)
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (unreachable)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (struct.new $B1
  ;; CHECK-NEXT:    (i32.const 20)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (call $called
  ;; CHECK-NEXT:   (unreachable)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $caller (export "caller") (param $any anyref)
    ;; The cast of this A to B1 will fail, so it is unreachable.
    (call $called
      (struct.new $A
        (i32.const 10)
      )
    )
    ;; This cast will succeed, so nothing changes.
    (call $called
      (struct.new $B1
        (i32.const 20)
      )
    )
    ;; Casting B2 to B1 will fail.
    (call $called
      (struct.new $B2
        (i32.const 30)
      )
    )
  )
)
