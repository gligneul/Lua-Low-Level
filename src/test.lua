print("----------------------------------------")
print("> function luafoo() ... end")
function luafoo(a, b) if a < b then return 1 end return 0 end

print("----------------------------------------")
print("> jitfoo = jit.compile(luafoo)")
jitfoo = jit.compile(luafoo)

print("----------------------------------------")
print("> jitfoo:dump()")
jitfoo:dump()

print("----------------------------------------")
print("> jitfoo()")
print(jitfoo(4.1, 4))

