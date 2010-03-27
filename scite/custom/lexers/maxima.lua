-- Copyright 2006-2010 Mitchell Foral mitchell<att>caladbolg.net. See LICENSE.
-- Maxima LPeg Lexer

module(..., package.seeall)
local P, R, S = lpeg.P, lpeg.R, lpeg.S

local ws = token('whitespace', whitespace^1)

function LoadTokens()
  local maxima = maxima
  add_token(maxima, 'whitespace', ws)
end
