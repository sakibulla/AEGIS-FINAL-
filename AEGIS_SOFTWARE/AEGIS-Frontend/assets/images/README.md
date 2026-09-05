# Images Folder

This folder contains image assets for the AEGIS app.

## Structure

```
assets/images/
├── logos/          - App logos and branding
├── icons/          - Custom icons and graphics
├── screenshots/    - App screenshots and demos
├── bots/           - Robot/bot related images
└── misc/           - Other miscellaneous images
```

## Supported Formats

- PNG (recommended for transparency)
- JPG/JPEG (for photos)
- SVG (vector graphics)
- GIF (animations)
- WebP (modern format)

## Usage in Code

### Import and use images:

```javascript
import { Image } from 'react-native';

// Local image
const logo = require('../../assets/images/logos/aegis-logo.png');

<Image source={logo} style={{ width: 100, height: 100 }} />
```

### Using with Expo Asset:

```javascript
import { Asset } from 'expo-asset';

const image = Asset.fromModule(require('../../assets/images/logos/aegis-logo.png'));
```

### Remote images:

```javascript
<Image 
  source={{ uri: 'https://example.com/image.png' }} 
  style={{ width: 100, height: 100 }}
/>
```

## Image Optimization Tips

1. Use appropriate dimensions (don't use oversized images)
2. Compress images before adding them
3. Use @2x and @3x variants for retina displays
4. Consider using WebP format for better compression

## Naming Convention

- Use lowercase with hyphens: `aegis-logo.png`
- Include size variants: `icon-48.png`, `icon-96.png`
- Be descriptive: `bot-pathfinder-front.png`

## Upload Instructions

1. Place images in the appropriate subfolder
2. Follow the naming convention
3. Update this README if adding new categories
4. Ensure images are optimized before uploading
