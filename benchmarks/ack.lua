-- LLL - Lua Low Level
-- September, 2015
-- Author: Gabriel de Quadros Ligneul
-- Copyright Notice for LLL: see lllcore.h
--
-- Adapted from:
-- $Id: ackermann.lua,v 1.5 2000/12/09 20:07:43 doug Exp $
-- http://www.bagley.org/~doug/shootout/

local benchmark_util = require 'benchmarks/util'

benchmark_util(function()

local function Ack(M, N)
    if M == 0 then
        return N + 1
    end
    if N == 0 then
        return Ack(M - 1, 1)
    end
    return Ack(M - 1, Ack(M, (N - 1)))
end

N = tonumber((arg and arg[1])) or 3
M = tonumber((arg and arg[2])) or 8
Ack(M, N)

end)
