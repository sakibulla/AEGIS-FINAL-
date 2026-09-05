# Responsive Design Implementation

This document describes the responsive design system implemented in the Aegis app, making it fully responsive across mobile, tablet, and desktop devices.

## Overview

The Aegis app now features a comprehensive responsive design system that automatically adapts the UI for different screen sizes and orientations. The implementation ensures optimal user experience across:

- **Mobile** (< 768px width) - Phones in portrait and landscape
- **Tablet** (768px - 1023px) - iPads and Android tablets
- **Desktop** (1024px - 1439px) - Laptops and smaller monitors
- **Wide** (≥ 1440px) - Large desktop monitors

## Architecture

### Core Utilities (`src/utils/responsive.js`)

The responsive system is built around a central utilities file that provides:

#### Breakpoints
```javascript
export const BREAKPOINTS = {
  mobile: 0,
  tablet: 768,
  desktop: 1024,
  wide: 1440,
};
```

#### Main Hook: `useResponsive()`
Returns comprehensive responsive information:

```javascript
const responsive = useResponsive();
// Returns:
{
  width,           // Current window width
  height,          // Current window height
  deviceType,      // 'mobile' | 'tablet' | 'desktop' | 'wide'
  isMobile,        // Boolean
  isTablet,        // Boolean
  isDesktop,       // Boolean
  isWide,          // Boolean
  isPortrait,      // Boolean
  isLandscape,     // Boolean
  isSmallDevice,   // width < 768
  isMediumDevice,  // 768 ≤ width < 1024
  isLargeDevice,   // width ≥ 1024
}
```

#### Helper Functions

- **`scaleFont(size, deviceType)`** - Scales font sizes appropriately
- **`scaleSpacing(size, deviceType)`** - Scales spacing values
- **`getGridColumns(deviceType, options)`** - Returns optimal column count
- **`responsive(values, deviceType)`** - Selects value based on device
- **`wp(percentage)` / `hp(percentage)`** - Width/height percentages
- **`getContainerPadding(deviceType)`** - Returns optimal padding
- **`getMaxContentWidth(deviceType)`** - Returns max content width

### Theme Enhancements (`src/constants/theme.js`)

#### Responsive Typography
```javascript
export const getResponsiveTypography = (deviceType) => {
  // Returns scaled font sizes based on device type
  // Mobile: 1x, Tablet: 1.1x, Desktop: 1.15x, Wide: 1.2x
}
```

#### Responsive Spacing
```javascript
export const getResponsiveSpacing = (deviceType) => {
  // Returns scaled spacing values based on device type
  // Mobile: 1x, Tablet: 1.2x, Desktop: 1.4x, Wide: 1.5x
}
```

## Implementation Patterns

### Screen Components

All screen components follow this pattern:

```javascript
import { useResponsive, getGridColumns } from '../utils/responsive';
import { getResponsiveTypography, getResponsiveSpacing } from '../constants/theme';

export default function MyScreen() {
  const { colors } = useTheme();
  const responsive = useResponsive();
  const styles = getStyles(colors, responsive);

  return (
    // JSX using responsive styles
  );
}

const getStyles = (colors, responsive) => {
  const typo = getResponsiveTypography(responsive.deviceType);
  const space = getResponsiveSpacing(responsive.deviceType);

  return StyleSheet.create({
    container: {
      padding: space.lg,
      maxWidth: responsive.isDesktop ? 1200 : '100%',
      alignSelf: 'center',
    },
    title: {
      fontSize: typo.xl,
      fontWeight: Typography.bold,
    },
    // Responsive layout
    row: {
      flexDirection: responsive.isSmallDevice ? 'column' : 'row',
      gap: space.sm,
    },
  });
};
```

### Component Responsive Props

Components receive responsive information via props:

```javascript
<BotCard 
  bot={bot} 
  responsive={responsive}
  onPress={handlePress} 
/>
```

Inside the component:
```javascript
export function BotCard({ bot, responsive: responsiveProp }) {
  const responsiveHook = useResponsive();
  const responsive = responsiveProp || responsiveHook;
  
  const iconSize = responsive.isDesktop ? 22 : responsive.isTablet ? 20 : 18;
  // ...
}
```

## Responsive Features by Screen

### Dashboard Screen
- **Top Bar**: Stacks vertically on mobile, horizontal on larger screens
- **Stat Cards**: Column layout on mobile, row layout on tablet+
- **Bot Grid**: 1 column (mobile), 2 columns (tablet), 3 columns (desktop)
- **Mode Buttons**: Stack vertically on mobile
- **Max Width**: 1400px on large screens for optimal readability

### Control Screen
- **Header**: Stacks elements vertically on mobile
- **Bot Selector**: Full width buttons on mobile
- **Mode Toggle**: Vertical stack on mobile, horizontal on tablet+
- **D-Pad**: 64px (mobile), 72px (tablet), 80px (desktop) buttons
- **Command Grid**: 2 columns with responsive widths
- **Speed Controls**: Larger touch targets on desktop
- **Max Width**: 1200px on large screens

### Feeds Screen
- **Feed Cards**: Centered with max-width on desktop (900px)
- **Feed Height**: 250px (mobile), 320px (tablet), 400px (desktop)
- **Header**: Elements stack on mobile
- **Modal**: 100% width (mobile), 400px (tablet), 500px (desktop)
- **Control Buttons**: Wrap on mobile, row layout on larger screens

