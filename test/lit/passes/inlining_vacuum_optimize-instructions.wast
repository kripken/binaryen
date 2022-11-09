;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.

;; RUN: wasm-opt %s --inlining --vacuum --optimize-instructions -all -S -o - | filecheck %s

;; Check for a specific bug involving inlining first turning a struct.get's
;; input into a null, then vacuum gets rid of intervening blocks, and then
;; optimize-instructions runs into the following situation: first it turns the
;; struct.get of a null into an unreachable, then it makes a note to itself to
;; refinalize at the end, but before the end it tries to optimize the ref.cast.
;; The ref.cast's type has not been updated to unreachable yet, but its ref has,
;; which is temporarily inconsistent. We must be careful to avoid confusion
;; there.
(module
 (type $A (struct_subtype (field (ref null $B)) data))
 (type $B (struct_subtype  data))
 (func $target (param $0 (ref null $A))
  (drop
   (ref.cast_static $B
    (struct.get $A 0
     (call $get-null)
    )
   )
  )
  (unreachable)
 )
 (func $get-null (result (ref null $A))
  (ref.null none)
 )
)

