;; When string builtins are enabled, we verify string constant imports are utf8.

;; RUN: wasm-opt %s -all --disable-string-builtins 2>&1 | filecheck %s --check-prefix NO-SB
;; RUN: not wasm-opt %s -all -S -o - | filecheck %s --check-prefix YESSB

;; YESSB: validation errar

(module
  (type $array16 (array (mut i16)))

  (import "\'" "foo" (global $foo (ref extern)))
)
