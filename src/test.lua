print("----------------------------------------")
print("> function luafoo() ... end")
function luafoo(a, b) return 123 end -- a + b end

print("----------------------------------------")
print("> jitfoo = jit.compile(luafoo)")
jitfoo = jit.compile(luafoo)

print("----------------------------------------")
print("> jitfoo:dump()")
jitfoo:dump()

print("----------------------------------------")
print("> jitfoo()")
print(jitfoo(2.5, 4))

