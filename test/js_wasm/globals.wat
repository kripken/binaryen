(module
  ;; Send values to JS through globals.
  (global $global1 i32 (i32.const 10))
  (global $global2 (mut f64) (f64.const 22.34))

  (export "global-1" (global $global1))
  (export "global-2" (global $global2))
)