### Map Screen
- **Header**: Stacks vertically on mobile
- **Map Placeholder**: 300px (mobile), 400px (tablet), 500px (desktop) height
- **Bot Buttons**: Larger touch targets on desktop
- **Stat Cards**: Column layout on mobile, row layout on tablet+
- **Max Width**: 1200px on large screens

### SwarmUI Components
- **BotCard**: Width adjusts: 100% (mobile), 48% (tablet), 32% (desktop)
- **AlertBanner**: Vertical layout on mobile, horizontal on larger screens
- **All Components**: Scaled font sizes and spacing based on device

## Scaling Factors

### Font Scaling
- Mobile: 1.0x (base sizes)
- Tablet: 1.1x
- Desktop: 1.15x
- Wide: 1.2x

### Spacing Scaling
- Mobile: 1.0x (base sizes)
- Tablet: 1.2x
- Desktop: 1.4x
- Wide: 1.5x

### Icon Scaling
Icons scale contextually:
- Small icons: +2px per tier (16 → 18 → 20)
- Medium icons: +2-4px per tier (20 → 24 → 26)
- Large icons: +4-8px per tier (48 → 56 → 64)

## Content Width Constraints

Large screens use maximum content widths to prevent excessive line lengths:

- **Dashboard**: 1400px max
- **Control**: 1200px max
- **Feeds**: 900px per feed card
- **Map**: 1200px max

All content is horizontally centered on large screens.

## Orientation Support

The system automatically detects and responds to orientation changes:

```javascript
const { isPortrait, isLandscape } = useResponsive();
```

Components re-render and adjust layouts when orientation changes.

## Grid Layouts

Grid columns are automatically calculated:

```javascript
const columns = getGridColumns(responsive.deviceType, {
  mobile: 1,
  tablet: 2,
  desktop: 3,
  wide: 4
});
```

## Touch Target Sizes

Touch targets scale appropriately:
- Mobile: Minimum 44x44px (iOS), 48x48px (Android)
- Tablet: 48-56px
- Desktop: Can be smaller as mouse precision is higher

## Best Practices

### 1. Always Use Responsive Hook
```javascript
const responsive = useResponsive();
```

### 2. Pass to getStyles
```javascript
const styles = getStyles(colors, responsive);
```

### 3. Use Scaled Typography and Spacing
```javascript
const typo = getResponsiveTypography(responsive.deviceType);
const space = getResponsiveSpacing(responsive.deviceType);
```

### 4. Conditional Layouts
```javascript
flexDirection: responsive.isSmallDevice ? 'column' : 'row'
```

### 5. Max Width for Large Screens
```javascript
maxWidth: responsive.isDesktop ? 1200 : '100%',
alignSelf: 'center',
```

### 6. Pass Responsive to Components
```javascript
<MyComponent responsive={responsive} />
```

## Testing

Test the responsive design on:

1. **Mobile Phones**
   - iPhone SE (375px)
   - iPhone 12/13 (390px)
   - iPhone 14 Pro Max (430px)
   - Android phones (360-412px)

2. **Tablets**
   - iPad Mini (744px)
   - iPad (768px)
   - iPad Pro 11" (834px)

3. **Desktop**
   - Laptop (1024-1440px)
   - Desktop (1440-1920px)
   - Large monitors (1920px+)

4. **Orientations**
   - Portrait and landscape on all devices

## Platform Differences

The system works seamlessly across:
- **React Native** (iOS/Android apps)
- **React Native Web** (web browsers)

The `Dimensions` API automatically detects the correct platform and provides appropriate values.

## Performance

The responsive system is optimized for performance:

- Hook subscriptions are cleaned up properly
- Styles are memoized via `StyleSheet.create()`
- Re-renders only occur on actual dimension changes
- No unnecessary calculations on every render

## Future Enhancements

Potential improvements:

1. **Custom Breakpoints**: Allow per-screen custom breakpoints
2. **Responsive Images**: Different image sources for different devices
3. **Font Variants**: Different font families for different sizes
4. **Accessibility Scaling**: Support for user-preferred text sizes
5. **Responsive Animations**: Different animation durations based on device

## Migration Guide

To make an existing component responsive:

1. Import responsive utilities:
```javascript
import { useResponsive } from '../utils/responsive';
import { getResponsiveTypography, getResponsiveSpacing } from '../constants/theme';
```

2. Use hook in component:
```javascript
const responsive = useResponsive();
const styles = getStyles(colors, responsive);
```

3. Update getStyles function:
```javascript
const getStyles = (colors, responsive) => {
  const typo = getResponsiveTypography(responsive.deviceType);
  const space = getResponsiveSpacing(responsive.deviceType);
  
  return StyleSheet.create({
    // Use typo.* instead of Typography.*
    // Use space.* instead of Spacing.*
    // Add responsive conditionals where needed
  });
};
```

4. Update any hardcoded dimensions to use responsive values

5. Test on all device sizes

## Support

For questions or issues with responsive design:
1. Check this documentation
2. Review the example implementations in existing screens
3. Test on actual devices or browser dev tools
4. Ensure `Dimensions` event listeners are properly cleaned up
