--LUJUNLIN modified on 2019.10.24

module("luci.controller.stp", package.seeall)

function index()
	if not nixio.fs.access("/etc/config/stp") then
		return
	end

	entry({"admin", "services", "stp"}, cbi("stp"), _("STP"), 90).dependent = true

	entry({"admin", "services", "stp", "status"}, call("action_status")).leaf = true
		
	entry({"admin", "services", "stp", "bt_action"}, call("bt_click")).leaf = true
	
	entry({"admin", "services", "stp", "param"}, call("sync_param")).leaf = true

	entry({"admin", "services", "flashops"}, call("action_flashops"), _("Stp Configuration"), 70)
	entry({"admin", "services", "flashops", "stupgrade"}, call("action_stupgrade"))
	entry({"admin", "services", "flashops", "progress"}, call("action_progress"))
	entry({"admin", "services", "flashops", "jump"}, call("action_jump"))
end

local function is_running(name)
	return luci.sys.call("pidof %s >/dev/null" %{name}) == 0
end


function action_jump()
	local http = require "luci.http"
	http.redirect(luci.dispatcher.build_url('admin/services/stp'))
end

function action_flashops()
	local http = require "luci.http"
	local step = tonumber(http.formvalue("step") or 1)
	if step == 1 then
		if(is_running("stp")) then
			--
			-- Overview
			--
			luci.template.render("stp/upgrade", {
					reset_avail   = 0
			})
			return 
		else
			luci.template.render("stp/info", {
				msg = luci.i18n.translate("Stp is not running.<br /> Please start Stp,<br />  It will automatically jump to the stp startup page after 3 seconds or click go to jump.")
			})
			return 
		end
	end
	if step == 2 then
		http.redirect(luci.dispatcher.build_url('admin/services/stp'))
		return
	end
end


function sync_param()
	if(is_running("stp")) then
		luci.sys.exec("sleep 2");
		luci.sys.exec("echo -e \"getparameters\r\" > /mnt/uwb");
		local resp;
		-- 15s listening response
		for i = 1,15 ,1 do
			resp = luci.sys.exec("ubus -t 1 listen param");
			if(#resp>100) then 
				break;
			end
		end
		luci.http.prepare_content("application/json");
		luci.http.write_json({
			data = resp
		});
	end
end

function action_progress()

	local resp;
	luci.sys.exec('echo -e \"getprogress\r\" > /mnt/uwb');
	-- 15s listening response
	for i = 1,15 ,1 do                                                        
			resp = luci.sys.exec("ubus -t 1 listen getprogress");
			if(#resp>2) then
					break;                                                  
			end
	end
	luci.http.prepare_content("application/json");
	luci.http.write_json({
            data = resp
	});
end

-- button click triggers data communication
function bt_click(btn, arg)
	local path;
	if(btn=="distance") then
			luci.sys.exec('echo -e "setdistance %s\r" > /mnt/uwb' %{arg});
			path="dis";
	elseif(btn=="mode") then
			luci.sys.exec('echo -e "setmode %s\r" > /mnt/uwb' %{arg});
			path="mode"
	elseif(btn=="role") then
			luci.sys.exec('echo -e "setrole %s\r" > /mnt/uwb' %{arg});
			path="role";
	elseif(btn=="clock") then
			luci.sys.exec('echo -e "setclocksyn %s\r" > /mnt/uwb' %{arg});
			path="clock";
	elseif(btn=="dimension") then
			luci.sys.exec('echo -e "setdimension %s\r" > /mnt/uwb' %{arg});
			path="dimension";
	elseif(btn=="panid") then
			luci.sys.exec('echo -e "setpanid %s\r" > /mnt/uwb' %{arg});
			path="panid";
	end

	local resp;                                                               
	-- 15s listening response                                                 
	for i = 1,15 ,1 do                                                        
			resp = luci.sys.exec("ubus -t 1 listen %s" %{path});              
			if(#resp>2) then                                                      
					break;                                                  
			end                                                                   
	end
	luci.http.prepare_content("application/json");
	luci.http.write_json({                                                        
            reply = resp           
    })
end

function action_status()
	--json, in status.htm use
	luci.http.prepare_content("application/json")
	luci.http.write_json({
		status = is_running("stp")
	})
end

local function storage_size()
	local size = 0
	if nixio.fs.access("/proc/mtd") then
		for l in io.lines("/proc/mtd") do
			local d, s, e, n = l:match('^([^%s]+)%s+([^%s]+)%s+([^%s]+)%s+"([^%s]+)"')
			if n == "linux" or n == "firmware" then
				size = tonumber(s, 16)
				break
			end
		end
	elseif nixio.fs.access("/proc/partitions") then
		for l in io.lines("/proc/partitions") do
			local x, y, b, n = l:match('^%s*(%d+)%s+(%d+)%s+([^%s]+)%s+([^%s]+)')
			if b and n and not n:match('[0-9]') then
				size = tonumber(b) * 1024
				break
			end
		end
	end
	return size
end

function image_supported(image)

	

end

function action_stupgrade()
	local fs = require "nixio.fs"
	local http = require "luci.http"
	local image_tmp = "/tmp/uwb_firmware.hex"

	local fp
	http.setfilehandler(
		function(meta, chunk, eof)
			if not fp and meta then
				fp = io.open(image_tmp, "w")
			end
			if fp and chunk then
				fp:write(chunk)
			end
			if fp and eof then
				fp:close()
			end
		end
	)
--	if not luci.dispatcher.test_post_security() then
--		fs.unlink(image_tmp)
--		return
--	end

	--
	-- Cancel firmware flash
	--
	if http.formvalue("cancel") then
		fs.unlink(image_tmp)
		http.redirect(luci.dispatcher.build_url('admin/services/flashops'))
		return
	end

	--
	-- Initiate firmware flash
	--
	local step = tonumber(http.formvalue("step") or 1)
	if step == 1 then
--		if image_supported(image_tmp) then
			luci.template.render("stp/upgrades", {
				checksum = 0,
				sha256ch = 0,
				storage  = storage_size(),
				size     = (fs.stat(image_tmp, "size") or 0),
				keep     = 0
			})
--		else
---			fs.unlink(image_tmp)
--			luci.template.render("admin_system/flashops", {
--				reset_avail   = 0,
--				image_invalid = true
--			})
--		end
	--
	-- Start sysupgrade flash
	--
	elseif step == 2 then
		luci.sys.exec("echo -e \"upgrade 83689216\r\" > /mnt/uwb");
		luci.template.render("stp/upgradeing", {
			title = luci.i18n.translate("upgraded..."),
			size  = (fs.stat(image_tmp, "size") or 0),
			msg   = luci.i18n.translate("The firmware is being upgraded now.<br />DO NOT POWER OFF THE DEVICE!<br />Wait a few minutes."),
		})
--		fork_exec("sleep 1; killall dropbear uhttpd; sleep 1; /sbin/sysupgrade %s %q" %{ keep, image_tmp })
--		fork_exec("sleep 1; killall dropbear uhttpd;")
		http.redirect(luci.dispatcher.build_url('admin/services/stp'))
	end
end
