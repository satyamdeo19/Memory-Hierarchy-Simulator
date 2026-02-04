@echo off
echo Building backend... > build_status.txt
cmake --build backend/build >> build_status.txt 2>&1
echo Done >> build_status.txt
