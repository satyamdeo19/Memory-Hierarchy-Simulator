New-Item -ItemType Directory -Force -Path "frontend"
New-Item -ItemType Directory -Force -Path "frontend_test_ps"
cmd /c "npx -y create-vite@latest frontend --template react"
