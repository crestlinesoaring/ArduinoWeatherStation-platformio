#Requires -Version 5.1
<#
.SYNOPSIS
  Retry until the weather station accepts telnet, then open an interactive session.

.DESCRIPTION
  After each upload the station listens on TCP port 23537 for a few seconds.
  This script uses a short connect timeout and fast retries to hit that window,
  then lets you type single-letter commands (M, ?, R, etc.) like Ethernet telnet.

.PARAMETER StationHost
  Station IP (default 192.168.2.227 for env tavis).

.PARAMETER Port
  Telnet port (default 23537).

.PARAMETER ConnectTimeoutMs
  Per-attempt TCP connect timeout (default 500).

.PARAMETER RetryDelayMs
  Delay between failed attempts (default 150).

.EXAMPLE
  .\scripts\telnet-snipe.ps1
  .\scripts\telnet-snipe.ps1 -StationHost 192.168.222.222
#>
[CmdletBinding()]
param(
    [string] $StationHost = "192.168.2.227",
    [int] $Port = 23537,
    [int] $ConnectTimeoutMs = 500,
    [int] $RetryDelayMs = 150
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Wait-TcpPort {
    param(
        [string] $TargetHost,
        [int] $TargetPort,
        [int] $TimeoutMs
    )
    $client = [System.Net.Sockets.TcpClient]::new()
    try {
        $task = $client.ConnectAsync($TargetHost, $TargetPort)
        if (-not $task.Wait($TimeoutMs)) {
            return $null
        }
        if (-not $client.Connected) {
            return $null
        }
        return $client
    } catch {
        return $null
    } finally {
        if ($null -eq $client -or -not $client.Connected) {
            $client.Dispose()
        }
    }
}

function Read-AvailableLines {
    param(
        [System.IO.StreamReader] $Reader,
        [System.Net.Sockets.NetworkStream] $Stream
    )
    while ($Stream.DataAvailable) {
        $line = $Reader.ReadLine()
        if ($null -eq $line) { break }
        Write-Host $line
    }
}

Write-Host "Sniping telnet on ${StationHost}:${Port} (Ctrl+C to cancel)..."
$tcp = $null
while ($null -eq $tcp) {
    $tcp = Wait-TcpPort -TargetHost $StationHost -TargetPort $Port -TimeoutMs $ConnectTimeoutMs
    if ($null -eq $tcp) {
        Write-Host "." -NoNewline
        Start-Sleep -Milliseconds $RetryDelayMs
    }
}
Write-Host ""
Write-Host "Connected. Commands are single letters + Enter (?, M, R, q). Ctrl+C to exit."
Write-Host ""

$stream = $tcp.GetStream()
$reader = [System.IO.StreamReader]::new($stream)
$writer = [System.IO.StreamWriter]::new($stream)
$writer.NewLine = "`n"
$writer.AutoFlush = $true

# Welcome banner
Start-Sleep -Milliseconds 200
Read-AvailableLines -Reader $reader -Stream $stream

try {
    while ($tcp.Connected) {
        Read-AvailableLines -Reader $reader -Stream $stream
        $line = Read-Host "cmd"
        if ([string]::IsNullOrWhiteSpace($line)) { continue }

        $writer.WriteLine($line.Trim())

        Start-Sleep -Milliseconds 150
        Read-AvailableLines -Reader $reader -Stream $stream

        if ($line.Trim().Equals("q", [System.StringComparison]::OrdinalIgnoreCase)) {
            break
        }
    }
} finally {
    $writer.Dispose()
    $reader.Dispose()
    $stream.Dispose()
    $tcp.Dispose()
    Write-Host "Disconnected."
}
