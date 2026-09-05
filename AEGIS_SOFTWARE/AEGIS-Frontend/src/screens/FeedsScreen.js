import React, { useState } from 'react';
import { View, Text, StyleSheet, ScrollView, Image } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { useTheme } from '../context/ThemeContext';
import { useTelemetry } from '../hooks/useTelemetry';

// Camera port is a firmware-level detail (which HTTP server port each bot's
// snapshot endpoint listens on) — it doesn't change with the network, unlike
// the IP address, so it's fine to keep this small and local.
const PORTS = { guardian: '', pathfinder: '', warden: ':81' };
const ROBOT_ORDER = ['guardian', 'pathfinder', 'warden'];

export default function FeedsScreen() {
  const { colors } = useTheme();
  const { bots } = useTelemetry();
  const [timestamp, setTimestamp] = useState(Date.now());

  // Auto-refresh every 1000ms (1 FPS)
  React.useEffect(() => {
    const interval = setInterval(() => {
      setTimestamp(Date.now());
    }, 1000);
    return () => clearInterval(interval);
  }, []);

  const robots = ROBOT_ORDER.map((id) => {
    const bot = bots.find((b) => b.id === id);
    return {
      id,
      name: bot?.name || id.charAt(0).toUpperCase() + id.slice(1),
      ip: bot?.ipAddress || '0.0.0.0',
      port: PORTS[id] || '',
      online: Boolean(bot) && bot.ipAddress && bot.ipAddress !== '0.0.0.0',
    };
  });

  return (
    <SafeAreaView style={[styles.safe, { backgroundColor: colors.bg0 }]} edges={['top']}>
      <View style={[styles.topBar, { borderBottomColor: colors.border, backgroundColor: colors.bg1 }]}>
        <Text style={[styles.title, { color: colors.cyan }]}>AEGIS LIVE FEEDS</Text>
        <Text style={[styles.subtitle, { color: colors.textSecondary }]}>
          Snapshot Mode · Auto-refresh 1 FPS
        </Text>
      </View>

      <ScrollView style={styles.scroll} contentContainerStyle={styles.content}>
        {robots.map((robot) => {
          const snapshotUrl = `http://${robot.ip}${robot.port}/snapshot?t=${timestamp}`;

          return (
            <View key={robot.id} style={[styles.feedCard, { backgroundColor: colors.bg1, borderColor: colors.border }]}>
              <Text style={[styles.botName, { color: colors.cyan }]}>{robot.name}</Text>
              <Text style={[styles.botIp, { color: colors.textMuted }]}>
                {robot.online ? `${robot.ip}${robot.port}` : 'Not connected'}
              </Text>

              <View style={[styles.imageContainer, { backgroundColor: '#070b12' }]}>
                {robot.online ? (
                  <Image
                    source={{ uri: snapshotUrl }}
                    style={styles.snapshotImage}
                    resizeMode="contain"
                    onError={() => {
                      console.error(`Failed to load snapshot for ${robot.name}`);
                    }}
                  />
                ) : (
                  <View style={styles.offlinePlaceholder}>
                    <Text style={{ color: colors.textMuted }}>Waiting for {robot.name}...</Text>
                  </View>
                )}
              </View>
            </View>
          );
        })}
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safe: {
    flex: 1,
  },
  topBar: {
    paddingHorizontal: 20,
    paddingVertical: 16,
    borderBottomWidth: 1,
  },
  title: {
    fontSize: 18,
    fontWeight: 'bold',
    letterSpacing: 2,
  },
  subtitle: {
    fontSize: 12,
    marginTop: 4,
  },
  scroll: {
    flex: 1,
  },
  content: {
    padding: 20,
    gap: 20,
  },
  feedCard: {
    padding: 16,
    borderRadius: 12,
    borderWidth: 1,
  },
  botName: {
    fontSize: 16,
    fontWeight: 'bold',
    marginBottom: 4,
  },
  botIp: {
    fontSize: 12,
    marginBottom: 12,
  },
  imageContainer: {
    width: '100%',
    height: 300,
    borderRadius: 8,
    overflow: 'hidden',
  },
  snapshotImage: {
    width: '100%',
    height: '100%',
  },
  offlinePlaceholder: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'center',
  },
});
