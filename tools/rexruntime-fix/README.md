# Clean rexruntime.dll (antivirus false-positive fix)

The official ReXGlue SDK v0.10.0 Windows `rexruntime.dll`
(SHA-256 `e359209fb2b0570e693c966d4c1d99a82465d36ef70d033833fae56adb2f1b7a`) is flagged by many antivirus
engines, including Microsoft Defender (`Trojan:Win32/Wacatac.B!ml`). It is a false positive, reported upstream as
[rexglue/rexglue-sdk#485](https://github.com/rexglue/rexglue-sdk/issues/485), but Defender blocks or quarantines it.

`rexruntime.dll` here is built from the **same v0.10.0 source** (tag commit f5337cdc), unmodified.
SHA-256 `e87c3555602c41b18579ef8932639f7afc9324b0ca448f6e7608010d10edbbc3`.
[VirusTotal](https://www.virustotal.com/gui/file/e87c3555602c41b18579ef8932639f7afc9324b0ca448f6e7608010d10edbbc3)

`setup.ps1` checks both hashes and swaps it into the downloaded SDK (keeping the official one as
`rexruntime.dll.official-v0.10.0`), so builds copy it next to the game. It exports every function the game and
the official `rexgpu-xenos.dll` import, so nothing else changes. `rexgpu-xenos.dll` stays the official one.
