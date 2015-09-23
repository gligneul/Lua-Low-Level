print("> function f() end")
function f() end

print("> jit.compile(f)")
jit.compile(f)

print("> jit.dump(f)")
jit.dump(f)

