import React, { useState, useEffect, useRef } from 'react';
import { View, Text, StyleSheet, TouchableOpacity, ActivityIndicator } from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { Radius } from '../constants/theme';
import { useTheme } from '../context/ThemeContext';

/**
 * SimpleSnapshotViewer - Ultra-lightweight snapshot viewer
 * Just shows a refreshing image, nothing fancy
 */
export function SimpleSnapshotViewer({ botName, snapshotUrl, height = 300 }) {
  const { colors } = useTheme();
  const [timestamp, setTimestamp] = useState(Date.now());
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(false);
  const intervalRef = useRef(null);

  // Auto-refresh every 500ms
  useEffect(() => {
    intervalRef.current = setInterval(() => {
      setTimestamp(Date.now());
    }, 500);
    return () => {
      if (intervalRef.current) clearInterval(intervalRef.current);
    };
  }, []);

  const handleRefresh = () => {
    setTimestamp(Date.now());
    setLoading(true);
    setError(false);
  };

  const imageUrl = `${snapshotUrl}?t=${timestamp}`;

  return (
    <View style={[styles.container, { height, borderColor: error ? colors.amber : colors.cyan }]}>
      <img
        src={imageUrl}
        alt={`${botName} Snapshot`}
        style={{
          width: '100%',
          height: '100%',
          objectFit: 'contain',
          display: 'block',
          backgroundColor: '#070b12',
        }}
        onLoad={() => {
          setLoading(false);
          setError(false);
        }}
        onError={() => {
          setLoading(false);
          setError(true);
        }}
      />

      {loading && (
        <View style={styles.loadingOverlay}>
          <ActivityIndicator color={colors.cyan} size="small" />
        </View>
      )}

      {error && (
        <View style={styles.errorOverlay}>
          <Ionicons name="warning" size={24} color={colors.amber} />
          <Text style={styles.errorText}>Snapshot unavailable</Text>
          <TouchableOpacity style={styles.retryBtn} onPress={handleRefresh}>
            <Text style={styles.retryText}>Retry</Text>
          </TouchableOpacity>
        </View>
      )}

      {/* Top label */}
      <View style={styles.topBar}>
        <View style={styles.badge}>
          <View style={[styles.dot, { backgroundColor: error ? colors.amber : colors.green }]} />
          <Text style={styles.badgeText}>{error ? 'OFFLINE' : 'SNAPSHOT MODE'}</Text>
        </View>
        <Text style={styles.botLabel}>{botName}</Text>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    width: '100%',
    backgroundColor: '#070b12',
    borderRadius: Radius.md,
    borderWidth: 1.5,
    overflow: 'hidden',
    position: 'relative',
  },
  loadingOverlay: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    backgroundColor: 'rgba(7, 11, 18, 0.7)',
    justifyContent: 'center',
    alignItems: 'center',
    zIndex: 5,
  },
  errorOverlay: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    backgroundColor: 'rgba(7, 11, 18, 0.9)',
    justifyContent: 'center',
    alignItems: 'center',
    zIndex: 10,
  },
  errorText: {
    color: '#ff9d00',
    fontSize: 12,
    marginTop: 8,
  },
  retryBtn: {
    marginTop: 12,
    paddingHorizontal: 16,
    paddingVertical: 6,
    backgroundColor: '#00d2ff',
    borderRadius: Radius.sm,
  },
  retryText: {
    color: '#000',
    fontSize: 11,
    fontWeight: 'bold',
  },
  topBar: {
    position: 'absolute',
    top: 8,
    left: 8,
    right: 8,
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    backgroundColor: 'rgba(7, 11, 18, 0.7)',
    paddingHorizontal: 8,
    paddingVertical: 4,
    borderRadius: Radius.sm,
    zIndex: 3,
  },
  badge: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
  },
  dot: {
    width: 6,
    height: 6,
    borderRadius: 3,
  },
  badgeText: {
    color: '#00d2ff',
    fontSize: 9,
    fontWeight: 'bold',
    letterSpacing: 0.5,
  },
  botLabel: {
    color: '#e2e8f0',
    fontSize: 10,
    fontWeight: 'bold',
  },
});
