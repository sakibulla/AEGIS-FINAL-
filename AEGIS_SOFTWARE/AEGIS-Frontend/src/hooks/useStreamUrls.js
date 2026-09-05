import { useState, useEffect, useCallback } from 'react';
import AsyncStorage from '@react-native-async-storage/async-storage';
import {
  getBackendBaseUrl,
  getVideoStreamUrl,
  getVideoSnapshotUrl,
} from '../services/AegisService';

const STORAGE_KEY = 'aegis_stream_urls';
const STORAGE_MODES_KEY = 'aegis_stream_modes';
const STORAGE_IPS_KEY = 'aegis_esp32_ips';

const DEFAULT_BOT_IPS = {
  guardian: '10.75.11.110',
  pathfinder: '10.75.11.115',
  warden: '10.75.11.50',
};

/**
 * Custom hook for managing video stream sources, modes (backend proxy vs direct ESP32),
 * and URLs for each robot.
 */
export const useStreamUrls = () => {
  const [urls, setUrlsState] = useState({});
  const [streamModes, setStreamModesState] = useState({
    guardian: 'proxy',
    pathfinder: 'proxy',
    warden: 'proxy',
  });
  const [esp32Ips, setEsp32IpsState] = useState(DEFAULT_BOT_IPS);
  const [loading, setLoading] = useState(true);

  // Generate fallback default stream URL for a bot
  const getDefaultUrlFor = useCallback((botId, mode = 'proxy', ip = null) => {
    const normId = (botId || 'guardian').toLowerCase();
    const capId = normId.charAt(0).toUpperCase() + normId.slice(1);

    if (mode === 'direct') {
      const targetIp = ip || esp32Ips[normId] || DEFAULT_BOT_IPS[normId] || '10.75.11.110';
      return `http://${targetIp}:80/stream`;
    }

    if (mode === 'snapshot') {
      return getVideoSnapshotUrl(capId);
    }

    // Default: Backend live MJPEG stream proxy
    return getVideoStreamUrl(capId);
  }, [esp32Ips]);

  // Load URLs and settings from AsyncStorage on mount
  useEffect(() => {
    const loadSettings = async () => {
      try {
        const [storedUrls, storedModes, storedIps] = await Promise.all([
          AsyncStorage.getItem(STORAGE_KEY),
          AsyncStorage.getItem(STORAGE_MODES_KEY),
          AsyncStorage.getItem(STORAGE_IPS_KEY),
        ]);

        const parsedModes = storedModes ? JSON.parse(storedModes) : { guardian: 'proxy', pathfinder: 'proxy', warden: 'proxy' };
        const parsedIps = storedIps ? JSON.parse(storedIps) : DEFAULT_BOT_IPS;

        setStreamModesState(parsedModes);
        setEsp32IpsState(parsedIps);

        let initialUrls = {};
        if (storedUrls) {
          try {
            initialUrls = JSON.parse(storedUrls) || {};
          } catch (e) {
            initialUrls = {};
          }
        }

        // Fill defaults for bots that have no explicit custom URL or broken / YouTube URLs
        ['guardian', 'pathfinder', 'warden'].forEach((bId) => {
          const val = initialUrls[bId];
          const isInvalid =
            !val ||
            typeof val !== 'string' ||
            val.includes('youtube.com') ||
            val.includes('youtu.be') ||
            !val.startsWith('http');

          if (isInvalid) {
            initialUrls[bId] = getDefaultUrlFor(bId, parsedModes[bId] || 'proxy', parsedIps[bId]);
          }
        });

        setUrlsState(initialUrls);
        setLoading(false);
      } catch (error) {
        console.error('[AEGIS] Error loading stream URLs:', error);
        // Fallback defaults
        setUrlsState({
          guardian: getVideoStreamUrl('Guardian'),
          pathfinder: getVideoStreamUrl('Pathfinder'),
          warden: getVideoStreamUrl('Warden'),
        });
        setLoading(false);
      }
    };
    loadSettings();
  }, [getDefaultUrlFor]);

  /**
   * Set custom stream URL for a specific bot
   */
  const setUrl = useCallback(async (botId, url) => {
    const normId = (botId || '').toLowerCase();
    try {
      const updated = { ...urls, [normId]: url };
      setUrlsState(updated);
      await AsyncStorage.setItem(STORAGE_KEY, JSON.stringify(updated));
    } catch (error) {
      console.error(`[AEGIS] Error setting URL for ${botId}:`, error);
      throw error;
    }
  }, [urls]);

  /**
   * Set streaming mode for a specific bot ('proxy' | 'direct' | 'snapshot')
   */
  const setStreamMode = useCallback(async (botId, mode) => {
    const normId = (botId || '').toLowerCase();
    try {
      const updatedModes = { ...streamModes, [normId]: mode };
      setStreamModesState(updatedModes);
      await AsyncStorage.setItem(STORAGE_MODES_KEY, JSON.stringify(updatedModes));

      const newUrl = getDefaultUrlFor(normId, mode, esp32Ips[normId]);
      await setUrl(normId, newUrl);
    } catch (error) {
      console.error(`[AEGIS] Error setting stream mode for ${botId}:`, error);
    }
  }, [esp32Ips, getDefaultUrlFor, setUrl, streamModes]);

  /**
   * Update known hardware IP for a bot
   */
  const setBotIp = useCallback(async (botId, ip) => {
    const normId = (botId || '').toLowerCase();
    const cleanIp = String(ip || '').trim();
    if (!cleanIp) return;

    try {
      const updatedIps = { ...esp32Ips, [normId]: cleanIp };
      setEsp32IpsState(updatedIps);
      await AsyncStorage.setItem(STORAGE_IPS_KEY, JSON.stringify(updatedIps));

      if (streamModes[normId] === 'direct') {
        const directUrl = `http://${cleanIp}:80/stream`;
        await setUrl(normId, directUrl);
      }
    } catch (error) {
      console.error(`[AEGIS] Error setting bot IP for ${botId}:`, error);
    }
  }, [esp32Ips, setUrl, streamModes]);

  /**
   * Reset a bot's URL back to default backend MJPEG proxy
   */
  const resetToDefault = useCallback(async (botId) => {
    const normId = (botId || '').toLowerCase();
    const defaultUrl = getVideoStreamUrl(normId.charAt(0).toUpperCase() + normId.slice(1));
    await setUrl(normId, defaultUrl);
    await setStreamMode(normId, 'proxy');
  }, [getVideoStreamUrl, setStreamMode, setUrl]);

  /**
   * Reset all bots to default backend MJPEG proxy streams
   */
  const resetAllToDefaults = useCallback(async () => {
    try {
      await AsyncStorage.multiRemove([STORAGE_KEY, STORAGE_MODES_KEY]);
      const defaultModes = { guardian: 'proxy', pathfinder: 'proxy', warden: 'proxy' };
      const defaultUrls = {
        guardian: getVideoStreamUrl('Guardian'),
        pathfinder: getVideoStreamUrl('Pathfinder'),
        warden: getVideoStreamUrl('Warden'),
      };
      setStreamModesState(defaultModes);
      setUrlsState(defaultUrls);
    } catch (e) {
      console.warn('[AEGIS] Error resetting stream URLs:', e);
    }
  }, [getVideoStreamUrl]);

  /**
   * Helper: Convert video_feed URL to detections URL if available
   */
  const detectionsUrlFor = useCallback((streamUrl) => {
    if (!streamUrl) return null;
    return `${getBackendBaseUrl()}/api/v1/video/status`;
  }, []);

  return {
    urls,
    streamModes,
    esp32Ips,
    setUrl,
    setStreamMode,
    setBotIp,
    resetToDefault,
    resetAllToDefaults,
    loading,
    getDefaultUrlFor,
    detectionsUrlFor,
  };
};

