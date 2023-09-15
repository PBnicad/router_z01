module("luci.controller.quectel", package.seeall)

function index()
	if not nixio.fs.access("/etc/config/quectel") then
		return
	end

	entry({"admin", "services", "quectel"}, 
		cbi("quectel"), 
		_("4G"), 80).dependent = true
		
	entry({"admin", "services", "quectel", "status"}, 
		call("action_status")).leaf = true
end

local function is_running(name)
	return luci.sys.call("pidof %s >/dev/null" %{name}) == 0
end

function action_status()
	luci.http.prepare_content("application/json")
	luci.http.write_json({
		status = is_running("quectel")
	})
end