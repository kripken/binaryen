;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.

;; Test that new-style nominal types are parsed correctly.
;; TODO: Remove --nominal below once nominal types are parsed as nominal by default.

;; RUN: foreach %s %t wasm-opt --nominal -all -S -o - | filecheck %s
;; RUN: foreach %s %t wasm-opt --nominal -all --roundtrip -S -o - | filecheck %s

;; void function type
(module
  (type $sub (func) (extends $super))

  ;; CHECK:      (type $super (func))
  (type $super (func))

  ;; CHECK:      (global $g (ref null $super) (ref.null nofunc))
  (global $g (ref null $super) (ref.null $sub))
)

;; function type with params and results
(module
  (type $sub (func (param i32) (result i32)) (extends $super))

  ;; CHECK:      (type $super (func (param i32) (result i32)))
  (type $super (func (param i32) (result i32)))

  ;; CHECK:      (global $g (ref null $super) (ref.null nofunc))
  (global $g (ref null $super) (ref.null $sub))
)

;; empty struct type
(module
  (type $sub (struct) (extends $super))

  ;; CHECK:      (type $super (struct ))
  (type $super (struct))

  ;; CHECK:      (global $g (ref null $super) (ref.null none))
  (global $g (ref null $super) (ref.null $sub))
)

;; struct type with fields
(module
  (type $sub (struct i32 (field i64)) (extends $super))

  ;; CHECK:      (type $super (struct (field i32) (field i64)))
  (type $super (struct (field i32) i64))

  ;; CHECK:      (global $g (ref null $super) (ref.null none))
  (global $g (ref null $super) (ref.null $sub))
)

;; array type
(module
  (type $sub (array i8) (extends $super))

  ;; CHECK:      (type $super (array i8))
  (type $super (array i8))

  ;; CHECK:      (global $g (ref null $super) (ref.null none))
  (global $g (ref null $super) (ref.null $sub))
)
