// ============================================================================
// frontend/src/i18n/index.ts
// [translated comment]
// ============================================================================

import i18n from 'i18next';
import { initReactI18next } from 'react-i18next';
import LanguageDetector from 'i18next-browser-languagedetector';
import HttpBackend from 'i18next-http-backend';

// [translated comment]
export const SUPPORTED_LANGUAGES = [
    { code: 'ko', name: '한국어', flag: '🇰🇷' },
    { code: 'en', name: 'English', flag: '🇺🇸' },
];

// [translated comment]
export const NAMESPACES = [
    'common',
    'sidebar',
    'devices',
    'alarms',
    'dashboard',
    'monitor',
    'settings',
    // Management pages
    'deviceTemplates',
    'manufacturers',
    'protocols',
    'sites',
    'tenants',
    // Feature pages
    'virtualPoints',
    'control',
    'backup',
    // Admin / system pages
    'network',
    'auditLog',
    'permissions',
    'dataExplorer',
    'historicalData',
    'dataExport',
    // Existing extended namespaces
    'systemSettings',
    'errorMonitoring',
    'loginHistory',
    'userManagement',
    'tenantManagement',
    'config',
];

i18n
    .use(HttpBackend)
    .use(LanguageDetector)
    .use(initReactI18next)
    .init({
        // [translated comment]
        fallbackLng: 'en',
        defaultNS: 'common',
        ns: NAMESPACES,

        // [translated comment]
        detection: {
            order: ['localStorage', 'navigator', 'htmlTag'],
            lookupLocalStorage: 'pulseone_lang',
            caches: ['localStorage'],
        },

        // [translated comment]
        backend: {
            loadPath: '/locales/{{lng}}/{{ns}}.json',
        },

        interpolation: {
            escapeValue: false, // [translated comment]
        },

        // [translated comment]
        saveMissing: import.meta.env.DEV,
        missingKeyHandler: import.meta.env.DEV
            ? (lngs: readonly string[], ns: string, key: string) => {
                console.warn(`[i18n] Missing key: ${ns}:${key} (${lngs.join(',')})`);
            }
            : undefined,
    });

export default i18n;
