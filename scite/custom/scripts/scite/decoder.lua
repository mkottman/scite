--[[
  Mitchell's decoder.lua
  Copyright (c) 2006-2008 Mitchell Foral. All rights reserved.

  SciTE-tools homepage: http://caladbolg.net/scite.php
  Send email to: mitchell<att>caladbolg<dott>net

  Permission to use, copy, modify, and distribute this file
  is granted, provided credit is given to Mitchell.
]]--

---
-- Parses the line in the SciTE output pane double-clicked.
-- Called by SciTE to parse the line.
-- @param cdoc The line text to parse.
-- @param format The integer style of the line to parse.
-- @return filename, row, and column. If row and column data isn't
--   available, return them as -1. All three return parameters are
--   expected or an error will occur.
function DecodeMessage(cdoc, format)
  if format < 11 then return "", -1, -1 end -- not an error
  if format == 11 then -- generic error
    local filename, line = cdoc:match('^(.-):(%d+):.-$')
    return filename, line - 1, -1
  end
end
