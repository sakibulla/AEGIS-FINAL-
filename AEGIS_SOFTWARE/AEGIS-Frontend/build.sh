#!/bin/bash
set -e

echo "Starting Expo web build..."

# Install dependencies
echo "Installing dependencies..."
npm ci

# Export for web
echo "Exporting for web platform..."
npx expo export --platform web

echo "Build completed successfully!"
