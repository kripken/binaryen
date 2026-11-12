;; RUN: wasm-opt %s --inlining -o %t.wasm -osm %t.map -g -q
;; RUN: cat %t.map | filecheck %s --check-prefix MAP
;; RUN: wasm-opt %t.wasm -ism %t.map -o %t2.wasm -osm %t2.map -g -q
;; RUN: cat %t2.map | filecheck %s --check-prefix MAP
;; RUN: wasm-opt %t2.wasm -ism %t2.map -S -o - | filecheck %s --check-prefix WAT

;; Verify that inlining a call with debug locations emits "scopes" and "ranges"
;; (including the 'I' CallSite record at caller.cpp:10:5 -> I, src=1 ('B'),
;; 0-based line=9 ('J'), col=5 ('F') => "IBJF"), and preserves them across a
;; binary + source map roundtrip.

;; MAP: "sources":["callee.cpp","caller.cpp"]
;; MAP-SAME: "scopes":[null,null]
;; MAP-SAME: "ranges":["EEjB,EAA,IBJF,FB,FA"]

;; WAT:      (export "caller" (func $caller))

;; WAT:      (func $caller
;; WAT-NEXT:  ;;@ callee.cpp:2:3:CalleeFunc
;; WAT-NEXT:  (nop)
;; WAT-NEXT: )

(module
  (export "caller" (func $caller))

  (func $callee
    ;;@ callee.cpp:2:3:CalleeFunc
    (nop)
  )

  (func $caller
    ;;@ caller.cpp:10:5:CallerFunc
    (call $callee)
  )
)
