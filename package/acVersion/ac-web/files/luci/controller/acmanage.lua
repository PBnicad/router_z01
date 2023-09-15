module("luci.controller.acmanage", package.seeall)

function index()
	if not nixio.fs.access("/etc/config/acmanage") then
		return
	end

	entry({"admin", "services", "acmanage"}, cbi("acmanage"), _("Access Control"), 80).dependent = true
		
	entry({"admin", "services", "acmanage", "status"}, call("action_status")).leaf = true
	
	entry({"admin", "services", "shareInfo"}, call("action_viewgw"), _("Gw Info"), 70)
end

local function is_running(name)
	return luci.sys.call("pidof %s >/dev/null" %{name}) == 0
end

local function getShareInfo()
	
	local fs = require "nixio.fs"
	local fp
	local path = "/tmp/shareInfo"
	
	fp = io.open(path, "r")
	local content = fp:read('*all')
	fp:close()
	
	return content
end


function action_status()
	luci.http.prepare_content("application/json")
	luci.http.write_json({
		status = is_running("acmanage")
	})
end

function action_viewgw()
	
	luci.template.render("acmanage/gateway_table", {
			shareinfo  = getShareInfo()
	})

end
