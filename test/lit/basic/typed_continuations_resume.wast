;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.

;; RUN: wasm-opt %s -all -o %t.text.wast -g -S
;; RUN: wasm-as %s -all -g -o %t.wasm
;; RUN: wasm-dis %t.wasm -all -o %t.bin.wast
;; RUN: wasm-as %s -all -o %t.nodebug.wasm
;; RUN: wasm-dis %t.nodebug.wasm -all -o %t.bin.nodebug.wast
;; RUN: cat %t.text.wast | filecheck %s --check-prefix=CHECK-TEXT
;; RUN: cat %t.bin.wast | filecheck %s --check-prefix=CHECK-BIN
;; RUN: cat %t.bin.nodebug.wast | filecheck %s --check-prefix=CHECK-BIN-NODEBUG

(module
 ;; CHECK-BINARY:      (type $ft (func (param i32) (result i32)))
 ;; CHECK-TEXT:      (type $ft (func (param i32) (result i32)))
 ;; CHECK-BIN:      (type $ft (func (param i32) (result i32)))
 (type $ft (func (param i32) (result i32)))
 ;; CHECK-BINARY:      (type $ct (cont $ft))
 ;; CHECK-TEXT:      (type $ct (cont $ft))
 ;; CHECK-BIN:      (type $ct (cont $ft))
 (type $ct (cont $ft))
 ;; CHECK-BINARY:      (type $2 (func (result i32)))

 ;; CHECK-BINARY:      (type $3 (func (param (ref $ct)) (result i32)))

 ;; CHECK-BINARY:      (tag $t (result i32))
 ;; CHECK-TEXT:      (type $2 (func (result i32 (ref $ct))))

 ;; CHECK-TEXT:      (type $3 (func (param (ref $ct)) (result i32)))

 ;; CHECK-TEXT:      (tag $t (param i32) (result i32))
 ;; CHECK-BIN:      (type $2 (func (result i32 (ref $ct))))

 ;; CHECK-BIN:      (type $3 (func (param (ref $ct)) (result i32)))

 ;; CHECK-BIN:      (tag $t (param i32) (result i32))
 (tag $t (param i32) (result i32))

 ;; CHECK-BINARY:      (func $go (type $3) (param $x (ref $ct)) (result i32)
 ;; CHECK-BINARY-NEXT:  (drop
 ;; CHECK-BINARY-NEXT:   (block $label$1 (result (ref $ct))
 ;; CHECK-BINARY-NEXT:    (return
 ;; CHECK-BINARY-NEXT:     (resume $ct (tag $t $label$1)
 ;; CHECK-BINARY-NEXT:      (i32.const 123)
 ;; CHECK-BINARY-NEXT:      (local.get $x)
 ;; CHECK-BINARY-NEXT:     )
 ;; CHECK-BINARY-NEXT:    )
 ;; CHECK-BINARY-NEXT:   )
 ;; CHECK-BINARY-NEXT:  )
 ;; CHECK-BINARY-NEXT:  (i32.const 123)
 ;; CHECK-BINARY-NEXT: )
 ;; CHECK-TEXT:      (func $go (type $3) (param $x (ref $ct)) (result i32)
 ;; CHECK-TEXT-NEXT:  (tuple.extract 2 0
 ;; CHECK-TEXT-NEXT:   (block $handler (type $2) (result i32 (ref $ct))
 ;; CHECK-TEXT-NEXT:    (return
 ;; CHECK-TEXT-NEXT:     (resume $ct (tag $t $handler)
 ;; CHECK-TEXT-NEXT:      (i32.const 123)
 ;; CHECK-TEXT-NEXT:      (local.get $x)
 ;; CHECK-TEXT-NEXT:     )
 ;; CHECK-TEXT-NEXT:    )
 ;; CHECK-TEXT-NEXT:   )
 ;; CHECK-TEXT-NEXT:  )
 ;; CHECK-TEXT-NEXT: )
 ;; CHECK-BIN:      (func $go (type $3) (param $x (ref $ct)) (result i32)
 ;; CHECK-BIN-NEXT:  (local $1 (tuple i32 (ref $ct)))
 ;; CHECK-BIN-NEXT:  (local $2 i32)
 ;; CHECK-BIN-NEXT:  (local.set $1
 ;; CHECK-BIN-NEXT:   (block $label$1 (type $2) (result i32 (ref $ct))
 ;; CHECK-BIN-NEXT:    (return
 ;; CHECK-BIN-NEXT:     (resume $ct (tag $t $label$1)
 ;; CHECK-BIN-NEXT:      (i32.const 123)
 ;; CHECK-BIN-NEXT:      (local.get $x)
 ;; CHECK-BIN-NEXT:     )
 ;; CHECK-BIN-NEXT:    )
 ;; CHECK-BIN-NEXT:   )
 ;; CHECK-BIN-NEXT:  )
 ;; CHECK-BIN-NEXT:  (block (result i32)
 ;; CHECK-BIN-NEXT:   (local.set $2
 ;; CHECK-BIN-NEXT:    (tuple.extract 2 0
 ;; CHECK-BIN-NEXT:     (local.get $1)
 ;; CHECK-BIN-NEXT:    )
 ;; CHECK-BIN-NEXT:   )
 ;; CHECK-BIN-NEXT:   (drop
 ;; CHECK-BIN-NEXT:    (tuple.extract 2 1
 ;; CHECK-BIN-NEXT:     (local.get $1)
 ;; CHECK-BIN-NEXT:    )
 ;; CHECK-BIN-NEXT:   )
 ;; CHECK-BIN-NEXT:   (local.get $2)
 ;; CHECK-BIN-NEXT:  )
 ;; CHECK-BIN-NEXT: )
 (func $go (param $x (ref $ct)) (result i32)
  (tuple.extract 2 0
   (block $handler (result i32 (ref $ct))
    (return
     (resume $ct
      (tag $t $handler)
      (i32.const 123)
      (local.get $x)
     )
    )
   )
  )
 )
)
;; CHECK-NODEBUG:      (type $0 (func (param i32) (result i32)))

