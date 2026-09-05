# AEGIS Web Deployment Guide

## Quick Summary

Your React Native Expo app can be deployed as a static web site to **Vercel** or **Netlify**.

The build command is:
```bash
npx expo export --platform web
```

This generates a `dist/` folder with all static assets ready to deploy.

---

## Local Testing (Before Deployment)

### 1. Build the web version locally
```bash
npm run export:web
```

### 2. Preview the build
Install a static server:
```bash
npm install -g serve
```

Serve the dist folder:
```bash
serve -s dist
```

Open `http://localhost:3000` in your browser.

---

## Deployment Option A: Vercel

### Step 1: Push to GitHub
```bash
git add .
git commit -m "Add web deployment config"
git push origin main
```

### Step 2: Connect to Vercel
1. Go to [vercel.com](https://vercel.com)
2. Click **Add New Project**
3. Import your GitHub repository
4. Vercel will auto-detect `vercel.json` configuration
5. Click **Deploy**

### Step 3: Access Your App
- Vercel provides a URL like `https://aegis-app.vercel.app`
- Every push to main redeploys automatically

---

## Deployment Option B: Netlify

### Step 1: Push to GitHub
```bash
git add .
git commit -m "Add web deployment config"
git push origin main
```

### Step 2: Connect to Netlify
1. Go to [netlify.com](https://netlify.com)
2. Click **Add new site** → **Import an existing project**
3. Choose your Git provider (GitHub)
4. Select your repository
5. Netlify will auto-detect `netlify.toml` configuration
6. Click **Deploy site**

### Step 3: Access Your App
- Netlify provides a URL like `https://aegis-app-xyz.netlify.app`
- Every push to main redeploys automatically

---

## Configuration Files Explained

### `vercel.json`
```json
{
  "buildCommand": "npx expo export --platform web",
  "outputDirectory": "dist",
  "framework": "html"
}
```
- Tells Vercel how to build and where to find static files

### `netlify.toml`
```toml
[build]
  command = "npx expo export --platform web"
  publish = "dist"

[[redirects]]
  from = "/*"
  to = "/index.html"
  status = 200
```
- Tells Netlify how to build
- Redirects all routes to `index.html` (needed for React Router)

### `package.json`
Added script:
```json
"export:web": "npx expo export --platform web"
```

---

## Using the Web App in Your Mobile App (WebView)

Once deployed, you can embed it in your mobile app:

```javascript
// src/screens/WebViewScreen.js
import React from 'react';
import { WebView } from 'react-native-webview';
import { View } from 'react-native';

export default function WebViewScreen() {
  return (
    <View style={{ flex: 1 }}>
      <WebView
        source={{ uri: 'https://aegis-app.vercel.app' }}
        style={{ flex: 1 }}
      />
    </View>
  );
}
```

Then add to navigation:
```javascript
// src/navigation/AppNavigator.js
import WebViewScreen from '../screens/WebViewScreen';

const TABS = [
  // ... existing tabs
  { name: 'Dashboard', component: DashboardScreen, icon: 'grid' },
  { name: 'Web', component: WebViewScreen, icon: 'globe' },  // ← Add this
];
```

---

## Troubleshooting

### Build fails with "expo export --platform web can only be used with Webpack"
This is an old error. Make sure you're using:
- `npx expo export --platform web` (not `expo export:web`)
- Expo 52.0+ (check: `npm list expo`)

### Website is blank after deployment
- Check browser console for errors (F12)
- Verify the app has no platform-specific imports (native modules won't work on web)
- Test locally first with `serve -s dist`

### Mobile app WebView shows blank
- Verify the deployed URL is accessible in a browser
- Check that your mobile device can reach the internet (if deployed to cloud)
- For local testing, use your machine's IP address: `http://192.168.1.100:3000`

### Large bundle size
- Consider code splitting if adding more screens
- Lazy load components with React.lazy()

---

## Next Steps

1. **Test locally**: `npm run export:web` → `serve -s dist`
2. **Push to GitHub**: Commit all files
3. **Deploy**: Choose Vercel or Netlify
4. **Embed in mobile app**: Create WebViewScreen and add to navigation

---

## Links

- [Vercel Dashboard](https://vercel.com/dashboard)
- [Netlify Dashboard](https://app.netlify.com)
- [Expo Web Documentation](https://docs.expo.dev/guides/publishing-websites/)
- [React Native Web](https://necolas.github.io/react-native-web/)

---

**Last Updated**: July 2026
