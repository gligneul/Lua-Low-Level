-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h

local benchmark_util = require 'benchmarks/util'

benchmark_util(function()

local random, floor = math.random, math.floor
floor = math.ifloor or floor

function heapsort(n, ra)
    local j, i, rra
    local l = floor(n/2) + 1
    -- local l = (n//2) + 1
    local ir = n;
    while 1 do
        if l > 1 then
            l = l - 1
            rra = ra[l]
        else
            rra = ra[ir]
            ra[ir] = ra[1]
            ir = ir - 1
            if (ir == 1) then
                ra[1] = rra
                return
            end
        end
        i = l
        j = l * 2
        while j <= ir do
            if (j < ir) and (ra[j] < ra[j+1]) then
                j = j + 1
            end
            if rra < ra[j] then
                ra[i] = ra[j]
                i = j
                j = j + i
            else
                j = ir + 1
            end
        end
        ra[i] = rra
    end
end

local Num = tonumber((arg and arg[1])) or 5
for i=1,Num do
  local N = tonumber((arg and arg[2])) or 100000
  local a = {}
  for i=1,N do a[i] = random() end
  heapsort(N, a)
  for i=1,N-1 do assert(a[i] <= a[i+1]) end
end

end)
