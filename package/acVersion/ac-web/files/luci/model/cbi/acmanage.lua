-- Copyright (C) 2020 Access Control
-- Copyright (C) 2020 Kyson Lok <kysonlok@gmail.com>
-- Licensed to the public under the GNU General Public License v3.
local m, s, o

local function is_running(name)
	return luci.sys.call("pidof %s >/dev/null" %{name}) == 0
end

local function get_status(name)
	return is_running(name) and translate("RUNNING") or translate("NOT RUNNING")
end

local function get_mac()
	return luci.sys.exec("hexdump -v -n 6 -s 4 -e '6/1 \"%02x\"' /dev/mtd2 2>/dev/null")
end

m = Map("acmanage", translate("Access Control"))

s = m:section(TypedSection, "server", translate("General Setting"))
s.anonymous   = true

--if is_running("stp") or is_running("uart2tcp") then
--	o = s:option(DummyValue, "waring", translate("Waring"))
--	o.template = "mqtt/waring"
--end

o = s:option(DummyValue, "_status", translate("Status"))
o.value = "<span id=\"_mqtt_status\">%s</span>" %{get_status("acmanage")}
o.rawhtml     = true

o = s:option(Flag, "enable", translate("Enable"))
o.rmempty     = false

o = s:option(Value, "host", translate("Host"))
o.placeholder = "www.example.com"	-- test.mosquitto.org
o.datatype    = "host"

o = s:option(Value, "port", translate("Port"))
o.placeholder = 61613
o.datatype    = "port"

o = s:option(Value, "username", translate("Username"))
o.placeholder = "username"

o = s:option(Value, "password", translate("Password"))
o.placeholder = "password"
o.password = true

o = s:option(DynamicList, "pub", translate("Publish Topic"))
o.placeholder = "ac/cloud/" .. get_mac()

o = s:option(DynamicList, "sub", translate("Private Subscribe Topic"))
o.placeholder = "ac/" .. get_mac() .. "/cloud"

o = s:option(Value, "id", translate("ClinetID"))
o.placeholder = "ac" .. get_mac()

o = s:option(Flag, "debug", translate("Log Information"))
o.rmempty     = false



return m
