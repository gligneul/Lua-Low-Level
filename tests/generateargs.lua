-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllvmjit.h
--
-- generateargs.lua

local values = {'nil', 'true', 'false', '-5000', '0', '123', '0.00001', '1.23',
        '10e999', '"0"', '"15.4"', '"a"'}

local function copylist(l)
    local out = {}
    for i, v in ipairs(l) do
        out[i] = v
    end
    return out
end

local function generateargs(args, n)
    if n == 0 then
        return args
    else
        newargs = {}
        for _, a in ipairs(args) do
            for _, v in ipairs(values) do
                newa = copylist(a)
                table.insert(newa, v)
                table.insert(newargs, newa)
            end
        end
        return generateargs(newargs, n - 1)
    end
end

-- Creates lists of size $n containing all possible combination values
return function(n)
    return generateargs({{}}, n)
end

