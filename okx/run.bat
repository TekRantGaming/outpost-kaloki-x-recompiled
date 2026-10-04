@echo off
rem Launch the recompiled game. Extra args are passed through, e.g.:
rem   run.bat --license_mask=0      (play as the trial version)
rem   run.bat --log_level=debug
cd /d "%~dp0out\build\okx-relwithdebinfo"
start "" outpost_kaloki_x.exe --game_data_root="%~dp0assets" --log_file=run.log %*
