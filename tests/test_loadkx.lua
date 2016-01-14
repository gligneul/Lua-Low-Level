-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllvmjit.h
--
-- test_loadkx.lua

local executetests = require 'tests/executetests' 

-- Test obtained at: http://lua-users.org/lists/lua-l/2012-03/msg00769.html
local a = {"return {0"}
for i = 1,2^18 + 10 do
   a[#a + 1] = i
end
a[#a + 1] = "}"
local f = table.concat(a, ",")

executetests({f}, {{}})

