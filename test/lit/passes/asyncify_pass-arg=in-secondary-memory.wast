;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.

;; RUN: wasm-opt --enable-multimemory --asyncify --pass-arg=asyncify-in-secondary-memory %s -S -o - | filecheck %s
;; RUN: wasm-opt --enable-multimemory --asyncify --pass-arg=asyncify-in-secondary-memory --pass-arg=asyncify-secondary-memory-size@3 %s -S -o - | filecheck %s --check-prefix SIZE

(module
  (memory 1 2)
  ;; CHECK:      (type $0 (func))

  ;; CHECK:      (type $1 (func (param i32)))

  ;; CHECK:      (type $2 (func (param i32 i32)))

  ;; CHECK:      (type $3 (func (result i32)))

  ;; CHECK:      (import "env" "import" (func $import))
  ;; SIZE:      (type $0 (func))

  ;; SIZE:      (type $1 (func (param i32)))

  ;; SIZE:      (type $2 (func (param i32 i32)))

  ;; SIZE:      (type $3 (func (result i32)))

  ;; SIZE:      (import "env" "import" (func $import))
  (import "env" "import" (func $import))
  ;; CHECK:      (global $__asyncify_state (mut i32) (i32.const 0))

  ;; CHECK:      (global $__asyncify_data (mut i32) (i32.const 0))

  ;; CHECK:      (memory $0 1 2)

  ;; CHECK:      (memory $asyncify_memory 1 1)

  ;; CHECK:      (export "asyncify_start_unwind" (func $asyncify_start_unwind))

  ;; CHECK:      (export "asyncify_stop_unwind" (func $asyncify_stop_unwind))

  ;; CHECK:      (export "asyncify_start_rewind" (func $asyncify_start_rewind))

  ;; CHECK:      (export "asyncify_stop_rewind" (func $asyncify_stop_rewind))

  ;; CHECK:      (export "asyncify_get_state" (func $asyncify_get_state))

  ;; CHECK:      (func $liveness1 (param $live0 i32) (param $dead0 i32)
  ;; CHECK-NEXT:  (local $live1 i32)
  ;; CHECK-NEXT:  (local $dead1 i32)
  ;; CHECK-NEXT:  (local $4 i32)
  ;; CHECK-NEXT:  (local $5 i32)
  ;; CHECK-NEXT:  (local $6 i32)
  ;; CHECK-NEXT:  (local $7 i32)
  ;; CHECK-NEXT:  (local $8 i32)
  ;; CHECK-NEXT:  (local $9 i32)
  ;; CHECK-NEXT:  (local $10 i32)
  ;; CHECK-NEXT:  (local $11 i32)
  ;; CHECK-NEXT:  (if
  ;; CHECK-NEXT:   (i32.eq
  ;; CHECK-NEXT:    (global.get $__asyncify_state)
  ;; CHECK-NEXT:    (i32.const 2)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (block
  ;; CHECK-NEXT:    (i32.store $asyncify_memory
  ;; CHECK-NEXT:     (global.get $__asyncify_data)
  ;; CHECK-NEXT:     (i32.add
  ;; CHECK-NEXT:      (i32.load $asyncify_memory
  ;; CHECK-NEXT:       (global.get $__asyncify_data)
  ;; CHECK-NEXT:      )
  ;; CHECK-NEXT:      (i32.const -8)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (local.set $10
  ;; CHECK-NEXT:     (i32.load $asyncify_memory
  ;; CHECK-NEXT:      (global.get $__asyncify_data)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (local.set $live0
  ;; CHECK-NEXT:     (i32.load $asyncify_memory
  ;; CHECK-NEXT:      (local.get $10)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (local.set $live1
  ;; CHECK-NEXT:     (i32.load $asyncify_memory offset=4
  ;; CHECK-NEXT:      (local.get $10)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (local.set $8
  ;; CHECK-NEXT:   (block $__asyncify_unwind (result i32)
  ;; CHECK-NEXT:    (block
  ;; CHECK-NEXT:     (block
  ;; CHECK-NEXT:      (if
  ;; CHECK-NEXT:       (i32.eq
  ;; CHECK-NEXT:        (global.get $__asyncify_state)
  ;; CHECK-NEXT:        (i32.const 2)
  ;; CHECK-NEXT:       )
  ;; CHECK-NEXT:       (block
  ;; CHECK-NEXT:        (i32.store $asyncify_memory
  ;; CHECK-NEXT:         (global.get $__asyncify_data)
  ;; CHECK-NEXT:         (i32.add
  ;; CHECK-NEXT:          (i32.load $asyncify_memory
  ;; CHECK-NEXT:           (global.get $__asyncify_data)
  ;; CHECK-NEXT:          )
  ;; CHECK-NEXT:          (i32.const -4)
  ;; CHECK-NEXT:         )
  ;; CHECK-NEXT:        )
  ;; CHECK-NEXT:        (local.set $9
  ;; CHECK-NEXT:         (i32.load $asyncify_memory
  ;; CHECK-NEXT:          (i32.load $asyncify_memory
  ;; CHECK-NEXT:           (global.get $__asyncify_data)
  ;; CHECK-NEXT:          )
  ;; CHECK-NEXT:         )
  ;; CHECK-NEXT:        )
  ;; CHECK-NEXT:       )
  ;; CHECK-NEXT:      )
  ;; CHECK-NEXT:      (block
  ;; CHECK-NEXT:       (if
  ;; CHECK-NEXT:        (i32.eq
  ;; CHECK-NEXT:         (global.get $__asyncify_state)
  ;; CHECK-NEXT:         (i32.const 0)
  ;; CHECK-NEXT:        )
  ;; CHECK-NEXT:        (block
  ;; CHECK-NEXT:         (local.set $4
  ;; CHECK-NEXT:          (local.get $dead0)
  ;; CHECK-NEXT:         )
  ;; CHECK-NEXT:         (drop
  ;; CHECK-NEXT:          (local.get $4)
  ;; CHECK-NEXT:         )
  ;; CHECK-NEXT:         (local.set $5
  ;; CHECK-NEXT:          (local.get $dead1)
  ;; CHECK-NEXT:         )
  ;; CHECK-NEXT:         (drop
  ;; CHECK-NEXT:          (local.get $5)
  ;; CHECK-NEXT:         )
  ;; CHECK-NEXT:        )
  ;; CHECK-NEXT:       )
  ;; CHECK-NEXT:       (nop)
  ;; CHECK-NEXT:       (nop)
  ;; CHECK-NEXT:       (nop)
  ;; CHECK-NEXT:       (if
  ;; CHECK-NEXT:        (if (result i32)
  ;; CHECK-NEXT:         (i32.eq
  ;; CHECK-NEXT:          (global.get $__asyncify_state)
  ;; CHECK-NEXT:          (i32.const 0)
  ;; CHECK-NEXT:         )
  ;; CHECK-NEXT:         (i32.const 1)
  ;; CHECK-NEXT:         (i32.eq
  ;; CHECK-NEXT:          (local.get $9)
  ;; CHECK-NEXT:          (i32.const 0)
  ;; CHECK-NEXT:         )
  ;; CHECK-NEXT:        )
  ;; CHECK-NEXT:        (block
  ;; CHECK-NEXT:         (call $import)
  ;; CHECK-NEXT:         (if
  ;; CHECK-NEXT:          (i32.eq
  ;; CHECK-NEXT:           (global.get $__asyncify_state)
  ;; CHECK-NEXT:           (i32.const 1)
  ;; CHECK-NEXT:          )
  ;; CHECK-NEXT:          (br $__asyncify_unwind
  ;; CHECK-NEXT:           (i32.const 0)
  ;; CHECK-NEXT:          )
  ;; CHECK-NEXT:         )
  ;; CHECK-NEXT:        )
  ;; CHECK-NEXT:       )
  ;; CHECK-NEXT:       (if
  ;; CHECK-NEXT:        (i32.eq
  ;; CHECK-NEXT:         (global.get $__asyncify_state)
  ;; CHECK-NEXT:         (i32.const 0)
  ;; CHECK-NEXT:        )
  ;; CHECK-NEXT:        (block
  ;; CHECK-NEXT:         (local.set $6
  ;; CHECK-NEXT:          (local.get $live0)
  ;; CHECK-NEXT:         )
  ;; CHECK-NEXT:         (drop
  ;; CHECK-NEXT:          (local.get $6)
  ;; CHECK-NEXT:         )
  ;; CHECK-NEXT:         (local.set $7
  ;; CHECK-NEXT:          (local.get $live1)
  ;; CHECK-NEXT:         )
  ;; CHECK-NEXT:         (drop
  ;; CHECK-NEXT:          (local.get $7)
  ;; CHECK-NEXT:         )
  ;; CHECK-NEXT:        )
  ;; CHECK-NEXT:       )
  ;; CHECK-NEXT:       (nop)
  ;; CHECK-NEXT:       (nop)
  ;; CHECK-NEXT:       (nop)
  ;; CHECK-NEXT:      )
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:     (return)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (block
  ;; CHECK-NEXT:   (i32.store $asyncify_memory
  ;; CHECK-NEXT:    (i32.load $asyncify_memory
  ;; CHECK-NEXT:     (global.get $__asyncify_data)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:    (local.get $8)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (i32.store $asyncify_memory
  ;; CHECK-NEXT:    (global.get $__asyncify_data)
  ;; CHECK-NEXT:    (i32.add
  ;; CHECK-NEXT:     (i32.load $asyncify_memory
  ;; CHECK-NEXT:      (global.get $__asyncify_data)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:     (i32.const 4)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT:  (block
  ;; CHECK-NEXT:   (local.set $11
  ;; CHECK-NEXT:    (i32.load $asyncify_memory
  ;; CHECK-NEXT:     (global.get $__asyncify_data)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (i32.store $asyncify_memory
  ;; CHECK-NEXT:    (local.get $11)
  ;; CHECK-NEXT:    (local.get $live0)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (i32.store $asyncify_memory offset=4
  ;; CHECK-NEXT:    (local.get $11)
  ;; CHECK-NEXT:    (local.get $live1)
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:   (i32.store $asyncify_memory
  ;; CHECK-NEXT:    (global.get $__asyncify_data)
  ;; CHECK-NEXT:    (i32.add
  ;; CHECK-NEXT:     (i32.load $asyncify_memory
  ;; CHECK-NEXT:      (global.get $__asyncify_data)
  ;; CHECK-NEXT:     )
  ;; CHECK-NEXT:     (i32.const 8)
  ;; CHECK-NEXT:    )
  ;; CHECK-NEXT:   )
  ;; CHECK-NEXT:  )
  ;; CHECK-NEXT: )
  ;; SIZE:      (global $__asyncify_state (mut i32) (i32.const 0))

  ;; SIZE:      (global $__asyncify_data (mut i32) (i32.const 0))

  ;; SIZE:      (memory $0 1 2)

  ;; SIZE:      (memory $asyncify_memory 3 3)

  ;; SIZE:      (export "asyncify_start_unwind" (func $asyncify_start_unwind))

  ;; SIZE:      (export "asyncify_stop_unwind" (func $asyncify_stop_unwind))

  ;; SIZE:      (export "asyncify_start_rewind" (func $asyncify_start_rewind))

  ;; SIZE:      (export "asyncify_stop_rewind" (func $asyncify_stop_rewind))

  ;; SIZE:      (export "asyncify_get_state" (func $asyncify_get_state))

  ;; SIZE:      (func $liveness1 (param $live0 i32) (param $dead0 i32)
  ;; SIZE-NEXT:  (local $live1 i32)
  ;; SIZE-NEXT:  (local $dead1 i32)
  ;; SIZE-NEXT:  (local $4 i32)
  ;; SIZE-NEXT:  (local $5 i32)
  ;; SIZE-NEXT:  (local $6 i32)
  ;; SIZE-NEXT:  (local $7 i32)
  ;; SIZE-NEXT:  (local $8 i32)
  ;; SIZE-NEXT:  (local $9 i32)
  ;; SIZE-NEXT:  (local $10 i32)
  ;; SIZE-NEXT:  (local $11 i32)
  ;; SIZE-NEXT:  (if
  ;; SIZE-NEXT:   (i32.eq
  ;; SIZE-NEXT:    (global.get $__asyncify_state)
  ;; SIZE-NEXT:    (i32.const 2)
  ;; SIZE-NEXT:   )
  ;; SIZE-NEXT:   (block
  ;; SIZE-NEXT:    (i32.store $asyncify_memory
  ;; SIZE-NEXT:     (global.get $__asyncify_data)
  ;; SIZE-NEXT:     (i32.add
  ;; SIZE-NEXT:      (i32.load $asyncify_memory
  ;; SIZE-NEXT:       (global.get $__asyncify_data)
  ;; SIZE-NEXT:      )
  ;; SIZE-NEXT:      (i32.const -8)
  ;; SIZE-NEXT:     )
  ;; SIZE-NEXT:    )
  ;; SIZE-NEXT:    (local.set $10
  ;; SIZE-NEXT:     (i32.load $asyncify_memory
  ;; SIZE-NEXT:      (global.get $__asyncify_data)
  ;; SIZE-NEXT:     )
  ;; SIZE-NEXT:    )
  ;; SIZE-NEXT:    (local.set $live0
  ;; SIZE-NEXT:     (i32.load $asyncify_memory
  ;; SIZE-NEXT:      (local.get $10)
  ;; SIZE-NEXT:     )
  ;; SIZE-NEXT:    )
  ;; SIZE-NEXT:    (local.set $live1
  ;; SIZE-NEXT:     (i32.load $asyncify_memory offset=4
  ;; SIZE-NEXT:      (local.get $10)
  ;; SIZE-NEXT:     )
  ;; SIZE-NEXT:    )
  ;; SIZE-NEXT:   )
  ;; SIZE-NEXT:  )
  ;; SIZE-NEXT:  (local.set $8
  ;; SIZE-NEXT:   (block $__asyncify_unwind (result i32)
  ;; SIZE-NEXT:    (block
  ;; SIZE-NEXT:     (block
  ;; SIZE-NEXT:      (if
  ;; SIZE-NEXT:       (i32.eq
  ;; SIZE-NEXT:        (global.get $__asyncify_state)
  ;; SIZE-NEXT:        (i32.const 2)
  ;; SIZE-NEXT:       )
  ;; SIZE-NEXT:       (block
  ;; SIZE-NEXT:        (i32.store $asyncify_memory
  ;; SIZE-NEXT:         (global.get $__asyncify_data)
  ;; SIZE-NEXT:         (i32.add
  ;; SIZE-NEXT:          (i32.load $asyncify_memory
  ;; SIZE-NEXT:           (global.get $__asyncify_data)
  ;; SIZE-NEXT:          )
  ;; SIZE-NEXT:          (i32.const -4)
  ;; SIZE-NEXT:         )
  ;; SIZE-NEXT:        )
  ;; SIZE-NEXT:        (local.set $9
  ;; SIZE-NEXT:         (i32.load $asyncify_memory
  ;; SIZE-NEXT:          (i32.load $asyncify_memory
  ;; SIZE-NEXT:           (global.get $__asyncify_data)
  ;; SIZE-NEXT:          )
  ;; SIZE-NEXT:         )
  ;; SIZE-NEXT:        )
  ;; SIZE-NEXT:       )
  ;; SIZE-NEXT:      )
  ;; SIZE-NEXT:      (block
  ;; SIZE-NEXT:       (if
  ;; SIZE-NEXT:        (i32.eq
  ;; SIZE-NEXT:         (global.get $__asyncify_state)
  ;; SIZE-NEXT:         (i32.const 0)
  ;; SIZE-NEXT:        )
  ;; SIZE-NEXT:        (block
  ;; SIZE-NEXT:         (local.set $4
  ;; SIZE-NEXT:          (local.get $dead0)
  ;; SIZE-NEXT:         )
  ;; SIZE-NEXT:         (drop
  ;; SIZE-NEXT:          (local.get $4)
  ;; SIZE-NEXT:         )
  ;; SIZE-NEXT:         (local.set $5
  ;; SIZE-NEXT:          (local.get $dead1)
  ;; SIZE-NEXT:         )
  ;; SIZE-NEXT:         (drop
  ;; SIZE-NEXT:          (local.get $5)
  ;; SIZE-NEXT:         )
  ;; SIZE-NEXT:        )
  ;; SIZE-NEXT:       )
  ;; SIZE-NEXT:       (nop)
  ;; SIZE-NEXT:       (nop)
  ;; SIZE-NEXT:       (nop)
  ;; SIZE-NEXT:       (if
  ;; SIZE-NEXT:        (if (result i32)
  ;; SIZE-NEXT:         (i32.eq
  ;; SIZE-NEXT:          (global.get $__asyncify_state)
  ;; SIZE-NEXT:          (i32.const 0)
  ;; SIZE-NEXT:         )
  ;; SIZE-NEXT:         (i32.const 1)
  ;; SIZE-NEXT:         (i32.eq
  ;; SIZE-NEXT:          (local.get $9)
  ;; SIZE-NEXT:          (i32.const 0)
  ;; SIZE-NEXT:         )
  ;; SIZE-NEXT:        )
  ;; SIZE-NEXT:        (block
  ;; SIZE-NEXT:         (call $import)
  ;; SIZE-NEXT:         (if
  ;; SIZE-NEXT:          (i32.eq
  ;; SIZE-NEXT:           (global.get $__asyncify_state)
  ;; SIZE-NEXT:           (i32.const 1)
  ;; SIZE-NEXT:          )
  ;; SIZE-NEXT:          (br $__asyncify_unwind
  ;; SIZE-NEXT:           (i32.const 0)
  ;; SIZE-NEXT:          )
  ;; SIZE-NEXT:         )
  ;; SIZE-NEXT:        )
  ;; SIZE-NEXT:       )
  ;; SIZE-NEXT:       (if
  ;; SIZE-NEXT:        (i32.eq
  ;; SIZE-NEXT:         (global.get $__asyncify_state)
  ;; SIZE-NEXT:         (i32.const 0)
  ;; SIZE-NEXT:        )
  ;; SIZE-NEXT:        (block
  ;; SIZE-NEXT:         (local.set $6
  ;; SIZE-NEXT:          (local.get $live0)
  ;; SIZE-NEXT:         )
  ;; SIZE-NEXT:         (drop
  ;; SIZE-NEXT:          (local.get $6)
  ;; SIZE-NEXT:         )
  ;; SIZE-NEXT:         (local.set $7
  ;; SIZE-NEXT:          (local.get $live1)
  ;; SIZE-NEXT:         )
  ;; SIZE-NEXT:         (drop
  ;; SIZE-NEXT:          (local.get $7)
  ;; SIZE-NEXT:         )
  ;; SIZE-NEXT:        )
  ;; SIZE-NEXT:       )
  ;; SIZE-NEXT:       (nop)
  ;; SIZE-NEXT:       (nop)
  ;; SIZE-NEXT:       (nop)
  ;; SIZE-NEXT:      )
  ;; SIZE-NEXT:     )
  ;; SIZE-NEXT:     (return)
  ;; SIZE-NEXT:    )
  ;; SIZE-NEXT:   )
  ;; SIZE-NEXT:  )
  ;; SIZE-NEXT:  (block
  ;; SIZE-NEXT:   (i32.store $asyncify_memory
  ;; SIZE-NEXT:    (i32.load $asyncify_memory
  ;; SIZE-NEXT:     (global.get $__asyncify_data)
  ;; SIZE-NEXT:    )
  ;; SIZE-NEXT:    (local.get $8)
  ;; SIZE-NEXT:   )
  ;; SIZE-NEXT:   (i32.store $asyncify_memory
  ;; SIZE-NEXT:    (global.get $__asyncify_data)
  ;; SIZE-NEXT:    (i32.add
  ;; SIZE-NEXT:     (i32.load $asyncify_memory
  ;; SIZE-NEXT:      (global.get $__asyncify_data)
  ;; SIZE-NEXT:     )
  ;; SIZE-NEXT:     (i32.const 4)
  ;; SIZE-NEXT:    )
  ;; SIZE-NEXT:   )
  ;; SIZE-NEXT:  )
  ;; SIZE-NEXT:  (block
  ;; SIZE-NEXT:   (local.set $11
  ;; SIZE-NEXT:    (i32.load $asyncify_memory
  ;; SIZE-NEXT:     (global.get $__asyncify_data)
  ;; SIZE-NEXT:    )
  ;; SIZE-NEXT:   )
  ;; SIZE-NEXT:   (i32.store $asyncify_memory
  ;; SIZE-NEXT:    (local.get $11)
  ;; SIZE-NEXT:    (local.get $live0)
  ;; SIZE-NEXT:   )
  ;; SIZE-NEXT:   (i32.store $asyncify_memory offset=4
  ;; SIZE-NEXT:    (local.get $11)
  ;; SIZE-NEXT:    (local.get $live1)
  ;; SIZE-NEXT:   )
  ;; SIZE-NEXT:   (i32.store $asyncify_memory
  ;; SIZE-NEXT:    (global.get $__asyncify_data)
  ;; SIZE-NEXT:    (i32.add
  ;; SIZE-NEXT:     (i32.load $asyncify_memory
  ;; SIZE-NEXT:      (global.get $__asyncify_data)
  ;; SIZE-NEXT:     )
  ;; SIZE-NEXT:     (i32.const 8)
  ;; SIZE-NEXT:    )
  ;; SIZE-NEXT:   )
  ;; SIZE-NEXT:  )
  ;; SIZE-NEXT: )
  (func $liveness1 (param $live0 i32) (param $dead0 i32)
    (local $live1 i32)
    (local $dead1 i32)
    (drop (local.get $dead0))
    (drop (local.get $dead1))
    (call $import)
    (drop (local.get $live0))
    (drop (local.get $live1))
  )
)
;; CHECK:      (func $asyncify_start_unwind (param $0 i32)
;; CHECK-NEXT:  (global.set $__asyncify_state
;; CHECK-NEXT:   (i32.const 1)
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (global.set $__asyncify_data
;; CHECK-NEXT:   (local.get $0)
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (if
;; CHECK-NEXT:   (i32.gt_u
;; CHECK-NEXT:    (i32.load $asyncify_memory
;; CHECK-NEXT:     (global.get $__asyncify_data)
;; CHECK-NEXT:    )
;; CHECK-NEXT:    (i32.load $asyncify_memory offset=4
;; CHECK-NEXT:     (global.get $__asyncify_data)
;; CHECK-NEXT:    )
;; CHECK-NEXT:   )
;; CHECK-NEXT:   (unreachable)
;; CHECK-NEXT:  )
;; CHECK-NEXT: )

;; CHECK:      (func $asyncify_stop_unwind
;; CHECK-NEXT:  (global.set $__asyncify_state
;; CHECK-NEXT:   (i32.const 0)
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (if
;; CHECK-NEXT:   (i32.gt_u
;; CHECK-NEXT:    (i32.load $asyncify_memory
;; CHECK-NEXT:     (global.get $__asyncify_data)
;; CHECK-NEXT:    )
;; CHECK-NEXT:    (i32.load $asyncify_memory offset=4
;; CHECK-NEXT:     (global.get $__asyncify_data)
;; CHECK-NEXT:    )
;; CHECK-NEXT:   )
;; CHECK-NEXT:   (unreachable)
;; CHECK-NEXT:  )
;; CHECK-NEXT: )

;; CHECK:      (func $asyncify_start_rewind (param $0 i32)
;; CHECK-NEXT:  (global.set $__asyncify_state
;; CHECK-NEXT:   (i32.const 2)
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (global.set $__asyncify_data
;; CHECK-NEXT:   (local.get $0)
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (if
;; CHECK-NEXT:   (i32.gt_u
;; CHECK-NEXT:    (i32.load $asyncify_memory
;; CHECK-NEXT:     (global.get $__asyncify_data)
;; CHECK-NEXT:    )
;; CHECK-NEXT:    (i32.load $asyncify_memory offset=4
;; CHECK-NEXT:     (global.get $__asyncify_data)
;; CHECK-NEXT:    )
;; CHECK-NEXT:   )
;; CHECK-NEXT:   (unreachable)
;; CHECK-NEXT:  )
;; CHECK-NEXT: )

;; CHECK:      (func $asyncify_stop_rewind
;; CHECK-NEXT:  (global.set $__asyncify_state
;; CHECK-NEXT:   (i32.const 0)
;; CHECK-NEXT:  )
;; CHECK-NEXT:  (if
;; CHECK-NEXT:   (i32.gt_u
;; CHECK-NEXT:    (i32.load $asyncify_memory
;; CHECK-NEXT:     (global.get $__asyncify_data)
;; CHECK-NEXT:    )
;; CHECK-NEXT:    (i32.load $asyncify_memory offset=4
;; CHECK-NEXT:     (global.get $__asyncify_data)
;; CHECK-NEXT:    )
;; CHECK-NEXT:   )
;; CHECK-NEXT:   (unreachable)
;; CHECK-NEXT:  )
;; CHECK-NEXT: )

;; CHECK:      (func $asyncify_get_state (result i32)
;; CHECK-NEXT:  (global.get $__asyncify_state)
;; CHECK-NEXT: )

;; SIZE:      (func $asyncify_start_unwind (param $0 i32)
;; SIZE-NEXT:  (global.set $__asyncify_state
;; SIZE-NEXT:   (i32.const 1)
;; SIZE-NEXT:  )
;; SIZE-NEXT:  (global.set $__asyncify_data
;; SIZE-NEXT:   (local.get $0)
;; SIZE-NEXT:  )
;; SIZE-NEXT:  (if
;; SIZE-NEXT:   (i32.gt_u
;; SIZE-NEXT:    (i32.load $asyncify_memory
;; SIZE-NEXT:     (global.get $__asyncify_data)
;; SIZE-NEXT:    )
;; SIZE-NEXT:    (i32.load $asyncify_memory offset=4
;; SIZE-NEXT:     (global.get $__asyncify_data)
;; SIZE-NEXT:    )
;; SIZE-NEXT:   )
;; SIZE-NEXT:   (unreachable)
;; SIZE-NEXT:  )
;; SIZE-NEXT: )

;; SIZE:      (func $asyncify_stop_unwind
;; SIZE-NEXT:  (global.set $__asyncify_state
;; SIZE-NEXT:   (i32.const 0)
;; SIZE-NEXT:  )
;; SIZE-NEXT:  (if
;; SIZE-NEXT:   (i32.gt_u
;; SIZE-NEXT:    (i32.load $asyncify_memory
;; SIZE-NEXT:     (global.get $__asyncify_data)
;; SIZE-NEXT:    )
;; SIZE-NEXT:    (i32.load $asyncify_memory offset=4
;; SIZE-NEXT:     (global.get $__asyncify_data)
;; SIZE-NEXT:    )
;; SIZE-NEXT:   )
;; SIZE-NEXT:   (unreachable)
;; SIZE-NEXT:  )
;; SIZE-NEXT: )

;; SIZE:      (func $asyncify_start_rewind (param $0 i32)
;; SIZE-NEXT:  (global.set $__asyncify_state
;; SIZE-NEXT:   (i32.const 2)
;; SIZE-NEXT:  )
;; SIZE-NEXT:  (global.set $__asyncify_data
;; SIZE-NEXT:   (local.get $0)
;; SIZE-NEXT:  )
;; SIZE-NEXT:  (if
;; SIZE-NEXT:   (i32.gt_u
;; SIZE-NEXT:    (i32.load $asyncify_memory
;; SIZE-NEXT:     (global.get $__asyncify_data)
;; SIZE-NEXT:    )
;; SIZE-NEXT:    (i32.load $asyncify_memory offset=4
;; SIZE-NEXT:     (global.get $__asyncify_data)
;; SIZE-NEXT:    )
;; SIZE-NEXT:   )
;; SIZE-NEXT:   (unreachable)
;; SIZE-NEXT:  )
;; SIZE-NEXT: )

;; SIZE:      (func $asyncify_stop_rewind
;; SIZE-NEXT:  (global.set $__asyncify_state
;; SIZE-NEXT:   (i32.const 0)
;; SIZE-NEXT:  )
;; SIZE-NEXT:  (if
;; SIZE-NEXT:   (i32.gt_u
;; SIZE-NEXT:    (i32.load $asyncify_memory
;; SIZE-NEXT:     (global.get $__asyncify_data)
;; SIZE-NEXT:    )
;; SIZE-NEXT:    (i32.load $asyncify_memory offset=4
;; SIZE-NEXT:     (global.get $__asyncify_data)
;; SIZE-NEXT:    )
;; SIZE-NEXT:   )
;; SIZE-NEXT:   (unreachable)
;; SIZE-NEXT:  )
;; SIZE-NEXT: )

;; SIZE:      (func $asyncify_get_state (result i32)
;; SIZE-NEXT:  (global.get $__asyncify_state)
;; SIZE-NEXT: )
