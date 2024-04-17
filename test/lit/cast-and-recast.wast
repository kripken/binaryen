;; NOTE: Assertions have been generated by update_lit_checks.py and should not be edited.

;; Test that our hack for br_if output types does not cause the binary to grow
;; linearly with each roundtrip. When we emit a br_if whose output type is not
;; refined enough (Binaryen IR uses the value's type; wasm uses the target's)
;; then we add a cast. We then remove trivial casts like it during load, when we
;; see they are unneeded, and as a result we should see no ref.cast anywhere in
;; the CHECKs.
;;
;; This is also used as the input in test/lit/binary/cast-and-recast.test, which
;; verifies the binary format itself.

;; RUN: wasm-opt %s -all --generate-stack-ir --optimize-stack-ir --roundtrip --roundtrip --roundtrip -S -o - | filecheck %s

(module
  (rec
    ;; CHECK:      (rec
    ;; CHECK-NEXT:  (type $A (sub (struct )))
    (type $A (sub (struct)))
    ;; CHECK:       (type $B (sub $A (struct )))
    (type $B (sub $A (struct)))
  )

  ;; CHECK:      (func $test (type $2) (param $B (ref $B)) (param $x i32) (result anyref)
  ;; CHECK-NEXT:  (block $label$1 (result (ref $A))
  ;; CHECK-NEXT:   (br_if $label$1
  ;; CHECK-NEXT:    (local.get $B)
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $test (param $B (ref $B)) (param $x i32) (result anyref)
    (block $out (result (ref $A))
      ;; The br_if's value is of type $B which is more precise than the block's
      ;; type, $A, so we emit a cast here, but we remove it when we read the
      ;; binary.
      (br_if $out
        (local.get $B)
        (local.get $x)
      )
    )
  )

  ;; CHECK:      (func $test-local (type $2) (param $B (ref $B)) (param $x i32) (result anyref)
  ;; CHECK-NEXT:  (local $temp (ref $B))
  ;; CHECK-NEXT:  (block $label$1 (result (ref $A))
  ;; CHECK-NEXT:   (local.set $temp
  ;; CHECK-NEXT:    (br_if $label$1
  ;; CHECK-NEXT:     (local.get $B)
  ;; CHECK-NEXT:     (local.get $x)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (unreachable)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $test-local (param $B (ref $B)) (param $x i32) (result anyref)
    (local $temp (ref $B))
    ;; As above, but with local.set that receives the br_if's value, verifying
    ;; it is refined. We emit a cast here.
    (block $out (result (ref $A))
      (local.set $temp
        (br_if $out
          (local.get $B)
          (local.get $x)
        )
      )
      (unreachable)
    )
  )

  ;; CHECK:      (func $test-drop (type $2) (param $B (ref $B)) (param $x i32) (result anyref)
  ;; CHECK-NEXT:  (block $label$1 (result (ref $A))
  ;; CHECK-NEXT:   (drop
  ;; CHECK-NEXT:    (br_if $label$1
  ;; CHECK-NEXT:     (local.get $B)
  ;; CHECK-NEXT:     (local.get $x)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (unreachable)
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $test-drop (param $B (ref $B)) (param $x i32) (result anyref)
    ;; As above, but with a drop of the br_if value. We do not emit a cast here.
    ;; That cannot be observed in this test (as if a cast were added, the binary
    ;; reader would remove it), but keep it here for completeness, and because
    ;; this file serves as the input to test/lit/binary/cast-and-recast.test.
    (block $out (result (ref $A))
      (drop
        (br_if $out
          (local.get $B)
          (local.get $x)
        )
      )
      (unreachable)
    )
  )

  ;; CHECK:      (func $test-same (type $3) (param $A (ref $A)) (param $x i32) (result anyref)
  ;; CHECK-NEXT:  (block $label$1 (result (ref $A))
  ;; CHECK-NEXT:   (br_if $label$1
  ;; CHECK-NEXT:    (local.get $A)
  ;; CHECK-NEXT:    (local.get $x)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  (func $test-same (param $A (ref $A)) (param $x i32) (result anyref)
    ;; As above, but now we use $A everywhere, which means there is no
    ;; difference between the type in Binaryen IR and wasm, so we do not need
    ;; to emit any extra cast here.
    (block $out (result (ref $A))
      (br_if $out
        (local.get $A)
        (local.get $x)
      )
    )
  )
)
