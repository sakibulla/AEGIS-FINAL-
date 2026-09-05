/**
 * Image Assets Manager
 * Centralized image imports for the AEGIS app
 */

// ─── Logos ────────────────────────────────────────────────────────
export const logos = {
  // aegisLogo: require('../../assets/images/logos/aegis-logo.png'),
  // Add your logo imports here
};

// ─── Icons ────────────────────────────────────────────────────────
export const icons = {
  // customIcon: require('../../assets/images/icons/custom-icon.png'),
  // Add your custom icon imports here
};

// ─── Screenshots ──────────────────────────────────────────────────
export const screenshots = {
  // dashboard: require('../../assets/images/screenshots/dashboard.png'),
  // Add your screenshot imports here
};

// ─── Bot Images ───────────────────────────────────────────────────
export const bots = {
  // pathfinder: require('../../assets/images/bots/pathfinder.png'),
  // guardian: require('../../assets/images/bots/guardian.png'),
  // warden: require('../../assets/images/bots/warden.png'),
  // Add your bot images here
};

// ─── Miscellaneous ────────────────────────────────────────────────
export const misc = {
  // placeholder: require('../../assets/images/misc/placeholder.png'),
  // Add your miscellaneous images here
};

// ─── Helper Functions ─────────────────────────────────────────────

/**
 * Get image from assets by category and name
 * @param {string} category - Image category (logos, icons, screenshots, bots, misc)
 * @param {string} name - Image name/key
 * @returns {any} Image asset or null
 */
export const getImage = (category, name) => {
  const categories = { logos, icons, screenshots, bots, misc };
  return categories[category]?.[name] || null;
};

/**
 * Check if image exists in assets
 * @param {string} category - Image category
 * @param {string} name - Image name/key
 * @returns {boolean}
 */
export const hasImage = (category, name) => {
  return getImage(category, name) !== null;
};

// ─── Default Export ───────────────────────────────────────────────
export default {
  logos,
  icons,
  screenshots,
  bots,
  misc,
  getImage,
  hasImage,
};
