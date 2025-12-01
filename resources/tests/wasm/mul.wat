(module
  (func $mul (; 0 ;) (param $0 i32) (param $1 i32) (result i32)
    (i32.mul
      (local.get $1)
      (local.get $0)
    )
  )
  (export "MY::MUL" (func $mul))
)