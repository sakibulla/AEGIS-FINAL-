import React, { useState, useEffect, useRef } from 'react';
import {
  View,
  Text,
  StyleSheet,
  TouchableOpacity,
  Platform,
  Image,
  ActivityIndicator,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { Radius, Typography } from '../constants/theme';
import { useTheme } from '../context/ThemeContext';
import { getVideoSnapshotUrl } from '../services/AegisService';

/**
 * StreamViewer — Futuristic live surveillance video viewer
 * Handles MJPEG streaming, snapshot fallbacks, real-time HUD, and True Fullscreen
 */
export function StreamViewer({
  streamUrl,
  botId = 'Guardian',
  botName = 'Guardian',
  botIp = '10.75.11.110',
  visionDetections = [],
  isExpanded = false,
  onToggleExpand,
  height,
  showControls = true,
  autoReconnect = true,
  requireClickToStart = false, // NEW: Don't auto-play stream on mount
  defaultSnapshotMode = false, // NEW: Start in snapshot mode by default
}) {
  const { colors } = useTheme();
  const [streamActive, setStreamActive] = useState(false); // Always start inactive to prevent freeze
  const [streamError, setStreamError] = useState(false);
  const [loading, setLoading] = useState(false);
  const [reloadKey, setReloadKey] = useState(0);
  const [snapshotMode, setSnapshotMode] = useState(defaultSnapshotMode);
  const [snapshotTimestamp, setSnapshotTimestamp] = useState(Date.now());
  const [capturedSnapshot, setCapturedSnapshot] = useState(null);
  const [snapshotSuccess, setSnapshotSuccess] = useState(false);
  const [isFullscreen, setIsFullscreen] = useState(false);
  const [mounted, setMounted] = useState(false);
  const containerRef = useRef(null);
  const refreshIntervalRef = useRef(null);

  // Delay initial load to prevent freeze on navigation
  useEffect(() => {
    const timer = setTimeout(() => {
      setMounted(true);
      if (defaultSnapshotMode && !requireClickToStart) {
        setStreamActive(true); // Auto-start only for snapshot mode after mount
      } else if (!requireClickToStart) {
        setStreamActive(true);
      }
    }, 100); // Small delay to let page render first
    return () => clearTimeout(timer);
  }, [defaultSnapshotMode, requireClickToStart]);

  // Restart loading state when streamUrl changes
  useEffect(() => {
    setStreamError(false);
    setLoading(true);
    setReloadKey((prev) => prev + 1);
  }, [streamUrl]);

  // Snapshot fallback interval if in snapshot mode
  useEffect(() => {
    if (snapshotMode && streamActive) {
      refreshIntervalRef.current = setInterval(() => {
        setSnapshotTimestamp(Date.now());
      }, 500);
    } else if (refreshIntervalRef.current) {
      clearInterval(refreshIntervalRef.current);
      refreshIntervalRef.current = null;
    }
    return () => {
      if (refreshIntervalRef.current) {
        clearInterval(refreshIntervalRef.current);
        refreshIntervalRef.current = null;
      }
    };
  }, [snapshotMode, streamActive]);

  // Sync native HTML5 fullscreen state on Web
  useEffect(() => {
    if (Platform.OS === 'web' && typeof document !== 'undefined') {
      const handleFullscreenChange = () => {
        const isDocFs = !!(
          document.fullscreenElement ||
          document.webkitFullscreenElement ||
          document.mozFullScreenElement ||
          document.msFullscreenElement
        );
        setIsFullscreen(isDocFs);
      };

      document.addEventListener('fullscreenchange', handleFullscreenChange);
      document.addEventListener('webkitfullscreenchange', handleFullscreenChange);
      return () => {
        document.removeEventListener('fullscreenchange', handleFullscreenChange);
        document.removeEventListener('webkitfullscreenchange', handleFullscreenChange);
      };
    }
  }, []);

  const handleStartStream = () => {
    setStreamActive(true);
    setStreamError(false);
    setLoading(true);
    setReloadKey((prev) => prev + 1);
  };

  const handleStopStream = () => {
    setStreamActive(false);
    setLoading(false);
    setStreamError(false);
  };

  const handleReload = () => {
    setStreamError(false);
    setLoading(true);
    setReloadKey((prev) => prev + 1);
    setSnapshotTimestamp(Date.now());
  };

  const handleTakeSnapshot = () => {
    const url = `${getVideoSnapshotUrl(botId)}?t=${Date.now()}`;
    setCapturedSnapshot(url);
    setSnapshotSuccess(true);
    setTimeout(() => setSnapshotSuccess(false), 3000);
  };

  const handleToggleFullscreen = () => {
    if (Platform.OS === 'web' && typeof document !== 'undefined') {
      const doc = document;
      const isDocFs = !!(
        doc.fullscreenElement ||
        doc.webkitFullscreenElement ||
        doc.mozFullScreenElement
      );

      if (!isDocFs) {
        const elem = containerRef.current || doc.documentElement;
        if (elem.requestFullscreen) {
          elem.requestFullscreen().catch(() => {});
        } else if (elem.webkitRequestFullscreen) {
          elem.webkitRequestFullscreen();
        } else if (elem.mozRequestFullScreen) {
          elem.mozRequestFullScreen();
        }
        setIsFullscreen(true);
      } else {
        if (doc.exitFullscreen) {
          doc.exitFullscreen().catch(() => {});
        } else if (doc.webkitExitFullscreen) {
          doc.webkitExitFullscreen();
        }
        setIsFullscreen(false);
      }
    } else {
      setIsFullscreen((prev) => !prev);
    }

    if (onToggleExpand) {
      onToggleExpand();
    }
  };

  // Determine active threat state from detections
  const activeThreat = (visionDetections || []).some((d) => d && d.is_threat);

  const hudBorderColor = activeThreat
    ? colors.red
    : streamError
    ? colors.amber
    : colors.cyan;

  const currentSnapshotUrl = `${getVideoSnapshotUrl(botId)}?t=${snapshotTimestamp}`;

  const viewerHeight = isFullscreen
    ? '100%'
    : height || (isExpanded ? 580 : 380);

  return (
    <View
      ref={containerRef}
      style={[
        styles.container,
        isFullscreen ? styles.fullscreenContainer : { height: viewerHeight, borderColor: hudBorderColor },
      ]}
    >
      {/* ── Video Stream Content ── */}
      <View style={styles.videoSurface}>
        {!mounted ? (
          // Initial mount - show placeholder to prevent freeze
          <View style={styles.clickToStartOverlay}>
            <ActivityIndicator color={colors.cyan} size="large" />
            <Text style={styles.loadingText}>Initializing feed...</Text>
          </View>
        ) : !streamActive && !snapshotMode ? (
          // Stream not started yet - Show "Click to Start" screen
          <View style={styles.clickToStartOverlay}>
            <Ionicons name="play-circle-outline" size={64} color={colors.cyan} />
            <Text style={styles.clickToStartTitle}>Click to Start Live Stream</Text>
            <Text style={styles.clickToStartSub}>
              AI fire detection is currently running.{'\n'}
              Stream will pause AI inference while active.
            </Text>
            <TouchableOpacity style={styles.startStreamBtn} onPress={handleStartStream}>
              <Ionicons name="videocam" size={18} color="#000" />
              <Text style={styles.startStreamBtnText}>Start Video Stream</Text>
            </TouchableOpacity>
          </View>
        ) : snapshotMode ? (
          // Snapshot mode - simple image that refreshes periodically
          Platform.OS === 'web' ? (
            <img
              key={`snap-${snapshotTimestamp}`}
              src={currentSnapshotUrl}
              alt={`${botName} Snapshot`}
              style={webImgStyle}
              onLoad={() => setLoading(false)}
              onError={() => {
                setLoading(false);
                setStreamError(true);
              }}
            />
          ) : (
            <Image
              key={`snap-${snapshotTimestamp}`}
              source={{ uri: currentSnapshotUrl }}
              style={styles.nativeImage}
              onLoadStart={() => setLoading(true)}
              onLoadEnd={() => setLoading(false)}
              onError={() => {
                setLoading(false);
                setStreamError(true);
              }}
              resizeMode="contain"
            />
          )
        ) : Platform.OS === 'web' ? (
          // Stream mode - MJPEG continuous stream
          <img
            key={`stream-${reloadKey}`}
            src={streamUrl}
            alt={`${botName} Live Stream`}
            style={webImgStyle}
            onLoad={() => {
              setLoading(false);
              setStreamError(false);
            }}
            onError={() => {
              setLoading(false);
              setStreamError(true);
            }}
          />
        ) : (
          // Native stream fallback
          <Image
            key={`native-stream-${reloadKey}`}
            source={{ uri: streamUrl }}
            style={styles.nativeImage}
            onLoadStart={() => setLoading(true)}
            onLoadEnd={() => setLoading(false)}
            onError={() => {
              setLoading(false);
              setStreamError(true);
            }}
            resizeMode="contain"
          />
        )}

        {/* Loading Spinner Overlay */}
        {loading && !snapshotMode && (
          <View style={styles.loadingOverlay}>
            <ActivityIndicator color={colors.cyan} size="large" />
            <Text style={styles.loadingText}>Establishing live feed...</Text>
          </View>
        )}

        {/* Stream Error & Fallback Overlay */}
        {streamError && (
          <View style={styles.errorOverlay}>
            <Ionicons name="warning" size={36} color={colors.amber} />
            <Text style={styles.errorTitle}>Stream Connection Interrupted</Text>
            <Text style={styles.errorSub}>Target: {streamUrl}</Text>
            <View style={styles.errorButtons}>
              <TouchableOpacity style={styles.retryBtn} onPress={handleReload}>
                <Ionicons name="refresh" size={14} color="#000" />
                <Text style={styles.retryBtnText}>Retry Stream</Text>
              </TouchableOpacity>
              <TouchableOpacity
                style={styles.snapshotModeBtn}
                onPress={() => {
                  setSnapshotMode(!snapshotMode);
                  handleReload();
                }}
              >
                <Text style={styles.snapshotModeBtnText}>
                  {snapshotMode ? 'Use MJPEG Stream' : 'Switch to Snapshot Mode'}
                </Text>
              </TouchableOpacity>
            </View>
          </View>
        )}

        {/* ── Cyberpunk Reticle & HUD Corners (Clean Unblocked View) ── */}
        <View style={[styles.hudOverlay, { pointerEvents: 'none' }]}>
          {/* Top-Left Corner */}
          <View style={[styles.cornerTL, { borderColor: hudBorderColor }]} />
          {/* Top-Right Corner */}
          <View style={[styles.cornerTR, { borderColor: hudBorderColor }]} />
          {/* Bottom-Left Corner */}
          <View style={[styles.cornerBL, { borderColor: hudBorderColor }]} />
          {/* Bottom-Right Corner */}
          <View style={[styles.cornerBR, { borderColor: hudBorderColor }]} />

          {/* Center Crosshair */}
          <View style={styles.crosshair}>
            <View style={[styles.crosshairH, { backgroundColor: hudBorderColor }]} />
            <View style={[styles.crosshairV, { backgroundColor: hudBorderColor }]} />
            <View style={[styles.crosshairCircle, { borderColor: hudBorderColor }]} />
          </View>
        </View>

        {/* ── Top HUD Bar ── */}
        <View style={styles.topHudBar}>
          <View style={styles.hudBadge}>
            <View
              style={[
                styles.livePulseDot,
                {
                  backgroundColor: activeThreat
                    ? colors.red
                    : streamError
                    ? colors.amber
                    : colors.green,
                },
              ]}
            />
            <Text
              style={[
                styles.hudLiveText,
                {
                  color: activeThreat
                    ? colors.red
                    : streamError
                    ? colors.amber
                    : colors.green,
                },
              ]}
            >
              {activeThreat ? 'THREAT DETECTED' : streamError ? 'STANDBY / RETRY' : snapshotMode ? 'SNAPSHOT FEED' : '● LIVE VGA 640x480'}
            </Text>
          </View>

          <View style={styles.hudMeta}>
            <Text style={styles.hudMetaText}>CAM // {botId.toUpperCase()}</Text>
            <Text style={styles.hudMetaSub}>IP: {botIp}</Text>
          </View>
        </View>

        {/* ── Bottom HUD Bar ── */}
        <View style={styles.bottomHudBar}>
          <View style={styles.hudStatusRow}>
            {activeThreat ? (
              <View
                style={[
                  styles.aiBadge,
                  {
                    borderColor: colors.red,
                    backgroundColor: colors.redDim,
                  },
                ]}
              >
                <Ionicons name="warning" size={12} color={colors.red} />
                <Text style={[styles.aiBadgeText, { color: colors.red }]}>SECURITY ALERT</Text>
              </View>
            ) : (
              <View style={[styles.aiBadge, { borderColor: colors.border, backgroundColor: colors.bg2 }]}>
                <Ionicons name="shield-checkmark" size={12} color={colors.cyan} />
                <Text style={[styles.aiBadgeText, { color: colors.cyan }]}>AEGIS Vision Active</Text>
              </View>
            )}

            {snapshotSuccess && (
              <View style={styles.snapshotSuccessBadge}>
                <Ionicons name="checkmark-circle" size={12} color={colors.green} />
                <Text style={styles.snapshotSuccessText}>Snapshot Saved</Text>
              </View>
            )}
          </View>

          {/* Quick HUD Controls */}
          {showControls && streamActive && (
            <View style={styles.hudControls}>
              <TouchableOpacity
                style={styles.hudIconBtn}
                onPress={handleTakeSnapshot}
                title="Capture Snapshot"
              >
                <Ionicons name="camera" size={16} color={colors.textPrimary} />
              </TouchableOpacity>
              <TouchableOpacity
                style={styles.hudIconBtn}
                onPress={handleReload}
                title="Refresh Stream"
              >
                <Ionicons name="refresh" size={16} color={colors.textPrimary} />
              </TouchableOpacity>
              {requireClickToStart && (
                <TouchableOpacity
                  style={[styles.hudIconBtn, styles.stopStreamBtn]}
                  onPress={handleStopStream}
                  title="Stop Stream (Resume AI)"
                >
                  <Ionicons name="stop-circle" size={16} color={colors.red} />
                </TouchableOpacity>
              )}
              <TouchableOpacity
                style={[styles.hudIconBtn, isFullscreen && styles.hudIconBtnActive]}
                onPress={handleToggleFullscreen}
                title={isFullscreen ? 'Exit Fullscreen' : 'Fullscreen'}
              >
                <Ionicons
                  name={isFullscreen ? 'contract' : 'expand'}
                  size={16}
                  color={isFullscreen ? '#000' : colors.cyan}
                />
              </TouchableOpacity>
            </View>
          )}
        </View>
      </View>
    </View>
  );
}

const webImgStyle = {
  width: '100%',
  height: '100%',
  objectFit: 'contain',
  display: 'block',
  backgroundColor: '#070b12',
  imageRendering: 'auto',
};

const styles = StyleSheet.create({
  container: {
    width: '100%',
    backgroundColor: '#070b12',
    borderRadius: Radius.md,
    borderWidth: 1.5,
    overflow: 'hidden',
    position: 'relative',
  },
  fullscreenContainer: {
    position: 'fixed',
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    width: '100vw',
    height: '100vh',
    zIndex: 99999,
    borderRadius: 0,
    borderWidth: 0,
    backgroundColor: '#000',
  },
  videoSurface: {
    width: '100%',
    height: '100%',
    backgroundColor: '#070b12',
    position: 'relative',
    justifyContent: 'center',
    alignItems: 'center',
  },
  nativeImage: {
    width: '100%',
    height: '100%',
    backgroundColor: '#070b12',
  },
  loadingOverlay: {
    ...StyleSheet.absoluteFillObject,
    backgroundColor: 'rgba(7, 11, 18, 0.85)',
    justifyContent: 'center',
    alignItems: 'center',
    zIndex: 10,
  },
  loadingText: {
    color: '#00d2ff',
    fontSize: 12,
    marginTop: 8,
    fontWeight: 'bold',
    letterSpacing: 0.5,
  },
  errorOverlay: {
    ...StyleSheet.absoluteFillObject,
    backgroundColor: 'rgba(10, 14, 23, 0.95)',
    justifyContent: 'center',
    alignItems: 'center',
    padding: 16,
    zIndex: 15,
  },
  errorTitle: {
    color: '#ff9d00',
    fontSize: 14,
    fontWeight: 'bold',
    marginTop: 8,
    textAlign: 'center',
  },
  errorSub: {
    color: '#8ba2b9',
    fontSize: 11,
    marginTop: 4,
    marginBottom: 12,
    textAlign: 'center',
  },
  errorButtons: {
    flexDirection: 'row',
    gap: 8,
    flexWrap: 'wrap',
    justifyContent: 'center',
  },
  retryBtn: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    backgroundColor: '#00d2ff',
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: Radius.sm,
  },
  retryBtnText: {
    color: '#070b12',
    fontSize: 11,
    fontWeight: 'bold',
  },
  snapshotModeBtn: {
    backgroundColor: '#162238',
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: Radius.sm,
    borderWidth: 1,
    borderColor: '#00d2ff55',
  },
  snapshotModeBtnText: {
    color: '#00d2ff',
    fontSize: 11,
    fontWeight: 'medium',
  },
  hudOverlay: {
    ...StyleSheet.absoluteFillObject,
    zIndex: 5,
  },
  cornerTL: {
    position: 'absolute',
    top: 8,
    left: 8,
    width: 14,
    height: 14,
    borderTopWidth: 2,
    borderLeftWidth: 2,
  },
  cornerTR: {
    position: 'absolute',
    top: 8,
    right: 8,
    width: 14,
    height: 14,
    borderTopWidth: 2,
    borderRightWidth: 2,
  },
  cornerBL: {
    position: 'absolute',
    bottom: 8,
    left: 8,
    width: 14,
    height: 14,
    borderBottomWidth: 2,
    borderLeftWidth: 2,
  },
  cornerBR: {
    position: 'absolute',
    bottom: 8,
    right: 8,
    width: 14,
    height: 14,
    borderBottomWidth: 2,
    borderRightWidth: 2,
  },
  crosshair: {
    position: 'absolute',
    top: '50%',
    left: '50%',
    width: 32,
    height: 32,
    marginLeft: -16,
    marginTop: -16,
    alignItems: 'center',
    justifyContent: 'center',
    opacity: 0.35,
  },
  crosshairH: {
    position: 'absolute',
    width: 24,
    height: 1,
  },
  crosshairV: {
    position: 'absolute',
    width: 1,
    height: 24,
  },
  crosshairCircle: {
    width: 16,
    height: 16,
    borderRadius: 8,
    borderWidth: 1,
  },
  topHudBar: {
    position: 'absolute',
    top: 10,
    left: 12,
    right: 12,
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    zIndex: 20,
    backgroundColor: 'rgba(7, 11, 18, 0.65)',
    paddingHorizontal: 8,
    paddingVertical: 4,
    borderRadius: Radius.sm,
  },
  hudBadge: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
  },
  livePulseDot: {
    width: 8,
    height: 8,
    borderRadius: 4,
  },
  hudLiveText: {
    fontSize: 10,
    fontWeight: 'bold',
    letterSpacing: 0.8,
  },
  hudMeta: {
    alignItems: 'flex-end',
  },
  hudMetaText: {
    color: '#e2e8f0',
    fontSize: 10,
    fontWeight: 'bold',
    letterSpacing: 0.5,
  },
  hudMetaSub: {
    color: '#8ba2b9',
    fontSize: 9,
  },
  bottomHudBar: {
    position: 'absolute',
    bottom: 10,
    left: 12,
    right: 12,
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    zIndex: 20,
    backgroundColor: 'rgba(7, 11, 18, 0.75)',
    paddingHorizontal: 8,
    paddingVertical: 4,
    borderRadius: Radius.sm,
  },
  hudStatusRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
    flex: 1,
  },
  aiBadge: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
    paddingHorizontal: 6,
    paddingVertical: 2,
    borderRadius: Radius.sm,
    borderWidth: 1,
  },
  aiBadgeText: {
    fontSize: 9,
    fontWeight: 'bold',
  },
  snapshotSuccessBadge: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
    backgroundColor: 'rgba(0, 255, 136, 0.15)',
    paddingHorizontal: 6,
    paddingVertical: 2,
    borderRadius: Radius.sm,
  },
  snapshotSuccessText: {
    color: '#00ff88',
    fontSize: 9,
    fontWeight: 'bold',
  },
  hudControls: {
    flexDirection: 'row',
    gap: 6,
  },
  hudIconBtn: {
    padding: 5,
    backgroundColor: '#162238',
    borderRadius: Radius.sm,
    borderWidth: 1,
    borderColor: 'rgba(0, 210, 255, 0.25)',
  },
  hudIconBtnActive: {
    backgroundColor: '#00d2ff',
    borderColor: '#00d2ff',
  },
  stopStreamBtn: {
    borderColor: 'rgba(255, 64, 64, 0.4)',
  },
  clickToStartOverlay: {
    ...StyleSheet.absoluteFillObject,
    backgroundColor: 'rgba(7, 11, 18, 0.95)',
    justifyContent: 'center',
    alignItems: 'center',
    padding: 24,
    zIndex: 30,
  },
  clickToStartTitle: {
    color: '#00d2ff',
    fontSize: 18,
    fontWeight: 'bold',
    marginTop: 16,
    marginBottom: 8,
    textAlign: 'center',
  },
  clickToStartSub: {
    color: '#8ba2b9',
    fontSize: 13,
    marginBottom: 24,
    textAlign: 'center',
    lineHeight: 20,
  },
  startStreamBtn: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
    backgroundColor: '#00d2ff',
    paddingHorizontal: 24,
    paddingVertical: 12,
    borderRadius: Radius.md,
  },
  startStreamBtnText: {
    color: '#070b12',
    fontSize: 14,
    fontWeight: 'bold',
    letterSpacing: 0.5,
  },
});
