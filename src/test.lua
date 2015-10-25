print("> function luafoo() return 123 end")
function luafoo() return 123 end

print("> jitfoo = jit.compile(luafoo)")
jitfoo = jit.compile(luafoo)

print("> jitfoo:dump()")
jitfoo:dump()

