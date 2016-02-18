-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h

local benchmark_util = require 'benchmarks/util'

benchmark_util(function()

-- fixed-point operator
local function Z(le)
    local function a(f)
        return le(function (x) return f(f)(x) end)
    end
    return a(a)
end

-- non-recursive factorial
local function F(f)
    return function(n)
        if n == 0 then
            return 1
        else
            return n * f(n - 1)
        end
    end
end

local fat = Z(F)

local s = 0
for i = 1, arg[1] or 100 do
    s = s + fat(i)
end
--print(s)

end)
