import React from 'react';
import {
  View,
  Text,
  ScrollView,
  StyleSheet,
  Image,
  TouchableOpacity,
  Linking,
} from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { Ionicons } from '@expo/vector-icons';
import { Typography, Radius, getResponsiveTypography, getResponsiveSpacing } from '../constants/theme';
import { useTheme } from '../context/ThemeContext';
import { useResponsive } from '../utils/responsive';
import { SectionLabel, MeshBadge } from '../components/SwarmUI';
import { BOTS } from '../constants/mockData';

export default function AboutScreen({ navigation }) {
  const { colors, isDarkMode, toggleTheme } = useTheme();
  const responsive = useResponsive();
  const styles = getStyles(colors, responsive);

  const onlineCount = BOTS.filter(b => b.status === 'online').length;

  const supervisor = {
    name: 'Dr. Shahnewaz Siddique',
    title: 'Project Supervisor',
    role: 'Associate Professor',
    department: 'Electrical and Computer Engineering (ECE)',
    institution: 'North South University (NSU)',
    image: require('../../assets/images/team/Dr. Shahnewaz Siddique.jpg'),
    bio: 'A researcher and educator specializing in Artificial Intelligence, Robotics, Intelligent Systems, and Modeling & Simulation. He holds a Ph.D. in Aerospace Engineering from Georgia Institute of Technology and has authored 50+ research publications. Dr. Siddique has supervised numerous research and development projects in AI, robotics, and intelligent systems.',
    email: 'shahnewaz@example.edu',
    achievements: [
      'Ph.D. in Aerospace Engineering, Georgia Institute of Technology',
      'Published 50+ research publications',
      'Expert in AI, Robotics, and Intelligent Systems',
      'Specialist in Modeling & Simulation',
    ],
  };

  const teamMembers = [
    {
      id: 1,
      name: 'Ahmed Sadman Sadik',
      role: 'AI/ML Engineer',
      image: require('../../assets/images/team/Ahmed Sadman Sadik.jpg'),
      expertise: 'Model Training, Computer Vision, YOLO',
      contributions: 'Model training for Pathfinder image recognition',
      github: 'github.com/Sadik00789',
      linkedin: 'linkedin.com/in/ahmed-sadman-sadik-348533263',
    },
    {
      id: 2,
      name: 'Md Sakib Sarker',
      role: 'AI/ML Engineer',
      image: require('../../assets/images/team/Md Sakib Sarker.jpg'),
      expertise: 'Model Training, Deep Learning, AI/ML',
      contributions: 'Model training and AI system development',
      github: 'github.com/sakib2004',
      linkedin: 'linkedin.com/in/md-sakib-sarker-55b3b3202',
    },
    {
      id: 3,
      name: 'Md. Fahmidul Hasan',
      role: 'Frontend Developer',
      image: require('../../assets/images/team/Md. Fahmidul Hasan.jpg'),
      expertise: 'React Native, UI Development, Mobile Apps',
      contributions: 'Frontend development and user interface implementation',
      github: 'github.com/sakibulla',
      linkedin: 'linkedin.com/in/md-fahmidul-hasan',
    },
    {
      id: 4,
      name: 'Mohammed Bin Ahmed',
      role: 'Robotics Engineer',
      image: require('../../assets/images/team/Mohammed Bin Ahmed .jpg'),
      expertise: 'Voice Recognition, AI Integration, Robotics',
      contributions: 'Implemented voice recognition and Warden bot',
      github: 'github.com/mbamotria',
      linkedin: 'linkedin.com/in/mbamahee',
    },
    {
      id: 5,
      name: 'Shahriar Jaman',
      role: 'Hardware Engineer',
      image: require('../../assets/images/team/Shahriar Jaman.jpg'),
      expertise: 'ESP32, Robotics, Hardware Integration',
      contributions: 'Built Pathfinder bot hardware and systems',
      github: 'github.com/Shahriar-jaman',
      linkedin: 'linkedin.com/in/shahriar-jaman-262677298',
    },
    {
      id: 6,
      name: 'Zahid Hasan Rana',
      role: 'Backend Developer',
      image: require('../../assets/images/team/Zahid Hasan Rana.jpg'),
      expertise: 'Node.js, API Development, Documentation',
      contributions: 'Backend development and research paper work',
      github: 'github.com/ZahidRana',
      linkedin: 'linkedin.com/in/zahid-rana-630244154',
    },
  ];

  const openLink = (url) => {
    Linking.openURL(url).catch((err) => console.error('Failed to open URL:', err));
  };

  return (
    <SafeAreaView style={styles.safe} edges={['top']}>
      {/* Top Bar */}
      <View style={styles.topBar}>
        <View>
          <Text style={styles.appTitle}>A.E.G.I.S.</Text>
          <View style={styles.topBarSub}>
            <MeshBadge />
            <Text style={styles.topBarSubText}>· {onlineCount}/3 bots active</Text>
          </View>
        </View>
        <View style={styles.topBarActions}>
          <TouchableOpacity style={styles.themeBtn} onPress={toggleTheme} activeOpacity={0.7}>
            <Ionicons name={isDarkMode ? 'moon' : 'sunny'} size={22} color={colors.textSecondary} />
          </TouchableOpacity>
          <TouchableOpacity style={styles.backBtn} onPress={() => navigation.goBack()}>
            <Ionicons name="arrow-back" size={24} color={colors.textPrimary} />
          </TouchableOpacity>
        </View>
      </View>

      <ScrollView style={styles.scroll} contentContainerStyle={styles.content}>
        {/* About Header */}
        <View style={styles.aboutHeader}>
          <Text style={styles.aboutTitle}>About Us</Text>
          <Text style={styles.aboutSubtitle}>Team Hexahive</Text>
        </View>

        {/* Supervisor Section */}
        <View style={styles.section}>
          <SectionLabel>Project Supervisor</SectionLabel>
          <View style={styles.supervisorCard}>
            <View style={styles.supervisorHeader}>
              <View style={styles.avatarLarge}>
                {supervisor.image ? (
                  <Image
                    source={supervisor.image}
                    style={styles.avatarImage}
                    resizeMode="cover"
                  />
                ) : (
                  <Ionicons name="person" size={60} color={colors.cyan} />
                )}
              </View>
              <View style={styles.supervisorInfo}>
                <Text style={styles.supervisorName}>{supervisor.name}</Text>
                <Text style={styles.supervisorTitle}>{supervisor.title}</Text>
                <Text style={styles.supervisorRole}>{supervisor.role}</Text>
                <Text style={styles.supervisorDept}>{supervisor.department}</Text>
                <Text style={styles.supervisorDept}>{supervisor.institution}</Text>
              </View>
            </View>

            <Text style={styles.bioText}>{supervisor.bio}</Text>

            <View style={styles.achievementsSection}>
              <Text style={styles.achievementsTitle}>Key Achievements:</Text>
              {supervisor.achievements.map((achievement, index) => (
                <View key={index} style={styles.achievementItem}>
                  <Ionicons name="checkmark-circle" size={16} color={colors.green} />
                  <Text style={styles.achievementText}>{achievement}</Text>
                </View>
              ))}
            </View>
          </View>
        </View>

        {/* Team Hexahive Section */}
        <View style={styles.section}>
          <SectionLabel>Team Hexahive</SectionLabel>
          <View style={styles.teamGrid}>
            {teamMembers.map((member) => (
              <View key={member.id} style={styles.memberCard}>
                {/* Avatar */}
                <View style={styles.avatar}>
                  {member.image ? (
                    <Image
                      source={member.image}
                      style={styles.avatarImage}
                      resizeMode="cover"
                    />
                  ) : (
                    <Ionicons name="person" size={32} color={colors.cyan} />
                  )}
                </View>

                {/* Member Info */}
                <Text style={styles.memberName}>{member.name}</Text>
                <Text style={styles.memberRole}>{member.role}</Text>

                {/* Expertise */}
                <View style={styles.expertiseSection}>
                  <Ionicons name="code-slash" size={14} color={colors.textMuted} />
                  <Text style={styles.expertiseText}>{member.expertise}</Text>
                </View>

                {/* Contributions */}
                <View style={styles.contributionSection}>
                  <Ionicons name="bulb" size={14} color={colors.amber} />
                  <Text style={styles.contributionText}>{member.contributions}</Text>
                </View>

                {/* Social Links */}
                <View style={styles.socialLinks}>
                  {member.github && (
                    <TouchableOpacity
                      style={styles.socialBtn}
                      onPress={() => openLink(`https://${member.github}`)}
                    >
                      <Ionicons name="logo-github" size={20} color={colors.textSecondary} />
                    </TouchableOpacity>
                  )}
                  {member.linkedin && (
                    <TouchableOpacity
                      style={styles.socialBtn}
                      onPress={() => openLink(`https://${member.linkedin}`)}
                    >
                      <Ionicons name="logo-linkedin" size={20} color={colors.cyan} />
                    </TouchableOpacity>
                  )}
                </View>
              </View>
            ))}
          </View>
        </View>

        {/* Footer */}
        <View style={styles.footer}>
          <Text style={styles.footerText}>A.E.G.I.S.</Text>
          <Text style={styles.footerSubtext}>
            © 2026 Team Hexahive. All rights reserved.
          </Text>
          <Text style={styles.footerSubtext}>
            Built with ❤️ for emergency response & safety
          </Text>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

// ─── Styles ───────────────────────────────────────────────────────

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
      gap: space.md,
      marginLeft: responsive.isSmallDevice ? 0 : 'auto',
    },
    themeBtn: { padding: space.xs },
    backBtn: { padding: space.xs },

    aboutHeader: {
      paddingHorizontal: space.lg,
      paddingTop: space.xl,
      paddingBottom: space.md,
    },
    aboutTitle: {
      fontSize: typo.xxl,
      fontWeight: Typography.bold,
      color: colors.textPrimary,
      marginBottom: space.xs,
    },
    aboutSubtitle: {
      fontSize: typo.md,
      color: colors.textSecondary,
    },

    banner: {
      alignItems: 'center',
      paddingVertical: space.xxl,
      paddingHorizontal: space.lg,
      backgroundColor: colors.bg1,
      borderBottomWidth: 1,
      borderBottomColor: colors.border,
    },
    bannerIcon: {
      width: 80,
      height: 80,
      borderRadius: 40,
      backgroundColor: colors.cyanFaint,
      alignItems: 'center',
      justifyContent: 'center',
      borderWidth: 2,
      borderColor: colors.cyan,
      marginBottom: space.lg,
    },
    bannerTitle: {
      fontSize: typo.xxl,
      fontWeight: Typography.bold,
      color: colors.textPrimary,
      letterSpacing: 2,
      marginBottom: space.xs,
    },
    bannerSubtitle: {
      fontSize: typo.sm,
      color: colors.textSecondary,
      textAlign: 'center',
      marginBottom: space.md,
    },
    versionBadge: {
      backgroundColor: colors.bg2,
      paddingHorizontal: space.md,
      paddingVertical: space.xs,
      borderRadius: Radius.full,
      borderWidth: 1,
      borderColor: colors.border,
    },
    versionText: {
      fontSize: typo.xs,
      color: colors.textMuted,
      fontWeight: Typography.bold,
    },

    section: { paddingHorizontal: space.lg, paddingTop: space.xl },

    descCard: {
      backgroundColor: colors.bg1,
      borderWidth: 1,
      borderColor: colors.border,
      borderRadius: Radius.lg,
      padding: space.lg,
    },
    descText: {
      fontSize: typo.sm,
      color: colors.textSecondary,
      lineHeight: typo.sm * 1.6,
    },

    supervisorCard: {
      backgroundColor: colors.bg1,
      borderWidth: 1,
      borderColor: colors.borderStrong,
      borderRadius: Radius.lg,
      padding: space.lg,
      borderLeftWidth: 4,
      borderLeftColor: colors.cyan,
    },
    supervisorHeader: {
      flexDirection: responsive.isSmallDevice ? 'column' : 'row',
      gap: space.lg,
      marginBottom: space.lg,
      alignItems: responsive.isSmallDevice ? 'center' : 'flex-start',
    },
    avatarLarge: {
      width: 120,
      height: 120,
      borderRadius: 60,
      backgroundColor: colors.cyanFaint,
      alignItems: 'center',
      justifyContent: 'center',
      borderWidth: 3,
      borderColor: colors.cyan,
      overflow: 'hidden',
    },
    avatarImage: {
      width: '100%',
      height: '100%',
    },
    supervisorInfo: {
      flex: 1,
      alignItems: responsive.isSmallDevice ? 'center' : 'flex-start',
    },
    supervisorName: {
      fontSize: typo.xl,
      fontWeight: Typography.bold,
      color: colors.textPrimary,
      marginBottom: space.xs,
    },
    supervisorTitle: {
      fontSize: typo.md,
      fontWeight: Typography.medium,
      color: colors.cyan,
      marginBottom: space.xs,
    },
    supervisorRole: {
      fontSize: typo.sm,
      color: colors.textSecondary,
    },
    supervisorDept: {
      fontSize: typo.xs,
      color: colors.textMuted,
      marginTop: 2,
    },
    bioText: {
      fontSize: typo.sm,
      color: colors.textSecondary,
      lineHeight: typo.sm * 1.5,
      marginBottom: space.lg,
    },
    achievementsSection: {
      marginBottom: space.lg,
    },
    achievementsTitle: {
      fontSize: typo.sm,
      fontWeight: Typography.bold,
      color: colors.textPrimary,
      marginBottom: space.sm,
    },
    achievementItem: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: space.sm,
      marginBottom: space.xs,
    },
    achievementText: {
      fontSize: typo.sm,
      color: colors.textSecondary,
    },
    contactBtn: {
      flexDirection: 'row',
      alignItems: 'center',
      justifyContent: 'center',
      gap: space.sm,
      paddingVertical: space.md,
      backgroundColor: colors.cyanFaint,
      borderWidth: 1,
      borderColor: colors.cyan,
      borderRadius: Radius.md,
    },
    contactBtnText: {
      fontSize: typo.sm,
      fontWeight: Typography.bold,
      color: colors.cyan,
    },

    teamGrid: {
      flexDirection: 'row',
      flexWrap: 'wrap',
      gap: space.md,
    },
    memberCard: {
      width: responsive.isDesktop ? '31%' : responsive.isTablet ? '47%' : '100%',
      backgroundColor: colors.bg1,
      borderWidth: 1,
      borderColor: colors.border,
      borderRadius: Radius.lg,
      padding: space.lg,
      alignItems: 'center',
    },
    avatar: {
      width: 70,
      height: 70,
      borderRadius: 35,
      backgroundColor: colors.cyanFaint,
      alignItems: 'center',
      justifyContent: 'center',
      borderWidth: 2,
      borderColor: colors.cyan,
      marginBottom: space.md,
      overflow: 'hidden',
    },
    memberName: {
      fontSize: typo.md,
      fontWeight: Typography.bold,
      color: colors.textPrimary,
      marginBottom: space.xs,
      textAlign: 'center',
    },
    memberRole: {
      fontSize: typo.sm,
      fontWeight: Typography.medium,
      color: colors.cyan,
      marginBottom: space.md,
      textAlign: 'center',
    },
    expertiseSection: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: space.xs,
      marginBottom: space.sm,
      paddingHorizontal: space.sm,
    },
    expertiseText: {
      fontSize: typo.xs,
      color: colors.textMuted,
      textAlign: 'center',
    },
    contributionSection: {
      flexDirection: 'row',
      alignItems: 'flex-start',
      gap: space.xs,
      marginBottom: space.md,
      paddingHorizontal: space.sm,
    },
    contributionText: {
      flex: 1,
      fontSize: typo.xs,
      color: colors.textSecondary,
      textAlign: 'center',
      lineHeight: typo.xs * 1.4,
    },
    socialLinks: {
      flexDirection: 'row',
      gap: space.sm,
      marginTop: space.sm,
    },
    socialBtn: {
      width: 40,
      height: 40,
      borderRadius: 20,
      backgroundColor: colors.bg2,
      alignItems: 'center',
      justifyContent: 'center',
      borderWidth: 1,
      borderColor: colors.border,
    },

    statsGrid: {
      flexDirection: 'row',
      flexWrap: 'wrap',
      gap: space.sm,
    },
    statCard: {
      flex: responsive.isSmallDevice ? 0 : 1,
      minWidth: responsive.isSmallDevice ? '47%' : 'auto',
      backgroundColor: colors.bg1,
      borderWidth: 1,
      borderColor: colors.border,
      borderRadius: Radius.md,
      padding: space.lg,
      alignItems: 'center',
    },
    statValue: {
      fontSize: typo.lg,
      fontWeight: Typography.bold,
      marginTop: space.xs,
    },
    statLabel: {
      fontSize: typo.xs,
      color: colors.textMuted,
      marginTop: space.xs,
      textAlign: 'center',
    },

    techGrid: {
      flexDirection: 'row',
      flexWrap: 'wrap',
      gap: space.sm,
    },
    techBadge: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: space.xs,
      paddingHorizontal: space.md,
      paddingVertical: space.sm,
      borderRadius: Radius.full,
      borderWidth: 1,
    },
    techBadgeText: {
      fontSize: typo.xs,
      fontWeight: Typography.medium,
    },

    footer: {
      alignItems: 'center',
      paddingVertical: space.xxl,
      paddingHorizontal: space.lg,
      marginTop: space.xl,
    },
    footerText: {
      fontSize: typo.sm,
      color: colors.textSecondary,
      marginBottom: space.xs,
    },
    footerSubtext: {
      fontSize: typo.xs,
      color: colors.textMuted,
    },
  });
};
