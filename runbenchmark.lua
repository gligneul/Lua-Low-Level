#!src/lua

local file = arg[1]
if not file then
    error("script must be passed")
end

local n = 10
local total = 0

for i = 1, n do
    local begin = os.clock()
    dofile(file)
    total = total + (os.clock() - begin)
end

print(total / n)
