# Shared compile/link settings for client and server builds.
$RaylibPath = "C:/raylib/raylib"
$Make = "C:/raylib/w64devkit/bin/mingw32-make.exe"
$EnetInclude = "-Ithirdparty/enet/include"
$EnetSources = @(
    "thirdparty/enet/callbacks.c",
    "thirdparty/enet/compress.c",
    "thirdparty/enet/host.c",
    "thirdparty/enet/list.c",
    "thirdparty/enet/packet.c",
    "thirdparty/enet/peer.c",
    "thirdparty/enet/protocol.c",
    "thirdparty/enet/win32.c"
)
$ClientCpp = @(
    "main.cpp",
    "BaseCharacter.cpp",
    "Character.cpp",
    "Enemy.cpp",
    "GameSimulation.cpp",
    "NetClient.cpp",
    "NetCommon.cpp",
    "NetServer.cpp",
    "NetStreamClient.cpp",
    "NetSession.cpp",
    "Pickup.cpp",
    "Prop.cpp",
    "Thunderstrike.cpp",
    "TileMap.cpp"
)
$ServerCpp = @(
    "server_main.cpp",
    "BaseCharacter.cpp",
    "Character.cpp",
    "Enemy.cpp",
    "GameSimulation.cpp",
    "NetClient.cpp",
    "NetCommon.cpp",
    "NetServer.cpp",
    "NetStreamServer.cpp",
    "NetGameHost.cpp",
    "Pickup.cpp",
    "Prop.cpp",
    "Thunderstrike.cpp",
    "TileMap.cpp"
)
$WebCpp = @(
    "main.cpp",
    "BaseCharacter.cpp",
    "Character.cpp",
    "Enemy.cpp",
    "GameSimulation.cpp",
    "NetCommon.cpp",
    "NetWebSession.cpp",
    "Pickup.cpp",
    "Prop.cpp",
    "Thunderstrike.cpp",
    "TileMap.cpp"
)

function Get-ObjList {
    param([string[]]$CppFiles)
    $objs = $CppFiles + $EnetSources
    return ($objs -join " ")
}
Set-Variable -Name RaylibPath -Value $RaylibPath -Scope Script
Set-Variable -Name Make -Value $Make -Scope Script
Set-Variable -Name EnetInclude -Value $EnetInclude -Scope Script
Set-Variable -Name ClientObjs -Value (Get-ObjList $ClientCpp) -Scope Script
Set-Variable -Name ServerObjs -Value (Get-ObjList $ServerCpp) -Scope Script
Set-Variable -Name WebObjs -Value ($WebCpp -join " ") -Scope Script
