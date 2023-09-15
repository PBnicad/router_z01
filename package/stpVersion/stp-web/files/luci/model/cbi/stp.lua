-- Copyright (C) 2019 Kyson Lok <kysonlok@gmail.com>
-- Licensed to the public under the GNU General Public License v3.

local m, s, o, u
local cbi = require 'luci.model.uci'

local function is_running(name)
	return luci.sys.call("pidof %s >/dev/null" %{name}) == 0
end

local function get_status(name)
	return is_running(name) and translate("RUNNING") or translate("NOT RUNNING")
end

local function getUwb(name)
	local result = luci.sys.exec("uci get stp.@stp[0].%s" %{name})
	if(result==nil) then
		result="NULL"
	end
	return result
end

local protocol = {
	"UDP",
	"TCP",
}

local device = {
	"/dev/ttyS0",
	"/dev/ttyS1",
	"/dev/ttyS2",
}

local stopbits = {
	1,
	2,
}

local parity = {
	"none",
	"odd",
	"even",
}

m = Map("stp", translate("Serial Transparent Proxy"))
m.template = "stp/status"

s = m:section(TypedSection, "stp", translate("Settings"))
s.anonymous  = true

if is_running("uart2tcp") or is_running("cositea_mqtt") then
	o = s:option(DummyValue, "waring", translate("Waring"))
	o.template = "stp/waring"
end

o = s:option(DummyValue, "_status", translate("Status"))
o.value = "<span id=\"_stp_status\">%s</span>" %{get_status("stp")}
o.rawhtml = true

o = s:option(Flag, "enable", translate("Enable"))
o.rmempty = false

o = s:option(Value, "host", translate("Server Host"))
o.placeholder = "127.0.0.1"
o.datatype = "host"
o.rmempty = false

o = s:option(Value, "port", translate("Server Port"))
o.placeholder = 8080
o.datatype = "port"
o.rmempty = false

o = s:option(ListValue, "udp", translate("Protocol"))
for _, v in ipairs(protocol) do o:value(v) end
o.default = "UDP"

o = s:option(Value, "device", translate("Serial Port"))
for _, v in ipairs(device) do o:value(v) end
o.default = "/dev/ttyS1"

o = s:option(ListValue, "baudrate", translate("Baud Rate"))
for _, v in ipairs({110, 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200}) do o:value(v) end
o.default = 115200

o = s:option(ListValue, "stopbits", translate("Stop Bits"))
for _, v in ipairs(stopbits) do o:value(v) end
o.default = 1

o = s:option(ListValue, "parity", translate("Parity"))
for _, v in ipairs(parity) do o:value(v) end
o.default = "none"

o = s:option(DummyValue, "version", translate("Version"))
o.placeholder = "none"
o.rmempty = false

o = s:option(Flag, "debug", translate("Log Information"))
o.rmempty     = false

--LUJUNLIN added on 2019.10.24
-----------------uwb configuration---------------------------

if(is_running("stp")) then 
	--luci.sys.exec("echo \"getparameters\r\" > /mnt/uwb")

--	o = s:option(DummyValue, "123", translate("styh")) 
--	o.template = "stp/sync"

--	o = s:option(DummyValue, "distance", translate("Distance"))
--	o.template = "stp/setdistance"

--	o = s:option(DummyValue, "mode", translate("Mode"))
--	o.template = "stp/setmode"
	
--	o = s:option(DummyValue, "role", translate("Role"))
--	o.template = "stp/setrole"

--	o = s:option(DummyValue, "clock", translate("Clock Status"))
--	o.template = "stp/setclocksyn"

	-- 0 1 2 3
--	o = s:option(DummyValue, "dimension", translate("Dimension"))
--	o.template = "stp/setdimension"
	
--	o = s:option(DummyValue, "panid", translate("panId"))
--	o.template = "stp/setpanid"
	
	
end

return m
