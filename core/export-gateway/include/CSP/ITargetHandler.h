/**
 * @file ITargetHandler.h
 * @brief 타겟 핸들러 공통 인터페이스 (ExportTypes.h로 통합됨)
 * @author PulseOne Development Team
 * @date 2025-10-23
 * @version 1.1.0
 * 저장 위치: core/export-gateway/include/CSP/ITargetHandler.h
 *
 * ⚠️ 중요: ITargetHandler 인터페이스는 Export/ExportTypes.h로 통합되었습니다.
 * 이 파일은 하위 호환성을 위해 유지되며, ExportTypes.h의 타입을 별칭으로
 * 제공합니다.
 */

#ifndef ITARGET_HANDLER_H
#define ITARGET_HANDLER_H

#include "Export/ExportTypes.h"

namespace PulseOne {
namespace CSP {

// Export 네임스페이스의 타입을 CSP 네임스페이스로 가져옴
using ITargetHandler = PulseOne::Export::ITargetHandler;
using TargetSendResult = PulseOne::Export::TargetSendResult;

} // namespace CSP
} // namespace PulseOne

#endif // ITARGET_HANDLER_H