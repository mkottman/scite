--[[
  Mitchell's pm/file_browser.lua
  Copyright (c) 2006-2007 Mitchell Foral. All rights reserved.

  SciTE-tools homepage: http://caladbolg.net/scite.php
  Send email to: mitchell<att>caladbolg<dott>net

  Permission to use, copy, modify, and distribute this file
  is granted, provided credit is given to Mitchell.
]]--

---
-- File Browser for the SciTE project manager.
-- It is enabled by typing the absolute path to a directory into
-- the project manager entry field.
module('browsers.file', package.seeall)

function Matches(entry_text)
  return entry_text:sub(1, 1) == '/'
end

function GetContentsForTreeView(directory)
  local out = io.popen('ls -1p "'..directory..'"'):read('*all')
  if #out == 0 then
    show_error('No such directory: '..directory)
    return {}
  end
  local dir = {}
  for entry in out:gmatch('[^\n]+') do
    if entry:sub(-1, -1) == '/' then
      local name = entry:sub(1, -2)
      dir[name] = {
        parent = true,
        display_text = name,
        pixbuf = 'gtk-directory'
      }
    else
      dir[entry] = { display_text = entry }
    end
  end
  return dir
end

function PerformAction(selected_item)
  Open(selected_item)
end

function GetContextMenu(selected_item)
  return { '_Change Directory', 'File _Details' }
end

function PerformMenuAction(menu_item, selected_item)
  if menu_item == 'Change Directory' then
    SetEntryText(selected_item)
    ActivateEntry()
  elseif menu_item == 'File Details' then
    local out = io.popen('ls -dhl "'..selected_item..'"'):read('*all')
    local perms, num_dirs, owner, group, size, mod_date =
      out:match('^(%S+) (%S+) (%S+) (%S+) (%S+) (%S+ %S)')
    out = 'File details for:\n'..selected_item..'\n'..
          'Perms:\t'..perms..'\n'..
          '#Dirs:\t'..num_dirs..'\n'..
          'Owner:\t'..owner..'\n'..
          'Group:\t'..group..'\n'..
          'Size:\t'..size..'\n'..
          'Date:\t'..mod_date
    text_input(out, nil, false, 250, 250)
  end
end
