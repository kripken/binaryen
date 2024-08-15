;; Test that an unused part of a rec group can still fail validation (from a
;; global).

;; RUN:  not wasm-opt %s -all --disable-gc 2>&1 | filecheck %s

;; CHECK: all used features in rec groups should be allowed

(module
 (rec
  (type $func (func))
  (type $unused (sub (struct (field v128))))
 )
 (global $global (ref null $func) (ref.null $func))
)

