-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- compare.lua
-- Compares two Lua values

local function compare(a, b)
    if type(a) ~= type(b) then
        return false
    elseif type(a) == 'number' then
        if a ~= a and b ~= b then
            return true
        else
            return a == b
        end
    elseif type(a) == 'table' then
        for k, v in ipairs(a) do
            if not compare(b[k], v) then
                return false
            end
        end
        for k, v in ipairs(b) do
            if not compare(a[k], v) then
                return false
            end
        end
        return true 
    else
        return a == b
    end
end

return compare
