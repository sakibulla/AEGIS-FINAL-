import React, { useState } from 'react';
import {
  View, Text, ScrollView, StyleSheet, TouchableOpacity, Platform, Linking,
} from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { Ionicons } from '@expo/vector-icons';
import { Typography, Spacing, Radius, getResponsiveTypography, getResponsiveSpacing } from '../constants/theme';
import { useTheme } from '../context/ThemeContext';
import {
  BotCard, AlertBanner, IncidentRow, SectionLabel, MeshBadge,
} from '../components/SwarmUI';
import { useTelemetry } from '../hooks/useTelemetry';
import { useResponsive, getGridColumns } from '../utils/responsive';
import { getIncidentsPdfUrl } from '../services/AegisService';

export default function DashboardScreen({ navigation }) {
  const { colors, isDarkMode, toggleTheme } = useTheme();
  const responsive = useResponsive();

  // Call useTelemetry hook directly
  const telemetryData = useTelemetry();
  
  const {
    bots = [],
    alerts = [],
    incidents = [],
    notifications = [],
    unreadCount = 0,
    emergencyStatus = null,
    connected = false,
    error = null,
    markNotificationsRead = () => {},
    clearNotifications = () => {},
  } = telemetryData || {};

  const [showNotifications, setShowNotifications] = useState(false);
  const [showAllIncidents, setShowAllIncidents] = useState(false);

  const styles = getStyles(colors, responsive);

  const onlineCount = bots.filter(b => b.status === 'online' || b.status === 'alert').length;
  const alertCount = bots.filter(b => b.status === 'alert').length;
  const guardianBot = bots.find(b => b.id === 'guardian') || {
    id: 'guardian',
    name: 'Guardian',
    ipAddress: '192.168.0.102',
    visionDetections: [],
  };

  function handleSOS() {
    if (typeof alert === 'function') {
      alert('SOS triggered - Emergency services contacted');
    }
  }

  function handleDownloadIncidentsPdf() {
    const url = getIncidentsPdfUrl();
    // Backend already sets Content-Disposition: attachment, so on web the
    // browser saves it directly; on native, hand off to the system browser.
    if (Platform.OS === 'web') {
      window.open(url, '_blank');
    } else {
      Linking.openURL(url);
    }
  }

  return (
    <SafeAreaView style={styles.safe} edges={['top']}>

      {/* ── Top Bar ── */}
      <View style={styles.topBar}>
        <View>
          <Text style={styles.appTitle}>A.E.G.I.S.</Text>
          <View style={styles.topBarSub}>
            <MeshBadge label="ESP-NOW + Wi-Fi" />
            <Text style={styles.topBarSubText}>· {onlineCount}/3 bots active</Text>
          </View>
        </View>
        <View style={styles.topBarActions}>
          <TouchableOpacity style={styles.themeBtn} onPress={toggleTheme} activeOpacity={0.7}>
            <Ionicons name={isDarkMode ? 'moon' : 'sunny'} size={22} color={colors.textSecondary} />
          </TouchableOpacity>
          <TouchableOpacity
            style={styles.notificationBtn}
            onPress={() => {
              setShowNotifications(true);
              markNotificationsRead();
            }}
            activeOpacity={0.7}
          >
            {unreadCount > 0 && <View style={styles.notificationBadge}><Text style={styles.notificationBadgeText}>{unreadCount}</Text></View>}
            <Text style={styles.notificationBtnText}>Notifications</Text>
          </TouchableOpacity>
        </View>
      </View>

      <ScrollView
        style={styles.scroll}
        contentContainerStyle={styles.content}
        showsVerticalScrollIndicator={false}
      >
        {/* ── Live Connection Status ── */}
        <View style={styles.statusBanner}>
          <View style={styles.statusDotRow}>
            <View style={[styles.pulseDot, { backgroundColor: connected ? colors.green : colors.amber }]} />
            <Text style={styles.statusText}>{connected ? 'Live Swarm Telemetry Online (FastAPI + WebSocket)' : 'Connecting to AEGIS Central Server...'}</Text>
          </View>
          {error ? <Text style={styles.statusError}>Error: {error}</Text> : null}
        </View>

        {emergencyStatus?.triggered && (
          <AlertBanner
            alert={{
              id: 'emergency-status',
              title: emergencyStatus.mode === 'AUTOMATIC'
                ? 'Automatic Emergency Dispatch Triggered'
                : 'Manual Emergency Dispatch',
              detail: `Services notified: ${emergencyStatus.services_notified?.join(', ') || 'None'}`,
              time: emergencyStatus.timestamp || new Date().toISOString(),
              severity: 'warning',
              icon: 'warning',
            }}
          />
        )}

        {/* ── Active Alerts ── */}
        {alerts && alerts.length > 0 && (
          <View style={styles.section}>
            {alerts.map((a, index) => <AlertBanner key={a.id || `alert-${index}`} alert={a} />)}
          </View>
        )}

        {/* ── Swarm Stat Cards ── */}
        <View style={styles.section}>
          <View style={styles.statRow}>
            <StatCard label="Bots online" value={`${onlineCount}/3`} color={colors.green} icon="hardware-chip" responsive={responsive} />
            <StatCard label="Active alerts" value={alertCount} color={colors.amber} icon="warning" responsive={responsive} />
            <StatCard label="Map coverage" value="78%" color={colors.cyan} icon="map" responsive={responsive} />
          </View>
        </View>

        {/* ── Bot Cards ── */}
        <View style={styles.section}>
          <SectionLabel action="Full status →" onAction={() => navigation.navigate('Status')}>Swarm Status</SectionLabel>
          <View style={styles.botGrid}>
            {bots && bots.map((bot, index) => (
              <BotCard
                key={bot.id || `bot-${index}`}
                bot={bot}
                onPress={() => navigation.navigate('Status', { botId: bot.id })}
                responsive={responsive}
              />
            ))}
          </View>
        </View>

        {/* ── Incident Log ── */}
        <View style={styles.section}>
          <SectionLabel action="All incidents →" onAction={() => setShowAllIncidents(true)}>Recent incidents</SectionLabel>
          <TouchableOpacity style={styles.downloadPdfBtn} onPress={handleDownloadIncidentsPdf} activeOpacity={0.8}>
            <Ionicons name="download-outline" size={16} color={colors.green} />
            <Text style={styles.downloadPdfBtnText}>Download Incident Report (PDF)</Text>
          </TouchableOpacity>
          <View style={styles.incidentList}>
            {incidents && incidents.slice(0, 4).map((inc, index) => (
              <IncidentRow key={inc.id || `incident-${index}`} incident={inc} />
            ))}
          </View>
        </View>

        {/* ── SOS ── */}
        <View style={styles.section}>
          <TouchableOpacity style={styles.sosBtn} onPress={handleSOS} activeOpacity={0.8}>
            <Ionicons name="call" size={18} color={colors.red} />
            <Text style={styles.sosBtnText}>Emergency Override — Contact Services</Text>
          </TouchableOpacity>
        </View>
      </ScrollView>
      {showNotifications && (
        <View style={styles.notificationsOverlay}>
          <View style={styles.notificationsPanel}>
            <View style={styles.notificationsHeader}>
              <Text style={styles.notificationsTitle}>System Alerts</Text>
              <View style={styles.notificationsHeaderActions}>
                <TouchableOpacity onPress={clearNotifications}>
                  <Text style={styles.notificationsClear}>Clear all</Text>
                </TouchableOpacity>
                <TouchableOpacity onPress={() => setShowNotifications(false)}>
                  <Text style={styles.notificationsClose}>Close</Text>
                </TouchableOpacity>
              </View>
            </View>
            <ScrollView style={styles.notificationsList}>
              {notifications.length > 0 ? notifications.map((notif) => (
                <View key={notif.id} style={styles.notificationItem}>
                  <Text style={styles.notificationItemTitle}>{notif.title}</Text>
                  <Text style={styles.notificationItemDetail}>{notif.detail}</Text>
                  <Text style={styles.notificationItemTime}>{notif.time}</Text>
                </View>
              )) : (
                <Text style={styles.notificationEmpty}>No recent alerts.</Text>
              )}
            </ScrollView>
          </View>
        </View>
      )}
      {showAllIncidents && (
        <View style={styles.notificationsOverlay}>
          <View style={styles.notificationsPanel}>
            <View style={styles.notificationsHeader}>
              <Text style={styles.notificationsTitle}>All Incidents</Text>
              <View style={styles.notificationsHeaderActions}>
                <TouchableOpacity onPress={() => setShowAllIncidents(false)}>
                  <Text style={styles.notificationsClose}>Close</Text>
                </TouchableOpacity>
              </View>
            </View>
            <ScrollView style={styles.notificationsList}>
              {incidents.length > 0 ? (
                <View style={styles.incidentList}>
                  {incidents.map((inc, index) => (
                    <IncidentRow key={inc.id || `incident-${index}`} incident={inc} />
                  ))}
                </View>
              ) : (
                <Text style={styles.notificationEmpty}>No incidents recorded.</Text>
              )}
            </ScrollView>
          </View>
        </View>
      )}
    </SafeAreaView>
  );
}

