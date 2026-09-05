import React, { useState } from 'react';
import {
  View, Text, StyleSheet, TouchableOpacity, ScrollView,
} from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { Ionicons } from '@expo/vector-icons';
import { Typography, Radius, getResponsiveTypography, getResponsiveSpacing } from '../constants/theme';
import { useTheme } from '../context/ThemeContext';
import { StatusPip } from '../components/SwarmUI';
import { useTelemetry } from '../hooks/useTelemetry';
import { useResponsive } from '../utils/responsive';

export default function ControlScreen({ route }) {
  const { colors } = useTheme();
  const responsive = useResponsive();
  const {
    bots,
    connected,
    error,
    triggerManualEmergency,
  } = useTelemetry();

  const paramBotId = route?.params?.botId ? String(route.params.botId).toLowerCase() : null;
  const activeBot = bots.find((b) => b.status === 'online' || b.status === 'alert') || bots.find((b) => b.id === 'pathfinder') || bots[0];
  const [selectedBot, setSelectedBot] = useState(paramBotId || (activeBot ? activeBot.id : 'pathfinder'));

  React.useEffect(() => {
    if (route?.params?.botId) {
      setSelectedBot(String(route.params.botId).toLowerCase());
    }
  }, [route?.params?.botId]);

  const styles = getStyles(colors, responsive);
  const bot = bots.find((b) => b.id === selectedBot) || activeBot || bots[0] || {
    id: 'pathfinder',
    name: 'Pathfinder',
    status: 'online',
    battery: 98,
    role: 'Explorer · SLAM',
  };

  const isBotActive = bot.status === 'online' || bot.status === 'alert';

  const handleManualSOS = async () => {
    try {
      await triggerManualEmergency('sos');
      alert('Manual SOS dispatch transmitted to AEGIS backend.');
    } catch (err) {
      alert('Error triggering emergency call.');
    }
  };

  return (
    <SafeAreaView style={styles.safe} edges={['top']}>
      <View style={styles.header}>
        <View>
          <Text style={styles.headerTitle}>Swarm Diagnostics & Telemetry</Text>
          <Text style={styles.headerSubtitle}>Real-time ESP32-S3 Hardware Health Monitor</Text>
        </View>
        <View style={[
          styles.statusBannerTop,
          { backgroundColor: connected ? colors.greenDim : colors.amberDim },
        ]}
        >
          <Ionicons name="pulse" size={14} color={connected ? colors.green : colors.amber} />
          <Text style={[styles.statusPillText, { color: connected ? colors.green : colors.amber }]}>
            {connected ? 'FastAPI Telemetry Live' : 'Connecting to Server...'}
          </Text>
        </View>
      </View>
      {error ? <Text style={styles.statusError}>Telemetry error: {error}</Text> : null}

      <ScrollView style={styles.scroll} contentContainerStyle={styles.content} showsVerticalScrollIndicator={false}>
        {/* Bot Selector Tabs */}
        <View style={styles.section}>
          <Text style={styles.sectionLabel}>SELECT ROBOT UNIT</Text>
          <View style={styles.botSelectorRow}>
            {bots.map((b) => (
              <TouchableOpacity
                key={b.id}
                style={[styles.botSel, selectedBot === b.id && { borderColor: b.color, backgroundColor: b.color + '18' }]}
                onPress={() => setSelectedBot(b.id)}
              >
                <View style={[styles.botSelDot, { backgroundColor: b.color }]} />
                <Text style={[styles.botSelName, selectedBot === b.id && { color: b.color, fontWeight: 'bold' }]}>{b.name}</Text>
                <StatusPip status={b.status} />
              </TouchableOpacity>
            ))}
          </View>
        </View>

        {/* Live Hardware Telemetry */}
        <View style={styles.section}>
          <Text style={styles.sectionLabel}>HARDWARE & NETWORK TELEMETRY</Text>
          <View style={styles.telemetryCard}>
            <TelRow label="Unit Identification" value={`${bot.name} (${bot.id?.toUpperCase()})`} color={bot.color} colors={colors} responsive={responsive} />
            <TelRow label="Operational Role" value={bot.role} colors={colors} responsive={responsive} />
            <TelRow
              label="Battery Charge"
              value={isBotActive ? `${bot.battery}%` : '0% (Offline)'}
              color={isBotActive && bot.battery > 30 ? colors.green : colors.red}
              colors={colors}
              responsive={responsive}
            />
            <TelRow
              label="Current Status"
              value={bot.status.toUpperCase()}
              color={isBotActive ? colors.green : colors.textMuted}
              colors={colors}
              responsive={responsive}
            />
            <TelRow
              label="IP Address"
              value={isBotActive ? (bot.ipAddress || '10.75.11.50') : '0.0.0.0 (Disconnected)'}
              color={isBotActive ? colors.cyan : colors.textMuted}
              colors={colors}
              responsive={responsive}
            />
            <TelRow
              label="Wi-Fi RSSI"
              value={isBotActive ? `${bot.wifiRssi || -58} dBm` : 'Disconnected'}
              color={isBotActive ? colors.textPrimary : colors.textMuted}
              colors={colors}
              responsive={responsive}
            />
            <TelRow
              label="Free Heap Memory"
              value={isBotActive ? `${(bot.freeHeap / 1024).toFixed(1)} KB` : '0 KB'}
              color={isBotActive ? colors.cyan : colors.textMuted}
              colors={colors}
              responsive={responsive}
            />
            <TelRow
              label="PSRAM Allocated"
              value={isBotActive ? `${(bot.psram / 1024).toFixed(1)} KB` : '0 KB'}
              colors={colors}
              responsive={responsive}
            />
            <TelRow
              label="Swarm Mesh Protocol"
              value={isBotActive ? "ESP-NOW + Wi-Fi HTTP / WebSocket" : "Standby"}
              color={colors.textSecondary}
              colors={colors}
              responsive={responsive}
            />
          </View>
        </View>

        {/* Guardian-Specific Telemetry (AI Vision, Wake-Word, First Aid) */}
        {bot.id === 'guardian' && (
          <View style={styles.section}>
            <Text style={styles.sectionLabel}>GUARDIAN DEFENCE & VISION AI SUBSYSTEMS</Text>
            <View style={styles.telemetryCard}>
              {/* Wake Word Subsystem */}
              <View style={styles.subsystemRow}>
                <View style={styles.subsystemLeft}>
                  <Ionicons name="mic" size={18} color={bot.wakeWordTriggered ? colors.amber : colors.cyan} />
                  <View>
                    <Text style={styles.subsystemTitle}>Wake-Word Activation</Text>
                    <Text style={styles.subsystemSub}>
                      {bot.wakeWordTriggered ? `Triggered: "${bot.wakeWordLabel || 'Hi ESP'}"` : 'Listening for "Hi ESP" wake-word'}
                    </Text>
                  </View>
                </View>
                <View style={[styles.subsystemBadge, { backgroundColor: bot.wakeWordTriggered ? colors.amberDim : colors.greenDim }]}>
                  <Text style={[styles.subsystemBadgeText, { color: bot.wakeWordTriggered ? colors.amber : colors.green }]}>
                    {bot.wakeWordTriggered ? 'TRIGGERED' : 'ARMED / READY'}
                  </Text>
                </View>
              </View>

              {/* First Aid Subsystem */}
              <View style={styles.subsystemRow}>
                <View style={styles.subsystemLeft}>
                  <Ionicons name="medkit" size={18} color={bot.firstAidDelivered ? colors.green : colors.cyan} />
                  <View>
                    <Text style={styles.subsystemTitle}>Autonomous First-Aid Box</Text>
                    <Text style={styles.subsystemSub}>
                      {bot.firstAidDelivered ? 'Delivered to casualty at target position' : 'Securely attached to robot chassis'}
                    </Text>
                  </View>
                </View>
                <View style={[styles.subsystemBadge, { backgroundColor: bot.firstAidDelivered ? colors.greenDim : colors.cyanFaint }]}>
                  <Text style={[styles.subsystemBadgeText, { color: bot.firstAidDelivered ? colors.green : colors.cyan }]}>
                    {bot.firstAidDelivered ? 'DELIVERED' : 'ATTACHED'}
                  </Text>
                </View>
              </View>

              {/* Vision AI Subsystem */}
              <View style={[styles.subsystemRow, { borderBottomWidth: 0 }]}>
                <View style={styles.subsystemLeft}>
                  <Ionicons name="eye" size={18} color={colors.cyan} />
                  <View>
                    <Text style={styles.subsystemTitle}>Edge FOMO / AI Vision Detections</Text>
                    <Text style={styles.subsystemSub}>
                      {(bot.visionDetections || []).length > 0
                        ? `Recent: ${bot.visionDetections[bot.visionDetections.length - 1].label} (${(bot.visionDetections[bot.visionDetections.length - 1].confidence || 0).toFixed(0)}%)`
                        : 'Scanning video feed for targets / threats'}
                    </Text>
                  </View>
                </View>
              </View>
            </View>

            {/* Vision Detection Log Chips */}
            {(bot.visionDetections || []).length > 0 && (
              <View style={styles.detectionLogSection}>
                <Text style={styles.sectionSubLabel}>Recent Vision Log Events</Text>
                <View style={styles.visionChipRow}>
                  {bot.visionDetections.map((det, i) => (
                    <View
                      key={i}
                      style={[
                        styles.visionChip,
                        {
                          borderColor: det.is_threat ? colors.red : colors.cyan,
                          backgroundColor: det.is_threat ? colors.redDim : colors.bg2,
                        },
                      ]}
                    >
                      <Ionicons
                        name={det.is_threat ? 'warning' : 'person'}
                        size={12}
                        color={det.is_threat ? colors.red : colors.cyan}
                      />
                      <Text style={[styles.visionChipText, { color: det.is_threat ? colors.red : colors.textPrimary }]}>
                        {det.label} {(det.confidence || 0).toFixed(0)}% {det.is_threat ? '· THREAT' : ''}
                      </Text>
                    </View>
                  ))}
                </View>
              </View>
            )}
          </View>
        )}

        {/* Pathfinder SLAM Sweep & AI Vision Subsystem */}
        {bot.id === 'pathfinder' && (
          <View style={styles.section}>
            <Text style={styles.sectionLabel}>PATHFINDER SLAM SWEEP & AI VISION</Text>
            {bot.mapPacket && (
              <>
                <DistanceSweep distances={bot.mapPacket.ultrasonic_distances_cm || []} colors={colors} responsive={responsive} />
                <View style={styles.mapSummary}>
                  <Text style={styles.mapSummaryText}>Snapshot {bot.mapPacket.snap_index}/{bot.mapPacket.total_snaps}</Text>
                  <Text style={styles.mapSummaryText}>{bot.mapPacket.has_door ? 'Door aperture detected' : 'Clear corridor'}</Text>
                </View>
              </>
            )}

            {/* Pathfinder Vision Detection Log Chips */}
            {(bot.visionDetections || []).length > 0 && (
              <View style={[styles.detectionLogSection, { marginTop: 12 }]}>
                <Text style={styles.sectionSubLabel}>Pathfinder Edge Impulse Vision Events</Text>
                <View style={styles.visionChipRow}>
                  {bot.visionDetections.map((det, i) => (
                    <View
                      key={i}
                      style={[
                        styles.visionChip,
                        {
                          borderColor: det.is_threat ? colors.red : colors.green,
                          backgroundColor: det.is_threat ? colors.redDim : colors.bg2,
                        },
                      ]}
                    >
                      <Ionicons
                        name={det.is_threat ? 'warning' : 'scan-outline'}
                        size={12}
                        color={det.is_threat ? colors.red : colors.green}
                      />
                      <Text style={[styles.visionChipText, { color: det.is_threat ? colors.red : colors.textPrimary }]}>
                        {det.label} {(det.confidence || 0).toFixed(0)}% {det.is_threat ? '· THREAT' : ''}
                      </Text>
                    </View>
                  ))}
                </View>
              </View>
            )}
          </View>
        )}

        {/* Warden Hazard Data */}
        {bot.id === 'warden' && bot.hazardData && (
          <View style={styles.section}>
            <Text style={styles.sectionLabel}>WARDEN HAZARD SENSORS</Text>
            <View style={styles.hazardCard}>
              <HazardRow label="Gas PPM (MQ-2)" value={`${bot.hazardData.gas_ppm || 200}`} colors={colors} styles={styles} />
              <HazardRow label="Gas Leak Alert" value={bot.hazardData.gas_alert ? 'YES (CRITICAL)' : 'NO (SAFE)'} colors={colors} styles={styles} />
              <HazardRow label="Thermal Signature" value={`${bot.hazardData.temperature_c || 25}°C`} colors={colors} styles={styles} />
              <HazardRow label="Fire Detected" value={bot.hazardData.fire_detected ? 'YES (ALERT)' : 'NO'} colors={colors} styles={styles} />
            </View>
          </View>
        )}

        {/* Emergency Manual Trigger */}
        <View style={styles.section}>
          <TouchableOpacity style={styles.sosBtn} onPress={handleManualSOS} activeOpacity={0.8}>
            <Ionicons name="call" size={18} color={colors.red} />
            <Text style={styles.sosBtnText}>Manual Emergency Services Call</Text>
          </TouchableOpacity>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

function TelRow({ label, value, color, colors, responsive }) {
  const styles = getStyles(colors, responsive);

  return (
    <View style={styles.telRow}>
      <Text style={styles.telLabel}>{label}</Text>
      <Text style={[styles.telValue, { color: color || colors.textPrimary }]}>{value}</Text>
    </View>
  );
}

function HazardRow({ label, value, colors, styles }) {
  return (
    <View style={[styles.hazardRow, { justifyContent: 'space-between' }]}>
      <Text style={styles.hazardLabelText}>{label}</Text>
      <Text style={styles.hazardValueText}>{value}</Text>
    </View>
  );
}

function DistanceSweep({ distances, colors, responsive }) {
  const styles = getSweepStyles(colors, responsive);
  const maxDistance = Math.max(...distances, 1);

  return (
    <View style={styles.sweepContainer}>
      <View style={styles.sweepRow}>
        {distances.map((distance, index) => {
          const height = Math.max(20, Math.min(100, Math.round((distance / maxDistance) * 100)));
          return <View key={index} style={[styles.sweepBar, { height }]} />;
        })}
      </View>
    </View>
  );
}

const getStyles = (colors, responsive) => {
  const typo = getResponsiveTypography(responsive.deviceType);
  const space = getResponsiveSpacing(responsive.deviceType);

  return StyleSheet.create({
    safe: { flex: 1, backgroundColor: colors.bg0 },
    scroll: { flex: 1 },
    content: {
      paddingBottom: space.xxl * 2,
      maxWidth: responsive.isDesktop ? 1200 : '100%',
      alignSelf: 'center',
      width: '100%',
    },
    header: {
      flexDirection: responsive.isSmallDevice ? 'column' : 'row',
      justifyContent: 'space-between',
      alignItems: responsive.isSmallDevice ? 'flex-start' : 'center',
      paddingHorizontal: space.lg,
      paddingVertical: space.md,
      borderBottomWidth: 0.5,
      borderBottomColor: colors.border,
      gap: responsive.isSmallDevice ? space.sm : 0,
    },
    headerTitle: { fontSize: typo.lg, fontWeight: Typography.bold, color: colors.textPrimary },
    headerSubtitle: { fontSize: typo.xs, color: colors.textMuted, marginTop: 2 },
    statusBannerTop: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: 6,
      paddingHorizontal: responsive.isDesktop ? space.md : 10,
      paddingVertical: responsive.isDesktop ? 6 : 4,
      borderRadius: Radius.full,
    },
    statusPillText: { fontSize: typo.xs, fontWeight: Typography.bold, letterSpacing: 0.5 },
    statusError: {
      marginTop: space.sm,
      paddingHorizontal: space.lg,
      color: colors.red,
      fontSize: typo.xs,
    },
    section: { paddingHorizontal: space.lg, paddingTop: space.xl },
    sectionLabel: { fontSize: typo.xs, fontWeight: Typography.bold, color: colors.textMuted, letterSpacing: 0.8, textTransform: 'uppercase', marginBottom: space.sm },
    sectionSubLabel: { fontSize: 10, fontWeight: Typography.bold, color: colors.textMuted, marginBottom: space.xs },
    botSelectorRow: { gap: space.sm },
    botSel: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: space.sm,
      paddingVertical: responsive.isDesktop ? space.md : 10,
      paddingHorizontal: space.md,
      backgroundColor: colors.bg1,
      borderWidth: 0.5,
      borderColor: colors.border,
      borderRadius: Radius.md,
    },
    botSelDot: { width: 8, height: 8, borderRadius: Radius.full },
    botSelName: { flex: 1, fontSize: typo.sm, fontWeight: Typography.medium, color: colors.textSecondary },
    telemetryCard: {
      backgroundColor: colors.bg1,
      borderWidth: 0.5,
      borderColor: colors.border,
      borderRadius: Radius.lg,
      overflow: 'hidden',
    },
    telRow: {
      flexDirection: 'row',
      justifyContent: 'space-between',
      alignItems: 'center',
      paddingHorizontal: space.md,
      paddingVertical: responsive.isDesktop ? space.md : 11,
      borderBottomWidth: 0.5,
      borderBottomColor: colors.border,
    },
    telLabel: { fontSize: typo.sm, color: colors.textSecondary },
    telValue: { fontSize: typo.sm, fontWeight: Typography.medium },

    subsystemRow: {
      flexDirection: 'row',
      justifyContent: 'space-between',
      alignItems: 'center',
      paddingHorizontal: space.md,
      paddingVertical: space.md,
      borderBottomWidth: 0.5,
      borderBottomColor: colors.border,
      gap: space.sm,
    },
    subsystemLeft: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: space.sm,
      flex: 1,
    },
    subsystemTitle: {
      fontSize: typo.sm,
      fontWeight: Typography.bold,
      color: colors.textPrimary,
    },
    subsystemSub: {
      fontSize: typo.xs,
      color: colors.textSecondary,
      marginTop: 1,
    },
    subsystemBadge: {
      paddingHorizontal: space.sm,
      paddingVertical: 3,
      borderRadius: Radius.sm,
    },
    subsystemBadgeText: {
      fontSize: 9,
      fontWeight: Typography.bold,
    },

    detectionLogSection: {
      marginTop: space.sm,
      padding: space.sm,
    },
    visionChipRow: {
      flexDirection: 'row',
      flexWrap: 'wrap',
      gap: space.xs,
    },
    visionChip: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: 4,
      paddingHorizontal: space.sm,
      paddingVertical: 4,
      borderRadius: Radius.sm,
      borderWidth: 1,
    },
    visionChipText: {
      fontSize: 10,
      fontWeight: Typography.medium,
    },

    testBanner: {
      marginHorizontal: space.lg,
      marginTop: space.md,
      padding: space.md,
      borderRadius: Radius.lg,
      backgroundColor: colors.bg1,
      borderWidth: 0.5,
      borderColor: colors.border,
      flexDirection: 'row',
      justifyContent: 'space-between',
      alignItems: 'center',
      gap: space.sm,
    },
    testTitle: { fontSize: typo.sm, fontWeight: Typography.bold, color: colors.textPrimary },
    testSubtitle: { fontSize: typo.xs, color: colors.textMuted, marginTop: 2 },
    testButton: {
      paddingVertical: space.sm,
      paddingHorizontal: space.md,
      backgroundColor: colors.cyan,
      borderRadius: Radius.full,
    },
    testButtonText: { fontSize: typo.xs, fontWeight: Typography.bold, color: colors.bg0 },

    mapSummary: {
      marginTop: space.sm,
      flexDirection: 'row',
      justifyContent: 'space-between',
      gap: space.sm,
    },
    mapSummaryText: { fontSize: typo.xs, color: colors.textSecondary },

    hazardCard: {
      backgroundColor: colors.bg1,
      borderRadius: Radius.lg,
      borderWidth: 0.5,
      borderColor: colors.border,
      padding: space.md,
    },
    hazardRow: {
      flexDirection: 'row',
      justifyContent: 'space-between',
      paddingVertical: 8,
      borderBottomWidth: 0.5,
      borderBottomColor: colors.border,
    },
    hazardLabelText: { fontSize: typo.xs, color: colors.textSecondary },
    hazardValueText: { fontSize: typo.sm, color: colors.textPrimary, fontWeight: Typography.medium },

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

const getSweepStyles = (colors, responsive) => {
  const space = getResponsiveSpacing(responsive.deviceType);
  return StyleSheet.create({
    sweepContainer: {
      width: '100%',
      minHeight: 120,
      borderRadius: Radius.md,
      backgroundColor: colors.bg1,
      borderWidth: 0.5,
      borderColor: colors.border,
      padding: space.sm,
    },
    sweepRow: {
      flexDirection: 'row',
      gap: 4,
      alignItems: 'flex-end',
      justifyContent: 'space-between',
      paddingTop: space.sm,
    },
    sweepBar: {
      width: 8,
      borderRadius: 4,
      backgroundColor: colors.cyan,
    },
  });
};

