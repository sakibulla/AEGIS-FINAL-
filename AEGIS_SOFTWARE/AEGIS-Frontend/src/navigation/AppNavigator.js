import React from 'react';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { NavigationContainer } from '@react-navigation/native';
import { Ionicons } from '@expo/vector-icons';
import { StyleSheet } from 'react-native';
import { Typography } from '../constants/theme';
import { useTheme } from '../context/ThemeContext';

import DashboardScreen from '../screens/DashboardScreen';
import FeedsScreen from '../screens/FeedsScreen';
import ControlScreen from '../screens/ControlScreen';
import AboutScreen from '../screens/AboutScreen';

const Tab = createBottomTabNavigator();

const TABS = [
  { name: 'Dashboard', component: DashboardScreen, icon: 'grid', iconActive: 'grid' },
  { name: 'Feeds', component: FeedsScreen, icon: 'videocam-outline', iconActive: 'videocam' },
  { name: 'Status', component: ControlScreen, icon: 'pulse-outline', iconActive: 'pulse' },
  { name: 'About', component: AboutScreen, icon: 'information-circle-outline', iconActive: 'information-circle' },
];

export default function AppNavigator() {
  const { colors, isDarkMode } = useTheme();

  const navigationTheme = {
    dark: isDarkMode,
    colors: {
      primary: colors.cyan,
      background: colors.bg0 || '#060d18',
      card: colors.bg1 || '#0a1525',
      text: colors.textPrimary || '#e8f2ff',
      border: colors.border || '#1a2e48',
      notification: colors.red || '#f87171',
    },
  };

  const styles = getStyles(colors);

  return (
    <NavigationContainer theme={navigationTheme}>
      <Tab.Navigator
        screenOptions={({ route }) => ({
          headerShown: false,
          tabBarStyle: styles.tabBar,
          tabBarActiveTintColor:   colors.cyan,
          tabBarInactiveTintColor: colors.textMuted,
          tabBarShowLabel: true,
          tabBarLabelStyle: styles.tabLabel,
          tabBarIcon: ({ focused, color, size }) => {
            const tab = TABS.find(t => t.name === route.name);
            const iconName = focused ? tab.iconActive : tab.icon;
            return <Ionicons name={iconName} size={22} color={color} />;
          },
        })}
      >
        {TABS.map(tab => (
          <Tab.Screen
            key={tab.name}
            name={tab.name}
            component={tab.component}
            options={tab.name === 'Feeds' ? { unmountOnBlur: true } : {}}
          />
        ))}
        <Tab.Screen
          name="BotDetail"
          component={ControlScreen}
          options={{
            tabBarButton: () => null,
            tabBarItemStyle: { display: 'none' },
          }}
        />
      </Tab.Navigator>
    </NavigationContainer>
  );
}

const getStyles = (colors) => StyleSheet.create({
  tabBar: {
    backgroundColor: colors.bg0,
    borderTopWidth: 0.5,
    borderTopColor: colors.border,
    height: 60,
    paddingBottom: 8,
    paddingTop: 6,
  },
  tabLabel: {
    fontSize: Typography.xs,
    fontWeight: Typography.medium,
  },
});
