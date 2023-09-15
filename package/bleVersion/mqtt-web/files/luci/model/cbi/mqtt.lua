-- Copyright (C) 2020 MQTT
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

local device = {
	"/dev/ttyS0",
	"/dev/ttyS1",
	"/dev/ttyS2",
}

m = Map("cositea_mqtt", translate("MQTT"))

s = m:section(TypedSection, "server", translate("General Setting"))
s.anonymous   = true

if is_running("stp") or is_running("uart2tcp") then
	o = s:option(DummyValue, "waring", translate("Waring"))
	o.template = "mqtt/waring"
end

o = s:option(DummyValue, "_status", translate("Status"))
o.value = "<span id=\"_mqtt_status\">%s</span>" %{get_status("cositea_mqtt")}
o.rawhtml     = true

o = s:option(Flag, "enable", translate("Enable"))
o.rmempty     = false

--o = s:option(Value, "version", translate("Protocol Version"))
--o.placeholder = "0~255"
--o.datatype    = "uchar"
o = s:option(Value, "device", translate("Serial Port"))
for _, v in ipairs(device) do o:value(v) end
o.default = "/dev/ttyS1"

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
o.placeholder = "sys/cloud/" .. get_mac()

-- o = s:option(Value, "public", translate("Public Subscribe Topic"))
-- o.placeholder = "sys/gateway/cloud"

o = s:option(DynamicList, "sub", translate("Private Subscribe Topic"))
o.placeholder = "sys/" .. get_mac() .. "/cloud"

o = s:option(Value, "id", translate("ClinetID"))
o.placeholder = get_mac()

o = s:option(Flag, "debug", translate("Log Information"))
o.rmempty     = false

-- this is long ago test action, after will be delete 
-- o = s:option(Flag, "mode", translate("Passthrough Mode"))
-- o.rmempty     = false

o = s:option(Flag, "automaticble", translate("automatic module"))
o.rmempty     = false

o = s:option(Value, "filter", translate("Filter Rssi"))
o.placeholder = "55"

o = s:option(Flag, "blelog", translate("Other Log Information"))
o.rmempty     = false

return m
