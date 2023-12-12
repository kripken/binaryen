;; NOTE: Assertions have been generated by update_lit_checks.py and should not be edited.

;; RUN: wasm-opt %s -all -S -o - | filecheck %s
;; RUN: wasm-opt %s -all --roundtrip -g -S -o - | filecheck %s --check-prefix=RTRIP

(module
 (rec
  ;; CHECK:      (rec
  ;; CHECK-NEXT:  (type $f1 (func (result (ref $f1) (ref $f2))))
  ;; RTRIP:      (rec
  ;; RTRIP-NEXT:  (type $f1 (func (result (ref $f1) (ref $f2))))
  (type $f1 (func (result (ref $f1) (ref $f2))))
  ;; CHECK:       (type $f2 (func (result (ref $f2) (ref $f1))))
  ;; RTRIP:       (type $f2 (func (result (ref $f2) (ref $f1))))
  (type $f2 (func (result (ref $f2) (ref $f1))))
 )

 ;; These types will be optimized out.
 (type $block1 (func (result (ref $f1) (ref $f2))))
 (type $block2 (func (result (ref $f2) (ref $f1))))

 ;; CHECK:      (func $f1 (type $f1) (result (ref $f1) (ref $f2))
 ;; CHECK-NEXT:  (loop $l (type $f1) (result (ref $f1) (ref $f2))
 ;; CHECK-NEXT:   (call $f1)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 ;; RTRIP:      (func $f1 (type $f1) (result (ref $f1) (ref $f2))
 ;; RTRIP-NEXT:  (local $0 ((ref $f1) (ref $f2)))
 ;; RTRIP-NEXT:  (local $1 ((ref $f1) (ref $f2)))
 ;; RTRIP-NEXT:  (local.set $1
 ;; RTRIP-NEXT:   (loop $label$1 (type $f1) (result (ref $f1) (ref $f2))
 ;; RTRIP-NEXT:    (local.set $0
 ;; RTRIP-NEXT:     (call $f1)
 ;; RTRIP-NEXT:    )
 ;; RTRIP-NEXT:    (tuple.make 2
 ;; RTRIP-NEXT:     (tuple.extract 0
 ;; RTRIP-NEXT:      (local.get $0)
 ;; RTRIP-NEXT:     )
 ;; RTRIP-NEXT:     (tuple.extract 1
 ;; RTRIP-NEXT:      (local.get $0)
 ;; RTRIP-NEXT:     )
 ;; RTRIP-NEXT:    )
 ;; RTRIP-NEXT:   )
 ;; RTRIP-NEXT:  )
 ;; RTRIP-NEXT:  (tuple.make 2
 ;; RTRIP-NEXT:   (tuple.extract 0
 ;; RTRIP-NEXT:    (local.get $1)
 ;; RTRIP-NEXT:   )
 ;; RTRIP-NEXT:   (tuple.extract 1
 ;; RTRIP-NEXT:    (local.get $1)
 ;; RTRIP-NEXT:   )
 ;; RTRIP-NEXT:  )
 ;; RTRIP-NEXT: )
 (func $f1 (type $f1) (result (ref $f1) (ref $f2))
  ;; This block will be emitted with type $f1
  (loop $l (type $block1) (result (ref $f1) (ref $f2))
   (call $f1)
  )
 )

 ;; CHECK:      (func $f2 (type $f2) (result (ref $f2) (ref $f1))
 ;; CHECK-NEXT:  (loop $l (type $f2) (result (ref $f2) (ref $f1))
 ;; CHECK-NEXT:   (call $f2)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 ;; RTRIP:      (func $f2 (type $f2) (result (ref $f2) (ref $f1))
 ;; RTRIP-NEXT:  (local $0 ((ref $f2) (ref $f1)))
 ;; RTRIP-NEXT:  (local $1 ((ref $f2) (ref $f1)))
 ;; RTRIP-NEXT:  (local.set $1
 ;; RTRIP-NEXT:   (loop $label$1 (type $f2) (result (ref $f2) (ref $f1))
 ;; RTRIP-NEXT:    (local.set $0
 ;; RTRIP-NEXT:     (call $f2)
 ;; RTRIP-NEXT:    )
 ;; RTRIP-NEXT:    (tuple.make 2
 ;; RTRIP-NEXT:     (tuple.extract 0
 ;; RTRIP-NEXT:      (local.get $0)
 ;; RTRIP-NEXT:     )
 ;; RTRIP-NEXT:     (tuple.extract 1
 ;; RTRIP-NEXT:      (local.get $0)
 ;; RTRIP-NEXT:     )
 ;; RTRIP-NEXT:    )
 ;; RTRIP-NEXT:   )
 ;; RTRIP-NEXT:  )
 ;; RTRIP-NEXT:  (tuple.make 2
 ;; RTRIP-NEXT:   (tuple.extract 0
 ;; RTRIP-NEXT:    (local.get $1)
 ;; RTRIP-NEXT:   )
 ;; RTRIP-NEXT:   (tuple.extract 1
 ;; RTRIP-NEXT:    (local.get $1)
 ;; RTRIP-NEXT:   )
 ;; RTRIP-NEXT:  )
 ;; RTRIP-NEXT: )
 (func $f2 (type $f2) (result (ref $f2) (ref $f1))
  ;; This block will be emitted with type $f2
  (loop $l (type $block2) (result (ref $f2) (ref $f1))
   (call $f2)
  )
 )
)