;; CHECK-NODEBUG:      (type $1 (cont $0))

;; CHECK-NODEBUG:      (type $2 (func (result i32)))

;; CHECK-NODEBUG:      (type $3 (func (param (ref $1)) (result i32)))

;; CHECK-NODEBUG:      (tag $tag$0 (result i32))

;; CHECK-NODEBUG:      (func $0 (type $3) (param $0 (ref $1)) (result i32)
;; CHECK-NODEBUG-NEXT:  (drop
;; CHECK-NODEBUG-NEXT:   (block $label$1 (result (ref $1))
;; CHECK-NODEBUG-NEXT:    (return
;; CHECK-NODEBUG-NEXT:     (resume $1 (tag $tag$0 $label$1)
;; CHECK-NODEBUG-NEXT:      (i32.const 123)
;; CHECK-NODEBUG-NEXT:      (local.get $0)
;; CHECK-NODEBUG-NEXT:     )
;; CHECK-NODEBUG-NEXT:    )
;; CHECK-NODEBUG-NEXT:   )
;; CHECK-NODEBUG-NEXT:  )
;; CHECK-NODEBUG-NEXT:  (i32.const 123)
;; CHECK-NODEBUG-NEXT: )
;; CHECK-BIN-NODEBUG:      (type $0 (func (param i32) (result i32)))

;; CHECK-BIN-NODEBUG:      (type $1 (cont $0))

;; CHECK-BIN-NODEBUG:      (type $2 (func (result i32 (ref $1))))

;; CHECK-BIN-NODEBUG:      (type $3 (func (param (ref $1)) (result i32)))

;; CHECK-BIN-NODEBUG:      (tag $tag$0 (param i32) (result i32))

;; CHECK-BIN-NODEBUG:      (func $0 (type $3) (param $0 (ref $1)) (result i32)
;; CHECK-BIN-NODEBUG-NEXT:  (local $1 (tuple i32 (ref $1)))
;; CHECK-BIN-NODEBUG-NEXT:  (local $2 i32)
;; CHECK-BIN-NODEBUG-NEXT:  (local.set $1
;; CHECK-BIN-NODEBUG-NEXT:   (block $label$1 (type $2) (result i32 (ref $1))
;; CHECK-BIN-NODEBUG-NEXT:    (return
;; CHECK-BIN-NODEBUG-NEXT:     (resume $1 (tag $tag$0 $label$1)
;; CHECK-BIN-NODEBUG-NEXT:      (i32.const 123)
;; CHECK-BIN-NODEBUG-NEXT:      (local.get $0)
;; CHECK-BIN-NODEBUG-NEXT:     )
;; CHECK-BIN-NODEBUG-NEXT:    )
;; CHECK-BIN-NODEBUG-NEXT:   )
;; CHECK-BIN-NODEBUG-NEXT:  )
;; CHECK-BIN-NODEBUG-NEXT:  (block (result i32)
;; CHECK-BIN-NODEBUG-NEXT:   (local.set $2
;; CHECK-BIN-NODEBUG-NEXT:    (tuple.extract 2 0
;; CHECK-BIN-NODEBUG-NEXT:     (local.get $1)
;; CHECK-BIN-NODEBUG-NEXT:    )
;; CHECK-BIN-NODEBUG-NEXT:   )
;; CHECK-BIN-NODEBUG-NEXT:   (drop
;; CHECK-BIN-NODEBUG-NEXT:    (tuple.extract 2 1
;; CHECK-BIN-NODEBUG-NEXT:     (local.get $1)
;; CHECK-BIN-NODEBUG-NEXT:    )
;; CHECK-BIN-NODEBUG-NEXT:   )
;; CHECK-BIN-NODEBUG-NEXT:   (local.get $2)
;; CHECK-BIN-NODEBUG-NEXT:  )
;; CHECK-BIN-NODEBUG-NEXT: )
