(module
 (type $0 (func))
 (type $1 (func (result i32)))
 (type $2 (func (param i32 i32) (result i32)))
 (memory $0 2)
 (table $0 1 1 funcref)
 (global $global$0 (mut i32) (i32.const 66112))
 (global $global$1 i32 (i32.const 576))
 (export "memory" (memory $0))
 (export "__wasm_call_ctors" (func $__wasm_call_ctors))
 (export "main" (func $main))
 (export "__data_end" (global $global$1))
 (func $__wasm_call_ctors (; 0 ;) (type $0)
  (call $init_x)
  (call $init_y)
 )
 (func $init_x (; 1 ;) (type $0)
  (i32.store offset=568
   (i32.const 0)
   (i32.const 14)
  )
 )
 (func $init_y (; 2 ;) (type $0)
  (i32.store offset=572
   (i32.const 0)
   (i32.const 144)
  )
 )
 (func $__original_main (; 3 ;) (type $1) (result i32)
  (i32.add
   (i32.load offset=568
    (i32.const 0)
   )
   (i32.load offset=572
    (i32.const 0)
   )
  )
 )
 (func $main (; 4 ;) (type $2) (param $0 i32) (param $1 i32) (result i32)
  (call $__original_main)
 )
 ;; custom section "producers", size 112
)

