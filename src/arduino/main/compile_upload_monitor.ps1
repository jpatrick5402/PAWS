param(
  [Parameter(Mandatory=$true)][string]$COMPort
)

$ErrorActionPreference = 'Stop'

Write-Host -ForegroundColor Green "-------------------------------------- Compile: started ---------------------------------------------------"
Start-Sleep -Milliseconds 300
arduino-cli.exe compile -v --fqbn esp32:esp32:esp32 main.ino --build-properties build.partitions=huge_app,upload.maximum_size=3145728 -v
if ($LASTEXITCODE -ne 0) {throw "Error Compiling"}
Write-Host -ForegroundColor Green "-------------------------------------- Compile: completed ---------------------------------------------------"
Write-Host -ForegroundColor Green "-------------------------------------- Upload: started ---------------------------------------------------"
Start-Sleep -Milliseconds 300
arduino-cli.exe upload --fqbn esp32:esp32:esp32 -p $COMPort main.ino
if ($LASTEXITCODE -ne 0) {throw "Error Uploading"}
Write-Host -ForegroundColor Green "-------------------------------------- Upload: completed ---------------------------------------------------"

$port = New-Object System.IO.Ports.SerialPort $COMPort, 115200, None, 8, one
$port.Open()

try {
    while ($port.IsOpen) {
        if ($port.BytesToRead -gt 0) {
            $data = $port.ReadExisting()
            Write-Host -NoNewline $data
        }
        Start-Sleep -Milliseconds 100
    }
}
finally {
    $port.Close()
}