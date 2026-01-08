// backend/lib/middleware/cacheControl.js - 브라우저 캐시 방지 미들웨어
const path = require('path');

/**
 * 브라우저 캐시 제어 미들웨어
 * Windows Chrome에서 Ctrl+F5 없이도 항상 최신 버전 로드
 */
class CacheControlMiddleware {
    constructor(options = {}) {
        this.options = {
            // 개발 모드에서는 캐시 완전 비활성화
            disableCacheInDev: options.disableCacheInDev !== false,
            // 정적 파일 캐시 설정
            staticFileMaxAge: options.staticFileMaxAge || 0,
            // API 응답 캐시 설정
            apiCacheMaxAge: options.apiCacheMaxAge || 0,
            // HTML 파일은 항상 캐시하지 않음
            htmlNoCache: options.htmlNoCache !== false,
            // 버전 쿼리 파라미터 추가
            addVersionParam: options.addVersionParam !== false,
            // ETag 비활성화
            disableETag: options.disableETag !== false,
            ...options
        };
    }

    /**
     * Express 미들웨어 팩토리
     */
    createMiddleware() {
        return (req, res, next) => {
            // 개발 모드에서 모든 캐시 비활성화
            if (this.options.disableCacheInDev && process.env.NODE_ENV === 'development') {
                this.setNoCacheHeaders(res);
                next();
                return;
            }

            // URL 경로 분석
            const pathname = req.path;
            const isAPI = pathname.startsWith('/api/');
            const isStaticFile = this.isStaticFile(pathname);
            const isHTML = pathname.endsWith('.html') || pathname === '/' || 
                          (!pathname.includes('.') && !isAPI);

            // 파일 타입별 캐시 설정
            if (isAPI) {
                this.setAPICacheHeaders(res);
            } else if (isHTML) {
                this.setHTMLCacheHeaders(res);
            } else if (isStaticFile) {
                this.setStaticFileCacheHeaders(res, pathname);
            } else {
                this.setDefaultCacheHeaders(res);
            }

            next();
        };
    }

    /**
     * 캐시 완전 비활성화 헤더
     */
    setNoCacheHeaders(res) {
        res.set({
            'Cache-Control': 'no-cache, no-store, must-revalidate, private, max-age=0',
            'Pragma': 'no-cache',
            'Expires': '0',
            'Last-Modified': new Date().toUTCString(),
            'ETag': this.generateETag(),
            // Windows Chrome 특화 헤더
            'X-Accel-Expires': '0',
            'Surrogate-Control': 'no-store',
            // Internet Explorer 호환성
            'X-UA-Compatible': 'IE=edge'
        });

        // ETag 비활성화 (Express 기본 ETag 제거)
        if (this.options.disableETag) {
            res.removeHeader('ETag');
        }
    }

    /**
     * API 응답 캐시 헤더
     */
    setAPICacheHeaders(res) {
        res.set({
            'Cache-Control': `no-cache, no-store, must-revalidate, max-age=${this.options.apiCacheMaxAge}`,
            'Pragma': 'no-cache',
            'Expires': '0',
            'Last-Modified': new Date().toUTCString(),
            'Vary': 'Accept-Encoding, User-Agent',
            // CORS 헤더도 함께 설정
            'Access-Control-Allow-Origin': '*',
            'Access-Control-Allow-Methods': 'GET, POST, PUT, DELETE, OPTIONS',
            'Access-Control-Allow-Headers': 'Content-Type, Authorization, X-Requested-With'
        });
    }

    /**
     * HTML 파일 캐시 헤더 (항상 새로고침)
     */
    setHTMLCacheHeaders(res) {
        res.set({
            'Cache-Control': 'no-cache, no-store, must-revalidate, private',
            'Pragma': 'no-cache',
            'Expires': '0',
            'Last-Modified': new Date().toUTCString(),
            'ETag': this.generateETag(),
            // HTML 특화 헤더
            'X-Frame-Options': 'DENY',
            'X-Content-Type-Options': 'nosniff',
            'X-XSS-Protection': '1; mode=block'
        });
    }

    /**
     * 정적 파일 캐시 헤더
     */
    setStaticFileCacheHeaders(res, pathname) {
        const ext = path.extname(pathname).toLowerCase();
        
        // CSS/JS 파일은 강제 새로고침
        if (['.css', '.js'].includes(ext)) {
            res.set({
                'Cache-Control': 'no-cache, must-revalidate, max-age=0',
                'Pragma': 'no-cache',
                'Expires': '0',
                'Last-Modified': new Date().toUTCString(),
                'ETag': this.generateVersionedETag(pathname)
            });
        }
        // 이미지 파일은 짧은 캐시
        else if (['.png', '.jpg', '.jpeg', '.gif', '.svg', '.ico'].includes(ext)) {
            res.set({
                'Cache-Control': `public, max-age=${this.options.staticFileMaxAge || 300}`,
                'Last-Modified': new Date().toUTCString()
            });
        }
        // 기타 파일
        else {
            this.setDefaultCacheHeaders(res);
        }
    }

