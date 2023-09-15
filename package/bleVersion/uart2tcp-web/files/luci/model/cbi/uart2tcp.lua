-- Copyright (C) 2019 UART2TCP
-- Copyright (C) 2019 Kyson Lok <kysonlok@gmail.com>
-- Licensed to the public under the GNU General Public License v3.

local m, s, o

local function is_running(name)
	return luci.sys.call("pidof %s >/dev/null" %{name}) == 0
end

local function get_status(name)
	return is_running(name) and translate("RUNNING") or translate("NOT RUNNING")
end

m = Map("uart2tcp", translate("UART2TCP"))
m.template    = "uart2tcp/status"

s = m:section(TypedSection, "uart2tcp", translate("General Setting"))
s.anonymous   = true


if is_running("stp") or is_running("cositea_mqtt") then
	o = s:option(DummyValue, "waring", translate("Waring"))
	o.template = "uart2tcp/waring"
end

o = s:option(DummyValue, "_status", translate("Status"))
o.value = "<span id=\"_uart2tcp_status\">%s</span>" %{get_status("uart2tcp")}
o.rawhtml     = true

o = s:option(Flag, "enable", translate("Enable"))
o.rmempty     = false

o = s:option(Value, "server", translate("Server Address"))
o.placeholder = "127.0.0.1"
o.datatype    = "ipaddr"

o = s:option(Value, "port", translate("Server Port"))
o.placeholder = 8080
o.datatype    = "port"

return m
