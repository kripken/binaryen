;; When string builtins are enabled, we verify string constant imports are utf8.

;; RUN: wasm-opt %s -all --disable-string-builtins 2>&1
;; RUN: not wasm-opt %s -all -S -o - | filecheck %s --check-prefix YESSB

;; YESSB: validation errar

(module
  (import "\'" "unpaired high surrogate \ED\A0\80 " (global $bad (ref extern)))
)
