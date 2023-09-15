module("luci.controller.mqtt", package.seeall)

function index()
	if not nixio.fs.access("/etc/config/cositea_mqtt") then
		return
	end

	entry({"admin", "services", "mqtt"}, cbi("mqtt"), _("BLE-MQTT"), 80).dependent = true
		
	entry({"admin", "services", "mqtt", "status"}, call("action_status")).leaf = true
end

local function is_running(name)
	return luci.sys.call("pidof %s >/dev/null" %{name}) == 0
end



function action_status()
	luci.http.prepare_content("application/json")
	luci.http.write_json({
		status = is_running("cositea_mqtt")
	})
end