// ─── Stat Card ────────────────────────────────────────────────────

function StatCard({ label, value, color, icon, responsive }) {
  const { colors } = useTheme();
  const styles = getStyles(colors, responsive);
  const iconSize = responsive.isDesktop ? 20 : responsive.isTablet ? 18 : 16;

  return (
    <View style={styles.statCard}>
      <Ionicons name={icon} size={iconSize} color={color} style={{ marginBottom: 4 }} />
      <Text style={[styles.statValue, { color }]}>{value}</Text>
      <Text style={styles.statLabel}>{label}</Text>
    </View>
  );
}

// ─── Styles ──────────────────────────────────────────────────────

const getStyles = (colors, responsive) => {
  const typo = getResponsiveTypography(responsive.deviceType);
  const space = getResponsiveSpacing(responsive.deviceType);
  const columns = getGridColumns(responsive.deviceType, { mobile: 1, tablet: 2, desktop: 3 });

  return StyleSheet.create({
    safe: { flex: 1, backgroundColor: colors.bg0 },
    scroll: { flex: 1 },
    content: {
      paddingBottom: space.xxl,
      maxWidth: responsive.isDesktop ? 1400 : '100%',
      alignSelf: 'center',
      width: '100%',
    },

    topBar: {
      flexDirection: responsive.isSmallDevice ? 'column' : 'row',
      justifyContent: 'space-between',
      alignItems: responsive.isSmallDevice ? 'flex-start' : 'center',
      paddingHorizontal: space.lg,
      paddingVertical: space.md,
      borderBottomWidth: 0.5,
      borderBottomColor: colors.border,
      gap: responsive.isSmallDevice ? space.sm : 0,
    },
    appTitle: { fontSize: typo.xl, fontWeight: Typography.bold, color: colors.textPrimary, letterSpacing: 1 },
    topBarSub: { flexDirection: 'row', alignItems: 'center', gap: 6, marginTop: 3 },
    topBarSubText: { fontSize: typo.xs, color: colors.textMuted },
    topBarActions: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: space.sm,
      alignSelf: responsive.isSmallDevice ? 'flex-end' : 'center',
    },
    themeBtn: { padding: space.xs },
    notificationBtn: {
      paddingHorizontal: space.md,
      paddingVertical: space.sm,
      backgroundColor: colors.bg1,
      borderRadius: Radius.md,
      borderWidth: 1,
      borderColor: colors.border,
      position: 'relative',
    },
    notificationBtnText: {
      fontSize: typo.sm,
      fontWeight: Typography.medium,
      color: colors.textPrimary,
    },
    notificationBadge: {
      position: 'absolute',
      top: -6,
      right: -6,
      backgroundColor: colors.amber,
      borderRadius: Radius.full,
      minWidth: 18,
      height: 18,
      alignItems: 'center',
      justifyContent: 'center',
      paddingHorizontal: 5,
      zIndex: 1,
      borderWidth: 2,
      borderColor: colors.bg0,
    },
    notificationBadgeText: {
      fontSize: 10,
      color: '#000',
      fontWeight: Typography.bold
    },
    notificationsOverlay: {
      position: 'absolute',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0, 0, 0, 0.45)',
      justifyContent: 'center',
      alignItems: 'center',
      zIndex: 999,
      elevation: 20,
    },
    notificationsPanel: {
      width: '90%',
      maxWidth: 520,
      maxHeight: '80%',
      backgroundColor: colors.bg0,
      borderRadius: Radius.lg,
      borderWidth: 1,
      borderColor: colors.border,
      overflow: 'hidden',
      elevation: 24,
      boxShadow: '0px 8px 16px rgba(0, 0, 0, 0.2)',
    },
    notificationsHeader: {
      flexDirection: 'row',
      justifyContent: 'space-between',
      alignItems: 'center',
      padding: space.md,
      backgroundColor: colors.bg1,
      borderBottomWidth: 1,
      borderBottomColor: colors.border,
    },
    notificationsHeaderActions: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: 12,
    },
    notificationsTitle: {
      fontSize: typo.sm,
      fontWeight: Typography.bold,
      color: colors.textPrimary,
    },
    notificationsClear: {
      fontSize: typo.xs,
      color: colors.red,
      fontWeight: Typography.bold,
    },
    notificationsClose: {
      fontSize: typo.xs,
      color: colors.cyan,
      fontWeight: Typography.bold,
    },
    notificationsList: {
      padding: space.md,
      paddingBottom: space.xl,
      backgroundColor: colors.bg0,
    },
    notificationItem: {
      marginBottom: space.md,
      padding: space.md,
      borderRadius: Radius.md,
      backgroundColor: colors.bg1,
      borderWidth: 1,
      borderColor: colors.border,
    },
    notificationItemTitle: {
      fontSize: typo.sm,
      fontWeight: Typography.bold,
      color: colors.textPrimary,
      marginBottom: 4,
    },
    notificationItemDetail: {
      fontSize: typo.xs,
      color: colors.textSecondary,
      marginBottom: 6,
      lineHeight: typo.xs * 1.4,
    },
    notificationItemTime: {
      fontSize: typo.xxSmall || typo.xs,
      color: colors.textMuted,
    },
    notificationEmpty: {
      fontSize: typo.sm,
      color: colors.textSecondary,
      textAlign: 'center',
      marginTop: space.md,
    },

    section: { paddingHorizontal: space.lg, paddingTop: space.xl },

    statRow: {
      flexDirection: responsive.isSmallDevice ? 'column' : 'row',
      gap: space.sm,
      width: '100%',
    },
    statCard: {
      flex: responsive.isSmallDevice ? undefined : 1,
      width: responsive.isSmallDevice ? '100%' : undefined,
      minWidth: responsive.isSmallDevice ? undefined : 0,
      backgroundColor: colors.bg1,
      borderWidth: 0.5,
      borderColor: colors.border,
      borderRadius: Radius.md,
      padding: responsive.isDesktop ? space.lg : space.md,
      alignItems: 'center',
    },
    statValue: { fontSize: typo.lg, fontWeight: Typography.bold },
    statLabel: { fontSize: typo.xs, color: colors.textMuted, textAlign: 'center', marginTop: 2 },

    botGrid: {
      flexDirection: 'row',
      flexWrap: 'wrap',
      gap: space.sm,
    },

    incidentList: { gap: space.sm },

    downloadPdfBtn: {
      flexDirection: 'row',
      alignItems: 'center',
      justifyContent: 'center',
      gap: 6,
      alignSelf: 'flex-start',
      marginTop: space.sm,
      marginBottom: space.md,
      paddingHorizontal: space.md,
      paddingVertical: space.sm,
      backgroundColor: colors.bg1,
      borderWidth: 1,
      borderColor: colors.green + '55',
      borderRadius: Radius.md,
    },
    downloadPdfBtnText: {
      fontSize: typo.xs,
      fontWeight: Typography.bold,
      color: colors.green,
    },

    statusBanner: {
      paddingHorizontal: space.lg,
      paddingVertical: space.sm,
      backgroundColor: colors.bg1,
      borderBottomWidth: 0.5,
      borderBottomColor: colors.border,
    },
    statusDotRow: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: 8,
    },
    pulseDot: {
      width: 8,
      height: 8,
      borderRadius: 4,
    },
    statusText: {
      fontSize: typo.xs,
      fontWeight: Typography.medium,
      color: colors.textPrimary,
    },
    statusError: {
      marginTop: space.xs,
      fontSize: typo.xs,
      color: colors.red,
    },

    feedPreviewContainer: {
      borderRadius: Radius.md,
      overflow: 'hidden',
      borderWidth: 1,
      borderColor: colors.border,
    },

    sosBtn: {
      flexDirection: 'row',
      alignItems: 'center',
      justifyContent: 'center',
      gap: space.sm,
      paddingVertical: responsive.isDesktop ? space.lg : space.md,
      backgroundColor: colors.redDim,
      borderWidth: 0.5,
      borderColor: colors.red + '55',
      borderRadius: Radius.lg,
    },
    sosBtnText: { fontSize: typo.base, fontWeight: Typography.bold, color: colors.red },
  });
};
