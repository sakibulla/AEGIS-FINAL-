import { Dimensions, PixelRatio, Platform } from 'react-native';
import { useState, useEffect } from 'react';

// Get screen dimensions
const getScreenDimensions = () => {
  const { width, height } = Dimensions.get('window');
  return { width, height };
};

// Breakpoints (following common standards)
export const BREAKPOINTS = {
  mobile: 0,
  tablet: 768,
  desktop: 1024,
  wide: 1440,
};

// Determine device type based on width
export const getDeviceType = (width) => {
  if (width >= BREAKPOINTS.wide) return 'wide';
  if (width >= BREAKPOINTS.desktop) return 'desktop';
  if (width >= BREAKPOINTS.tablet) return 'tablet';
  return 'mobile';
};

// Hook to get current device info and handle orientation changes
export const useResponsive = () => {
  const [dimensions, setDimensions] = useState(getScreenDimensions());

  useEffect(() => {
    const subscription = Dimensions.addEventListener('change', ({ window }) => {
      setDimensions({ width: window.width, height: window.height });
    });

    return () => subscription?.remove();
  }, []);

  const { width, height } = dimensions;
  const deviceType = getDeviceType(width);
  const isPortrait = height > width;
  const isLandscape = width > height;

  return {
    width,
    height,
    deviceType,
    isMobile: deviceType === 'mobile',
    isTablet: deviceType === 'tablet',
    isDesktop: deviceType === 'desktop' || deviceType === 'wide',
    isWide: deviceType === 'wide',
    isPortrait,
    isLandscape,
    // Helpers
    isSmallDevice: width < BREAKPOINTS.tablet,
    isMediumDevice: width >= BREAKPOINTS.tablet && width < BREAKPOINTS.desktop,
    isLargeDevice: width >= BREAKPOINTS.desktop,
  };
};

// Responsive font size scaling
export const scaleFont = (size, deviceType) => {
  const scale = {
    mobile: 1,
    tablet: 1.1,
    desktop: 1.15,
    wide: 1.2,
  }[deviceType] || 1;

  const newSize = size * scale;
  return Math.round(PixelRatio.roundToNearestPixel(newSize));
};

// Responsive spacing scaling
export const scaleSpacing = (size, deviceType) => {
  const scale = {
    mobile: 1,
    tablet: 1.2,
    desktop: 1.4,
    wide: 1.5,
  }[deviceType] || 1;

  return Math.round(size * scale);
};

// Get number of columns for grids based on device
export const getGridColumns = (deviceType, options = {}) => {
  const { mobile = 1, tablet = 2, desktop = 3, wide = 4 } = options;
  
  return {
    mobile,
    tablet,
    desktop,
    wide,
  }[deviceType] || mobile;
};

// Calculate responsive width for grid items
export const getGridItemWidth = (containerWidth, columns, gap = 0) => {
  return (containerWidth - gap * (columns - 1)) / columns;
};

// Responsive value selector - choose value based on device type
export const responsive = (values, deviceType) => {
  if (typeof values === 'object' && !Array.isArray(values)) {
    return values[deviceType] || values.mobile || values.default;
  }
  return values;
};

// Platform-specific responsive (useful for web vs native differences)
export const platformResponsive = (webValue, nativeValue) => {
  return Platform.OS === 'web' ? webValue : nativeValue;
};

// Check if width is within a range
export const isWithinRange = (width, min, max = Infinity) => {
  return width >= min && width < max;
};

// Get orientation
export const getOrientation = () => {
  const { width, height } = getScreenDimensions();
  return height > width ? 'portrait' : 'landscape';
};

// Hook for orientation changes
export const useOrientation = () => {
  const [orientation, setOrientation] = useState(getOrientation());

  useEffect(() => {
    const subscription = Dimensions.addEventListener('change', () => {
      setOrientation(getOrientation());
    });

    return () => subscription?.remove();
  }, []);

  return orientation;
};

// Get container padding based on device type
export const getContainerPadding = (deviceType) => {
  return {
    mobile: 16,
    tablet: 24,
    desktop: 32,
    wide: 40,
  }[deviceType] || 16;
};

// Get maximum content width for large screens (prevents content from being too wide)
export const getMaxContentWidth = (deviceType) => {
  return {
    mobile: '100%',
    tablet: '100%',
    desktop: 1200,
    wide: 1400,
  }[deviceType] || '100%';
};

// Calculate responsive dimensions
export const wp = (percentage) => {
  const { width } = getScreenDimensions();
  return (percentage * width) / 100;
};

export const hp = (percentage) => {
  const { height } = getScreenDimensions();
  return (percentage * height) / 100;
};
