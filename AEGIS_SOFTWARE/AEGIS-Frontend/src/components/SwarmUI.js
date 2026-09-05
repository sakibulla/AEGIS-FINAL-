import React, { useEffect, useRef } from 'react';
import {
  View, Text, StyleSheet, Animated, TouchableOpacity, Pressable, Platform,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { Typography, Spacing, Radius, getResponsiveTypography, getResponsiveSpacing } from '../constants/theme';
import { useTheme } from '../context/ThemeContext';
import { useResponsive } from '../utils/responsive';

// ─── StatusPip ────────────────────────────────────────────────────

export function StatusPip({ status }) {
  const pulse = useRef(new Animated.Value(1)).current;
  const { colors } = useTheme();

  useEffect(() => {
    if (status === 'alert') {
      Animated.loop(
        Animated.sequence([
          Animated.timing(pulse, { toValue: 0.2, duration: 700, useNativeDriver: Platform.OS !== 'web' }),
          Animated.timing(pulse, { toValue: 1,   duration: 700, useNativeDriver: Platform.OS !== 'web' }),
        ])
      ).start();
    }
  }, [status]);

  const config = {
    online:  { bg: colors.greenDim, color: colors.green,  label: 'ONLINE' },
    alert:   { bg: colors.amberDim, color: colors.amber,  label: 'ALERT'  },
    offline: { bg: colors.bg3,      color: colors.textMuted, label: 'OFFLINE' },
  }[status] || { bg: colors.bg3, color: colors.textMuted, label: '—' };

  return (
    <View style={[styles(colors).pip, { backgroundColor: config.bg }]}>
      <Animated.View style={[styles(colors).pipDot, { backgroundColor: config.color, opacity: status === 'alert' ? pulse : 1 }]} />
      <Text style={[styles(colors).pipLabel, { color: config.color }]}>{config.label}</Text>
    </View>
  );
}

// ─── BotCard ──────────────────────────────────────────────────────

export function BotCard({ bot, onPress, responsive: responsiveProp }) {
  const { colors } = useTheme();
  const responsiveHook = useResponsive();
  const responsive = responsiveProp || responsiveHook;

  const borderColor = bot.status === 'online' ? colors.greenDim
    : bot.status === 'alert' ? colors.amberDim
    : colors.border;

  const iconSize = responsive.isDesktop ? 22 : responsive.isTablet ? 20 : 18;
  const cardWidth = responsive.isDesktop ? '32%' : responsive.isTablet ? '48%' : '100%';

  return (
    <Pressable
      onPress={onPress}
      style={({ pressed }) => [
        styles(colors, responsive).botCard, 
        { borderColor, opacity: pressed ? 0.85 : 1, width: cardWidth }
      ]}
    >
      <View style={styles(colors, responsive).botCardHeader}>
        <View style={[styles(colors, responsive).botIcon, { backgroundColor: bot.color + '22' }]}>
          <Ionicons name={iconMap[bot.icon] || 'hardware-chip'} size={iconSize} color={bot.color} />
        </View>
        <StatusPip status={bot.status} />
      </View>
      <Text style={styles(colors, responsive).botName}>{bot.name}</Text>
      <Text style={styles(colors, responsive).botRole}>{bot.role}</Text>
      <View style={styles(colors, responsive).botStatRow}>
        <Ionicons name="battery-half" size={12} color={colors.textMuted} />
        <Text style={styles(colors, responsive).botStat}>{bot.battery}%</Text>
        {bot.mapCoverage != null && (
          <>
            <Text style={styles(colors, responsive).botStatSep}>·</Text>
            <Ionicons name="map" size={12} color={colors.textMuted} />
            <Text style={styles(colors, responsive).botStat}>{bot.mapCoverage}% mapped</Text>
          </>
        )}
      </View>
      <View style={styles(colors, responsive).botTaskRow}>
        {bot.tasks.slice(0, 1).map((t, i) => (
          <Text key={i} style={styles(colors, responsive).botTask} numberOfLines={1}>{t}</Text>
        ))}
      </View>
    </Pressable>
  );
}

// ─── AlertBanner ─────────────────────────────────────────────────

export function AlertBanner({ alert }) {
  const pulse = useRef(new Animated.Value(1)).current;
  const { colors } = useTheme();

  useEffect(() => {
    Animated.loop(
      Animated.sequence([
        Animated.timing(pulse, { toValue: 0.2, duration: 800, useNativeDriver: Platform.OS !== 'web' }),
        Animated.timing(pulse, { toValue: 1,   duration: 800, useNativeDriver: Platform.OS !== 'web' }),
      ])
    ).start();
  }, []);

  const isWarning = alert.severity === 'warning';
  const color = isWarning ? colors.amber : colors.red;
  const bg    = isWarning ? colors.amberDim : colors.redDim;

  return (
    <View style={[styles(colors).alertBanner, { backgroundColor: bg, borderColor: color + '55' }]}>
      <Animated.View style={[styles(colors).alertDot, { backgroundColor: color, opacity: pulse }]} />
      <View style={styles(colors).alertBody}>
        <Text style={[styles(colors).alertTitle, { color }]}>{alert.title}</Text>
        <Text style={styles(colors).alertDetail}>{alert.detail}</Text>
      </View>
      <Text style={[styles(colors).alertTime, { color: color + '99' }]}>{alert.time}</Text>
    </View>
  );
}

// ─── IncidentRow ─────────────────────────────────────────────────

export function IncidentRow({ incident }) {
  const { colors } = useTheme();

  const cfg = {
    warning: { color: colors.amber, bg: colors.amberDim },
    success: { color: colors.green, bg: colors.greenDim },
    info:    { color: colors.cyan,  bg: colors.cyanFaint },
  }[incident.severity] || { color: colors.textSecondary, bg: colors.bg2 };

  return (
    <View style={styles(colors).incidentRow}>
      <View style={[styles(colors).incIcon, { backgroundColor: cfg.bg }]}>
        <Ionicons name={iconMap[incident.icon] || 'information-circle'} size={14} color={cfg.color} />
      </View>
      <View style={styles(colors).incBody}>
        <Text style={styles(colors).incTitle}>{incident.title}</Text>
        <Text style={styles(colors).incSub}>{incident.sub}</Text>
      </View>
      <Text style={styles(colors).incTime}>{incident.time}</Text>
    </View>
  );
}

// ─── SectionLabel ────────────────────────────────────────────────

export function SectionLabel({ children, action, onAction }) {
  const { colors } = useTheme();

  return (
    <View style={styles(colors).sectionLabelRow}>
      <Text style={styles(colors).sectionLabel}>{children}</Text>
      {action && (
        <TouchableOpacity onPress={onAction}>
          <Text style={[styles(colors).sectionAction, { color: colors.cyan }]}>{action}</Text>
        </TouchableOpacity>
      )}
    </View>
  );
}

// ─── MeshBadge ───────────────────────────────────────────────────

export function MeshBadge({ label = 'ESP-NOW', active = true }) {
  const { colors } = useTheme();

  return (
    <View style={[styles(colors).meshBadge, { borderColor: active ? colors.cyanDim : colors.border }]}>
      <View style={[styles(colors).meshDot, { backgroundColor: active ? colors.green : colors.textMuted }]} />
      <Text style={[styles(colors).meshLabel, { color: active ? colors.cyan : colors.textMuted }]}>{label}</Text>
    </View>
  );
}

// ─── BotDetailPanel ──────────────────────────────────────────────

export function BotDetailPanel({ bot }) {
  const { colors } = useTheme();
  const responsive = useResponsive();

  if (!bot) return null;

  const formatBytes = (bytes) => {
    if (bytes >= 1000000) return `${(bytes / 1000000).toFixed(1)}MB`;
    if (bytes >= 1000) return `${(bytes / 1000).toFixed(0)}KB`;
    return `${bytes}B`;
  };

  return (
    <View style={styles(colors, responsive).detailPanel}>
      <View style={styles(colors, responsive).detailHeader}>
        <View style={[styles(colors, responsive).botIcon, { backgroundColor: bot.color + '22' }]}>
          <Ionicons name={iconMap[bot.icon] || 'hardware-chip'} size={24} color={bot.color} />
        </View>
        <View style={styles(colors, responsive).detailHeaderText}>
          <Text style={styles(colors, responsive).detailBotName}>{bot.name}</Text>
          <Text style={styles(colors, responsive).detailBotRole}>{bot.role}</Text>
        </View>
        <StatusPip status={bot.status} />
      </View>

      {/* System Info Grid */}
      <View style={styles(colors, responsive).detailGrid}>
        <View style={styles(colors, responsive).detailItem}>
          <Ionicons name="hardware-chip-outline" size={16} color={colors.cyan} />
          <Text style={styles(colors, responsive).detailLabel}>IP Address</Text>
          <Text style={styles(colors, responsive).detailValue}>{bot.ipAddress || 'N/A'}</Text>
        </View>

        <View style={styles(colors, responsive).detailItem}>
          <Ionicons name="battery-half" size={16} color={colors.green} />
          <Text style={styles(colors, responsive).detailLabel}>Battery</Text>
          <Text style={styles(colors, responsive).detailValue}>{bot.battery}%</Text>
        </View>

        <View style={styles(colors, responsive).detailItem}>
          <Ionicons name="wifi" size={16} color={colors.cyan} />
          <Text style={styles(colors, responsive).detailLabel}>WiFi RSSI</Text>
          <Text style={styles(colors, responsive).detailValue}>{bot.wifiRssi} dBm</Text>
        </View>

        <View style={styles(colors, responsive).detailItem}>
          <Ionicons name="albums-outline" size={16} color={colors.amber} />
          <Text style={styles(colors, responsive).detailLabel}>Free Heap</Text>
          <Text style={styles(colors, responsive).detailValue}>{formatBytes(bot.freeHeap)}</Text>
        </View>

        {bot.psram > 0 && (
          <View style={styles(colors, responsive).detailItem}>
            <Ionicons name="server-outline" size={16} color={colors.green} />
            <Text style={styles(colors, responsive).detailLabel}>PSRAM</Text>
            <Text style={styles(colors, responsive).detailValue}>{formatBytes(bot.psram)}</Text>
          </View>
        )}
      </View>

      {/* Bot-specific data */}
      {bot.id === 'warden' && bot.detectionState && (
        <View style={styles(colors, responsive).detailSection}>
          <Text style={styles(colors, responsive).detailSectionTitle}>Fire Detection State</Text>
          <View style={styles(colors, responsive).detailGrid}>
            <View style={[styles(colors, responsive).detailItem, bot.detectionState.fire_streak > 0 && { backgroundColor: colors.redDim, borderColor: colors.red }]}>
              <Ionicons name="flame" size={16} color={bot.detectionState.fire_streak > 0 ? colors.red : colors.textSecondary} />
              <Text style={styles(colors, responsive).detailLabel}>Fire Streak</Text>
              <Text style={[styles(colors, responsive).detailValue, bot.detectionState.fire_streak > 0 && { color: colors.red, fontWeight: Typography.bold }]}>
                {bot.detectionState.fire_streak} frames
              </Text>
            </View>

            <View style={[styles(colors, responsive).detailItem, bot.detectionState.smoke_streak > 0 && { backgroundColor: colors.amberDim, borderColor: colors.amber }]}>
              <Ionicons name="cloud-outline" size={16} color={bot.detectionState.smoke_streak > 0 ? colors.amber : colors.textSecondary} />
              <Text style={styles(colors, responsive).detailLabel}>Smoke Streak</Text>
              <Text style={[styles(colors, responsive).detailValue, bot.detectionState.smoke_streak > 0 && { color: colors.amber, fontWeight: Typography.bold }]}>
                {bot.detectionState.smoke_streak} frames
              </Text>
            </View>

            <View style={[styles(colors, responsive).detailItem, bot.detectionState.clear_streak > 5 && { backgroundColor: colors.greenDim, borderColor: colors.green }]}>
              <Ionicons name="checkmark-circle-outline" size={16} color={bot.detectionState.clear_streak > 5 ? colors.green : colors.textSecondary} />
              <Text style={styles(colors, responsive).detailLabel}>Clear Streak</Text>
              <Text style={[styles(colors, responsive).detailValue, bot.detectionState.clear_streak > 5 && { color: colors.green, fontWeight: Typography.bold }]}>
                {bot.detectionState.clear_streak} frames
              </Text>
            </View>
          </View>
        </View>
      )}

      {/* Tasks */}
      {bot.tasks && bot.tasks.length > 0 && (
        <View style={styles(colors, responsive).detailSection}>
          <Text style={styles(colors, responsive).detailSectionTitle}>Active Tasks</Text>
          {bot.tasks.map((task, idx) => (
            <View key={idx} style={styles(colors, responsive).detailTaskRow}>
              <Ionicons name="chevron-forward" size={12} color={colors.cyan} />
              <Text style={styles(colors, responsive).detailTaskText}>{task}</Text>
            </View>
          ))}
        </View>
      )}
    </View>
  );
}

// ─── Icon map (Ionicons names for each AEGIS icon key) ─────────────

const iconMap = {
  'map-search':    'map',
  'shield-check':  'shield-checkmark',
  'flame':         'flame',
  'alert-triangle':'warning',
  'first-aid-kit': 'medkit',
  'map-2':         'map',
  'eye':           'eye',
  'camera':        'videocam',
  'chart-bar':     'bar-chart',
};

// ─── Styles ──────────────────────────────────────────────────────

const styles = (colors, responsive) => {
  const resp = responsive || { deviceType: 'mobile', isDesktop: false, isTablet: false };
  const typo = getResponsiveTypography(resp.deviceType);
  const space = getResponsiveSpacing(resp.deviceType);

  return StyleSheet.create({
    pip: { flexDirection: 'row', alignItems: 'center', gap: 4, paddingHorizontal: 7, paddingVertical: 3, borderRadius: Radius.full },
    pipDot: { width: 5, height: 5, borderRadius: Radius.full },
    pipLabel: { fontSize: typo.xs, fontWeight: Typography.bold, letterSpacing: 0.5 },

    botCard: {
      backgroundColor: colors.bg1,
      borderWidth: 0.5,
      borderRadius: Radius.lg,
      padding: resp.isDesktop ? space.lg : space.md,
      minWidth: resp.isSmallDevice ? '100%' : '30%',
    },
    botCardHeader: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: space.sm },
    botIcon: { 
      width: resp.isDesktop ? 38 : 34, 
      height: resp.isDesktop ? 38 : 34, 
      borderRadius: Radius.md, 
      alignItems: 'center', 
      justifyContent: 'center' 
    },
    botName: { fontSize: typo.base, fontWeight: Typography.bold, color: colors.textPrimary, marginBottom: 2 },
    botRole: { fontSize: typo.xs, color: colors.textSecondary, marginBottom: space.sm },
    botStatRow: { flexDirection: 'row', alignItems: 'center', gap: 4 },
    botStat: { fontSize: typo.xs, color: colors.textMuted },
    botStatSep: { fontSize: typo.xs, color: colors.textMuted },
    botTaskRow: { marginTop: 6 },
    botTask: { fontSize: 10, color: colors.textSecondary },

    alertBanner: {
      flexDirection: resp.isSmallDevice ? 'column' : 'row',
      alignItems: resp.isSmallDevice ? 'flex-start' : 'center',
      gap: space.sm,
      padding: space.md,
      borderRadius: Radius.lg,
      borderWidth: 0.5,
    },
    alertDot: { width: 8, height: 8, borderRadius: Radius.full },
    alertBody: { flex: 1 },
    alertTitle: { fontSize: typo.sm, fontWeight: Typography.bold },
    alertDetail: { fontSize: typo.xs, color: colors.textSecondary, marginTop: 1 },
    alertTime: { fontSize: typo.xs },

    incidentRow: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: space.sm,
      backgroundColor: colors.bg1,
      borderWidth: 0.5,
      borderColor: colors.border,
      borderRadius: Radius.md,
      padding: space.md,
    },
    incIcon: { width: 30, height: 30, borderRadius: Radius.sm, alignItems: 'center', justifyContent: 'center' },
    incBody: { flex: 1 },
    incTitle: { fontSize: typo.sm, fontWeight: Typography.medium, color: colors.textPrimary },
    incSub: { fontSize: typo.xs, color: colors.textSecondary, marginTop: 1 },
    incTime: { fontSize: typo.xs, color: colors.textMuted },

    sectionLabelRow: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', marginBottom: space.sm },
    sectionLabel: { fontSize: typo.xs, fontWeight: Typography.bold, color: colors.textMuted, letterSpacing: 0.8, textTransform: 'uppercase' },
    sectionAction: { fontSize: typo.xs },

    meshBadge: { flexDirection: 'row', alignItems: 'center', gap: 5, paddingHorizontal: 9, paddingVertical: 3, borderRadius: Radius.full, borderWidth: 0.5 },
    meshDot: { width: 5, height: 5, borderRadius: Radius.full },
    meshLabel: { fontSize: typo.xs, fontWeight: Typography.bold, letterSpacing: 0.4 },

    // Bot Detail Panel
    detailPanel: {
      backgroundColor: colors.bg1,
      borderRadius: Radius.lg,
      borderWidth: 1,
      borderColor: colors.border,
      padding: space.lg,
      gap: space.lg,
    },
    detailHeader: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: space.md,
      paddingBottom: space.md,
      borderBottomWidth: 1,
      borderBottomColor: colors.border,
    },
    detailHeaderText: { flex: 1 },
    detailBotName: { fontSize: typo.lg, fontWeight: Typography.bold, color: colors.textPrimary },
    detailBotRole: { fontSize: typo.sm, color: colors.textSecondary, marginTop: 2 },
    detailGrid: {
      flexDirection: 'row',
      flexWrap: 'wrap',
      gap: space.sm,
    },
    detailItem: {
      flex: resp.isSmallDevice ? undefined : 1,
      minWidth: resp.isSmallDevice ? '100%' : resp.isTablet ? '48%' : '30%',
      backgroundColor: colors.bg2,
      borderRadius: Radius.md,
      borderWidth: 1,
      borderColor: colors.border,
      padding: space.md,
      gap: 4,
    },
    detailLabel: { fontSize: typo.xs, color: colors.textMuted, textTransform: 'uppercase', letterSpacing: 0.5 },
    detailValue: { fontSize: typo.base, fontWeight: Typography.bold, color: colors.textPrimary },
    detailSection: { gap: space.sm },
    detailSectionTitle: {
      fontSize: typo.sm,
      fontWeight: Typography.bold,
      color: colors.textPrimary,
      marginBottom: space.xs,
      textTransform: 'uppercase',
      letterSpacing: 0.5,
    },
    detailTaskRow: { flexDirection: 'row', alignItems: 'center', gap: space.xs, paddingVertical: 4 },
    detailTaskText: { fontSize: typo.sm, color: colors.textSecondary, flex: 1 },
  });
};
