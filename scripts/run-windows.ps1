# Execute na pasta raspberry-tv-isolada ou por caminho absoluto.
$project = Split-Path -Parent $PSScriptRoot
Set-Location $project
if (Get-Command py -ErrorAction SilentlyContinue) {
    py -3 .\server.py --host 127.0.0.1 --port 8090
} else {
    python .\server.py --host 127.0.0.1 --port 8090
}
