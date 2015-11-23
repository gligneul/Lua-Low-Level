print("----------------------------------------")
print("> function luafoo() ... end")
function luafoo(a, b) if a == b then return true end return false end

print("----------------------------------------")
print("> jitfoo = jit.compile(luafoo)")
jitfoo = jit.compile(luafoo)

print("----------------------------------------")
print("> jitfoo:dump()")
jitfoo:dump()

print("----------------------------------------")
print("> jitfoo()")
print(jitfoo(0, 1))

