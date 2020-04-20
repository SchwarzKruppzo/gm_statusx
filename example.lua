require("statusx")

hook.Add("STATUS_INFO", "status", function(ply, info)
	info.hostname = "Example"
	info.version = "20.04.2020"
	info.protocol = 24
	info.build = 7777
	info.VAC = true
	info.localIP = "255.255.255.255"
	info.gameport = 27015
	info.publicIP = "255.255.255.255"
	info.map = "gm_unknown"
	info.pos = Vector(0, 0, 0)
	info.players = 100
	info.maxplayers = 100

	return info
end)

hook.Add("STATUS_PLAYERINFO", "status", function(ply, players)
	players = {}

	for i = 1, 100 do
		players[i] = {
			userid = i,
			name = "Player",
			steamid = "STEAM:312123",
			connected = "",
			ip = "255.255.255.255",
			loss = 0,
			ping = 0,
			state = "active"
		}
	end

	return players
end)
