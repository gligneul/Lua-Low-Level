-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- matmul.lua

return function(f)
    if arg[1] == '--lll-compile' then
        assert(lll.compile(f))
    end
    f()
end
