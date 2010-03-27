require 'socket'
local udp = socket.udp()
udp:setoption('broadcast',true)

local function log(...)
	local n,p = select('#', ...), {...}
	for i=1,n do p[i] = tostring(p[i]) end
	local s = table.concat(p, '\t')
	local ok, err = udp:sendto(s, '255.255.255.255', 5555)
	if not ok then print(err) end
end

scite_OnChar(function(c)
	local pos = editor.CurrentPos
	log(c, pos)
end)