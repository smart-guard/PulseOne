/**
 * Utility functions for handling data point values and blob storage references.
 */

/**
 * Checks if a value is a blob file reference (file://).
 */
export const isBlobValue = (value: any): boolean => {
    if (typeof value !== 'string') return false;
    return value.startsWith('file://');
};

/**
 * Extracts the filename from a blob URI.
 * Example: file:///app/data/blobs/test.bin -> test.bin
 */
export const getBlobId = (value: string): string => {
    if (!isBlobValue(value)) return '';
    const parts = value.split('/');
    return parts[parts.length - 1];
};

/**
 * Generates the backend API URL for downloading a blob.
 */
export const getBlobDownloadUrl = (value: string): string => {
    const fileId = getBlobId(value);
    if (!fileId) return '';

    // Use absolute path or relative depending on environment
    // Assuming API is at /api/blobs/
    return `/api/blobs/${fileId}`;
};

/**
 * Formats a value for display.
 * If it's a blob, returns a placeholder text.
 */
export const formatDisplayValue = (value: any, quality?: string): string => {
    if (value === null || value === undefined) return '—';

    if (isBlobValue(value)) {
        return 'FILE DATA';
    }

    if (typeof value === 'number') {
        return value.toLocaleString(undefined, {
            minimumFractionDigits: 0,
            maximumFractionDigits: 3
        });
    }

    return String(value);
};
