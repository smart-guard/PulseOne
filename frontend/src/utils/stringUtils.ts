/**
 * Masks an email address for privacy.
 * Example: test@example.com -> te**@example.com
 */
export const maskEmail = (email: string): string => {
    if (!email || !email.includes('@')) return email;

    const [localPart, domain] = email.split('@');

    if (localPart.length <= 2) {
        return `${localPart}***@${domain}`;
    }

    const visiblePart = localPart.substring(0, 2); // Show first 2 chars
    return `${visiblePart}**@${domain}`;
};