    /**
     * 기본 캐시 헤더
     */
    setDefaultCacheHeaders(res) {
        res.set({
            'Cache-Control': 'no-cache, must-revalidate',
            'Pragma': 'no-cache',
            'Last-Modified': new Date().toUTCString()
        });
    }

    /**
     * 정적 파일 여부 확인
     */
    isStaticFile(pathname) {
        const staticExtensions = [
            '.css', '.js', '.png', '.jpg', '.jpeg', '.gif', '.svg', '.ico',
            '.woff', '.woff2', '.ttf', '.eot', '.otf', '.mp4', '.webm', '.ogg',
            '.mp3', '.wav', '.flac', '.aac', '.pdf', '.doc', '.docx', '.zip'
        ];
        
        const ext = path.extname(pathname).toLowerCase();
        return staticExtensions.includes(ext);
    }

    /**
     * 고유 ETag 생성
     */
    generateETag() {
        const timestamp = Date.now();
        const random = Math.random().toString(36).substring(2);
        return `"${timestamp}-${random}"`;
    }

    /**
     * 버전 기반 ETag 생성
     */
    generateVersionedETag(pathname) {
        const version = process.env.APP_VERSION || '1.0.0';
        const timestamp = Date.now();
        const pathHash = Buffer.from(pathname).toString('base64').substring(0, 8);
        return `"v${version}-${timestamp}-${pathHash}"`;
    }
}

/**
 * 정적 파일 서빙 미들웨어 (버전 파라미터 추가)
 */
function createVersionedStaticMiddleware(staticPath, options = {}) {
    const express = require('express');
    
    return (req, res, next) => {
        // URL에 버전 파라미터 추가 (cache busting)
        const version = process.env.APP_VERSION || Date.now().toString();
        
        // 이미 버전 파라미터가 있으면 패스
        if (req.query.v || req.query.t) {
            return express.static(staticPath, options)(req, res, next);
        }
        
        // CSS/JS 파일에 버전 파라미터 추가
        const pathname = req.path;
        const ext = path.extname(pathname).toLowerCase();
        
        if (['.css', '.js'].includes(ext)) {
            req.url += (req.url.includes('?') ? '&' : '?') + `v=${version}`;
        }
        
        express.static(staticPath, options)(req, res, next);
    };
}

/**
 * SPA (Single Page Application) 캐시 방지 미들웨어
 */
function createSPACacheMiddleware() {
    return (req, res, next) => {
        // SPA 라우팅을 위한 모든 non-API, non-static 요청 처리
        if (!req.path.startsWith('/api/') && 
            !req.path.includes('.') && 
            req.method === 'GET') {
            
            // 강력한 캐시 방지 헤더 설정
            res.set({
                'Cache-Control': 'no-cache, no-store, must-revalidate, private, max-age=0',
                'Pragma': 'no-cache',
                'Expires': '0',
                'Last-Modified': new Date().toUTCString(),
                'ETag': `"spa-${Date.now()}-${Math.random().toString(36).substring(2)}"`,
                // Vary 헤더로 다양한 조건에서 캐시 방지
                'Vary': 'Accept-Encoding, User-Agent, Accept-Language, Cookie'
            });
        }
        
        next();
    };
}

/**
 * Windows Chrome 특화 캐시 버스팅
 */
function createChromeCacheBuster() {
    return (req, res, next) => {
        // User-Agent에서 Chrome 감지
        const userAgent = req.get('User-Agent') || '';
        const isChrome = userAgent.includes('Chrome') && !userAgent.includes('Edg');
        const isWindows = userAgent.includes('Windows');
        
        if (isChrome && isWindows) {
            // Chrome + Windows 조합에서 특별한 헤더 추가
            res.set({
                'Cache-Control': 'no-cache, no-store, must-revalidate, private, max-age=0, s-maxage=0',
                'Pragma': 'no-cache',
                'Expires': 'Thu, 01 Jan 1970 00:00:00 GMT',
                'Last-Modified': new Date().toUTCString(),
                'ETag': `"chrome-win-${Date.now()}"`,
                // Chrome 특화 헤더
                'Clear-Site-Data': '"cache", "storage"',
                'X-Chrome-No-Cache': '1'
            });
        }
        
        next();
    };
}

module.exports = {
    CacheControlMiddleware,
    createVersionedStaticMiddleware,
    createSPACacheMiddleware,
    createChromeCacheBuster
};