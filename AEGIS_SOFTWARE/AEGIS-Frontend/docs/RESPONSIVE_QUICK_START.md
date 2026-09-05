# Responsive Design - Quick Start Guide

Quick reference for implementing responsive design in the Aegis app.

## 🚀 Quick Setup (3 Steps)

### 1. Import the Hook
```javascript
import { useResponsive } from '../utils/responsive';
import { getResponsiveTypography, getResponsiveSpacing } from '../constants/theme';
```

### 2. Use in Component
```javascript
export default function MyScreen() {
  const { colors } = useTheme();
  const responsive = useResponsive();
  const styles = getStyles(colors, responsive);
  
  return (
    <View style={styles.container}>
      {/* Your JSX */}
    </View>
  );
}
```

### 3. Update Styles
```javascript
const getStyles = (colors, responsive) => {
  const typo = getResponsiveTypography(responsive.deviceType);
  const space = getResponsiveSpacing(responsive.deviceType);
  
  return StyleSheet.create({
    container: {
      padding: space.lg,  // Was: Spacing.lg
    },
    title: {
      fontSize: typo.xl,  // Was: Typography.xl
    },
  });
};
```

## 📱 Common Patterns

### Stack on Mobile, Row on Desktop
```javascript
flexDirection: responsive.isSmallDevice ? 'column' : 'row',
```

### Conditional Sizing
```javascript
width: responsive.isDesktop ? 80 : responsive.isTablet ? 72 : 64,
```

### Full Width on Mobile
```javascript
flex: responsive.isSmallDevice ? 0 : 1,
width: responsive.isSmallDevice ? '100%' : 'auto',
```

### Center Large Content
```javascript
maxWidth: responsive.isDesktop ? 1200 : '100%',
alignSelf: 'center',
width: '100%',
```

### Responsive Icon Sizes
```javascript
const iconSize = responsive.isDesktop ? 24 : responsive.isTablet ? 20 : 18;

<Ionicons name="icon-name" size={iconSize} />
```

### Grid Columns
```javascript
import { getGridColumns } from '../utils/responsive';

const columns = getGridColumns(responsive.deviceType, {
  mobile: 1,
  tablet: 2,
  desktop: 3,
});

// Use in card width calculation
width: responsive.isDesktop ? '32%' : responsive.isTablet ? '48%' : '100%'
```

## 🎯 Available Values

### Device Type Checks
```javascript
responsive.deviceType  // 'mobile' | 'tablet' | 'desktop' | 'wide'
responsive.isMobile
responsive.isTablet
responsive.isDesktop
responsive.isWide
responsive.isSmallDevice   // < 768px
responsive.isMediumDevice  // 768-1023px
responsive.isLargeDevice   // ≥ 1024px
```

### Dimensions
```javascript
responsive.width
responsive.height
responsive.isPortrait
responsive.isLandscape
```

### Typography Sizes (scaled)
```javascript
typo.xs    // 10-12px depending on device
typo.sm    // 12-14px
typo.base  // 14-17px
typo.md    // 16-19px
typo.lg    // 18-22px
typo.xl    // 22-26px
typo.xxl   // 28-34px
```

### Spacing Values (scaled)
```javascript
space.xs   // 4-6px depending on device
space.sm   // 8-12px
space.md   // 12-17px
space.lg   // 16-24px
space.xl   // 20-30px
space.xxl  // 28-42px
```

## 🔧 Pass to Components

When using custom components:

```javascript
// In parent
<BotCard bot={bot} responsive={responsive} />

// In component
export function BotCard({ bot, responsive: responsiveProp }) {
  const responsiveHook = useResponsive();
  const responsive = responsiveProp || responsiveHook;
  
  // Use responsive...
}
```

## 📐 Breakpoints

```javascript
Mobile:  0-767px
Tablet:  768-1023px
Desktop: 1024-1439px
Wide:    1440px+
```

## 💡 Pro Tips

1. **Always scale typography and spacing** - Don't use `Typography.*` or `Spacing.*` directly in responsive components
2. **Test all device sizes** - Mobile, tablet, desktop, and orientation changes
3. **Mind touch targets** - Minimum 44x44px on mobile
4. **Constrain content width** - Use max-width on large screens (1200-1400px)
5. **Center constrained content** - `alignSelf: 'center'` with max-width

## 🐛 Common Mistakes

❌ **Don't:**
```javascript
fontSize: Typography.lg  // Fixed size, won't scale
padding: Spacing.lg      // Fixed spacing, won't scale
```

✅ **Do:**
```javascript
fontSize: typo.lg       // Scales appropriately
padding: space.lg       // Scales appropriately
```

❌ **Don't:**
```javascript
width: 64  // Fixed width on all devices
```

✅ **Do:**
```javascript
width: responsive.isDesktop ? 80 : 64  // Adapts to device
```

## 📚 See Full Documentation

For complete details, see [RESPONSIVE_DESIGN.md](./RESPONSIVE_DESIGN.md)

## 🎨 Example: Complete Component

```javascript
import React from 'react';
import { View, Text, StyleSheet, TouchableOpacity } from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { Typography, Radius, getResponsiveTypography, getResponsiveSpacing } from '../constants/theme';
import { useTheme } from '../context/ThemeContext';
import { useResponsive } from '../utils/responsive';

export default function MyCard({ title, onPress }) {
  const { colors } = useTheme();
  const responsive = useResponsive();
  const styles = getStyles(colors, responsive);
  
  const iconSize = responsive.isDesktop ? 24 : 20;

  return (
    <TouchableOpacity 
      style={styles.card} 
      onPress={onPress}
      activeOpacity={0.8}
    >
      <Ionicons name="information-circle" size={iconSize} color={colors.cyan} />
      <Text style={styles.title}>{title}</Text>
    </TouchableOpacity>
  );
}

const getStyles = (colors, responsive) => {
  const typo = getResponsiveTypography(responsive.deviceType);
  const space = getResponsiveSpacing(responsive.deviceType);

  return StyleSheet.create({
    card: {
      flexDirection: responsive.isSmallDevice ? 'column' : 'row',
      alignItems: 'center',
      gap: space.md,
      padding: space.lg,
      backgroundColor: colors.bg1,
      borderWidth: 1,
      borderColor: colors.border,
      borderRadius: Radius.md,
      maxWidth: responsive.isDesktop ? 600 : '100%',
      alignSelf: 'center',
      width: '100%',
    },
    title: {
      fontSize: typo.lg,
      fontWeight: Typography.bold,
      color: colors.textPrimary,
    },
  });
};
```

## ✅ Testing Checklist

- [ ] Mobile portrait (320-428px)
- [ ] Mobile landscape
- [ ] Tablet portrait (768-834px)
- [ ] Tablet landscape
- [ ] Desktop (1024-1920px)
- [ ] Large desktop (1920px+)
- [ ] All text is readable
- [ ] Touch targets are adequate
- [ ] Content is properly centered
- [ ] No horizontal scrolling
- [ ] Layouts don't break
