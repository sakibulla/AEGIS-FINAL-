import React, { useState } from 'react';
import {
  View, Text, ScrollView, StyleSheet, TouchableOpacity,
} from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { Ionicons } from '@expo/vector-icons';
import { Typography, Spacing, Radius, getResponsiveTypography, getResponsiveSpacing } from '../constants/theme';
import { useTheme } from '../context/ThemeContext';
import { SectionLabel, MeshBadge } from '../components/SwarmUI';
import { MAP_ROOMS } from '../constants/mockData';
import { useTelemetry } from '../hooks/useTelemetry';
import { useResponsive } from '../utils/responsive';

export default function MapScreen() {
  const { colors } = useTheme();
  const responsive = useResponsive();
  const { bots, connected } = useTelemetry();
  const [selectedBotId, setSelectedBotId] = useState(null);

  const styles = getStyles(colors, responsive);

  const selectedBot = bots.find((b) => b.id === selectedBotId) || null;
  const pathfinderBot = bots.find((b) => b.id === 'pathfinder');
  const coveragePct = pathfinderBot?.mapCoverage ?? 78;

  return (
    <SafeAreaView style={styles.safe} edges={['top']}>
      <View style={styles.header}>
        <View>
          <Text style={styles.headerTitle}>Live Swarm Floorplan</Text>
          <Text style={styles.headerSubtitle}>Pathfinder SLAM Occupancy Grid & Positions</Text>
        </View>
        <MeshBadge label={connected ? 'SLAM Active' : 'Offline Map'} active={connected} />
      </View>

      <ScrollView style={styles.scroll} contentContainerStyle={styles.content} showsVerticalScrollIndicator={false}>
        
        {/* Interactive Tactical Map Container */}
        <View style={styles.mapContainer}>
          <View style={styles.mapFrame}>
            {/* Tactical Grid Lines */}
            <View style={[styles.gridOverlay, { pointerEvents: 'none' }]}>
              <View style={[styles.gridLineH, { top: '25%' }]} />
              <View style={[styles.gridLineH, { top: '50%' }]} />
              <View style={[styles.gridLineH, { top: '75%' }]} />
              <View style={[styles.gridLineV, { left: '25%' }]} />
              <View style={[styles.gridLineV, { left: '50%' }]} />
              <View style={[styles.gridLineV, { left: '75%' }]} />
            </View>

            {/* Room Boxes */}
            {MAP_ROOMS.map((room) => (
              <View
                key={room.id}
                style={[
                  styles.roomBox,
                  {
                    left: `${room.x * 100}%`,
                    top: `${room.y * 100}%`,
                    width: `${room.w * 100}%`,
                    height: `${room.h * 100}%`,
                  },
                ]}
              >
                <Text style={styles.roomLabel}>{room.label.toUpperCase()}</Text>
              </View>
            ))}

            {/* Live Bot Markers on Tactical Map */}
            {bots.map((bot) => {
              const posX = bot.location?.x ?? 0.5;
              const posY = bot.location?.y ?? 0.5;
              const isSelected = selectedBotId === bot.id;

              return (
                <TouchableOpacity
                  key={bot.id}
                  style={[
                    styles.botMarker,
                    {
                      left: `${Math.max(5, Math.min(90, posX * 100))}%`,
                      top: `${Math.max(5, Math.min(90, posY * 100))}%`,
                      borderColor: bot.color,
                      backgroundColor: bot.color + '33',
                    },
                    isSelected && styles.botMarkerSelected,
                  ]}
                  onPress={() => setSelectedBotId(isSelected ? null : bot.id)}
                >
                  <View style={[styles.radarPing, { backgroundColor: bot.color }]} />
                  <View style={[styles.botMarkerDot, { backgroundColor: bot.color }]} />
                  <Text style={[styles.botMarkerName, { color: bot.color }]}>
                    {bot.name}
                  </Text>
                </TouchableOpacity>
              );
            })}
          </View>
        </View>

        {/* Selected Bot Details Pill */}
        {selectedBot && (
          <View style={styles.selectedBotCard}>
            <View style={styles.selectedBotHeader}>
              <View style={[styles.selectedDot, { backgroundColor: selectedBot.color }]} />
              <Text style={styles.selectedTitle}>{selectedBot.name} ({selectedBot.role})</Text>
              <Text style={[styles.selectedStatus, { color: colors.cyan }]}>{selectedBot.status.toUpperCase()}</Text>
            </View>
            <Text style={styles.selectedTasks}>
              {selectedBot.tasks?.join(' · ') || 'Patrolling area'}
            </Text>
          </View>
        )}

        {/* Bot Selector */}
        <View style={styles.section}>
          <SectionLabel>Filter Unit Position</SectionLabel>
          <View style={styles.botList}>
            {bots.map((bot) => (
              <TouchableOpacity
                key={bot.id}
                style={[
                  styles.botButton,
                  selectedBotId === bot.id && styles.botButtonActive,
                ]}
                onPress={() => setSelectedBotId(selectedBotId === bot.id ? null : bot.id)}
              >
                <View
                  style={[
                    styles.botButtonDot,
                    { backgroundColor: bot.color },
                  ]}
                />
                <Text
                  style={[
                    styles.botButtonLabel,
                    selectedBotId === bot.id && { color: colors.cyan, fontWeight: 'bold' },
                  ]}
                >
                  {bot.name}
                </Text>
              </TouchableOpacity>
            ))}
          </View>
        </View>

        {/* Map Stats */}
        <View style={styles.section}>
          <SectionLabel>SLAM Coverage & Topology</SectionLabel>
          <View style={styles.statRow}>
            <MapStat label="Map Coverage" value={`${coveragePct}%`} colors={colors} responsive={responsive} />
            <MapStat label="Mapped Rooms" value={`${MAP_ROOMS.length} Zones`} colors={colors} responsive={responsive} />
            <MapStat label="Mesh Nodes" value={`${bots.length} Active`} colors={colors} responsive={responsive} />
          </View>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

function MapStat({ label, value, colors, responsive }) {
  return (
    <View style={[getStyles(colors, responsive).statCard, { backgroundColor: colors.bg1 }]}>
      <Text style={[getStyles(colors, responsive).statValue, { color: colors.cyan }]}>{value}</Text>
      <Text style={getStyles(colors, responsive).statLabel}>{label}</Text>
    </View>
  );
}

const getStyles = (colors, responsive) => {
  const typo = getResponsiveTypography(responsive.deviceType);
  const space = getResponsiveSpacing(responsive.deviceType);

  const mapHeight = responsive.isDesktop ? 480 : responsive.isTablet ? 400 : 320;

  return StyleSheet.create({
    safe: { flex: 1, backgroundColor: colors.bg0 },
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
    headerTitle: {
      fontSize: typo.xl,
      fontWeight: Typography.bold,
      color: colors.textPrimary,
      letterSpacing: 0.8,
    },
    headerSubtitle: {
      fontSize: typo.xs,
      color: colors.textMuted,
      marginTop: 2,
    },
    scroll: { flex: 1 },
    content: { 
      paddingBottom: space.xxl,
      maxWidth: responsive.isDesktop ? 1200 : '100%',
      alignSelf: 'center',
      width: '100%',
    },

    mapContainer: {
      paddingHorizontal: space.lg,
      paddingTop: space.lg,
      paddingBottom: space.md,
    },
    mapFrame: {
      backgroundColor: '#070b12',
      borderWidth: 1,
      borderColor: colors.borderStrong,
      borderRadius: Radius.lg,
      height: mapHeight,
      position: 'relative',
      overflow: 'hidden',
    },
    gridOverlay: {
      ...StyleSheet.absoluteFillObject,
    },
    gridLineH: {
      position: 'absolute',
      left: 0,
      right: 0,
      height: 1,
      backgroundColor: 'rgba(0, 210, 255, 0.08)',
    },
    gridLineV: {
      position: 'absolute',
      top: 0,
      bottom: 0,
      width: 1,
      backgroundColor: 'rgba(0, 210, 255, 0.08)',
    },
    roomBox: {
      position: 'absolute',
      borderWidth: 1,
      borderColor: 'rgba(0, 210, 255, 0.3)',
      backgroundColor: 'rgba(13, 20, 36, 0.65)',
      borderRadius: Radius.sm,
      padding: 6,
    },
    roomLabel: {
      fontSize: 9,
      fontWeight: Typography.bold,
      color: colors.textSecondary,
      letterSpacing: 0.5,
    },

    botMarker: {
      position: 'absolute',
      flexDirection: 'row',
      alignItems: 'center',
      gap: 4,
      paddingHorizontal: 8,
      paddingVertical: 4,
      borderRadius: Radius.full,
      borderWidth: 1.5,
      zIndex: 10,
    },
    botMarkerSelected: {
      transform: [{ scale: 1.15 }],
      zIndex: 20,
    },
    radarPing: {
      position: 'absolute',
      width: 24,
      height: 24,
      borderRadius: 12,
      opacity: 0.25,
      left: -6,
      top: -4,
    },
    botMarkerDot: {
      width: 8,
      height: 8,
      borderRadius: 4,
    },
    botMarkerName: {
      fontSize: 10,
      fontWeight: Typography.bold,
    },

    selectedBotCard: {
      marginHorizontal: space.lg,
      marginBottom: space.sm,
      backgroundColor: colors.bg1,
      borderRadius: Radius.md,
      padding: space.md,
      borderWidth: 1,
      borderColor: colors.cyan + '44',
    },
    selectedBotHeader: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: 8,
      marginBottom: 4,
    },
    selectedDot: {
      width: 8,
      height: 8,
      borderRadius: 4,
    },
    selectedTitle: {
      fontSize: typo.sm,
      fontWeight: Typography.bold,
      color: colors.textPrimary,
      flex: 1,
    },
    selectedStatus: {
      fontSize: 10,
      fontWeight: Typography.bold,
    },
    selectedTasks: {
      fontSize: typo.xs,
      color: colors.textSecondary,
    },

    section: { paddingHorizontal: space.lg, paddingTop: space.md },

    botList: { 
      flexDirection: 'row', 
      flexWrap: 'wrap', 
      gap: space.sm 
    },
    botButton: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: space.sm,
      paddingHorizontal: responsive.isDesktop ? space.lg : space.md,
      paddingVertical: responsive.isDesktop ? space.md : space.sm,
      backgroundColor: colors.bg1,
      borderWidth: 0.5,
      borderColor: colors.border,
      borderRadius: Radius.full,
    },
    botButtonActive: {
      borderColor: colors.cyan,
      backgroundColor: colors.cyanFaint,
    },
    botButtonDot: {
      width: 8,
      height: 8,
      borderRadius: Radius.full,
    },
    botButtonLabel: {
      fontSize: typo.sm,
      fontWeight: Typography.medium,
      color: colors.textSecondary,
    },

    statRow: { 
      flexDirection: responsive.isSmallDevice ? 'column' : 'row', 
      gap: space.sm 
    },
    statCard: {
      flex: responsive.isSmallDevice ? 0 : 1,
      width: responsive.isSmallDevice ? '100%' : 'auto',
      borderWidth: 0.5,
      borderColor: colors.border,
      borderRadius: Radius.md,
      padding: responsive.isDesktop ? space.lg : space.md,
      alignItems: 'center',
    },
    statValue: {
      fontSize: typo.lg,
      fontWeight: Typography.bold,
    },
    statLabel: {
      fontSize: typo.xs,
      color: colors.textMuted,
      textAlign: 'center',
      marginTop: space.sm,
    },
  });
};

