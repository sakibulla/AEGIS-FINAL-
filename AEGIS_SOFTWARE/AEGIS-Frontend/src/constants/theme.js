// AEGIS Design System
// Dark tactical UI — deep navy base, cyan/amber status accents

const darkMode = {
  // Backgrounds
  bg: '#060d18',       // deepest — screen bg
  bg0: '#060d18',       // deepest — screen bg
  bg1: '#0a1525',       // card bg
  bg2: '#0f1e34',       // elevated card
  bg3: '#162844',       // input / tag bg
  card: '#0a1525',
  surface: '#0f1e34',

  // Brand
  cyan: '#3dd8ff',
  cyanDim: '#1a6a85',
  cyanFaint: '#0a2a38',

  // Status
  green: '#3dd68c',
  greenDim: '#0d2e1a',
  amber: '#f5a524',
  amberDim: '#2a1800',
  red: '#f87171',
  redDim: '#2a0808',

  // Bots
  pathfinder: '#3dd68c',   // green
  guardian: '#3dd8ff',   // cyan
  warden: '#f5a524',   // amber

  // Text
  textPrimary: '#e8f2ff',
  textSecondary: '#6a8aaa',
  textMuted: '#3a5070',

  // Border
  border: '#1a2e48',
  borderStrong: '#2a4060',
};

const lightMode = {
  // Backgrounds
  bg: '#f8f9fa',
  bg0: '#f8f9fa',       // deepest — screen bg
  bg1: '#ffffff',       // card bg
  bg2: '#f5f7fa',       // elevated card
  bg3: '#e8ecf1',       // input / tag bg
  card: '#ffffff',
  surface: '#f5f7fa',

  // Brand
  cyan: '#0066cc',
  cyanDim: '#4d94ff',
  cyanFaint: '#e6f2ff',

  // Status
  green: '#00a651',
  greenDim: '#d4f4e1',
  amber: '#cc7700',
  amberDim: '#ffe6cc',
  red: '#d32f2f',
  redDim: '#ffebee',

  // Bots
  pathfinder: '#00a651',   // green
  guardian: '#0066cc',   // cyan
  warden: '#cc7700',   // amber

  // Text
  textPrimary: '#1a1a1a',
  textSecondary: '#4a5568',
  textMuted: '#8b95a5',

  // Border
  border: '#d0d8e0',
  borderStrong: '#b8c5d6',
};

export const Colors = darkMode;
export const ColorSchemes = {
  dark: darkMode,
  light: lightMode,
};

export const Typography = {
  // Font sizes (base sizes - can be scaled with scaleFont)
  xs: 10,
  sm: 12,
  base: 14,
  md: 16,
  lg: 18,
  xl: 22,
  xxl: 28,

  // Weights
  regular: '400',
  medium: '500',
  bold: '600',
};

// Responsive typography - returns font sizes based on device type
export const getResponsiveTypography = (deviceType) => {
  const scale = {
    mobile: 1,
    tablet: 1.1,
    desktop: 1.15,
    wide: 1.2,
  }[deviceType] || 1;

  return {
    xs: Math.round(10 * scale),
    sm: Math.round(12 * scale),
    base: Math.round(14 * scale),
    md: Math.round(16 * scale),
    lg: Math.round(18 * scale),
    xl: Math.round(22 * scale),
    xxl: Math.round(28 * scale),
    regular: '400',
    medium: '500',
    bold: '600',
  };
};

export const Spacing = {
  xs: 4,
  sm: 8,
  md: 12,
  lg: 16,
  xl: 20,
  xxl: 28,
};

// Responsive spacing - returns spacing values based on device type
export const getResponsiveSpacing = (deviceType) => {
  const scale = {
    mobile: 1,
    tablet: 1.2,
    desktop: 1.4,
    wide: 1.5,
  }[deviceType] || 1;

  return {
    xs: Math.round(4 * scale),
    sm: Math.round(8 * scale),
    md: Math.round(12 * scale),
    lg: Math.round(16 * scale),
    xl: Math.round(20 * scale),
    xxl: Math.round(28 * scale),
  };
};

export const Radius = {
  sm: 6,
  md: 10,
  lg: 14,
  xl: 20,
  full: 999,
};

export const Shadows = {
  card: {
    boxShadow: '0px 2px 8px rgba(0, 0, 0, 0.4)',
    elevation: 4,
  },
};
