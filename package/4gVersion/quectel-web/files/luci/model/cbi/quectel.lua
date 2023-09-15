local m, s, o

local function is_running(name)
	return luci.sys.call("pidof %s >/dev/null" %{name}) == 0
end

local function get_status(name)
	return is_running(name) and translate("RUNNING") or translate("NOT RUNNING")
end



m = Map("quectel", translate("4G"))

s = m:section(TypedSection, "server", translate("General Setting"))
s.anonymous   = true

o = s:option(DummyValue, "_status", translate("Status"))
o.value = "<span id=\"_quectel_status\">%s</span>" %{get_status("quectel")}
o.rawhtml     = true

o = s:option(Flag, "enable", translate("Enable"))
o.rmempty     = false


return m
