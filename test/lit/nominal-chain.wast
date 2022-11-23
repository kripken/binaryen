;; NOTE: Assertions have been generated by update_lit_checks.py and should not be edited.
;; RUN: wasm-opt %s -all --nominal -S -o - | filecheck %s
;; RUN: wasm-opt %s -all --nominal --roundtrip -S -o - | filecheck %s

;; Check that intermediate types in subtype chains are also included in the
;; output module, even if there are no other references to those intermediate
;; types.

(module
  ;; CHECK:      (type $root (struct ))

  ;; CHECK:      (type $trunk (struct_subtype (field i32) $root))

  ;; CHECK:      (type $branch (struct_subtype (field i32) (field i64) $trunk))

  ;; CHECK:      (type $twig (struct_subtype (field i32) (field i64) (field f32) $branch))

  ;; CHECK:      (type $leaf (struct_subtype (field i32) (field i64) (field f32) (field f64) $twig))
  (type $leaf (struct_subtype i32 i64 f32 f64 $twig))

  (type $twig (struct_subtype i32 i64 f32 $branch))

  (type $branch (struct_subtype i32 i64 $trunk))

  (type $trunk (struct_subtype i32 $root))

  (type $root (struct))

  ;; CHECK:      (func $make-root (type $ref|$leaf|_=>_ref?|$root|) (param $leaf (ref $leaf)) (result (ref null $root))
  ;; CHECK-NEXT:  (local.get $leaf)
  ;; CHECK-NEXT: )
  (func $make-root (param $leaf (ref $leaf)) (result (ref null $root))
    (local.get $leaf)
  )
)
