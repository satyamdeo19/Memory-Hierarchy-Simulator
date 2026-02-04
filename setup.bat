@echo off
echo Starting setup > setup_log.txt
mkdir manual_test_dir
cmake -S backend -B backend/build >> setup_log.txt 2>&1
echo CMake done >> setup_log.txt
call npx -y create-vite@latest frontend --template react >> setup_log.txt 2>&1
echo Frontend setup done >> setup_log.txt
