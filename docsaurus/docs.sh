#!/bin/bash
# serve_docs.sh - Build and serve documentation
set -e

cd "$(dirname "$0")"

# Use Node 20
export NVM_DIR="$HOME/.nvm"
[ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh"
nvm use 20 > /dev/null

echo "Building docs..."
npm run build

echo "Moving build to ../docs..."
rm -rf ../docs/*
cp -r build/* ../docs/
rm -rf build

echo "Starting dev server in background..."
npm run serve > /dev/null 2>&1 &
sleep 3
../notify.sh "Docs" "Documentation server started!"