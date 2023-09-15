module("luci.controller.uart2tcp", package.seeall)

local function is_running(name)
	return luci.sys.call("pidof %s >/dev/null" %{name}) == 0
end

function index()
	if not nixio.fs.access("/etc/config/uart2tcp") then
		return
	end

	entry({"admin", "services", "uart2tcp"}, 
		cbi("uart2tcp"), 
		_("UART2TCP"), 70).dependent = true

	entry({"admin", "services", "uart2tcp", "status"}, call("action_status")).leaf = true
	
end


function action_status()
	luci.http.prepare_content("application/json")
	luci.http.write_json({
		status = is_running("uart2tcp")
	})
end
